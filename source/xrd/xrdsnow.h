#pragma once
// ---------------------------------------------------------------------------
// The snow and the world space rain streaks.
//
// This is the second half of what used to be snow.h: the particles that live in
// the world around the camera instead of on the lens. The simulation is the one
// of the original code, only the drawing changed.
//
// The original built a Direct3D 9 vertex declaration, a vertex buffer and an
// index buffer and used the immediate mode of the engine to push one quad at a
// time (im3DTransform/im3DRenderIndexedPrimitive), which meant the whole thing
// only ever worked on one API and cost one draw call per particle.
//
// Here the same quad maths lands in a list of Xrd::Vertex, the per particle
// transform is folded into the vertices and the whole batch goes to whichever
// backend is active in a single call. The projection handed to the backend is
// the view matrix times the perspective matrix of the original, so the streaks
// are still world space geometry that the GPU clips and divides.
//
// What the games keep using:
//   CSnow::targetSnow  - 0..1, how much snow should be falling
//   CSnow::Snow        - 0..1, how much is falling right now (smoothed)
//   CSnow::zn/zf       - near and far plane of the original perspective
//   CSnow::AddSnow()   - replaces the Direct3D 9 version, no device argument
// ---------------------------------------------------------------------------

#include "xrdrender.h"

class CSnow
{
    struct CVector
    {
        float x, y, z;
    };

    struct snowFlake
    {
        CVector pos;
        float xChange;
        float yChange;

        // Rain-specific extras (unused for snow)
        float fallSpeed;   // units/sec
        float length;      // base half-length in world units
        uint32_t color;    // ARGB
        float gustPhaseX;  // per-drop smooth gust phase
        float gustPhaseY;
        float gustFreqX;   // per-drop smooth gust frequency
        float gustFreqY;
        float shimmerPhase;
        float shimmerFreq;
    };

    class CBox
    {
    public:
        CVector min;
        CVector max;
        void Set(CVector a, CVector b)
        {
            min = a; max = b;
        }
    };

    // The original vertices, kept as they were so the quad maths reads the same.
    struct Im3DVertex
    {
        RwV3d position;
        RwV3d normal;
        uint32_t color;
        float u, v;
    };

public:
    static inline float targetSnow = 0.0f;
    static inline int snowFlakes = 400;
    static inline std::vector<snowFlake> snowArray;
    static inline bool snowArrayInitialized = false;
    static inline float Snow;
    static inline CBox snowBox;

    static inline float zn = 1.0f;
    static inline float zf = 500.0f;

    // Rain tuning parameters (static so they can be tweaked externally if needed)
    static inline float RainMinSpeed = 22.0f;     // units/sec
    static inline float RainMaxSpeed = 48.0f;     // units/sec
    static inline float RainMinLength = 0.35f;    // world-space half-length of streak
    static inline float RainMaxLength = 1.00f;    // world-space half-length of streak
    static inline float RainWidth = 0.025f;       // world-space half-width of streak
    static inline float RainWindX = 5.0f;             // world-space wind (x) units/sec
    static inline float RainWindY = 2.0f;             // world-space wind (y) units/sec
    static inline float RainGustStrengthX = 4.5f;     // smooth per-drop gust amplitude (x)
    static inline float RainGustStrengthY = 3.0f;     // smooth per-drop gust amplitude (y)
    static inline float RainMacroGustStrengthX = 1.6f; // coherent field gust amplitude (x)
    static inline float RainMacroGustStrengthY = 1.1f; // coherent field gust amplitude (y)
    static inline float RainLodNear = 8.0f;           // camera-distance LOD near
    static inline float RainLodFar = 70.0f;           // camera-distance LOD far
    static inline float RainNearDropDistance = 13.0f; // where larger near drops can appear
    static inline float RainNearDropChance = 0.08f;   // chance for occasional fat near streaks
    static inline float RainNearDropWidthMul = 1.9f;
    static inline float RainNearDropLengthMul = 1.35f;
    static inline float RainNearDropAlphaMul = 1.30f;
    static inline uint8_t RainMinAlpha = 0x66;        // ARGB alpha range
    static inline uint8_t RainMaxAlpha = 0x99;

private:
    static inline Xrd::Texture* ms_snowTexture = nullptr;   // soft disc, snow
    static inline Xrd::Texture* ms_rainTexture = nullptr;   // tapered streak, rain
    static inline std::vector<Xrd::Vertex> ms_vertices;
    static inline float ms_rainTime = 0.0f;

    static inline float Clamp(float v, float low, float high)
    {
        return ((v) < (low) ? (low) : (v) > (high) ? (high) : (v));
    }

    static inline float GetRandomFloat(float range = (float)RAND_MAX)
    {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(0.0f, range);
        return static_cast<float>(dis(gen));
    }

    // The atlas of one particle, 64x64, straight RGBA which is what the
    // backends upload. The original filled a Direct3D 9 A8R8G8B8 texture, the
    // colours below are the same values in RGBA order.
    static inline void BuildMask(bool rain, uint8_t* pixels)
    {
        static constexpr auto MaskSize = 64;
        static constexpr auto sigma = 0.25f;

        for (int y = 0; y < MaskSize; y++)
        {
            float yf = ((y + 0.5f) / MaskSize - 0.5f) * 2.0f; // [-1,1]
            for (int x = 0; x < MaskSize; x++)
            {
                float xf = ((x + 0.5f) / MaskSize - 0.5f) * 2.0f; // [-1,1]
                uint8_t* px = &pixels[(y * MaskSize + x) * 4];

                if (!rain)
                {
                    // Snow: soft disc
                    float r2 = xf * xf + yf * yf;
                    float a = r2 < 1.0f ? (1.0f - r2) : 0.0f; // falloff to edge
                    uint8_t alpha = (uint8_t)Clamp(a * 255.0f, 0.0f, 255.0f);
                    px[0] = alpha;
                    px[1] = alpha;
                    px[2] = alpha;
                    px[3] = alpha;
                }
                else
                {
                    // Rain: narrow vertical gaussian with tapered ends
                    float core = expf(-(xf * xf) / (2.0f * sigma * sigma));
                    float tipTaper = 1.0f - Clamp((fabsf(yf) - 0.6f) / 0.4f, 0.0f, 1.0f);
                    float a = Clamp(core * tipTaper, 0.0f, 1.0f);
                    float brightness = 0.85f;
                    uint8_t alpha = (uint8_t)Clamp(a * 220.0f, 0.0f, 255.0f);
                    uint8_t rgb = (uint8_t)Clamp(a * 255.0f * brightness, 0.0f, 255.0f);
                    px[0] = rgb;
                    px[1] = rgb;
                    px[2] = rgb;
                    px[3] = alpha;
                }
            }
        }
    }

    static inline Xrd::Texture* CreateMask(bool rain)
    {
        static constexpr auto MaskSize = 64;
        uint8_t pixels[MaskSize * MaskSize * 4];
        BuildMask(rain, pixels);
        return Xrd::CreateTexture(MaskSize, MaskSize, pixels);
    }

    // row vector maths, the same thing the fixed function pipeline of the
    // original did with SetTransform
    static inline Xrd::Matrix MakeView(const RwMatrix* view)
    {
        Xrd::Matrix r{};
        r.m[0][0] = view->right.x; r.m[0][1] = view->right.y; r.m[0][2] = view->right.z; r.m[0][3] = 0.0f;
        r.m[1][0] = view->up.x;    r.m[1][1] = view->up.y;    r.m[1][2] = view->up.z;    r.m[1][3] = 0.0f;
        r.m[2][0] = view->at.x;    r.m[2][1] = view->at.y;    r.m[2][2] = view->at.z;    r.m[2][3] = 0.0f;
        r.m[3][0] = view->pos.x;   r.m[3][1] = view->pos.y;   r.m[3][2] = view->pos.z;   r.m[3][3] = 1.0f;
        return r;
    }

    static inline Xrd::Matrix MakeWorld(const RwMatrix* cam, const CVector& pos)
    {
        Xrd::Matrix r{};
        r.m[0][0] = cam->right.x; r.m[0][1] = cam->right.z; r.m[0][2] = cam->right.y; r.m[0][3] = 0.0f;
        r.m[1][0] = cam->up.x;    r.m[1][1] = cam->up.z;    r.m[1][2] = cam->up.y;    r.m[1][3] = 0.0f;
        r.m[2][0] = cam->at.x;    r.m[2][1] = cam->at.z;    r.m[2][2] = cam->at.y;    r.m[2][3] = 0.0f;
        r.m[3][0] = pos.x;        r.m[3][1] = pos.y;        r.m[3][2] = pos.z;        r.m[3][3] = 1.0f;
        return r;
    }

    static inline Xrd::Matrix MakePerspective()
    {
        // D3DXMatrixPerspectiveFovLH(90 degrees, aspect, zn, zf)
        const float aspect = ms_fbHeight > 0 ? (float)ms_fbWidth / (float)ms_fbHeight : 1.0f;
        const float h = 1.0f; // 1 / tan(45 degrees)
        const float w = h / (aspect > 0.0f ? aspect : 1.0f);

        Xrd::Matrix r{};
        r.m[0][0] = w;
        r.m[1][1] = h;
        r.m[2][2] = zf / (zf - zn);
        r.m[2][3] = 1.0f;
        r.m[3][2] = -zn * zf / (zf - zn);
        return r;
    }

    static inline float Transform(const Xrd::Matrix& m, const RwV3d& in, Xrd::Vertex& out)
    {
        // world, view and the perspective of the original in one go, including
        // the w the GPU used to divide by
        const float w = in.x * m.m[0][3] + in.y * m.m[1][3] + in.z * m.m[2][3] + m.m[3][3];
        const float iw = w != 0.0f ? 1.0f / w : 0.0f;

        out.x = (in.x * m.m[0][0] + in.y * m.m[1][0] + in.z * m.m[2][0] + m.m[3][0]) * iw;
        out.y = (in.x * m.m[0][1] + in.y * m.m[1][1] + in.z * m.m[2][1] + m.m[3][1]) * iw;
        out.z = (in.x * m.m[0][2] + in.y * m.m[1][2] + in.z * m.m[2][2] + m.m[3][2]) * iw;

        return w;
    }

    // Returns false when the quad reaches behind the near plane, which the fixed
    // function pipeline used to clip for us.
    static inline bool Push(const Xrd::Matrix& matrix, const Im3DVertex* quad, Xrd::Vertex* out)
    {
        for (int i = 0; i < 4; i++)
        {
            if (Transform(matrix, quad[i].position, out[i]) < zn)
                return false;

            out[i].color = quad[i].color;
            out[i].u0 = quad[i].u;
            out[i].v0 = quad[i].v;
            out[i].u1 = 0.0f;
            out[i].v1 = 0.0f;
        }

        return true;
    }

public:
    static inline int32_t ms_fbWidth;
    static inline int32_t ms_fbHeight;

    static inline void Reset()
    {
        Xrd::DestroyTexture(ms_snowTexture);
        Xrd::DestroyTexture(ms_rainTexture);
        ms_snowTexture = nullptr;
        ms_rainTexture = nullptr;
        ms_vertices.clear();
        snowArrayInitialized = false;
        snowArray.clear();
        Snow = 0.0f;
    }

    static inline void AddSnow(int32_t Width, int32_t Height, RwMatrix* camMatrix, RwMatrix* viewMatrix, float* fTimeStep, bool swapWithRain = false)
    {
        if (!camMatrix || !viewMatrix || !fTimeStep)
            return;

        const float dt = *fTimeStep;

        if (!Xrd::IsActive())
            return;

        ms_fbWidth = Width;
        ms_fbHeight = Height;

        // Denser for rain for better coverage
        const int wanted = swapWithRain ? 3000 : 2000;
        if (snowFlakes != wanted)
        {
            snowFlakes = wanted;
            snowArrayInitialized = false;
            snowArray.clear();
        }

        if (!swapWithRain)
        {
            if (!ms_snowTexture)
                ms_snowTexture = CreateMask(false);
        }
        else
        {
            if (!ms_rainTexture)
                ms_rainTexture = CreateMask(true);
        }

        Xrd::Texture* pMask = swapWithRain ? ms_rainTexture : ms_snowTexture;
        if (!pMask)
            return;

        if (targetSnow != 0.0f || Snow != 0.0f)
        { // Weather == SNOW/RAIN

            if (targetSnow == 0.0f)
            {
                Snow -= Snow / 100.0f;
                Snow = Clamp(Snow, 0.0f, 1.0f);
            }
            else
            {
                if (Snow < targetSnow)
                    Snow += targetSnow / 100.0f;
                Snow = Clamp(Snow, 0.0f, 1.0f);
            }
        }
        else
        {
            return;
        }

        ms_rainTime += dt;

        const auto snowAmount = (int)min(snowFlakes, Snow * snowFlakes);
        if (snowAmount <= 0)
            return;

        snowBox.Set(CVector(camMatrix->pos.x, camMatrix->pos.y, camMatrix->pos.z), CVector(camMatrix->pos.x, camMatrix->pos.y, camMatrix->pos.z));
        // Spawn volume around camera
        snowBox.min.x -= 40.0f;
        snowBox.min.y -= 40.0f;
        snowBox.max.x += 40.0f;
        snowBox.min.z -= 15.0f; // vertical span (z acts as "height" here)
        snowBox.max.z += 15.0f;
        snowBox.max.y += 40.0f;

        if (!snowArrayInitialized)
        {
            snowArrayInitialized = true;
            snowArray.resize(snowFlakes);
            for (int i = 0; i < snowFlakes; i++)
            {
                snowArray[i].pos.x = snowBox.min.x + ((snowBox.max.x - snowBox.min.x) * (GetRandomFloat() / (float)RAND_MAX));
                snowArray[i].pos.y = snowBox.min.y + ((snowBox.max.y - snowBox.min.y) * (GetRandomFloat() / (float)RAND_MAX));
                snowArray[i].pos.z = snowBox.min.z + ((snowBox.max.z - snowBox.min.z) * (GetRandomFloat() / (float)RAND_MAX));

                if (swapWithRain)
                {
                    // Bias rain spawn slightly in front of camera.
                    snowArray[i].pos.x += camMatrix->at.x * 12.0f;
                    snowArray[i].pos.y += camMatrix->at.y * 12.0f;
                }

                snowArray[i].xChange = 0.0f;
                snowArray[i].yChange = 0.0f;

                // Initialize rain attributes too (cheap for snow; ignored in snow path)
                float spdNorm = GetRandomFloat(1.0f); // [0..1]
                snowArray[i].fallSpeed = RainMinSpeed + (RainMaxSpeed - RainMinSpeed) * spdNorm;
                snowArray[i].length = RainMinLength + (RainMaxLength - RainMinLength) * spdNorm;
                uint8_t alpha = (uint8_t)(RainMinAlpha + (RainMaxAlpha - RainMinAlpha) * spdNorm);
                snowArray[i].color = (uint32_t(alpha) << 24) | 0x00FFFFFF;

                // Smooth per-drop gust profile.
                snowArray[i].gustPhaseX = GetRandomFloat(6.28318f);
                snowArray[i].gustPhaseY = GetRandomFloat(6.28318f);
                snowArray[i].gustFreqX = 0.5f + GetRandomFloat(1.0f) * 1.1f;
                snowArray[i].gustFreqY = 0.4f + GetRandomFloat(1.0f) * 0.9f;
                snowArray[i].shimmerPhase = GetRandomFloat(6.28318f);
                snowArray[i].shimmerFreq = 1.2f + GetRandomFloat(1.0f) * 2.2f;
            }
        }

        const RwMatrix& cam = *camMatrix;

        // Everything the original did with SetTransform, folded into one matrix.
        const Xrd::Matrix viewProjection = Xrd::Matrix::Multiply(MakeView(viewMatrix), MakePerspective());

        ms_vertices.clear();
        ms_vertices.reserve((size_t)snowAmount * 4);

        const float rainTime = ms_rainTime;
        const float zSpan = (snowBox.max.z - snowBox.min.z);

        for (int i = 0; i < snowAmount; i++)
        {
            float& xPos = snowArray[i].pos.x;
            float& yPos = snowArray[i].pos.y;
            float& zPos = snowArray[i].pos.z;
            float& xChangeRate = snowArray[i].xChange;
            float& yChangeRate = snowArray[i].yChange;

            if (!swapWithRain)
            {
                // Original snow drift and fall (z acts as vertical here)
                float minChange = -dt / 10.0f;
                float maxChange = -minChange;

                zPos -= maxChange;

                xChangeRate += minChange + (2 * maxChange * (rand() / (float)RAND_MAX));
                yChangeRate += minChange + (2 * maxChange * (rand() / (float)RAND_MAX));

                xChangeRate = Clamp(xChangeRate, minChange, maxChange);
                yChangeRate = Clamp(yChangeRate, minChange, maxChange);

                yPos += yChangeRate;
                xPos += xChangeRate;
            }
            else
            {
                // RAIN: move down fast, drift with coherent macro gust + smooth per-drop gusts
                const float macroPhase = rainTime * 0.35f + (xPos + yPos) * 0.035f;
                const float macroGustX = sinf(macroPhase) * RainMacroGustStrengthX;
                const float macroGustY = cosf(macroPhase * 1.13f) * RainMacroGustStrengthY;

                const float gustX = sinf(rainTime * snowArray[i].gustFreqX + snowArray[i].gustPhaseX) * RainGustStrengthX;
                const float gustY = sinf(rainTime * snowArray[i].gustFreqY + snowArray[i].gustPhaseY) * RainGustStrengthY;

                const float windX = RainWindX + macroGustX + gustX;
                const float windY = RainWindY + macroGustY + gustY;
                xChangeRate = windX;
                yChangeRate = windY;

                const float shimmer = 0.90f + 0.18f * sinf(rainTime * snowArray[i].shimmerFreq + snowArray[i].shimmerPhase);
                float speed = snowArray[i].fallSpeed * shimmer; // units/sec
                zPos -= speed * dt; // fall along -Z

                xPos += xChangeRate * dt;
                yPos += yChangeRate * dt;
            }

            // Wrap/respawn in the local volume
            if (zPos < snowBox.min.z)
            {
                if (swapWithRain)
                {
                    // Respawn at top with forward-biased placement and new attributes.
                    zPos = snowBox.max.z;
                    xPos = snowBox.min.x + ((snowBox.max.x - snowBox.min.x) * (GetRandomFloat() / (float)RAND_MAX));
                    yPos = snowBox.min.y + ((snowBox.max.y - snowBox.min.y) * (GetRandomFloat() / (float)RAND_MAX));

                    // Frustum-ish bias: keep more particles in front of camera.
                    xPos += cam.at.x * 10.0f;
                    yPos += cam.at.y * 10.0f;

                    float spdNorm = GetRandomFloat(1.0f);
                    snowArray[i].fallSpeed = RainMinSpeed + (RainMaxSpeed - RainMinSpeed) * spdNorm;
                    snowArray[i].length = RainMinLength + (RainMaxLength - RainMinLength) * spdNorm;
                    uint8_t alpha = (uint8_t)(RainMinAlpha + (RainMaxAlpha - RainMinAlpha) * spdNorm);
                    snowArray[i].color = (uint32_t(alpha) << 24) | 0x00FFFFFF;

                    snowArray[i].gustPhaseX = GetRandomFloat(6.28318f);
                    snowArray[i].gustPhaseY = GetRandomFloat(6.28318f);
                    snowArray[i].gustFreqX = 0.5f + GetRandomFloat(1.0f) * 1.1f;
                    snowArray[i].gustFreqY = 0.4f + GetRandomFloat(1.0f) * 0.9f;
                    snowArray[i].shimmerPhase = GetRandomFloat(6.28318f);
                    snowArray[i].shimmerFreq = 1.2f + GetRandomFloat(1.0f) * 2.2f;
                    snowArray[i].xChange = 0.0f;
                    snowArray[i].yChange = 0.0f;
                }
                else
                {
                    zPos += zSpan; // snow wrap
                }
            }
            while (zPos > snowBox.max.z)
            {
                zPos -= zSpan;
            }

            const float xSpan = (snowBox.max.x - snowBox.min.x);
            const float ySpan = (snowBox.max.y - snowBox.min.y);
            while (xPos < snowBox.min.x)
            {
                xPos += xSpan;
            }
            while (xPos > snowBox.max.x)
            {
                xPos -= xSpan;
            }
            while (yPos < snowBox.min.y)
            {
                yPos += ySpan;
            }
            while (yPos > snowBox.max.y)
            {
                yPos -= ySpan;
            }

            const CVector particle{ xPos, yPos, zPos };
            const Xrd::Matrix world = MakeWorld(camMatrix, particle);
            const Xrd::Matrix matrix = Xrd::Matrix::Multiply(world, viewProjection);

            const size_t base = ms_vertices.size();
            ms_vertices.resize(base + 4);
            Xrd::Vertex* out = &ms_vertices[base];
            bool emitted = false;

            if (!swapWithRain)
            {
                // SNOW quad (billboard), same corners as the original
                static const Im3DVertex snowVertexBuffer[] =
                {
                    {RwV3d(0.1f / 5.0f,  0.1f / 5.0f, 1.0f), RwV3d(), 0xFFFFFFFF, 1.0f, 1.0f},
                    {RwV3d(-0.1f / 5.0f,  0.1f / 5.0f, 1.0f), RwV3d(), 0xFFFFFFFF, 0.0f, 1.0f},
                    {RwV3d(-0.1f / 5.0f, -0.1f / 5.0f, 1.0f), RwV3d(), 0xFFFFFFFF, 0.0f, 0.0f},
                    {RwV3d(0.1f / 5.0f, -0.1f / 5.0f, 1.0f), RwV3d(), 0xFFFFFFFF, 1.0f, 0.0f},
                };
                Push(matrix, snowVertexBuffer, out);
                emitted = true;
            }
            else
            {
                // RAIN: 3D velocity-aligned billboard
                // The streak is oriented along its actual world-space velocity vector so
                // it foreshortens naturally when viewed from any angle (dots when looking
                // straight down, long streaks when looking horizontally).

                // Distance-based LOD + speed-based stretching.
                float cdx = xPos - cam.pos.x;
                float cdy = yPos - cam.pos.y;
                float cdz = zPos - cam.pos.z;
                float dist = sqrtf(cdx * cdx + cdy * cdy + cdz * cdz);
                float lodT = Clamp((dist - RainLodNear) / (RainLodFar - RainLodNear), 0.0f, 1.0f);

                const float speedNorm = Clamp((snowArray[i].fallSpeed - RainMinSpeed) / (RainMaxSpeed - RainMinSpeed), 0.0f, 1.0f);
                float halfWidth = RainWidth * (1.20f - 0.55f * lodT); // thinner at distance
                float halfLen = snowArray[i].length * (0.90f + speedNorm * 0.85f) * (0.95f + lodT * 0.35f);

                // Occasional larger near-camera drops.
                const float nearDropRnd = GetRandomFloat(1.0f);
                const bool isNearFatDrop = (dist < RainNearDropDistance) && (nearDropRnd < RainNearDropChance);
                if (isNearFatDrop)
                {
                    halfWidth *= RainNearDropWidthMul;
                    halfLen *= RainNearDropLengthMul;
                }

                // --- fall velocity direction in world space (Z-up world) ---
                float vx = snowArray[i].xChange;
                float vy = snowArray[i].yChange;
                float vz = -snowArray[i].fallSpeed;
                float vlen = sqrtf(vx * vx + vy * vy + vz * vz);
                if (vlen < 0.001f) vlen = 0.001f;
                float fdx = vx / vlen, fdy = vy / vlen, fdz = vz / vlen; // fall dir (world)

                // --- camera-to-drop direction in world space ---
                float clen = dist;
                if (clen < 0.001f) clen = 0.001f;
                cdx /= clen; cdy /= clen; cdz /= clen;

                // --- width axis = cross(fallDir, viewDir), gives a vector perpendicular
                //     to the streak that always faces the camera ---
                float wx = fdy * cdz - fdz * cdy;
                float wy = fdz * cdx - fdx * cdz;
                float wz = fdx * cdy - fdy * cdx;
                float wlen = sqrtf(wx * wx + wy * wy + wz * wz);
                if (wlen < 0.001f)
                {
                    // fallDir nearly parallel to viewDir (drop falling straight at us):
                    // use camera right as width axis instead
                    wx = cam.right.x; wy = cam.right.y; wz = cam.right.z;
                    wlen = 1.0f;
                }
                wx /= wlen; wy /= wlen; wz /= wlen;

                // --- project both axes into camera local space so the world matrix,
                //     which is built from the camera matrix, positions them correctly ---

                // fall axis in camera local
                float fl_r = fdx * cam.right.x + fdy * cam.right.y + fdz * cam.right.z;
                float fl_u = fdx * cam.up.x + fdy * cam.up.y + fdz * cam.up.z;
                float fl_a = fdx * cam.at.x + fdy * cam.at.y + fdz * cam.at.z;

                // width axis in camera local
                float wl_r = wx * cam.right.x + wy * cam.right.y + wz * cam.right.z;
                float wl_u = wx * cam.up.x + wy * cam.up.y + wz * cam.up.z;
                float wl_a = wx * cam.at.x + wy * cam.at.y + wz * cam.at.z;

                // Subtle brightness/alpha shaping by distance and view angle.
                float facing = fabsf(fdx * cdx + fdy * cdy + fdz * cdz);
                float nearFade = 1.0f - lodT * 0.55f;
                const float shimmer = 0.88f + 0.22f * sinf(rainTime * snowArray[i].shimmerFreq + snowArray[i].shimmerPhase);
                float sparkle = (0.84f + (1.0f - facing) * 0.20f) * shimmer;

                uint8_t baseA = uint8_t((snowArray[i].color >> 24) & 0xFF);
                float alphaScale = nearFade * shimmer;
                if (isNearFatDrop)
                    alphaScale *= RainNearDropAlphaMul;
                uint8_t outA = (uint8_t)Clamp(baseA * alphaScale, 25.0f, 255.0f);
                uint8_t outRGB = (uint8_t)Clamp(230.0f * sparkle, 0.0f, 255.0f);
                const uint32_t col = (uint32_t(outA) << 24) | (uint32_t(outRGB) << 16) | (uint32_t(outRGB) << 8) | uint32_t(outRGB);

                // top-right, top-left, bot-left, bot-right
                Im3DVertex v[4];
                v[0] = { RwV3d(wl_r * halfWidth + fl_r * halfLen,  wl_u * halfWidth + fl_u * halfLen,  wl_a * halfWidth + fl_a * halfLen), RwV3d(), col, 1.0f, 0.0f };
                v[1] = { RwV3d(-wl_r * halfWidth + fl_r * halfLen, -wl_u * halfWidth + fl_u * halfLen, -wl_a * halfWidth + fl_a * halfLen), RwV3d(), col, 0.0f, 0.0f };
                v[2] = { RwV3d(-wl_r * halfWidth - fl_r * halfLen, -wl_u * halfWidth - fl_u * halfLen, -wl_a * halfWidth - fl_a * halfLen), RwV3d(), col, 0.0f, 1.0f };
                v[3] = { RwV3d(wl_r * halfWidth - fl_r * halfLen,  wl_u * halfWidth - fl_u * halfLen,  wl_a * halfWidth - fl_a * halfLen), RwV3d(), col, 1.0f, 1.0f };

                Push(matrix, v, out);
                emitted = true;
            }

            if (!emitted)
                ms_vertices.resize(base);
        }

        if (ms_vertices.empty())
            return;

        // Straight alpha blending of the particle atlas, no look at the frame
        // behind it, which is what the original snow states did. The vertices
        // are already in clip space, so the backend gets an identity transform.
        static const Xrd::Matrix identity = Xrd::Matrix::Identity();

        Xrd::SetMaskTexture(pMask);
        Xrd::SetSceneSampling(false);
        Xrd::SetSceneComplement(false);
        Xrd::SetProjection(Xrd::PROJECTION_WORLD, &identity);
        Xrd::Render(ms_vertices.data(), (int)ms_vertices.size(), Xrd::PRIMITIVE_TRIANGLES);
    }
};
