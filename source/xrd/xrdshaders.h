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
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.position = mul(projection, float4(input.position, 1.0f));
    output.color = input.color;
    output.atlas = input.atlas;
    output.scene = input.scene * uvScale.xy + uvOffset.xy;
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

    color.rgb *= backdrop;
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
