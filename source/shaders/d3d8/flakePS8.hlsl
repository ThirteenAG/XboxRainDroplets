// ---------------------------------------------------------------------------
// A flake of snow on Direct3D 8, the way Forza Horizon 4 draws one as far as a
// shader of model 1 goes: see source/shaders/d3d10/drops.hlsl for the whole of
// it. The atlas is the one of the flakes of that game, a map of normals (rg),
// the frost of the ice (b) and the mask of the specks (a).
//
//   - a speck refracts the frame through its normal: the bend of the index of
//     0.8 and the scale of 3 is near enough a line in the normal here, 0.65 of
//     it in each direction of the picture, out of the middle of the texture,
//   - what it shows is darkened by its frost to a tenth at most,
//   - the outline of its specks is blurred the way the game blurs the flake: the
//     mask of the atlas is that already, see WaterDrops::SoftenFlakes.
//
// The blur of what the specks show and the levels of the frame by the frost are
// left out: the frame is a texture of one level here. A shader of model 1.1 cannot read a
// texture where another one says, so its flakes show the frame where they are,
// frosted.
//
//   COLOR0     the life of the flake, in its alpha
//   TEXCOORD0  the flake in the atlas
//   TEXCOORD1  where the pixel is in the copy of the frame
//   c0.xy      the bend of a normal of one: -0.65 times the crop of the frame
//              the game asks for (the texture holds half of the normal, and
//              the bend is added twice)
// ---------------------------------------------------------------------------
sampler maskSampler  : register(s0);
sampler sceneSampler : register(s1);
float4 params : register(c0);

#ifndef XRD_REFRACT
#define XRD_REFRACT 1
#endif

float4 main(float4 color : COLOR0, float4 atlas : TEXCOORD0, float4 scene : TEXCOORD1) : COLOR
{
    float4 ice = tex2D(maskSampler, atlas.xy);

#if XRD_REFRACT
    float2 bend = (ice.rg - 0.5) * params.xy;
    float3 frame = tex2D(sceneSampler, scene.xy + bend + bend).rgb;
#else
    float3 frame = tex2D(sceneSampler, scene.xy).rgb;
#endif

    return float4(frame - frame * ice.b * 0.9, color.a * ice.a);
}
