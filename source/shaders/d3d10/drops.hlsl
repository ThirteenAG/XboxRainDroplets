
cbuffer XrdConstants : register(b0)
{
    float4x4 projection;
    float4 uvOffset;
    float4 uvScale;
    float4 sceneComplement;
};

Texture2D sceneTexture : register(t0);
Texture2D maskTexture : register(t1);
SamplerState linearClamp : register(s0);

// how far the atlas coordinate of a drop that gathers light was moved down,
// and back up again, see WaterDrops::AddToRenderList
static const float AtlasLightMarker = 2.0f;

// The gather is a grid of cells over a square around the drop, in fractions of
// the target, and every cell is the average of four samples of the whole cell
// rather than one sample of a point of it: a small light is then found wherever
// the cells happen to fall, so the colour a drop takes does not jump about as
// the scenery moves past it, which is what one sample per cell looks like.
static const float LightRadius = 0.14f;
static const int LightCells = 8;

// What a drop adds to what it shows is the light of the frame around it that
// stands out of that frame, and nothing else: a frame that is bright is not a
// light, so a drop in a bright frame is the colour of water. LightFloor is how
// much brighter than the frame around it a light has to be to count at all, and
// the ramp is how much of it there has to be to light a drop up in full: a lamp of
// a night city stands out of what is around it by a lot, so the ramp has to be
// over well below its kind of brightness or the whole of it never arrives.
static const float LightFloor = 1.25f;
static const float LightRampLow = 0.03f;
static const float LightRampHigh = 0.20f;

// How much of the colour of a light a drop keeps. What a lens does with a colour
// is to keep it, and what washes the colour out of a drop is the grey of the
// whole neighbourhood the light was measured against, so the colour that was left
// over is pushed back out of its own grey: a tail light is then a red drop and
// not a grey one with a warm tint, and a white light, which has no colour to push
// out, is left exactly as it was. Nothing about which places of the frame count
// as a light changes here, so a drop takes on no new colour from one frame to the
// next and nothing flickers.
static const float LightChroma = 2.5f;


struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
    float2 atlas : TEXCOORD0;
    float2 scene : TEXCOORD1;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 atlas : TEXCOORD0;
    float2 scene : TEXCOORD1;
    float3 light : TEXCOORD2;
    // where the pixel itself is in the copy of the frame
    float2 own : TEXCOORD3;
    // one for the film of water a drop leaves, see WaterDrops::AddStripQuad
    float film : TEXCOORD4;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.position = mul(projection, float4(input.position, 1.0f));
    output.color = input.color;
    output.atlas = input.atlas;
    output.scene = input.scene * uvScale.xy + uvOffset.xy;
    output.light = 0.0f;
    output.own = (output.position.xy / output.position.w * float2(0.5f, -0.5f) + 0.5f) * uvScale.xy + uvOffset.xy;
    output.film = 0.0f;

    // The water a drop leaves on the glass is marked in the v of its atlas
    // coordinate, see WaterDrops::AddStripQuad: it is drawn as a film along the
    // path of the drop, see PSMain.
    if (input.atlas.y < -0.5f)
    {
        output.atlas.y += AtlasLightMarker;
        output.film = 1.0f;
    }

    // A drop of clear water gathers the lights around it: a drop next to a tail
    // light or a street lamp glows in its colour, the way the drops on the lens
    // of Forza do at night. The drop is marked in the u of its atlas coordinate.
    if (input.atlas.x < -0.5f)
    {
        // the atlas coordinate the drop really has, the marker is only a flag
        output.atlas.x += AtlasLightMarker;

        // where the drop is on the target, in the coordinates of the copy of the
        // frame the pixel shader samples. The uvOffset and uvScale of the
        // constants are the crop a game asks the refraction to sample, and they
        // are deliberately not applied here: a light is where it is on the
        // screen, and moving the field a light is looked up in by the crop of
        // the refraction would light every drop up from the wrong place.
        float2 lightUV = output.position.xy / output.position.w * float2(0.5f, -0.5f) + 0.5f;

        // the same reach around the drop in pixels, on a target of any shape
        float aspect = abs(projection[1][1] / projection[0][0]);
        float2 radius = float2(LightRadius / max(aspect, 0.1f), LightRadius);
        float2 cell = radius / LightCells;

        // The grid is aligned to the target and not to the drop, so a drop that
        // moves gathers its light from the very same places of the frame and the
        // field does not crawl over the scenery with it.
        float2 anchor = floor(lightUV / cell) * cell;

        // the light of the frame around the drop, the plain colour of it, and
        // how much of each there was found
        float3 energy = 0.0f;
        float3 plain = 0.0f;
        float weight = 0.0f;
        float falloffSum = 0.0f;

        [loop] for (int y = -LightCells; y <= LightCells; ++y)
        {
            [loop] for (int x = -LightCells; x <= LightCells; ++x)
            {
                float2 corner = anchor + float2(x, y) * cell;
                float2 offset = (corner - lightUV) / radius;
                float distance2 = dot(offset, offset);

                // a soft kernel that is cut off where it is small anyway, so the
                // edge of the reach of a drop is not a visible circle
                float falloff = exp2(-2.0f * distance2) * (1.0f - smoothstep(0.75f, 1.0f, distance2));

                // what the frame holds over the whole cell
                float3 source = sceneTexture.SampleLevel(linearClamp, corner + float2(-0.25f, -0.25f) * cell, 0).rgb;
                source += sceneTexture.SampleLevel(linearClamp, corner + float2(0.25f, -0.25f) * cell, 0).rgb;
                source += sceneTexture.SampleLevel(linearClamp, corner + float2(-0.25f, 0.25f) * cell, 0).rgb;
                source += sceneTexture.SampleLevel(linearClamp, corner + float2(0.25f, 0.25f) * cell, 0).rgb;
                source *= 0.25f;

                float brightness = max(source.r, max(source.g, source.b));
                float saturation = (brightness - min(source.r, min(source.g, source.b))) / max(brightness, 0.001f);

                // How much light there is in what the cell holds and how coloured
                // it is. There is deliberately no brightness a cell has to pass to
                // count: a light fades in with the distance like everything else,
                // and nothing about the colour of a drop flickers while a light
                // comes into its reach or leaves it.
                float w = brightness * brightness * (1.0f + saturation) * falloff;

                energy += source * w;
                plain += source * falloff;
                weight += w;
                falloffSum += falloff;
            }
        }

        // Two averages of the very same surroundings of the drop: the one weighted
        // by how much light is in them and the plain one. A frame that is bright
        // all over gives the same for both and adds nothing to a drop, which is
        // what keeps a drop in a bright frame the colour of water instead of
        // white, and only what stands out of the frame around the drop is added to
        // it, which is what a drop of water collects out of a lamp.
        float3 gathered = energy / (weight + 0.5f);
        float3 average = plain / max(falloffSum, 0.001f);

        // The frame itself is taken off, and a foot is put under what is left of
        // it, because a half of a frame that happens to be brighter than the other
        // is not a light either. What is left over is a lamp, a window, a sign, or
        // anything else that is brighter than everything near it.
        float3 excess = max(gathered - average * LightFloor, 0.0f);

        // The colour of it is pushed out of its own grey, which is what the water
        // of the drop does with a light: what is added is the colour of the lamp
        // and not the grey of the frame it was found in, so a drop next to the
        // tail light of a car is as red as the light is. Its brightness is left
        // where the gather put it, so a drop does not start to glow where the
        // scenery got brighter.
        float grey = dot(excess, float3(0.2126f, 0.7152f, 0.0722f));
        float3 light = max(grey + (excess - grey) * LightChroma, 0.0f);

        // What a drop takes from a light fades in as the drop comes near it
        // instead of switching on, which is what keeps the colour of a drop from
        // flickering while the scenery moves past it.
        float peak = max(light.r, max(light.g, light.b));

        output.light = light * smoothstep(LightRampLow, LightRampHigh, peak);
    }

    return output;
}

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

// the height of the dome at a place of the atlas, nothing outside the tile
float HeightAt(float2 at, float2 lo, float2 hi)
{
    if (any(at < lo) || any(at > hi))
        return 0.0f;

    float a = maskTexture.SampleLevel(linearClamp, at, 0).a;
    return a * a;
}

// The height of the dome blurred over a footprint, a 3 x 3 tent, and its slope
// along the footprint out of the same nine reads, the way a Sobel filter does.
// r is the reach of the footprint in pixels.
float3 SoftHeight(float2 at, float2 rx, float2 ry, float r, float2 lo, float2 hi)
{
    float h00 = HeightAt(at - rx - ry, lo, hi), h10 = HeightAt(at - ry, lo, hi), h20 = HeightAt(at + rx - ry, lo, hi);
    float h01 = HeightAt(at - rx, lo, hi), h11 = HeightAt(at, lo, hi), h21 = HeightAt(at + rx, lo, hi);
    float h02 = HeightAt(at - rx + ry, lo, hi), h12 = HeightAt(at + ry, lo, hi), h22 = HeightAt(at + rx + ry, lo, hi);
    float h = (4.0f * h11 + 2.0f * (h10 + h01 + h21 + h12) + h00 + h20 + h02 + h22) / 16.0f;
    float gx = ((h20 + 2.0f * h21 + h22) - (h00 + 2.0f * h01 + h02)) / (8.0f * r);
    float gy = ((h02 + 2.0f * h12 + h22) - (h00 + 2.0f * h10 + h20)) / (8.0f * r);
    return float3(h, gx, gy);
}

// how wide the patch a soft drop blurs what it shows over is, in heights of the
// picture
static const float LensBlur = 0.05f;

// how milky a flake of snow is, see PSMain
static const float SnowMilk = 0.05f;

// the frame turned back into light, and light into a value again
float3 ToLight(float3 c) { c = min(c, 0.96f); return c / (1.0f - c); }
float3 ToValue(float3 l) { return l / (1.0f + l); }

float4 PSMain(VSOutput input) : SV_TARGET
{
    float2 tile = 1.0f / max(uvScale.z, 1.0f);
    float2 base = floor(input.atlas / tile) * tile;

    float width, height;
    maskTexture.GetDimensions(width, height);
    float2 texel = 1.0f / float2(width, height);
    float2 lo = base + texel;
    float2 hi = base + tile - texel;

    // a pixel of the screen in the atlas, along the screen, and in the units of
    // the shape, which takes 0.6 of its tile
    float2 dx = ddx(input.atlas);
    float2 dy = ddy(input.atlas);
    float2 perPixel = float2(length(dx), length(dy)) / (tile * 0.6f);

    // the size of the screen, out of the projection of the drops, and the cell
    float2 invScreen = float2(abs(projection[0][0]), abs(projection[1][1])) * 0.5f;
    float lines = 1.0f / invScreen.y;
    float blur = saturate(uvScale.w);
    float cell = max(1.0f, blur * 4.0f * lines / 1080.0f);

    // the dome, and its slope along the screen
    float h;
    float2 slope;

    if (cell > 1.01f)
    {
        float3 soft = SoftHeight(input.atlas, dx * cell, dy * cell, cell, lo, hi);
        h = soft.x;
        slope = soft.yz / perPixel;
    }
    else
    {
        h = HeightAt(input.atlas, lo, hi);
        slope = float2(HeightAt(input.atlas + dx, lo, hi) - HeightAt(input.atlas - dx, lo, hi),
            HeightAt(input.atlas + dy, lo, hi) - HeightAt(input.atlas - dy, lo, hi)) / (2.0f * perPixel);
    }

    // the edge of the drop fades over the outer part of its dome, where Forza's
    // drop of a few cells is averaged into the frame around it
    // A soft drop fades from its middle to nothing at its edge, the way Forza's
    // drop of a few cells does once it is stretched over the picture; the
    // crisp drop has an edge.
    float cover = cell > 1.01f ? smoothstep(0.0f, 0.45f, h) : smoothstep(0.01f, 0.2f, h);

    // outwards, on the screen, the way Forza's normals are
    // no steeper than Forza's map of normals ever is, so the rim of a drop does
    // not shoot its refraction off the picture
    float2 normal = -0.168f * slope;
    normal *= min(1.0f, 0.5f / max(length(normal), 1e-6f));

    // The water a drop leaves is a film along its path, drawn as a strip: along
    // the strip is the u of its tile, across it the v. It is a low ridge, its
    // ends taper so the pieces of a trail run into each other as one line, and
    // its sides lean over, which is what bends the picture along the edges of a
    // trail of water on a pane of glass.
    // Water on glass has no corners and no points: a piece of the film is a
    // capsule, a band as wide as the strip with round ends. It is clear water:
    // it shows the frame right behind it, bent a little across the trail where
    // its sides lean over, the way a run of water on a pane of glass is only
    // seen by its edges. The pieces overlap and all show the same, so a trail is
    // one channel of water with no seams.
    float2 strip = (input.atlas - base) / tile;
    float stripLength = 1.0f / max(length(float2(ddx(strip.x), ddy(strip.x))), 1e-5f);
    float stripWidth = 1.0f / max(length(float2(ddx(strip.y), ddy(strip.y))), 1e-5f);
    float halfWidth = 0.5f * stripWidth;
    float beyond = max(abs(strip.x - 0.5f) * stripLength - max(0.5f * stripLength - halfWidth, 0.0f), 0.0f);
    float rim = length(float2(beyond, (strip.y - 0.5f) * stripWidth)) / max(halfWidth, 1e-3f);
    float ridge = saturate(1.0f - rim * rim);
    float2 acrossScreen = float2(ddx(strip.y), ddy(strip.y));
    acrossScreen /= max(length(acrossScreen), 1e-6f);
    float2 stripNormal = acrossScreen * (strip.y * 2.0f - 1.0f);

    bool film = input.film > 0.5f;
    normal = film ? stripNormal : normal;
    cover = film ? smoothstep(0.0f, cell > 1.01f ? 0.6f : 0.25f, ridge) * 0.9f : cover;

    float nz = sqrt(saturate(1.0f - dot(normal, normal)));
    float k = 1.0f - 0.64f * (1.0f - nz * nz);
    float2 bend = -(sqrt(max(k, 0.0f)) - 0.8f * nz) * normal * sceneComplement.w;
    float2 uv = input.own + bend * uvScale.xy;

    // How much of the frame one cell of the drop shows, in its texels. A drop
    // shows about all of the picture, so a cell of it shows the picture over
    // the cells across the drop: a drop of five cells shows the picture five
    // texels wide, blurred down to its light and its shade. The crisp drop
    // shows what the refraction sweeps over a pixel.
    float sceneWidth, sceneHeight, sceneLevels;
    sceneTexture.GetDimensions(0, sceneWidth, sceneHeight, sceneLevels);
    float2 sceneSize = float2(sceneWidth, sceneHeight);
    float dropPixels = 1.0f / max(max(perPixel.x, perPixel.y), 1e-4f);
    float2 sweepX = ddx(uv) * sceneSize;
    float2 sweepY = ddy(uv) * sceneSize;
    float2 shown = sceneSize * (cell / dropPixels) * blur;
    float2 sx = cell > 1.01f ? float2(shown.x, 0.0f) : sweepX;
    float2 sy = cell > 1.01f ? float2(0.0f, shown.y) : sweepY;

    float level = log2(max(max(length(sx), length(sy)), 1.0f));

    if (cover <= 0.0f)
        discard;

    // What the drop shows, in light. Forza's drop is a few cells of its quarter
    // resolution layer and shows about all of the picture, so a cell of it
    // holds the light of a wide patch of the frame, mixed before the frame is
    // tone mapped: the sky and the lamps in that patch outweigh the shade, and
    // a drop is a soft bright blob of the light around it. For a soft drop the
    // patch is a ring of reads round the point the drop refracts to, a sixth of
    // the height of the picture across, mixed in light; the crisp drop reads
    // the one point.
    float3 scene = 1.0f;
    if (sceneComplement.y > 0.5f)
    {
        // the film of a trail, see above: a faint sheen on what is behind it
        if (film)
            scene = sceneTexture.SampleLevel(linearClamp, input.own + stripNormal * 0.012f * uvScale.xy, 0.0f).rgb * 0.96f + 0.025f;
        else if (cell > 1.01f)
        {
            // The picture round the drop, upside down and mirrored, see
            // WaterDrops::LensWindow, and blurred over a small patch.
            float2 at = input.scene;
            float2 reach = float2(invScreen.y / invScreen.x, 1.0f) * (LensBlur * blur) * uvScale.xy;
            float patch = log2(max(LensBlur * blur * sceneHeight * 0.5f, 1.0f)) - 1.0f;
            float3 light = 2.0f * ToLight(sceneTexture.SampleLevel(linearClamp, at, patch).rgb);
            [unroll] for (int r = 0; r < 8; ++r)
            {
                float angle = r * 0.78539816f + 0.3927f;
                light += ToLight(sceneTexture.SampleLevel(linearClamp, at + float2(cos(angle), sin(angle)) * reach, patch).rgb);
            }
            scene = ToValue(light / 10.0f);
        }
        else
            scene = sceneTexture.SampleLevel(linearClamp, uv, max(level, 0.0f)).rgb;

        // A flake of snow (refracted by Forza's 3 of the snow and not by the 7
        // of the rain, see WaterDrops::SnowRefraction) has a touch of frost.
        if (sceneComplement.w < 4.0f)
            scene += SnowMilk * (1.0f - scene);
    }

    // The lights around the drop, see VSMain: a drop next to a lamp takes on its
    // colour, a bright light keeping some of the picture in the drop.
    float peak = max(input.light.r, max(input.light.g, input.light.b)) * 1.6f;
    float3 tint = input.light * (1.6f * 0.8f / (0.8f + peak));
    scene += tint * (1.0f - scene);
    cover = max(cover, cover * saturate(peak * 0.5f + 0.6f));

    return float4(scene * input.color.rgb, input.color.a * cover);
}
