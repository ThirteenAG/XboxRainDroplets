sampler texsampler;

float4 main(float2 texcoord : TEXCOORD) : COLOR
{
    return 0.0204081628 * (tex2D(texsampler, texcoord - float2(0.0029296875, 0.00390625)) +
        tex2D(texsampler, texcoord - float2(0.001953125, 0.00390625)) +
        tex2D(texsampler, texcoord - float2(0.0009765625, 0.00390625)) +
        tex2D(texsampler, float2(texcoord.x, texcoord.y - 0.00390625)) +
        tex2D(texsampler, float2(0.0009765625 + texcoord.x, texcoord.y - 0.00390625)) +
        tex2D(texsampler, float2(0.001953125 + texcoord.x, texcoord.y - 0.00390625)) +
        tex2D(texsampler, float2(0.0029296875 + texcoord.x, texcoord.y - 0.00390625)) +
        tex2D(texsampler, texcoord - float2(0.0029296875, 0.00260416674)) +
        tex2D(texsampler, texcoord - float2(0.001953125, 0.00260416674)) +
        tex2D(texsampler, texcoord - float2(0.0009765625, 0.00260416674)) +
        tex2D(texsampler, float2(texcoord.x, texcoord.y - 0.00260416674)) +
        tex2D(texsampler, float2(0.0009765625 + texcoord.x, texcoord.y - 0.00260416674)) +
        tex2D(texsampler, float2(0.001953125 + texcoord.x, texcoord.y - 0.00260416674)) +
        tex2D(texsampler, float2(0.0029296875 + texcoord.x, texcoord.y - 0.00260416674)) +
        tex2D(texsampler, texcoord - float2(0.0029296875, 0.00130208337)) +
        tex2D(texsampler, texcoord - float2(0.001953125, 0.00130208337)) +
        tex2D(texsampler, texcoord - float2(0.0009765625, 0.00130208337)) +
        tex2D(texsampler, float2(texcoord.x, texcoord.y - 0.00130208337)) +
        tex2D(texsampler, float2(0.0009765625 + texcoord.x, texcoord.y - 0.00130208337)) +
        tex2D(texsampler, float2(0.001953125 + texcoord.x, texcoord.y - 0.00130208337)) +
        tex2D(texsampler, float2(0.0029296875 + texcoord.x, texcoord.y - 0.00130208337)) +
        tex2D(texsampler, float2(texcoord.x - 0.0029296875, texcoord.y)) +
        tex2D(texsampler, float2(texcoord.x - 0.001953125, texcoord.y)) + 
        tex2D(texsampler, float2(texcoord.x - 0.0009765625, texcoord.y)) + 
        tex2D(texsampler, texcoord) + tex2D(texsampler, float2(0.0009765625 + texcoord.x, texcoord.y)) + 
        tex2D(texsampler, float2(0.001953125 + texcoord.x, texcoord.y)) + 
        tex2D(texsampler, abs(float2(-0.0029296875, 0)) + texcoord) + 
        tex2D(texsampler, float2(texcoord.x - 0.0029296875, 0.00130208337 + texcoord.y)) + 
        tex2D(texsampler, float2(texcoord.x - 0.001953125, 0.00130208337 + texcoord.y)) + 
        tex2D(texsampler, float2(texcoord.x - 0.0009765625, 0.00130208337 + texcoord.y)) + 
        tex2D(texsampler, abs(float2(0, -0.00130208337)) + texcoord) + 
        tex2D(texsampler, float2(0.0009765625, 0.00130208337) + texcoord) + 
        tex2D(texsampler, float2(0.001953125, 0.00130208337) + texcoord) + 
        tex2D(texsampler, float2(0.0029296875, 0.00130208337) + texcoord) + 
        tex2D(texsampler, float2(texcoord.x - 0.0029296875, 0.00260416674 + texcoord.y)) + 
        tex2D(texsampler, float2(texcoord.x - 0.001953125, 0.00260416674 + texcoord.y)) + 
        tex2D(texsampler, float2(texcoord.x - 0.0009765625, 0.00260416674 + texcoord.y)) + 
        tex2D(texsampler, abs(float2(0, -0.00260416674)) + texcoord) + 
        tex2D(texsampler, float2(0.0009765625, 0.00260416674) + texcoord) + 
        tex2D(texsampler, float2(0.001953125, 0.00260416674) + texcoord) + 
        tex2D(texsampler, float2(0.0029296875, 0.00260416674) + texcoord) + 
        tex2D(texsampler, float2(texcoord.x - 0.0029296875, 0.00390625 + texcoord.y)) + 
        tex2D(texsampler, float2(texcoord.x - 0.001953125, 0.00390625 + texcoord.y)) + 
        tex2D(texsampler, float2(texcoord.x - 0.0009765625, 0.00390625 + texcoord.y)) + 
        tex2D(texsampler, abs(float2(0, -0.00390625)) + texcoord) + tex2D(texsampler, float2(0.0009765625, 0.00390625) + texcoord) + 
        tex2D(texsampler, float2(0.001953125, 0.00390625) + texcoord) + tex2D(texsampler, float2(0.0029296875, 0.00390625) + texcoord));
}
