// ---------------------------------------------------------------------------
// The shader of the Vulkan backend.
//
// This is the basic atlas/backdrop modulation path. The descriptor bindings
// match VulkanBackend's pipeline layout.
//
//     binding 0  the constants (projection, scene texture coordinates, mode)
//     binding 1  the copy of the frame behind the drops
//     binding 2  the atlas of the drop shapes
//     binding 3  the sampler both textures use
//
// The vertex layout is the Xrd::Vertex of the renderer:
//     location 0  float3 position
//     location 1  float4 color      (B8G8R8A8_UNORM in memory)
//     location 2  float2 atlas      (into the drop shapes)
//     location 3  float2 scene      (into the copy of the frame)
//
// tools/BuildShaders.ps1 uses Vulkan SDK DXC when this source changes. Verified
// bytecode in source/shaders/generated/vulkan/spirv.h ships with the repository,
// so ordinary builds do not require the Vulkan SDK.
// ---------------------------------------------------------------------------

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

[[vk::binding(0, 0)]] cbuffer XrdConstants : register(b0)
{
    float4x4 projection;
    float4 uvOffset;
    float4 uvScale;
    float4 sceneComplement;
};

[[vk::binding(1, 0)]] Texture2D sceneTexture : register(t0);
[[vk::binding(2, 0)]] Texture2D maskTexture : register(t1);
[[vk::binding(3, 0)]] SamplerState linearClamp : register(s0);

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
