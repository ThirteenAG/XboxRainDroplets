
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
