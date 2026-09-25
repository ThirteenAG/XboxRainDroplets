
static const int LightCellCount = 6;

sampler2D sceneSampler : register(s0);
float4 field : register(c0); // inverse width/height of the field, cells, radius

void GatherTap(float2 uv, float falloff, inout float3 energy, inout float3 plain, inout float weight, inout float falloffSum)
{
    // Outside-screen taps must not replicate a border lamp via clamp sampling.
    // Normalize both averages by the same remaining, visible part of the kernel.
    falloff *= step(0, uv.x) * step(uv.x, 1) * step(0, uv.y) * step(uv.y, 1);
    falloffSum += falloff;
    float3 source = tex2Dlod(sceneSampler, float4(uv, 0, 0)).rgb;
    float brightness = max(source.r, max(source.g, source.b));
    float saturation = (brightness - min(source.r, min(source.g, source.b))) / max(brightness, 0.001);
    float w = brightness * brightness * (1 + saturation) * falloff;
    energy += source * w;
    plain += source * falloff;
    weight += w;
}

float4 PSMain(float4 color : COLOR0, float2 atlas : TEXCOORD0, float2 scene : TEXCOORD1,
    float2 pixel : VPOS) : COLOR0
{
    // Where this texel of the field is on the target, in the very coordinates the
    // copy of the frame is in: the field and the frame are of the same picture,
    // which is what makes one tap of the field enough for a drop. The crop a game
    // asks the refraction to sample is not applied to it: a light is where it is
    // on the screen.
    float2 lightUV = (pixel + 0.5) * field.xy;
    float2 radius = float2(field.w * field.x / field.y, field.w);
    float2 cell = radius / field.z;
    float2 anchor = floor(lightUV / cell) * cell;

    float3 energy = 0, plain = 0;
    float weight = 0, falloffSum = 0;

    [loop] for (int y = -LightCellCount; y <= LightCellCount; ++y)
    {
        [loop] for (int x = -LightCellCount; x <= LightCellCount; ++x)
        {
            float2 corner = anchor + float2(x, y) * cell;
            float2 offset = (corner - lightUV) / radius;
            float distance2 = dot(offset, offset);
            float falloff = exp2(-2 * distance2) * (1 - smoothstep(0.75, 1, distance2));
            GatherTap(corner + float2(-0.25, -0.25) * cell, falloff, energy, plain, weight, falloffSum);
            GatherTap(corner + float2( 0.25, -0.25) * cell, falloff, energy, plain, weight, falloffSum);
            GatherTap(corner + float2(-0.25,  0.25) * cell, falloff, energy, plain, weight, falloffSum);
            GatherTap(corner + float2( 0.25,  0.25) * cell, falloff, energy, plain, weight, falloffSum);
        }
    }

    float3 gathered = energy / (weight + 0.5);
    float3 average = plain / max(falloffSum, 0.001);

    // What is left of the frame around the light is the light itself, and the
    // colour of it is not pushed out of its own grey here: a drop may not be
    // brighter than the light it took, and the field is where both are kept.
    return float4(max(gathered - average * 1.25, 0), 1);
}
