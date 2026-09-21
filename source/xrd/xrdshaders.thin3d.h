#pragma once
// ---------------------------------------------------------------------------
// The shaders of the backend that draws with the drawing of the emulator.
//
// The same maths as every other backend of the effect - the vertex colour is
// modulated with the atlas of the drop shapes and then with the copy of the
// frame behind the drop - but in the shaders the drawing of the emulator asks
// for, see Common/GPU/thin3d.h:
//
//     OpenGL         GLSL, the words and the version come from the description
//                    the drawing carries, see ShaderLanguageDesc. An old
//                    context gets the attribute/varying of version 120, a core
//                    profile the in/out of version 150 and newer.
//     Direct3D 11    HLSL, shader model 5, compiled by the drawing.
//     Vulkan         SPIR-V, see xrdspirv.thin3d.h and the sources under
//                    shaders/thin3d.
//
// The vertices are handed over in the layout of the drawing, which knows one
// name for the second texture coordinate set only on Direct3D: the atlas and
// the copy of the frame therefore travel in the two halves of a single float4,
// which every backend of it binds to the same name.
//
//     location 0   vec3   Position    the position, in pixels of the frame
//     location 1   vec4   Color0      the colour of the drop (RGBA in memory)
//     location 3   vec4   TexCoord0   xy atlas, zw copy of the frame
//
// The colour of Xrd::Vertex is a D3DCOLOR in memory, which is BGRA, and the
// drawing takes it as RGBA: the backend swaps the two while it fills the vertex
// buffer, which is the only place the two conventions meet, see
// xrdrender.thin3d.h.
//
// The constants the pipelines all declare, and the names the drawing of the
// emulator looks up for them on a backend that has no constant buffer:
//
//     projection          mat4   the transform of the vertices
//     uvOffset            vec4   where the copy of the frame starts
//     uvScale             vec4   how much of it the drops use
//     sceneComplement     vec4   x picks the complement, y turns sampling off
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstring>
#include <string>

// The drawing of the emulator itself: the shaders are written for the language
// it asks for, and the words they are written with (attribute, varying, the
// output of the fragment shader) are what its own description of that language
// says. The header of that drawing is the copy of it in this repository, see
// external/ppsspp/README.md.
#include "Common/GPU/thin3d.h"

namespace Xrd
{
    namespace Shaders
    {
        namespace Thin3D
        {
            // The version directive of the GLSL of the context that is running.
            // The drawing reports the version of the context itself, and a
            // shader of an older version is refused by a core profile, so the
            // one of the context is what is written.
            inline std::string GlslVersion(const ShaderLanguageDesc& language)
            {
                char buffer[64] = {};

                if (language.gles)
                    snprintf(buffer, sizeof(buffer), "#version %d es\n", language.glslES30 ? 300 : 100);
                else
                    snprintf(buffer, sizeof(buffer), "#version %d\n", language.glslVersionNumber > 0 ? language.glslVersionNumber : 120);

                return buffer;
            }

            // The vertex shader: the position, the colour and the two texture
            // coordinates the fragment shader needs, with the copy of the frame
            // scaled and offset into the place of the frame the drops refract.
            inline std::string BuildVertexSource(const ShaderLanguageDesc& language)
            {
                std::string source = GlslVersion(language);
                source += language.attribute;
                source += " vec3 Position;\n";
                source += language.attribute;
                source += " vec4 Color0;\n";
                source += language.attribute;
                source += " vec4 TexCoord0;\n";
                source += "\nuniform mat4 projection;\nuniform vec4 uvOffset;\nuniform vec4 uvScale;\n";
                source += "\n";
                source += language.varying_vs;
                source += " vec4 vColor;\n";
                source += language.varying_vs;
                source += " vec2 vAtlas;\n";
                source += language.varying_vs;
                source += " vec2 vScene;\n";
                source += R"(
void main()
{
    gl_Position = projection * vec4(Position, 1.0);
    vColor = Color0;
    vAtlas = TexCoord0.xy;
    vScene = TexCoord0.zw * uvScale.xy + uvOffset.xy;
}
)";
                return source;
            }

            // The fragment shader: the shape of the drop, the frame behind it,
            // and the complement of that frame in the places the original code
            // kept bright instead of dark.
            inline std::string BuildFragmentSource(const ShaderLanguageDesc& language)
            {
                std::string source = GlslVersion(language);

                if (language.gles)
                    source += "precision mediump float;\n";

                source += "\n";
                source += language.varying_fs;
                source += " vec4 vColor;\n";
                source += language.varying_fs;
                source += " vec2 vAtlas;\n";
                source += language.varying_fs;
                source += " vec2 vScene;\n";
                source += "\nuniform sampler2D sceneTexture;\nuniform sampler2D maskTexture;\nuniform vec4 sceneComplement;\n";

                // A context that only knows gl_FragColor has no output to
                // declare, one of version 3 and newer has no gl_FragColor.
                const bool declaresOutput = language.fragColor0 && strcmp(language.fragColor0, "gl_FragColor") != 0;

                if (declaresOutput)
                {
                    source += "\nout vec4 ";
                    source += language.fragColor0;
                    source += ";\n";
                }

                source += "\nvoid main()\n{\n";
                source += "    vec4 mask = ";
                source += language.texture;
                source += "(maskTexture, vAtlas);\n";
                source += "    vec4 scene = ";
                source += language.texture;
                source += "(sceneTexture, vScene);\n";
                source += R"(
    vec4 color = vColor * mask;

    // sceneComplement.x picks the complement, .y turns the sampling off
    vec3 backdrop = vec3(1.0);
    if (sceneComplement.y > 0.5)
        backdrop = sceneComplement.x > 0.5 ? (vec3(1.0) - scene.rgb) : scene.rgb;

    color.rgb *= backdrop;
)";
                source += "    ";
                source += language.fragColor0;
                source += " = color;\n}\n";
                return source;
            }

            // Vulkan. The drawing of the emulator does not take SPIR-V at all: it takes the GLSL of
            // the shader and compiles it itself, with the rules of Vulkan and version 450, see
            // GLSLtoSPV in Common/GPU/Vulkan/VulkanContext.cpp of the emulator. So this is the
            // same shader as the one the GLSL above builds, with the bindings the descriptor set
            // layout of that drawing has (binding 0 is the constants, then one combined image
            // sampler per texture slot) and the vertex locations of the input layout of the
            // backend.
            inline const char* VulkanVertexSource = R"(
#version 450

layout(binding = 0, std140) uniform XrdConstants
{
    mat4 projection;
    vec4 uvOffset;
    vec4 uvScale;
    vec4 sceneComplement;
};

layout(location = 0) in vec3 Position;
layout(location = 1) in vec4 Color0;
layout(location = 3) in vec4 TexCoord0;

layout(location = 0) out vec4 vColor;
layout(location = 1) out vec2 vAtlas;
layout(location = 2) out vec2 vScene;

void main()
{
    gl_Position = projection * vec4(Position, 1.0);
    vColor = Color0;
    vAtlas = TexCoord0.xy;
    vScene = TexCoord0.zw * uvScale.xy + uvOffset.xy;
}
)";

            inline const char* VulkanPixelSource = R"(
#version 450

layout(binding = 0, std140) uniform XrdConstants
{
    mat4 projection;
    vec4 uvOffset;
    vec4 uvScale;
    vec4 sceneComplement;
};

layout(binding = 1) uniform sampler2D sceneTexture;
layout(binding = 2) uniform sampler2D maskTexture;

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec2 vAtlas;
layout(location = 2) in vec2 vScene;

layout(location = 0) out vec4 fragColor0;

void main()
{
    vec4 mask = texture(maskTexture, vAtlas);
    vec4 scene = texture(sceneTexture, vScene);

    vec4 color = vColor * mask;

    // sceneComplement.x picks the complement, .y turns the sampling off
    vec3 backdrop = vec3(1.0);
    if (sceneComplement.y > 0.5)
        backdrop = sceneComplement.x > 0.5 ? (vec3(1.0) - scene.rgb) : scene.rgb;

    color.rgb *= backdrop;
    fragColor0 = color;
}
)";

            // Direct3D 11. The constants are one constant buffer of the same four members, and the
            // two textures are sampled with one sampler, which the backend sets for both slots.
            //
            // One source per stage, not one for both: the drawing of the emulator compiles every
            // shader with the entry point "main" and with the stage given separately, see
            // Common/GPU/D3D11/thin3d_d3d11.cpp, so a vertex shader and a pixel shader cannot live
            // in the same source.
            inline const char* D3D11VertexSource = R"(
cbuffer XrdConstants : register(b0)
{
    float4x4 projection;
    float4 uvOffset;
    float4 uvScale;
    float4 sceneComplement;
};

struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
    float4 texcoord : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 atlas : TEXCOORD0;
    float2 scene : TEXCOORD1;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = mul(projection, float4(input.position, 1.0f));
    output.color = input.color;
    output.atlas = input.texcoord.xy;
    output.scene = input.texcoord.zw * uvScale.xy + uvOffset.xy;
    return output;
}
)";

            inline const char* D3D11PixelSource = R"(
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

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 atlas : TEXCOORD0;
    float2 scene : TEXCOORD1;
};

float4 main(PSInput input) : SV_TARGET
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
        }
    }
}
