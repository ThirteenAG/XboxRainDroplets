// ---------------------------------------------------------------------------
// The light of the frame: what a small area of it holds and what the frame around
// that area holds, which is a lamp, a window, a sign, anything that stands out of
// the frame around it. The field this writes is an eighth of the target in each
// direction, which is the field the renderer of Direct3D 9 keeps its light in, and
// a drop reads it with one tap (see dropPS8.hlsl).
//
// source/shaders/d3d9/light.hlsl is what this is, with one difference:
// a shader of model 1 has no loops, so the two averages it compares are two blurred
// copies of the frame instead of a disc of taps each, see blurPS8.hlsl and
// D3D8Backend::RenderLightField. Comparing the frame itself against a blur of it
// would be an edge and not a light: it is a small area against the wider frame
// around it, and both are blurs.
//
//   TEXCOORD0  where in the narrow blur this texel is
//   TEXCOORD1  and where in the wide one
//   c0.z       minus the floor: what is taken off a light for being the frame
//              around it and not the light itself
//   c1.xyz     the grey of a colour, so the colour of a light can be pushed out
//              of it, which is what makes a drop near a lamp that colour
//   c2.x/.y    how much of its own colour the light keeps, and what is left of it
//   c2.z/.w    how fast the light of a drop fades in and where it starts to
//   c3.xyz     one, for the brightness of the light as a whole
//
// A constant of a shader of model 1 holds a value between minus one and one and
// nothing else: a device clamps what is written into it, so the whole of the
// colour of a light and of the fade in of it is not a constant here but one
// instruction more. The colour of a light is pushed out of its own grey as many
// times as it is worth doing, and the fade in is a line and not a curve, which is
// why the thresholds of the fade in are what is passed in rather than the curve.
//
// The fade in of the light is three instructions of the eight a shader of model 1
// has, which is one more than ps_1_1 can spend on this pass: it is built for
// ps_1_4, and the ps_1_1 blob is built from the same source with XRD_LIGHT_RAMP
// at zero (see the build of the shaders, premake5.lua).
// ---------------------------------------------------------------------------
#ifndef XRD_LIGHT_RAMP
#define XRD_LIGHT_RAMP 1
#endif

// the small area of the frame, and the frame around it
sampler gatheredSampler : register(s0);
sampler aroundSampler : register(s1);
float4 params : register(c0);
float3 luma : register(c1);
float4 chroma : register(c2);
float3 peak : register(c3);

float4 main(float4 gathered : TEXCOORD0, float4 around : TEXCOORD1) : COLOR
{
    // What the small area of the frame holds, weighed by how bright it is: what the
    // frame around it holds is the level of the frame, and what stands out of that level
    // is a light only where it IS bright. The square of it is that weighing, and it is
    // what the renderers above do with the brightness of a cell of their gather, without
    // which a night sky that is brighter on one side of the frame than on the other is a
    // light over half of the frame of a drop. A light of the size of a rear light of a
    // car is a texel of the field and a sky is all of it, so the two are told apart by
    // what they are worth and not by how wide they are.
    //
    // It is the square of the frame and not of its level: a mean of the frame squared is
    // what the renderers above weigh their gather by, and a brightness that is the mean
    // of a small area of the frame is the brightness of the lamp in it.
    float3 frame = tex2D(gatheredSampler, gathered.xy).rgb;
    frame = frame + frame * frame;

    float3 excess = saturate(frame + params.z * tex2D(aroundSampler, around.xy).rgb);

    // The colour of the light is pushed out of its own grey, so a drop next to
    // the tail light of a car is as red as the light is and not as grey as the
    // frame it was found in. Its brightness is left where the gather put it.
    //
    // What the renderers above do with 2.5 is two and a half times the light of a
    // small area less one and a half times the grey of the frame it was found in,
    // and a constant of a shader of model 1 holds no more than one, so the two and
    // a half is the light, what of it is beyond its own grey, and half of that
    // again. It is the whole of the light that is saturated at the end and not
    // every step of it: a light of one colour is what a step that carries the grey
    // of the frame along with it kills, and a green lamp left no light at all.
    float grey = dot(excess, luma);
    float3 beyond = excess - grey;
    float3 light = saturate(excess + beyond + beyond * chroma.y);

#if XRD_LIGHT_RAMP
    // What is left of the light is only a light where there is enough of it: a frame
    // of the night holds a sky that is brighter on one side of it than on the other,
    // and a small area of a bright sky stands out of a wide one by a little wherever
    // the sky is at its brightest - which is a whole half of the frame of a drop, and
    // is what a frame of drops looks like coloured all over on the side of it the sky
    // is bright on and nowhere else. A light of a rear light of a car stands out by
    // several times what a sky does and is what is left of this. Four of the grey of
    // the light is what a constant of a shader of model 1 can afford, and the part of
    // it that is taken off is what c2.w holds: see the ramp of the renderers above,
    // which is the same thing over the brightness of the light.
    //
    // What is ramped is not the grey of the light but the brightness of it: a light of
    // one colour has a grey of a fifth or a quarter of what it is worth, so a ramp over
    // the grey of it holds a red lamp of a car back three times what it holds a white
    // one back, and what a drop next to a red lamp is coloured by is what is left of
    // that. c3 is the brightness of the light, a half of every channel of it: a peak
    // of the channels of it is what the renderers above ramp on and a shader of model 1
    // has no max, and the half of the sum of them is the middle of the two.
    float ramp = dot(light, peak);
    return float4(light * saturate(ramp + ramp + ramp + ramp + chroma.w), 1);
#else
    return float4(light, 1);
#endif
}
