// ---------------------------------------------------------------------------
// What of the light of the frame a drop keeps, out of how far the frame behind it
// is: a light of the frame is a light of the screen, and the light of a city in
// the distance is not the light that falls on the glass in front of a camera.
// What a drop of the rain takes is the light of the street it is on and not the
// light of the skyline behind it.
//
// It is one pass of the field of the light rather than one of every drop of the
// rain, so this reads ONE texel of the field rather than the frame: a light of a
// drop is a light of the area of the frame around it (see lightPS8.hlsl), and the
// depth of the frame is read the same way.
//
// A game that hands out no depth of its own to read - which is every game but the
// ones that replace their depth buffer with a texture, see DepthStencil.ixx of the
// widescreen fix of True Crime: New York City - leaves this pass out entirely and
// takes the light of the frame as it is.
//
//   TEXCOORD0  the light of the area of the frame around the drop
//   TEXCOORD1  and how far the frame behind the drop is
//   c0.x       what of the depth of the frame still counts as the street a drop
//              is on, and c0.y what is left of it once it is not
// ---------------------------------------------------------------------------
sampler lightSampler : register(s0);
sampler depthSampler : register(s1);
float4 params : register(c0);

float4 main(float4 light : TEXCOORD0, float4 depth : TEXCOORD1) : COLOR
{
    // The depth of a texture of the kind a game hands out to be read runs the other
    // way round to the depth buffer it replaces: what is close to the camera is one
    // and what is far is zero, so this is what is close and not what is far.
    float close = saturate(tex2D(depthSampler, depth.xy).r * params.x + params.y);

    return tex2D(lightSampler, light.xy) * close;
}
