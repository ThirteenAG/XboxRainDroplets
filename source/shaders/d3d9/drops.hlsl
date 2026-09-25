
sampler2D maskSampler : register(s0);
sampler2D sceneSampler : register(s1);
sampler2D lightSampler : register(s2);
float4 frame : register(c0); // inverse width/height, scene sampling, complement

float4 PSMain(float4 color : COLOR0, float2 atlas : TEXCOORD0, float2 scene : TEXCOORD1,
    float2 pixel : VPOS) : COLOR0
{
    bool lens = atlas.x < 0;
    if (lens) atlas.x += 2;
    float4 mask = tex2D(maskSampler, atlas);
    clip(mask.a * color.a - 0.001);
    float3 lightOut = 0;
    if (lens && frame.z > 0.5 && frame.w < 0.5)
    {
        float3 excess = tex2D(lightSampler, (pixel + 0.5) * frame.xy).rgb;

        // the same maths the other renderers do in their pixel shader
        float grey = dot(excess, float3(0.2126, 0.7152, 0.0722));
        float3 light = max(grey + (excess - grey) * 2.5, 0);
        float peak = max(light.r, max(light.g, light.b));
        lightOut = light * smoothstep(0.03, 0.20, peak);
    }
    float3 backdrop = 1;
    if (frame.z > 0.5)
    {
        backdrop = tex2D(sceneSampler, scene).rgb;
        if (frame.w > 0.5) backdrop = 1 - backdrop;
    }
    // Match the modern shader: preserve the refracted image under bright tint.
    float peak = max(lightOut.r, max(lightOut.g, lightOut.b)) * 1.6;
    float3 tint = lightOut * (1.6 * 0.8 / (0.8 + peak));
    float opacity = 1 - 0.2 * (peak * 0.8 / (0.8 + peak));
    return color * mask * float4(backdrop + tint * (1 - backdrop), opacity);
}
