#pragma once
// ---------------------------------------------------------------------------
// Shader sources.
//
// One small shader per API family, all of them doing the same thing as the
// two texture stages of the original code: modulate the vertex colour with the
// atlas of drop shapes, then modulate that with the copy of the frame behind
// the drop. Snow takes the complement of the frame instead, which is what kept
// it bright on the console versions.
//
//   Direct3D 8  fixed function, no shader at all (see the backend)
//   Direct3D 9  fixed function, no shader at all (see the backend)
//   Direct3D 10 shader model 4, compiled at runtime with the D3DCompiler
//   Direct3D 11 shader model 5, compiled at runtime with the D3DCompiler
//   Direct3D 12 shader model 5, compiled at runtime with the D3DCompiler
//   OpenGL      GLSL 1.20, compiled at runtime by the driver
//   Vulkan      SPIR-V, compiled while building with dxc
// ---------------------------------------------------------------------------

namespace Xrd
{
    namespace Shaders
    {
        // Direct3D 10 and above. The vertex buffer holds
        //   float3 position, uint color, float2 atlas, float2 scene
        // which is exactly the Vertex type.
        //
        // A drop of clear water is a lens of the colour of what is around it: the
        // vertex shader gathers the light of the frame around the drop and hands
        // it to the pixel shader, which adds it to the refracted backdrop. The
        // gather is per vertex because four vertices per drop are affordable
        // while a per pixel one would not be, and it is smooth enough to read as
        // the colour of a light, which is what it is for.
        //
        // Only the drops of clear rain gather light. The effect says so by
        // handing the atlas coordinate of such a drop over moved down by
        // AtlasLightMarker, see WaterDrops::AddToRenderList: the atlas is
        // sampled at whole numbers and above zero, so a negative coordinate is
        // one no drop of its own ever has.
        inline const char* D3D11Source = R"(
cbuffer XrdConstants : register(b0)
{
    float4x4 projection;
    float4 uvOffset;
    float4 uvScale;
    float4 sceneComplement;
};

Texture2D sceneTexture : register(t0);
Texture2D maskTexture : register(t1);
SamplerState linearClamp : register(s0);

// how far the atlas coordinate of a drop that gathers light was moved down,
// and back up again, see WaterDrops::AddToRenderList
static const float AtlasLightMarker = 2.0f;

// The gather is a grid of cells over a square around the drop, in fractions of
// the target, and every cell is the average of four samples of the whole cell
// rather than one sample of a point of it: a small light is then found wherever
// the cells happen to fall, so the colour a drop takes does not jump about as
// the scenery moves past it, which is what one sample per cell looks like.
static const float LightRadius = 0.14f;
static const int LightCells = 8;

// What a drop adds to what it shows is the light of the frame around it that
// stands out of that frame, and nothing else: a frame that is bright is not a
// light, so a drop in a bright frame is the colour of water. LightFloor is how
// much brighter than the frame around it a light has to be to count at all, and
// the ramp is how much of it there has to be to light a drop up in full: a lamp of
// a night city stands out of what is around it by a lot, so the ramp has to be
// over well below its kind of brightness or the whole of it never arrives.
static const float LightFloor = 1.25f;
static const float LightRampLow = 0.03f;
static const float LightRampHigh = 0.20f;

// How much of the colour of a light a drop keeps. What a lens does with a colour
// is to keep it, and what washes the colour out of a drop is the grey of the
// whole neighbourhood the light was measured against, so the colour that was left
// over is pushed back out of its own grey: a tail light is then a red drop and
// not a grey one with a warm tint, and a white light, which has no colour to push
// out, is left exactly as it was. Nothing about which places of the frame count
// as a light changes here, so a drop takes on no new colour from one frame to the
// next and nothing flickers.
static const float LightChroma = 2.5f;

struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
    float2 atlas : TEXCOORD0;
    float2 scene : TEXCOORD1;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 atlas : TEXCOORD0;
    float2 scene : TEXCOORD1;
    float3 light : TEXCOORD2;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.position = mul(projection, float4(input.position, 1.0f));
    output.color = input.color;
    output.atlas = input.atlas;
    output.scene = input.scene * uvScale.xy + uvOffset.xy;
    output.light = 0.0f;

    if (input.atlas.x < -0.5f)
    {
        // the atlas coordinate the drop really has, the marker is only a flag
        output.atlas.x += AtlasLightMarker;

        // where the drop is on the target, in the coordinates of the copy of the
        // frame the pixel shader samples. The uvOffset and uvScale of the
        // constants are the crop a game asks the refraction to sample, and they
        // are deliberately not applied here: a light is where it is on the
        // screen, and moving the field a light is looked up in by the crop of
        // the refraction would light every drop up from the wrong place.
        float2 lightUV = output.position.xy / output.position.w * float2(0.5f, -0.5f) + 0.5f;

        // the same reach around the drop in pixels, on a target of any shape
        float aspect = abs(projection[1][1] / projection[0][0]);
        float2 radius = float2(LightRadius / max(aspect, 0.1f), LightRadius);
        float2 cell = radius / LightCells;

        // The grid is aligned to the target and not to the drop, so a drop that
        // moves gathers its light from the very same places of the frame and the
        // field does not crawl over the scenery with it.
        float2 anchor = floor(lightUV / cell) * cell;

        // the light of the frame around the drop, the plain colour of it, and
        // how much of each there was found
        float3 energy = 0.0f;
        float3 plain = 0.0f;
        float weight = 0.0f;
        float falloffSum = 0.0f;

        [loop] for (int y = -LightCells; y <= LightCells; ++y)
        {
            [loop] for (int x = -LightCells; x <= LightCells; ++x)
            {
                float2 corner = anchor + float2(x, y) * cell;
                float2 offset = (corner - lightUV) / radius;
                float distance2 = dot(offset, offset);

                // a soft kernel that is cut off where it is small anyway, so the
                // edge of the reach of a drop is not a visible circle
                float falloff = exp2(-2.0f * distance2) * (1.0f - smoothstep(0.75f, 1.0f, distance2));

                // what the frame holds over the whole cell
                float3 source = sceneTexture.SampleLevel(linearClamp, corner + float2(-0.25f, -0.25f) * cell, 0).rgb;
                source += sceneTexture.SampleLevel(linearClamp, corner + float2(0.25f, -0.25f) * cell, 0).rgb;
                source += sceneTexture.SampleLevel(linearClamp, corner + float2(-0.25f, 0.25f) * cell, 0).rgb;
                source += sceneTexture.SampleLevel(linearClamp, corner + float2(0.25f, 0.25f) * cell, 0).rgb;
                source *= 0.25f;

                float brightness = max(source.r, max(source.g, source.b));
                float saturation = (brightness - min(source.r, min(source.g, source.b))) / max(brightness, 0.001f);

                // How much light there is in what the cell holds and how coloured
                // it is. There is deliberately no brightness a cell has to pass to
                // count: a light fades in with the distance like everything else,
                // and nothing about the colour of a drop flickers while a light
                // comes into its reach or leaves it.
                float w = brightness * brightness * (1.0f + saturation) * falloff;

                energy += source * w;
                plain += source * falloff;
                weight += w;
                falloffSum += falloff;
            }
        }

        // Two averages of the very same surroundings of the drop: the one weighted
        // by how much light is in them and the plain one. A frame that is bright
        // all over gives the same for both and adds nothing to a drop, which is
        // what keeps a drop in a bright frame the colour of water instead of
        // white, and only what stands out of the frame around the drop is added to
        // it, which is what a drop of water collects out of a lamp.
        float3 gathered = energy / (weight + 0.5f);
        float3 average = plain / max(falloffSum, 0.001f);

        // The frame itself is taken off, and a foot is put under what is left of
        // it, because a half of a frame that happens to be brighter than the other
        // is not a light either. What is left over is a lamp, a window, a sign, or
        // anything else that is brighter than everything near it.
        float3 excess = max(gathered - average * LightFloor, 0.0f);

        // The colour of it is pushed out of its own grey, which is what the water
        // of the drop does with a light: what is added is the colour of the lamp
        // and not the grey of the frame it was found in, so a drop next to the
        // tail light of a car is as red as the light is. Its brightness is left
        // where the gather put it, so a drop does not start to glow where the
        // scenery got brighter.
        float grey = dot(excess, float3(0.2126f, 0.7152f, 0.0722f));
        float3 light = max(grey + (excess - grey) * LightChroma, 0.0f);

        // What a drop takes from a light fades in as the drop comes near it
        // instead of switching on, which is what keeps the colour of a drop from
        // flickering while the scenery moves past it.
        float peak = max(light.r, max(light.g, light.b));

        output.light = light * smoothstep(LightRampLow, LightRampHigh, peak);
    }

    return output;
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    float4 mask = maskTexture.Sample(linearClamp, input.atlas);
    float4 scene = sceneTexture.Sample(linearClamp, input.scene);

    float4 color = input.color * mask;

    // sceneComplement.x picks the complement, .y turns the sampling off
    float3 backdrop = 1.0f;
    if (sceneComplement.y > 0.5f)
        backdrop = sceneComplement.x > 0.5f ? (1.0f - scene.rgb) : scene.rgb;

    // The drop shows the frame behind it, lifted by the light it gathered around
    // it. What it shows more of is the part the backdrop is missing, so a drop in
    // a frame that is already bright does not turn white, and the colour of a lamp
    // still comes through strong where the frame around the drop is dark.
    color.rgb *= backdrop + input.light * 1.6f * (1.0f - backdrop);
    return color;
}
)";

        // OpenGL, same maths with the GLSL of a 1.20 context, which every
        // driver that can run a game provides.
        inline const char* OpenGLVertexSource = R"(
#version 120

uniform mat4 projection;

attribute vec3 position;
attribute vec4 color;
attribute vec2 atlas;
attribute vec2 scene;

varying vec4 vColor;
varying vec2 vAtlas;
varying vec2 vScene;

void main()
{
    gl_Position = projection * vec4(position, 1.0);
    vColor = color;
    vAtlas = atlas;
    vScene = scene;
}
)";

        inline const char* OpenGLFragmentSource = R"(
#version 120

uniform sampler2D sceneTexture;
uniform sampler2D maskTexture;
uniform vec2 uvOffset;
uniform vec2 uvScale;
uniform float sceneComplement;

varying vec4 vColor;
varying vec2 vAtlas;
varying vec2 vScene;

void main()
{
    vec4 mask = texture2D(maskTexture, vAtlas);
    vec4 scene = texture2D(sceneTexture, vScene * uvScale + uvOffset);

    vec4 color = vColor * mask;
    vec3 backdrop = sceneComplement > 0.5 ? (vec3(1.0) - scene.rgb) : scene.rgb;

    color.rgb *= backdrop;
    gl_FragColor = color;
}
)";

        // The same shader again for a core profile context, which refuses the
        // gl_FragColor, the varying and the attribute of version 120. An
        // application that asks for a 3.2 context or newer only accepts this one.
        inline const char* OpenGLVertexSourceCore = R"(
#version 150

uniform mat4 projection;

in vec3 position;
in vec4 color;
in vec2 atlas;
in vec2 scene;

out vec4 vColor;
out vec2 vAtlas;
out vec2 vScene;

void main()
{
    gl_Position = projection * vec4(position, 1.0);
    vColor = color;
    vAtlas = atlas;
    vScene = scene;
}
)";

        inline const char* OpenGLFragmentSourceCore = R"(
#version 150

uniform sampler2D sceneTexture;
uniform sampler2D maskTexture;
uniform vec2 uvOffset;
uniform vec2 uvScale;
uniform float sceneComplement;

in vec4 vColor;
in vec2 vAtlas;
in vec2 vScene;

out vec4 fragColor;

void main()
{
    vec4 mask = texture(maskTexture, vAtlas);
    vec4 scene = texture(sceneTexture, vScene * uvScale + uvOffset);

    vec4 color = vColor * mask;
    vec3 backdrop = sceneComplement > 0.5 ? (vec3(1.0) - scene.rgb) : scene.rgb;

    color.rgb *= backdrop;
    fragColor = color;
}
)";

        // A tiny shader for the test applications, it only fills a rectangle
        // with one colour so there is a UI the drops have to stay behind.
        inline const char* OpenGLSimpleVertexSource = R"(
#version 120

uniform mat4 projection;

attribute vec3 position;
attribute vec4 color;

varying vec4 vColor;

void main()
{
    gl_Position = projection * vec4(position, 1.0);
    vColor = color;
}
)";

        inline const char* OpenGLSimpleFragmentSource = R"(
#version 120

varying vec4 vColor;

void main()
{
    gl_FragColor = vColor;
}
)";

        inline const char* D3D11SimpleSource = R"(
cbuffer SimpleConstants : register(b0)
{
    float4x4 projection;
};

struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

VSOutput SimpleVS(VSInput input)
{
    VSOutput output;
    output.position = mul(projection, float4(input.position, 1.0f));
    output.color = input.color;
    return output;
}

float4 SimplePS(VSOutput input) : SV_TARGET
{
    return input.color;
}
)";
    }
}
