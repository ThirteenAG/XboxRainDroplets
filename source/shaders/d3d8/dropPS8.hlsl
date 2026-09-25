// ---------------------------------------------------------------------------
// A drop of the rain on Direct3D 8, where the shaders of Direct3D 9 and above
// cannot be used: the shape of the drop out of the atlas, the copy of the frame
// behind it, and the light that drop gathered out of the frame around it, in one
// pass of a shader of model 1.
//
// It is the maths of source/shaders/d3d9/drops.hlsl, written for a shader
// that has four texture coordinate registers, eight arithmetic instructions and
// no branches at all. The light is not found per pixel here - a shader of model
// 1 cannot afford the taps that needs - it is gathered into a field an eighth of
// the target in each direction before the drops are drawn (see lightPS8.hlsl and
// blurPS8.hlsl) and this shader reads that field with one tap.
//
//   COLOR0     the colour and the alpha of the drop
//   COLOR1.w   1 when the drop is a lens, 0 when it is not
//   TEXCOORD0  the shape of the drop in the atlas of shapes
//   TEXCOORD1  where in the copy of the frame behind the drop it samples
//   TEXCOORD2  where on the screen the drop is, which is where its light is
//   c0.x       how much of the light of the frame a drop of clear water takes
// ---------------------------------------------------------------------------
sampler maskSampler  : register(s0);
sampler sceneSampler : register(s1);
sampler lightSampler : register(s2);
float4 params : register(c0);

float4 main(float4 color : COLOR0, float4 lens : COLOR1, float4 atlas : TEXCOORD0,
    float4 scene : TEXCOORD1, float4 screen : TEXCOORD2) : COLOR
{
    float4 mask = tex2D(maskSampler, atlas.xy);
    float3 frame = tex2D(sceneSampler, scene.xy).rgb;
    float3 light = tex2D(lightSampler, screen.xy).rgb * lens.w;

    // How much of the light of the frame a drop of clear water takes: the 1.6 of
    // the renderers of Direct3D 9 and above does not fit in a constant of a
    // shader of model 1, so it is one here and the part of it over one is added
    // separately. c0.x of this shader is that part.
    light = light + light * params.x;

    // What the drop shows of the frame behind it is lifted by the light, and by
    // the part of it the frame is missing: a drop in a frame that is already
    // bright does not turn white, and the colour of a lamp still comes through
    // where the frame around the drop is dark. The same thing the renderers of
    // Direct3D 10 and above do with backdrop + light * (1 - backdrop).
    return color * mask * float4(frame + light - frame * light, 1);
}
