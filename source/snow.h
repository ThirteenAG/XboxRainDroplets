#pragma once
#include <stdint.h>
#include <random>
#include <vector>
#include <cassert>

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
        float fallSpeed; // units/sec
        float length;    // half-length in view space units
        uint32_t color;  // ARGB
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

    struct Im3DVertex
    {
        RwV3d position;
        RwV3d normal;
        uint32_t color;
        float u, v;
    };

public:
    static inline int32_t ms_initialised;
    static inline IDirect3DVertexDeclaration9* im3ddecl;
    static inline IDirect3DVertexBuffer9* im3dvertbuf;
    static inline IDirect3DIndexBuffer9* im3dindbuf;
    static inline IDirect3DTexture9* snowTex;
    static inline int32_t num3DVertices;
    static inline int32_t ms_fbWidth;
    static inline int32_t ms_fbHeight;
    static inline float zn = 1.0f;
    static inline float zf = 500.0f;

public:
    static inline float targetSnow = 0.0f;
    static inline int snowFlakes = 400;
    static inline std::vector<snowFlake> snowArray;
    static inline bool snowArrayInitialized = false;
    static inline float Snow;
    static inline CBox snowBox;

    // Rain tuning parameters (static so they can be tweaked externally if needed)
    static inline float RainMinSpeed = 22.0f;     // units/sec
    static inline float RainMaxSpeed = 48.0f;     // units/sec
    static inline float RainMinLength = 0.20f;    // in view-space length units (half-length)
    static inline float RainMaxLength = 0.60f;    // "
    static inline float RainWidth = 0.0065f; // half-width in view-space
    static inline float RainWindX = 5.0f;    // world-space wind (x) units/sec
    static inline float RainWindY = 2.0f;    // world-space wind (y) units/sec
    static inline uint8_t RainMinAlpha = 0x66;    // ARGB alpha range
    static inline uint8_t RainMaxAlpha = 0xAA;

public:
    static inline void openIm3D(LPDIRECT3DDEVICE9 pDev)
    {
        D3DVERTEXELEMENT9 elements[5] = {
            { 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
            { 0, offsetof(Im3DVertex, normal), D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
            { 0, offsetof(Im3DVertex, color), D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
            { 0, offsetof(Im3DVertex, u), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
            D3DDECL_END()
        };

        static constexpr auto NUMINDICES = 10000;
        static constexpr auto NUMVERTICES = 10000;

        pDev->CreateVertexDeclaration(elements, &im3ddecl);
        assert(im3ddecl);

        pDev->CreateVertexBuffer(NUMVERTICES * sizeof(Im3DVertex), D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED, &im3dvertbuf, nullptr);
        assert(im3dvertbuf);

        pDev->CreateIndexBuffer(NUMINDICES * sizeof(uint16_t), D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_MANAGED, &im3dindbuf, nullptr);
        assert(im3dindbuf);
    }

    static inline void Reset()
    {
        auto SafeRelease = [](auto ppT) {
            if (*ppT)
            {
                (*ppT)->Release();
                *ppT = NULL;
            }
        };
        SafeRelease(&im3ddecl);
        SafeRelease(&im3dvertbuf);
        SafeRelease(&im3dindbuf);
        SafeRelease(&snowTex);
        ms_initialised = 0;
        snowArrayInitialized = false;
        snowArray.clear();
    }

    static inline void im3DRenderIndexedPrimitive(LPDIRECT3DDEVICE9 pDev, D3DPRIMITIVETYPE primType, void* indices, int32_t numIndices)
    {
        auto lockIndices = [](auto indexBuffer, uint32_t offset, uint32_t size, uint32_t flags) -> uint16_t*
        {
            if (indexBuffer == 0)
                return 0;
            uint16_t* indices;
            auto ibuf = indexBuffer;
            ibuf->Lock(offset, size, (void**)&indices, flags);
            return indices;
        };

        auto unlockIndices = [](auto indexBuffer)
        {
            if (indexBuffer == 0)
                return;
            auto ibuf = indexBuffer;
            ibuf->Unlock();
        };

        auto lockedindices = lockIndices(im3dindbuf, 0, numIndices * sizeof(uint16_t), D3DLOCK_DISCARD);
        memcpy(lockedindices, indices, numIndices * sizeof(uint16_t));
        unlockIndices(im3dindbuf);

        pDev->SetIndices(im3dindbuf);

        uint32_t primCount = 0;
        switch (primType) {
        case D3DPT_LINELIST:
            primCount = numIndices / 2;
            break;
        case D3DPT_TRIANGLELIST:
            primCount = numIndices / 3;
            break;
        case D3DPT_TRIANGLESTRIP:
            primCount = numIndices - 2;
            break;
        case D3DPT_TRIANGLEFAN:
            primCount = numIndices - 2;
            break;
        case D3DPT_POINTLIST:
            primCount = numIndices;
            break;
        }
        pDev->DrawIndexedPrimitive(primType, 0, 0, num3DVertices, 0, primCount);
    }

    static inline void im3DTransform(LPDIRECT3DDEVICE9 pDev, RwMatrix* view, void* vertices, int32_t numVertices, RwMatrix* world, uint32_t flags)
    {
        auto lockVertices = [](auto vertexBuffer, uint32_t offset, uint32_t size, uint32_t flags) -> uint8_t*
        {
            if (vertexBuffer == 0)
                return 0;
            uint8_t* verts;
            auto vertbuf = vertexBuffer;
            vertbuf->Lock(offset, size, (void**)&verts, flags);
            return verts;
        };

        auto unlockVertices = [](auto vertexBuffer)
        {
            if (vertexBuffer == 0)
                return;
            auto vertbuf = vertexBuffer;
            vertbuf->Unlock();
        };

        D3DXMATRIXA16 ProjectionMatrix;
        D3DXMatrixPerspectiveFovLH(&ProjectionMatrix, 3.14f / 2.0f, (float)ms_fbWidth / (float)ms_fbHeight, zn, zf);
        pDev->SetTransform(D3DTS_PROJECTION, &ProjectionMatrix);

        D3DMATRIX ViewMatrix{
            view->right.x,    view->right.y, view->right.z, 0.0f,
            view->up.x,       view->up.y,    view->up.z, 0.0f,
            view->at.x,       view->at.y,    view->at.z, 0.0f,
            view->pos.x,      view->pos.y,   view->pos.z, 1.0f,
        };
        pDev->SetTransform(D3DTS_VIEW, &ViewMatrix);

        D3DMATRIX WorldMatrix{
            world->right.x, world->right.z,   world->right.y, 0.0f,
            world->up.x,       world->up.z,      world->up.y, 0.0f,
            world->at.x,       world->at.z,      world->at.y, 0.0f,
            world->pos.x,     world->pos.z,     world->pos.y, 1.0f,
        };
        pDev->SetTransform(D3DTS_WORLD, &WorldMatrix);

        pDev->SetVertexDeclaration(im3ddecl);

        uint8_t* lockedvertices = lockVertices(im3dvertbuf, 0, numVertices * sizeof(Im3DVertex), D3DLOCK_DISCARD);
        memcpy(lockedvertices, vertices, numVertices * sizeof(Im3DVertex));
        unlockVertices(im3dvertbuf);

        pDev->SetStreamSource(0, im3dvertbuf, 0, sizeof(Im3DVertex));

        num3DVertices = numVertices;
    }

    static inline void AddSnow(LPDIRECT3DDEVICE9 pDev, int32_t Width, int32_t Height, RwMatrix* camMatrix, RwMatrix* viewMatrix, float* fTimeStep, bool swapWithRain = false)
    {
        auto clamp = [](auto v, auto low, auto high) -> float
        {
            return ((v) < (low) ? (low) : (v) > (high) ? (high) : (v));
        };

        static auto GetRandomFloat = [](float range = RAND_MAX) -> float
        {
            static std::random_device rd;
            static std::mt19937 gen(rd());
            std::uniform_real_distribution<> dis(0.0f, range);
            return static_cast<float>(dis(gen));
        };

        if (!ms_initialised)
        {
            ms_fbWidth = Width;
            ms_fbHeight = Height;

            // Denser for rain for better coverage
            snowFlakes = swapWithRain ? 3000 : 2000;
            snowArray.resize(snowFlakes);
            openIm3D(pDev);

            // Build texture: circular mask for snow, soft streak for rain
            static constexpr auto MaskSize = 64;
            D3DXCreateTexture(pDev, MaskSize, MaskSize, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &snowTex);
            D3DLOCKED_RECT LockedRect;
            snowTex->LockRect(0, &LockedRect, NULL, 0);
            uint8_t* pixels = (uint8_t*)LockedRect.pBits;
            int32_t stride = LockedRect.Pitch;

            for (int y = 0; y < MaskSize; y++)
            {
                float yf = ((y + 0.5f) / MaskSize - 0.5f) * 2.0f; // [-1,1]
                for (int x = 0; x < MaskSize; x++)
                {
                    float xf = ((x + 0.5f) / MaskSize - 0.5f) * 2.0f; // [-1,1]
                    uint8_t* px = &pixels[y * stride + x * 4];

                    if (!swapWithRain)
                    {
                        // Snow: soft disc
                        float r2 = xf * xf + yf * yf;
                        float a = r2 < 1.0f ? (1.0f - r2) : 0.0f; // falloff to edge
                        uint8_t alpha = (uint8_t)clamp(a * 255.0f, 0.0f, 255.0f);
                        // Set ARGB (in memory: BB GG RR AA for A8R8G8B8)
                        px[0] = alpha; // B
                        px[1] = alpha; // G
                        px[2] = alpha; // R
                        px[3] = alpha; // A
                    }
                    else
                    {
                        // Rain: narrow vertical gaussian with tapered ends
                        float sigma = 0.25f;             // controls width
                        float core = expf(-(xf * xf) / (2.0f * sigma * sigma));
                        float tipTaper = 1.0f - clamp((fabsf(yf) - 0.6f) / 0.4f, 0.0f, 1.0f); // fade to tips
                        float a = clamp(core * tipTaper, 0.0f, 1.0f);
                        float brightness = 0.85f; // slightly gray/white
                        uint8_t alpha = (uint8_t)clamp(a * 220.0f, 0.0f, 255.0f);
                        uint8_t rgb = (uint8_t)clamp(a * 255.0f * brightness, 0.0f, 255.0f);

                        px[0] = rgb;   // B
                        px[1] = rgb;   // G
                        px[2] = rgb;   // R
                        px[3] = alpha; // A
                    }
                }
            }
            snowTex->UnlockRect(0);

            ms_initialised = 1;
        }

        // InterpolationValue is a value between 0 and 1 corresponds to current minute 0-60
        auto InterpolationValue = 0.5f;
        //targetSnow = 1.0f;

        if (targetSnow != 0.0f || Snow != 0.0f)
        { // Weather == SNOW/RAIN

            if (targetSnow == 0.0f)
            {
                Snow -= Snow / 100.0f;
                Snow = clamp(Snow, 0.0f, 1.0f);
            }
            else
            {
                if (Snow < targetSnow)
                    Snow += targetSnow / 100.0f;
                Snow = clamp(Snow, 0.0f, 1.0f);
            }
        }
        else
        {
            return;
        }

        {
            auto snowAmount = (int)min(snowFlakes, Snow * snowFlakes);
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
                for (int i = 0; i < snowFlakes; i++)
                {
                    snowArray[i].pos.x = snowBox.min.x + ((snowBox.max.x - snowBox.min.x) * (GetRandomFloat() / (float)RAND_MAX));
                    snowArray[i].pos.y = snowBox.min.y + ((snowBox.max.y - snowBox.min.y) * (GetRandomFloat() / (float)RAND_MAX));
                    snowArray[i].pos.z = snowBox.min.z + ((snowBox.max.z - snowBox.min.z) * (GetRandomFloat() / (float)RAND_MAX));
                    snowArray[i].xChange = 0.0f;
                    snowArray[i].yChange = 0.0f;

                    // Initialize rain attributes too (cheap for snow; ignored in snow path)
                    float spdNorm = GetRandomFloat(1.0f); // [0..1]
                    snowArray[i].fallSpeed = RainMinSpeed + (RainMaxSpeed - RainMinSpeed) * spdNorm;
                    snowArray[i].length = RainMinLength + (RainMaxLength - RainMinLength) * spdNorm;
                    uint8_t alpha = (uint8_t)(RainMinAlpha + (RainMaxAlpha - RainMinAlpha) * spdNorm);
                    snowArray[i].color = (uint32_t(alpha) << 24) | 0x00FFFFFF;
                    // gusts
                    snowArray[i].xChange = (GetRandomFloat(2.0f) - 1.0f) * 3.0f;
                    snowArray[i].yChange = (GetRandomFloat(2.0f) - 1.0f) * 2.0f;
                }
            }

            LPDIRECT3DSTATEBLOCK9 pStateBlock = NULL;
            pDev->CreateStateBlock(D3DSBT_ALL, &pStateBlock);
            pStateBlock->Capture();

            pDev->SetRenderState(D3DRS_FOGENABLE, 0);
            pDev->SetTexture(0, snowTex);
            pDev->SetRenderState(D3DRS_ZENABLE, 1);
            pDev->SetRenderState(D3DRS_ZWRITEENABLE, 1);
            pDev->SetRenderState(D3DRS_SRCBLEND, 2);
            pDev->SetRenderState(D3DRS_DESTBLEND, 2);

            pDev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
            pDev->SetRenderState(D3DRS_LIGHTING, 0);
            pDev->SetRenderState(D3DRS_ZENABLE, 0);
            pDev->SetRenderState(D3DRS_ALPHABLENDENABLE, 1);
            pDev->SetRenderState(D3DRS_ALPHATESTENABLE, 0);
            pDev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
            pDev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
            pDev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
            pDev->SetRenderState(D3DRS_SCISSORTESTENABLE, 1);
            pDev->SetRenderState(D3DRS_COLORWRITEENABLE, 0xFFFFFFFF);

            // Cleaner sampling for streak texture
            pDev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
            pDev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
            pDev->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
            pDev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
            pDev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

            pDev->SetVertexShader(NULL);
            pDev->SetPixelShader(NULL);

            auto mat = *camMatrix;

            static constexpr uint16_t snowRenderOrder[] = { 0, 1, 2, 3, 4, 5 };
            #define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

            const float dt = *fTimeStep;

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
                    float minChange = -*fTimeStep / 10.0f;
                    float maxChange = -minChange;

                    zPos -= maxChange;

                    xChangeRate += minChange + (2 * maxChange * (rand() / (float)RAND_MAX));
                    yChangeRate += minChange + (2 * maxChange * (rand() / (float)RAND_MAX));

                    xChangeRate = clamp(xChangeRate, minChange, maxChange);
                    yChangeRate = clamp(yChangeRate, minChange, maxChange);

                    yPos += yChangeRate;
                    xPos += xChangeRate;
                }
                else
                {
                    // RAIN: move down fast, drift with wind and gusts
                    float speed = snowArray[i].fallSpeed; // units/sec
                    // Fall along -Z (consistent with original vertical handling)
                    zPos -= speed * dt;

                    // Horizontal wind + per-drop gusts
                    xPos += (RainWindX + xChangeRate) * dt;
                    yPos += (RainWindY + yChangeRate) * dt;

                    // mild time-varying gusts
                    xChangeRate += (GetRandomFloat(0.5f) - 0.25f) * dt * 2.0f;
                    yChangeRate += (GetRandomFloat(0.5f) - 0.25f) * dt * 2.0f;

                    // clamp gust magnitudes a bit
                    xChangeRate = clamp(xChangeRate, -6.0f, 6.0f);
                    yChangeRate = clamp(yChangeRate, -5.0f, 5.0f);
                }

                // Wrap/respawn in the local volume
                const float zSpan = (snowBox.max.z - snowBox.min.z);
                if (zPos < snowBox.min.z)
                {
                    if (swapWithRain)
                    {
                        // Respawn at top with new attributes for variation
                        zPos = snowBox.max.z;
                        xPos = snowBox.min.x + ((snowBox.max.x - snowBox.min.x) * (GetRandomFloat() / (float)RAND_MAX));
                        yPos = snowBox.min.y + ((snowBox.max.y - snowBox.min.y) * (GetRandomFloat() / (float)RAND_MAX));

                        float spdNorm = GetRandomFloat(1.0f);
                        snowArray[i].fallSpeed = RainMinSpeed + (RainMaxSpeed - RainMinSpeed) * spdNorm;
                        snowArray[i].length = RainMinLength + (RainMaxLength - RainMinLength) * spdNorm;
                        uint8_t alpha = (uint8_t)(RainMinAlpha + (RainMaxAlpha - RainMinAlpha) * spdNorm);
                        snowArray[i].color = (uint32_t(alpha) << 24) | 0x00FFFFFF;
                        snowArray[i].xChange = (GetRandomFloat(2.0f) - 1.0f) * 3.0f;
                        snowArray[i].yChange = (GetRandomFloat(2.0f) - 1.0f) * 2.0f;
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

                mat.pos.x = xPos;
                mat.pos.y = yPos;
                mat.pos.z = zPos;

                if (!swapWithRain)
                {
                    // SNOW quad (billboard)
                    static constexpr Im3DVertex snowVertexBuffer[] =
                    {
                        {RwV3d(0.1f / 5.0f,  0.1f / 5.0f, 1.0f), RwV3d(), 0xFFFFFFFF, 1.0f, 1.0f},
                        {RwV3d(-0.1f / 5.0f,  0.1f / 5.0f, 1.0f), RwV3d(), 0xFFFFFFFF, 0.0f, 1.0f},
                        {RwV3d(-0.1f / 5.0f, -0.1f / 5.0f, 1.0f), RwV3d(), 0xFFFFFFFF, 0.0f, 0.0f},
                        {RwV3d(0.1f / 5.0f,  0.1f / 5.0f, 1.0f), RwV3d(), 0xFFFFFFFF, 1.0f, 1.0f},
                        {RwV3d(0.1f / 5.0f, -0.1f / 5.0f, 1.0f), RwV3d(), 0xFFFFFFFF, 1.0f, 0.0f},
                        {RwV3d(-0.1f / 5.0f, -0.1f / 5.0f, 1.0f), RwV3d(), 0xFFFFFFFF, 0.0f, 0.0f},
                    };
                    im3DTransform(pDev, viewMatrix, (void*)snowVertexBuffer, ARRAY_SIZE(snowVertexBuffer), (RwMatrix*)&mat, 1);
                    im3DRenderIndexedPrimitive(pDev, D3DPT_TRIANGLELIST, (void*)snowRenderOrder, ARRAY_SIZE(snowRenderOrder));
                }
                else
                {
                    // RAIN: build per-drop stretched, slightly slanted quad in view space
                    // Width is constant; length scales with speed for "motion blur" feel
                    float halfWidth = RainWidth;
                    float halfLen = snowArray[i].length + snowArray[i].fallSpeed * 0.0045f;

                    // Slant proportionally to horizontal velocity onto screen X
                    float slant = (RainWindX + snowArray[i].xChange) * 0.004f;

                    Im3DVertex v[6];
                    const uint32_t col = snowArray[i].color;

                    // top
                    v[0] = { RwV3d(+halfWidth, +halfLen, 0.0f), RwV3d(), col, 1.0f, 0.0f };
                    v[1] = { RwV3d(-halfWidth, +halfLen, 0.0f), RwV3d(), col, 0.0f, 0.0f };
                    // bottom (slanted)
                    v[2] = { RwV3d(-halfWidth + slant, -halfLen, 0.0f), RwV3d(), col, 0.0f, 1.0f };
                    // re-emit for tri list
                    v[3] = { RwV3d(+halfWidth, +halfLen, 0.0f), RwV3d(), col, 1.0f, 0.0f };
                    v[4] = { RwV3d(+halfWidth + slant, -halfLen, 0.0f), RwV3d(), col, 1.0f, 1.0f };
                    v[5] = { RwV3d(-halfWidth + slant, -halfLen, 0.0f), RwV3d(), col, 0.0f, 1.0f };

                    im3DTransform(pDev, viewMatrix, (void*)v, ARRAY_SIZE(v), (RwMatrix*)&mat, 1);
                    im3DRenderIndexedPrimitive(pDev, D3DPT_TRIANGLELIST, (void*)snowRenderOrder, ARRAY_SIZE(snowRenderOrder));
                }
            }

            pStateBlock->Apply();
            pStateBlock->Release();
        }
    }
};