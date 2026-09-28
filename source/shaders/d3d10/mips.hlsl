// ---------------------------------------------------------------------------
// One level of the mip chain of the copy of the frame, on Direct3D 12, which has
// no call that generates a chain: every level is drawn out of the level above
// it, over the whole of the level, with one bilinear tap at the middle of every
// texel. The tap sits between the four texels of the level above that the texel
// covers, so it is their average, which is what a box filter is.
//
// The vertex shader makes the triangle that covers the level out of nothing, no
// vertex buffer is bound for it, see D3D12Backend::GenerateSceneMips.
// ---------------------------------------------------------------------------
Texture2D source : register(t0);
SamplerState linearClamp : register(s0);

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOutput VSMain(uint id : SV_VertexID)
{
    VSOutput output;
    float2 uv = float2((id << 1) & 2, id & 2);
    output.uv = uv;
    output.position = float4(uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return output;
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    return source.SampleLevel(linearClamp, input.uv, 0.0f);
}
