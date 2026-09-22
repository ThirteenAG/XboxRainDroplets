// ---------------------------------------------------------------------------
// The square of a field of the frame, on Direct3D 8.
//
// The light of a drop is what the frame around it holds that stands out of that
// frame, and it is found as the difference of two averages of the frame: one of
// a small area of it and one of the frame around that area (see lightPS8.hlsl
// and D3D8Backend::RenderLightField). An average of the frame itself cannot see
// a light of the size of a rear light of a car: the light covers a small part of
// the reach of the average, so its brightness is diluted into the frame around
// it, and what is left of it in both averages is nearly the same.
//
// The averages are taken of the square of the frame instead. A light twice as
// bright then holds four times as much of the average, which is the weighting
// the renderers of Direct3D 9 and above gather a light with (see GatherTap in
// source/xrd/xrdshaders.h), and a frame that is the same all over still gives
// the same for both averages, so a frame that is bright is still not a light:
// what is left over is a light and nothing else.
//
//   TEXCOORD0  where in the field of the frame this texel of the square is
//
// The square of a frame that is dark all over is a small number and the field of
// it is a byte for every channel of the target, so the square is scaled up into
// the range a byte can hold as it is written. The scale is two doublings of the
// value itself and not a constant, because a constant of a shader of model 1
// holds no more than one and a device clamps what is passed into it.
// ---------------------------------------------------------------------------
sampler sourceSampler : register(s0);

float4 main(float4 source : TEXCOORD0) : COLOR
{
    float4 value = tex2D(sourceSampler, source.xy);
    value = value * value;
    value = value + value;
    value = value + value;
    return value;
}
