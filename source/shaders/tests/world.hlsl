
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
