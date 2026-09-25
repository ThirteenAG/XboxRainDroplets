
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
