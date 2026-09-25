// ---------------------------------------------------------------------------
// A blurred copy of the frame, in a render target an eighth of the target in each
// direction: the four taps around every texel, at offsets the vertices of the
// pass hand in.
//
// A shader of model 1 has four texture coordinate registers and four texture
// instructions to spend on them, so this is what the reach of a wide gather is
// built out of: the pass is run more than once, from one small render target into
// the other, with a larger offset every time (see D3D8Backend::RenderLightField).
//
// Every tap is a sampler of its own on the stage of the coordinate it reads,
// which is what a shader of model 1 requires.
//
//   c0.x  the weight of one tap, a quarter for the average of the four
// ---------------------------------------------------------------------------
sampler tap0 : register(s0);
sampler tap1 : register(s1);
sampler tap2 : register(s2);
sampler tap3 : register(s3);
float4 params : register(c0);

float4 main(float4 right : TEXCOORD0, float4 left : TEXCOORD1,
    float4 up : TEXCOORD2, float4 down : TEXCOORD3) : COLOR
{
    return params.x * (tex2D(tap0, right.xy) + tex2D(tap1, left.xy) +
        tex2D(tap2, up.xy) + tex2D(tap3, down.xy));
}
