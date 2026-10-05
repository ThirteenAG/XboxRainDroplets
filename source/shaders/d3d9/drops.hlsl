
sampler2D maskSampler : register(s0);
sampler2D sceneSampler : register(s1);
sampler2D lightSampler : register(s2);
float4 frame : register(c0); // inverse width/height, scene sampling, complement
// how the drop is drawn, see the shader of Direct3D 10: unused, the blur, the
// shapes along a side of the atlas and the scale of the refraction
float4 drop : register(c1);
// the crop of the copy of the frame: scale and offset
float4 crop : register(c2);
// the size of a texel of the atlas
float4 atlasTexel : register(c3);

// ---------------------------------------------------------------------------
// The drop of water on the lens, the way Forza Horizon 4 draws it (taken from a
// GPU capture of the game: its drop shapes, its lens map and its composite).
//
// The atlas holds the shapes of the drops, and the alpha of a shape is the
// square root of the height of the dome of water it is. The slope of that dome
// is the normal of the surface of the drop, and the frame is refracted through
// it the way light goes into water: with an index of 0.8, the bent ray read the
// given scale of the frame away (7 for rain, about all of the picture across a
// drop, so a drop shows the picture turned round both ways). What the drop
// shows covers the frame behind it with the remaining life of the drop as its
// opacity. Nothing is added: no rim, no highlight, no shading.
//
// Forza works its drops out at a quarter of the resolution of the picture, a
// cell of four pixels of 1080 lines, and stretches them over it. Here the drop
// is worked out at every pixel, and the same softness comes from three things:
//
//   - the dome is blurred over a cell, so the edge of a drop and its normals
//     are as soft as Forza's,
//   - a drop of Forza holds all of the picture in a few of its cells, so what
//     it shows is the picture blurred down to those few cells: the frame is
//     read at the level of its chain that the refraction sweeps over a cell,
//   - Forza mixes its drops into the frame before the frame is tone mapped, in
//     light, so the sky and the lamps a drop shows outweigh the shade it shows:
//     four reads of the frame are turned back into light and averaged.
//
// A blur of zero is a cell of one pixel, the crisp drop.
// ---------------------------------------------------------------------------

float HeightAt(float2 at, float2 lo, float2 hi)
{
    float inside = all(at >= lo) && all(at <= hi) ? 1 : 0;
    float a = tex2Dlod(maskSampler, float4(clamp(at, lo, hi), 0, 0)).a;
    return a * a * inside;
}

// the height of the dome blurred over a footprint and its slope, see the
// shader of Direct3D 10
float3 SoftHeight(float2 at, float2 rx, float2 ry, float r, float2 lo, float2 hi)
{
    float h00 = HeightAt(at - rx - ry, lo, hi), h10 = HeightAt(at - ry, lo, hi), h20 = HeightAt(at + rx - ry, lo, hi);
    float h01 = HeightAt(at - rx, lo, hi), h11 = HeightAt(at, lo, hi), h21 = HeightAt(at + rx, lo, hi);
    float h02 = HeightAt(at - rx + ry, lo, hi), h12 = HeightAt(at + ry, lo, hi), h22 = HeightAt(at + rx + ry, lo, hi);
    float h = (4 * h11 + 2 * (h10 + h01 + h21 + h12) + h00 + h20 + h02 + h22) / 16;
    float gx = ((h20 + 2 * h21 + h22) - (h00 + 2 * h01 + h02)) / (8 * r);
    float gy = ((h02 + 2 * h12 + h22) - (h00 + 2 * h10 + h20)) / (8 * r);
    return float3(h, gx, gy);
}

float3 ToLight(float3 c) { c = min(c, 0.96); return c / (1 - c); }
float3 ToValue(float3 l) { return l / (1 + l); }

float4 PSMain(float4 color : COLOR0, float2 atlas : TEXCOORD0, float2 scene : TEXCOORD1,
    float2 pixel : VPOS) : COLOR0
{
    // the water a drop leaves is marked in v and drawn as a film, a drop that
    // gathers the lights around it is marked in u, see the shader of Direct3D 10
    bool trace = atlas.y < 0;
    if (trace) atlas.y += 2;
    bool lens = atlas.x < 0;
    if (lens) atlas.x += 2;

    float2 tile = 1 / max(drop.z, 1);
    float2 base = floor(atlas / tile) * tile;
    float2 lo = base + atlasTexel.xy;
    float2 hi = base + tile - atlasTexel.xy;

    float2 dx = ddx(atlas);
    float2 dy = ddy(atlas);
    float2 perPixel = float2(length(dx), length(dy)) / (tile * 0.6);

    float lines = 1 / frame.y;
    float blur = saturate(drop.y);
    float cell = max(1, blur * 4 * lines / 1080);

    float3 soft = SoftHeight(atlas, dx * cell, dy * cell, cell, lo, hi);
    float h = soft.x;
    float2 slope = soft.yz / perPixel;
    float cover = cell > 1.01 ? smoothstep(0, 0.45, h) : smoothstep(0.01, 0.2, h);

    float2 normal = -0.168 * slope;
    normal *= min(1, 0.5 / max(length(normal), 1e-6));

    // the water a drop leaves is a film along its path, see the shader of
    // Direct3D 10
    float2 strip = (atlas - base) / tile;
    float stripLength = 1 / max(length(float2(ddx(strip.x), ddy(strip.x))), 1e-5);
    float stripWidth = 1 / max(length(float2(ddx(strip.y), ddy(strip.y))), 1e-5);
    float halfWidth = 0.5 * stripWidth;
    float beyond = max(abs(strip.x - 0.5) * stripLength - max(0.5 * stripLength - halfWidth, 0), 0);
    float rim = length(float2(beyond, (strip.y - 0.5) * stripWidth)) / max(halfWidth, 1e-3);
    float ridge = saturate(1 - rim * rim);
    float2 acrossScreen = float2(ddx(strip.y), ddy(strip.y));
    acrossScreen /= max(length(acrossScreen), 1e-6);
    float2 stripNormal = acrossScreen * (strip.y * 2 - 1);
    normal = trace ? stripNormal : normal;
    cover = trace ? smoothstep(0, cell > 1.01 ? 0.6 : 0.25, ridge) * 0.9 : cover;

    float nz = sqrt(saturate(1 - dot(normal, normal)));
    float k = 1 - 0.64 * (1 - nz * nz);
    float2 bend = -(sqrt(max(k, 0)) - 0.8 * nz) * normal * drop.w;
    float2 uv = (pixel + 0.5) * frame.xy * crop.xy + crop.zw + bend * crop.xy;

    // how much of the frame one cell of the drop shows, see the shader of
    // Direct3D 10
    float2 size = 1 / frame.xy;
    float dropPixels = 1 / max(max(perPixel.x, perPixel.y), 1e-4);
    float2 shown = size * (cell / dropPixels) * blur;
    float2 sx = cell > 1.01 ? float2(shown.x, 0) : ddx(uv) * size;
    float2 sy = cell > 1.01 ? float2(0, shown.y) : ddy(uv) * size;
    float l = max(log2(max(max(length(sx), length(sy)), 1)) - 1, 0);

    clip(cover - 0.001);

    // what the drop shows, in light, see the shader of Direct3D 10
    float3 backdrop = 1;
    if (frame.z > 0.5)
    {
        // the film of a trail, see the shader of Direct3D 10
        if (trace)
            backdrop = tex2Dlod(sceneSampler, float4((pixel + 0.5) * frame.xy * crop.xy + crop.zw + stripNormal * 0.012 * crop.xy, 0, 0)).rgb * 0.96 + 0.025;
        else if (cell > 1.01)
        {
            // the picture round the drop, mirrored, see the shader of Direct3D 10
            float2 at = scene;
            float2 reach = float2(frame.x / frame.y, 1) * (0.05 * blur) * crop.xy;
            float patch = log2(max(0.05 * blur * size.y * 0.5, 1)) - 1;
            float3 light = 2 * ToLight(tex2Dlod(sceneSampler, float4(at, 0, patch)).rgb);
            [unroll] for (int r = 0; r < 8; ++r)
            {
                float angle = r * 0.78539816 + 0.3927;
                light += ToLight(tex2Dlod(sceneSampler, float4(at + float2(cos(angle), sin(angle)) * reach, 0, patch)).rgb);
            }
            backdrop = ToValue(light / 10);
        }
        else
            backdrop = tex2Dlod(sceneSampler, float4(uv, 0, l)).rgb;

        // a flake of snow is milky, see the shader of Direct3D 10
        if (drop.w < 4)
            backdrop += 0.05 * (1 - backdrop);
    }

    // the lights around the drop, out of the field the backend gathers them
    // into, see the shader of Direct3D 10
    float3 lightOut = 0;
    if (lens && frame.z > 0.5 && frame.w < 0.5)
    {
        float3 excess = tex2D(lightSampler, (pixel + 0.5) * frame.xy).rgb;
        float grey = dot(excess, float3(0.2126, 0.7152, 0.0722));
        float3 light = max(grey + (excess - grey) * 2.5, 0);
        float lightPeak = max(light.r, max(light.g, light.b));
        lightOut = light * smoothstep(0.03, 0.20, lightPeak);
    }
    float peak = max(lightOut.r, max(lightOut.g, lightOut.b)) * 1.6;
    float3 tint = lightOut * (1.6 * 0.8 / (0.8 + peak));
    backdrop += tint * (1 - backdrop);
    cover = max(cover, cover * saturate(peak * 0.5 + 0.6));

    return float4(backdrop * color.rgb, color.a * cover);
}
