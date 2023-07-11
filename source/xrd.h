#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define _USE_MATH_DEFINES
#include <cmath>
#ifndef DIRECT3D_VERSION
#define DIRECT3D_VERSION         0x0900
#endif  //DIRECT3D_VERSION
#if(DIRECT3D_VERSION < 0x0900)
#include <d3d8.h>
#include <d3dx8.h>
#include <d3dx8tex.h>
#pragma comment(lib, "legacy_stdio_definitions.lib")
#pragma comment(lib, "D3dx8.lib")
typedef LPDIRECT3DDEVICE8 LPDIRECT3DDEVICE;
typedef IDirect3DTexture8 IDirect3DTexture;
typedef IDirect3DSurface8 IDirect3DSurface;
typedef IDirect3DVertexBuffer8 IDirect3DVertexBuffer;
typedef IDirect3DIndexBuffer8 IDirect3DIndexBuffer;
#else
#include <d3d9.h>
#include <d3dx9.h>
#include <d3dx9tex.h>
#pragma comment(lib, "D3dx9.lib")
typedef LPDIRECT3DDEVICE9 LPDIRECT3DDEVICE;
typedef IDirect3DTexture9 IDirect3DTexture;
typedef IDirect3DSurface9 IDirect3DSurface;
typedef IDirect3DVertexBuffer9 IDirect3DVertexBuffer;
typedef IDirect3DIndexBuffer9 IDirect3DIndexBuffer;
#endif
#include <time.h>
#include <injector\injector.hpp>
#include <injector\hooking.hpp>
#include <injector\calling.hpp>
#include <injector\utility.hpp>
#ifdef _M_IX86
#include <injector\assembly.hpp>
#endif
#include <filesystem>
#include <algorithm>
#include <thread>
#include <mutex>
#include <map>
#include <iomanip>
#include <random>
#include <subauth.h>
#include "inireader/IniReader.h"
#include "Hooking.Patterns.h"
#include "includes/ModuleList.hpp"
#include "includes/FileWatch.hpp"

#define IDR_DROPMASK 100
#define IDR_SNOWDROPMASK 101
#define IDR_BLURPS 103
#define IDR_BLURVS 104                              

struct RwV3d
{
    float x;
    float y;
    float z;
};

inline void RwV3dSub(RwV3d* o, RwV3d* a, RwV3d* b)
{
    (o)->x = (((a)->x) - ((b)->x));
    (o)->y = (((a)->y) - ((b)->y));
    (o)->z = (((a)->z) - ((b)->z));
}

inline void RwV3dScale(RwV3d* o, RwV3d* a, float s)
{
    o->x = a->x * s;
    o->y = a->y * s;
    o->z = a->z * s;
}

inline float RwV3dDotProduct(RwV3d* a, RwV3d* b)
{
    return (((a->x * b->x) + (a->y * b->y))) + (a->z * b->z);
}

struct RwMatrix
{
    RwV3d    right;
    uint32_t flags;
    RwV3d    up;
    uint32_t pad1;
    RwV3d    at;
    uint32_t pad2;
    RwV3d    pos;
    uint32_t pad3;
};

struct VertexTex2
{
    float      x;
    float      y;
    float      z;
    float      rhw;
    uint32_t   emissiveColor;
    float      u0;
    float      v0;
    float      u1;
    float      v1;
};

#define DROPFVF (D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX2)
#define RAD2DEG(x) (180.0f*(x)/M_PI)

class WaterDrop
{
public:
    float x, y, time;
    int uv_index;
    float size, uvsize, ttl;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t alpha;
    bool active;
    bool fades;
    void Fade();
};

class WaterDropMoving
{
public:
    WaterDrop* drop;
    float dist;
};

class WaterDrops
{
public:
    static inline auto MinSize = 4;
    static inline auto MaxSize = 15;
    static inline auto MaxDrops = 2000;
    static inline auto MaxDropsMoving = 500;
    static inline constexpr float gravity = 9.807f;
    static inline constexpr float gdivmin = 100.0f;
    static inline constexpr float gdivmax = 30.0f;
    static inline auto fMoveStep = 0.0f;
    static inline uint32_t fps = 0;
    static inline float* fTimeStep;
    static inline bool isPaused = false;
    static inline float ms_scaling;
    #define SC(x) ((int32_t)((x)*ms_scaling))
    static inline float ms_xOff;
    static inline float ms_yOff;
    static inline auto ms_drops = std::vector<WaterDrop>(MaxDrops);
    static inline auto ms_dropsMoving = std::vector<WaterDropMoving>(MaxDropsMoving);
    static inline int32_t ms_numDrops;
    static inline int32_t ms_numDropsMoving;

    static inline bool ms_enabled;
    static inline bool ms_movingEnabled;

    static inline float ms_distMoved;
    static inline float ms_vecLen;
    static inline float ms_rainStrength;
    static inline float ms_rainIntensity = 1.0f;
    static inline RwV3d ms_vec;
    static inline RwV3d ms_lastAt;
    static inline RwV3d ms_lastPos;
    static inline RwV3d ms_posDelta;

    static inline int32_t ms_splashDuration;
    static inline RwV3d   ms_splashPoint;
    static inline float   ms_splashDistance;
    static inline float   ms_splashRemovalDistance;

    static inline bool sprayWater = false;
    static inline bool sprayBlood = false;
    static inline bool ms_StaticRain = false;
    static inline bool bRadial = false;
    static inline bool bGravity = true;
    static inline bool bBloodDrops = true;
    static inline bool bEnableSnow = false;
    static inline float fSpeedAdjuster = 1.0f;

    static inline RwV3d right;
    static inline RwV3d up;
    static inline RwV3d at;
    static inline RwV3d pos;

    static inline std::vector<std::pair<RwV3d, float>> ms_sprayLocations;

    static inline void(*ProcessCallback1)();
    static inline void(*ProcessCallback2)();

    static inline int GetRandomInt(int range)
    {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, range);
        return dis(gen);
    }
    static inline float GetRandomFloat(float range)
    {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(0.0f, range);
        return static_cast<float>(dis(gen));
    }
    static inline float GetTimeStep()
    {
        if (!fTimeStep)
            if (fps > 0)
                return (1.0f / fps);
            else
                return 0.0f;
        else
            return *fTimeStep;
    }
    static inline float GetTimeStepInMilliseconds()
    {
        return GetTimeStep() / 50.0f * 1000.0f;
    }
    static inline void Process(LPDIRECT3DDEVICE pDevice)
    {
        if (!fTimeStep)
        {
            static std::list<int> m_times;
            LARGE_INTEGER frequency;
            LARGE_INTEGER time;
            QueryPerformanceFrequency(&frequency);
            QueryPerformanceCounter(&time);

            if (m_times.size() == 50)
                m_times.pop_front();
            m_times.push_back(static_cast<int>(time.QuadPart));

            if (m_times.size() >= 2)
                fps = static_cast<uint32_t>(0.5f + (static_cast<float>(m_times.size() - 1) *
                      static_cast<float>(frequency.QuadPart)) / static_cast<float>(m_times.back() - m_times.front()));
        }
        if (!ms_initialised)
            InitialiseRender(pDevice);
        ProcessGlobalEmitters();
        CalculateMovement();
        SprayDrops();
        ProcessMoving();
        Fade();
    }

    static inline void ReadIniSettings(bool invertedRadial = false)
    {
        CIniReader iniReader("");
        MinSize = iniReader.ReadInteger("MAIN", "MinSize", 4);
        MaxSize = iniReader.ReadInteger("MAIN", "MaxSize", 15);
        MaxDrops = iniReader.ReadInteger("MAIN", "MaxDrops", 3000);
        MaxDropsMoving = iniReader.ReadInteger("MAIN", "MaxMovingDrops", 6000);
        bRadial = iniReader.ReadInteger("MAIN", "RadialMovement", 0) != 0;
        bGravity = iniReader.ReadInteger("MAIN", "EnableGravity", 1) != 0;
        fSpeedAdjuster = iniReader.ReadFloat("MAIN", "SpeedAdjuster", 1.0f);
        fMoveStep = iniReader.ReadFloat("MAIN", "MoveStep", 0.1f);
        bBloodDrops = iniReader.ReadInteger("MAIN", "BloodDrops", 1) != 0;
        bEnableSnow = iniReader.ReadInteger("BONUS", "EnableSnow", 0) != 0;

        static std::once_flag flag;
        std::call_once(flag, [&]() 
        { 
            if (invertedRadial)
                bRadial = !bRadial;

            if (std::filesystem::exists(iniReader.GetIniPath()))
            {
                static filewatch::FileWatch<std::string> watch(iniReader.GetIniPath(), [&](const std::string& path, const filewatch::Event change_type)
                {
                    if (change_type == filewatch::Event::modified)
                    {
                        ReadIniSettings(invertedRadial);
                        ms_initialised = 0;
                    }
                });
            }
        });
    }

    static inline float GetDistanceBetweenEmitterAndCamera(RwV3d* emitterPos)
    {
        RwV3d dist;
        RwV3dSub(&dist, emitterPos, &WaterDrops::ms_lastPos);
        return RwV3dDotProduct(&dist, &dist);
    }
    
    static inline float GetDistanceBetweenEmitterAndCamera(RwV3d emitterPos)
    {
        return GetDistanceBetweenEmitterAndCamera(&emitterPos);
    }
    
    static inline float GetDropsAmountBasedOnEmitterDistance(float emitterDistance, float maxDistance, float maxAmount = 100.0f)
    {
        static auto SolveEqSys = [](float a, float b, float c, float d, float value) -> float
        {
            float determinant = a - c;
            float x = (b - d) / determinant;
            float y = (a * d - b * c) / determinant;
            return min((x)*value + y, d);
        };
        constexpr float minDistance = 0.0f;
        constexpr float minAmount = 0.0f;
        return maxAmount - SolveEqSys(minDistance, minAmount, maxDistance, maxAmount, emitterDistance);
    }
    
    static inline void RegisterGlobalEmitter(RwV3d pos, float radius = 1.0f)
    {
        ms_sprayLocations.emplace_back(pos, radius);
    }
    
    static inline void ProcessGlobalEmitters()
    {
        for (auto& it: ms_sprayLocations)
        {
            RwV3d dist;
            RwV3dSub(&dist, &it.first, &WaterDrops::pos);
            if (RwV3dDotProduct(&dist, &dist) <= 50.0f)
                WaterDrops::FillScreenMoving(it.second);
        }
    }

    static inline void CalculateMovement()
    {
        RwV3dSub(&ms_posDelta, &pos, &ms_lastPos);
        ms_distMoved = RwV3dDotProduct(&ms_posDelta, &ms_posDelta);
        ms_distMoved = sqrt(ms_distMoved) * GetTimeStepInMilliseconds();

        if (fSpeedAdjuster)
        {
            at.x *= fSpeedAdjuster;
            at.y *= fSpeedAdjuster;
            at.z *= fSpeedAdjuster;
        }

        ms_lastAt = at;
        ms_lastPos = pos;

        ms_vec.x = -RwV3dDotProduct(&right, &ms_posDelta);
        if (!bRadial)
        {
            ms_vec.y = RwV3dDotProduct(&up, &ms_posDelta);
            ms_vec.z = RwV3dDotProduct(&at, &ms_posDelta);
        }
        else
        {
            ms_vec.y = RwV3dDotProduct(&at, &ms_posDelta);
            ms_vec.z = RwV3dDotProduct(&up, &ms_posDelta);
        }
        RwV3dScale(&ms_vec, &ms_vec, 10.0f);
        ms_vecLen = sqrt(ms_vec.y * ms_vec.y + ms_vec.x * ms_vec.x);

        ms_enabled = true; //!istopdown && !carlookdirection;
        ms_movingEnabled = true; //!istopdown && !carlookdirection;

        float c = at.z;
        if (c > 1.0f) c = 1.0f;
        if (c < -1.0f) c = -1.0f;
        ms_rainStrength = (float)RAD2DEG(acos(c));
    }

    static inline void SprayDrops()
    {
        if (!NoRain() && ms_rainIntensity != 0.0f && ms_enabled) {
            auto tmp = (int32_t)(180.0f - ms_rainStrength);
            if (tmp < 40) tmp = 40;
            FillScreenMoving((tmp - 40.0f) / 150.0f * ms_rainIntensity * 0.5f);
        }
        if (sprayWater)
            FillScreenMoving(0.5f, false);
        if (sprayBlood)
            FillScreenMoving(0.5f, true);
        if (ms_splashDuration >= 0) {
            if (ms_numDrops < int32_t(ms_drops.capacity() - 1)) {
                RwV3d dist;
                RwV3dSub(&dist, &ms_splashPoint, &ms_lastPos);
                float f = RwV3dDotProduct(&dist, &dist);
                f = sqrt(f);
                if (f <= ms_splashDistance)
                    FillScreenMoving(1.0f);
                else if (ms_splashRemovalDistance > 0.0f && f >= ms_splashRemovalDistance)
                    ms_splashDuration = -1;
            }
            ms_splashDuration--;
        }
    }

    static void MoveDrop(WaterDropMoving* moving)
    {
        WaterDrop* drop = moving->drop;
        if (!ms_movingEnabled)
            return;
        if (!drop->active) {
            moving->drop = NULL;
            ms_numDropsMoving--;
            return;
        }

        static float randgravity = 0.0f;
        if (bGravity)
        {
            randgravity = GetRandomFloat((gravity / gdivmax));
            if (randgravity < (gravity / gdivmin))
                randgravity = (gravity / gdivmin);
        }
        else
            randgravity = 0.0f;

        float d = abs(ms_vec.z * 0.2f);
        float dx, dy, sum;
        dx = drop->x - ms_fbWidth * 0.5f + ms_vec.x;
        dy = drop->y - ms_fbHeight * 0.5f - (ms_vec.y + randgravity);
        sum = fabs(dx) + fabs(dy);
        if (sum >= 0.001f) {
            dx *= (1.0f / sum);
            dy *= (1.0f / sum);
        }
        moving->dist += ((d + ms_vecLen));
        if (moving->drop->ttl > 7000.0f && moving->dist > fMoveStep)
        {
            float movttl = moving->drop->ttl / (float)(SC(4));
            NewTrace(moving, movttl);
        }
        drop->x += (dx * d) - ms_vec.x;
        drop->y += (dy * d) + (ms_vec.y + randgravity);

        drop->size -= (drop->size / 100.0f) * GetTimeStepInMilliseconds();

        if (drop->x < -(float)(SC(MaxSize)) || drop->y < -(float)(SC(MaxSize)) ||
            drop->x >(ms_fbWidth + SC(MaxSize)) || drop->y >(ms_fbHeight + SC(MaxSize))) {
            moving->drop = NULL;
            ms_numDropsMoving--;
        }
    }

    static inline void ProcessMoving()
    {
        if (!ms_movingEnabled)
            return;
        for (auto& moving : ms_dropsMoving)
            if (moving.drop)
                MoveDrop(&moving);
    }

    static inline void Fade()
    {
        for (auto& drop : ms_drops)
            if (drop.active)
                drop.Fade();
    }

    static inline WaterDrop* PlaceNew(float x, float y, float size, float ttl, bool fades, int R = 0xFF, int G = 0xFF, int B = 0xFF)
    {
        if (NoDrops())
            return NULL;
        
        for (auto& drop : ms_drops)
        {
            if (drop.active == 0)
            {
                ms_numDrops++;
                drop.x = x;
                drop.y = y;
                drop.size = size;
                drop.uv_index = ms_atlasUsed ? GetRandomInt(3) : 4; //sizeof(uv) - 2 || uv[last]
                drop.uvsize = (SC(MaxSize) - size + 1.0f) / (SC(MaxSize) - SC(MinSize) + 1.0f);
                drop.fades = fades;
                drop.active = 1;
                drop.r = R;
                drop.g = G;
                drop.b = B;
                drop.alpha = 0xFF;
                drop.time = 0.0f;
                drop.ttl = ttl;
                return &drop;
            }
        }
        return NULL;
    }

    static inline void NewTrace(WaterDropMoving* moving, float ttl)
    {
        if (ms_numDrops < int32_t(ms_drops.capacity() - 1)) {
            moving->dist = 0.0f;
            PlaceNew(moving->drop->x, moving->drop->y, (float)(SC(MinSize)), ttl, 1, moving->drop->r, moving->drop->g, moving->drop->b);
        }
    }

    static inline void NewDropMoving(WaterDrop *drop)
    {
        for (auto& moving : ms_dropsMoving)
        {
            if (moving.drop == NULL)
            {
                ms_numDropsMoving++;
                moving.drop = drop;
                moving.dist = 0.0f;
                return;
            }
        }
    }

    static inline void FillScreenMoving(float amount, bool isBlood = false)
    {
        if (ms_StaticRain)
            amount = 1.0f;

        int32_t n = int32_t((ms_vec.z <= 5.0f ? 1.0f : 1.5f) * amount * 20.0f);
        WaterDrop* drop;

        while (n--)
        {
            if (ms_numDrops < int32_t(ms_drops.capacity() - 1) && ms_numDropsMoving < int32_t(ms_dropsMoving.capacity() - 1))
            {
                float x = GetRandomFloat((float)ms_fbWidth);
                float y = GetRandomFloat((float)ms_fbHeight);
                float size = GetRandomFloat((float)(SC(MaxSize) - SC(MinSize)) + SC(MinSize));
                float ttl = GetRandomFloat((float)(8000.0f));
                if (ttl < 2000.0f)
                    ttl = 2000.0f;
                if (!isBlood)
                    drop = PlaceNew(x, y, size, ttl, 1);
                else
                    drop = PlaceNew(x, y, size, ttl, 1, 0xFF, 0x00, 0x00);
                if (drop)
                    NewDropMoving(drop);
            }
        }
    }

    static inline void FillScreen(int n)
    {
        if (!ms_initialised)
            return;

        ms_numDrops = 0;
        for (auto& drop : ms_drops) {
            drop.active = 0;
            if (&drop < &ms_drops[n]) {
                float x = (float)(rand() % ms_fbWidth);
                float y = (float)(rand() % ms_fbHeight);
                float time = (float)(rand() % (SC(MaxSize) - SC(MinSize)) + SC(MinSize));
                PlaceNew(x, y, time, 2000.0f, 1);
            }
        }
    }

    static inline void Clear()
    {
        for (auto& drop : ms_drops)
            drop.active = false;
        ms_numDrops = 0;
    }

    static inline void Reset()
    {
        Clear();
        ms_splashDuration = -1;
        ms_splashDistance = 0;
        ms_splashPoint = { 0 };

        auto SafeRelease = [](auto ppT) {
            if (*ppT)
            {
                (*ppT)->Release();
                *ppT = NULL;
            }
        };

        SafeRelease(&ms_maskTex);
        SafeRelease(&ms_tex);
        SafeRelease(&ms_surf);
        SafeRelease(&ms_bbuf);
        ms_initialised = 0;
    }

    static inline void RegisterSplash(RwV3d* point, float distance = 20.0f, int32_t duration = 14, float removaldistance = 0.0f)
    {
        ms_splashPoint = *point;
        ms_splashDistance = distance;
        ms_splashRemovalDistance = removaldistance;
        ms_splashDuration = duration;
    }

    static inline bool NoDrops()
    {
        return false; //CWeather__UnderWaterness > 0.339731634f || *CEntryExitManager__ms_exitEnterState != 0;
    }

    static inline bool NoRain()
    {
        return false; //CCullZones__CamNoRain() || CCullZones__PlayerNoRain() || *CGame__currArea != 0 || NoDrops();
    }

    // Rendering
    static inline IDirect3DTexture* ms_maskTex;
    static inline IDirect3DTexture* ms_tex;
    static inline IDirect3DSurface* ms_surf;
    static inline IDirect3DSurface* ms_bbuf;
    static inline IDirect3DVertexBuffer* ms_vertexBuf;
    static inline IDirect3DIndexBuffer* ms_indexBuf;
    static inline int32_t ms_fbWidth;
    static inline int32_t ms_fbHeight;
    static inline VertexTex2* ms_vertPtr;
    static inline int32_t ms_numBatchedDrops;
    static inline int32_t ms_initialised;
    static inline bool ms_atlasUsed = true;

    static inline void InitialiseRender(LPDIRECT3DDEVICE pDevice)
    {
        ms_drops.resize(MaxDrops);
        ms_dropsMoving.resize(MaxDropsMoving);
        

        IDirect3DVertexBuffer* vbuf;
        IDirect3DIndexBuffer* ibuf;
#if(DIRECT3D_VERSION < 0x0900)
        pDevice->CreateVertexBuffer(ms_drops.capacity() * 4 * sizeof(VertexTex2), D3DUSAGE_WRITEONLY, DROPFVF, D3DPOOL_MANAGED, &vbuf);
        pDevice->CreateIndexBuffer(ms_drops.capacity() * 6 * sizeof(short), D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_MANAGED, &ibuf);
#else
        if (FAILED(pDevice->CreateVertexBuffer(ms_drops.capacity() * 4 * sizeof(VertexTex2), D3DUSAGE_WRITEONLY, DROPFVF, D3DPOOL_MANAGED, &vbuf, nullptr)))
            pDevice->CreateVertexBuffer(ms_drops.capacity() * 4 * sizeof(VertexTex2), D3DUSAGE_DYNAMIC, DROPFVF, D3DPOOL_DEFAULT, &vbuf, nullptr);
        if (FAILED(pDevice->CreateIndexBuffer(ms_drops.capacity() * 6 * sizeof(short), D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_MANAGED, &ibuf, nullptr)))
            pDevice->CreateIndexBuffer(ms_drops.capacity() * 6 * sizeof(short), D3DUSAGE_DYNAMIC, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &ibuf, nullptr);
#endif
        ms_vertexBuf = vbuf;
        ms_indexBuf = ibuf;
        uint16_t* idx;
#if(DIRECT3D_VERSION < 0x0900)
        ibuf->Lock(0, 0, (BYTE**)&idx, 0);
#else
        ibuf->Lock(0, 0, (void**)&idx, 0);
#endif
        for (auto i = 0; i < int32_t(ms_drops.capacity()); i++) {
            idx[i * 6 + 0] = i * 4 + 0;
            idx[i * 6 + 1] = i * 4 + 1;
            idx[i * 6 + 2] = i * 4 + 2;
            idx[i * 6 + 3] = i * 4 + 0;
            idx[i * 6 + 4] = i * 4 + 2;
            idx[i * 6 + 5] = i * 4 + 3;
        }
        ibuf->Unlock();

        D3DSURFACE_DESC d3dsDesc;
#if(DIRECT3D_VERSION < 0x0900)
        pDevice->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &ms_bbuf);
#else
        pDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &ms_bbuf);
#endif
        ms_bbuf->GetDesc(&d3dsDesc);
#if(DIRECT3D_VERSION < 0x0900)
        pDevice->CreateTexture(d3dsDesc.Width, d3dsDesc.Height, 1, D3DUSAGE_RENDERTARGET, d3dsDesc.Format, D3DPOOL_DEFAULT, &ms_tex);
#else
        pDevice->CreateTexture(d3dsDesc.Width, d3dsDesc.Height, 1, D3DUSAGE_RENDERTARGET, d3dsDesc.Format, D3DPOOL_DEFAULT, &ms_tex, NULL);
#endif
        ms_tex->GetSurfaceLevel(0, &ms_surf);
        ms_fbWidth = d3dsDesc.Width;
        ms_fbHeight = d3dsDesc.Height;
        ms_scaling = ms_fbHeight / 480.0f;

        HRESULT res = NULL;
        HMODULE hm = NULL;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&InitialiseRender, &hm);
#if(DIRECT3D_VERSION >= 0x0900)
        res = D3DXCreateTextureFromResource(pDevice, hm, 
#ifndef SNOWDROPS
            MAKEINTRESOURCE(IDR_DROPMASK)
#else
            MAKEINTRESOURCE(IDR_SNOWDROPMASK)
#endif
            , &ms_maskTex);
#else
        //res = D3DXCreateTextureFromResource(pDevice, hm, MAKEINTRESOURCE(IDR_SNOWDROPMASK), &ms_maskTex);
        HRSRC hResource = FindResource(hm, 
#ifndef SNOWDROPS
            MAKEINTRESOURCE(IDR_DROPMASK)
#else
            MAKEINTRESOURCE(IDR_SNOWDROPMASK)
#endif
            , RT_RCDATA);
        if (hResource) {
            HGLOBAL hLoadedResource = LoadResource(hm, hResource);
            if (hLoadedResource) {
                LPVOID pLockedResource = LockResource(hLoadedResource);
                if (pLockedResource) {
                    size_t dwResourceSize = SizeofResource(hm, hResource);
                    if (dwResourceSize) {
                        res = D3DXCreateTextureFromFileInMemory(pDevice, pLockedResource, dwResourceSize, &ms_maskTex);
                    }
                }
                FreeResource(hLoadedResource);
            }
        }
#endif
        if (FAILED(res) || ms_maskTex == nullptr)
        {
            static constexpr auto MaskSize = 128;
            if (FAILED(D3DXCreateTexture(pDevice, MaskSize, MaskSize, 1, D3DUSAGE_WRITEONLY, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &ms_maskTex)))
                D3DXCreateTexture(pDevice, MaskSize, MaskSize, 1, D3DUSAGE_WRITEONLY, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &ms_maskTex);
            D3DLOCKED_RECT LockedRect;
            ms_maskTex->LockRect(0, &LockedRect, NULL, 0);
            uint8_t* pixels = (uint8_t*)LockedRect.pBits;
            int32_t stride = LockedRect.Pitch;
            for (int y = 0; y < MaskSize; y++)
            {
                float yf = ((y + 0.5f) / MaskSize - 0.5f) * 2.0f;
                for (int x = 0; x < MaskSize; x++)
                {
                    float xf = ((x + 0.5f) / MaskSize - 0.5f) * 2.0f;
                    memset(&pixels[y * stride + x * 4], xf * xf + yf * yf < 1.0f ? 0xFF : 0x00, 4);
                }
            }
            ms_maskTex->UnlockRect(0);
            ms_atlasUsed = false;
        }
        
        ms_initialised = 1;
    }

    static inline void AddToRenderList(WaterDrop *drop)
    {
        static float uv[5][8] = {
            { 0.0f, 0.0f, 0.0f, 0.5f, 0.5f, 0.5f, 0.5f, 0.0f },
            { 0.0f, 0.5f, 0.0f, 1.0f, 0.5f, 1.0f, 0.5f, 0.5f },
            { 0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f },
            { 0.5f, 0.0f, 0.5f, 0.5f, 1.0f, 0.5f, 1.0f, 0.0f },
            { 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f }
        };
        static float xy[] = {
            -1.0f, -1.0f, -1.0f,  1.0f,
            1.0f,  1.0f,  1.0f, -1.0f
        };

        int i;
        float scale;

        float u1_1, u1_2;
        float v1_1, v1_2;
        float tmp;

        tmp = drop->uvsize*(300.0f - 40.0f) + 40.0f;
        u1_1 = drop->x + ms_xOff - tmp;
        v1_1 = drop->y + ms_yOff - tmp;
        u1_2 = drop->x + ms_xOff + tmp;
        v1_2 = drop->y + ms_yOff + tmp;
        u1_1 = (u1_1 <= 0.0f ? 0.0f : u1_1) / ms_fbWidth;
        v1_1 = (v1_1 <= 0.0f ? 0.0f : v1_1) / ms_fbHeight;
        u1_2 = (u1_2 >= ms_fbWidth ? ms_fbWidth : u1_2) / ms_fbWidth;
        v1_2 = (v1_2 >= ms_fbHeight ? ms_fbHeight : v1_2) / ms_fbHeight;

        scale = drop->size * 0.5f;

        for (i = 0; i < 4; i++) {
            ms_vertPtr->x = drop->x + xy[i * 2] * scale + ms_xOff;
            ms_vertPtr->y = drop->y + xy[i * 2 + 1] * scale + ms_yOff;
            ms_vertPtr->z = 0.0f;
            ms_vertPtr->rhw = 1.0f;
            ms_vertPtr->emissiveColor = D3DCOLOR_ARGB(drop->alpha, drop->r, drop->g, drop->b);
            ms_vertPtr->u0 = uv[drop->uv_index][i * 2];
            ms_vertPtr->v0 = uv[drop->uv_index][i * 2 + 1];
            ms_vertPtr->u1 = i >= 2 ? u1_2 : u1_1;
            ms_vertPtr->v1 = i % 3 == 0 ? v1_2 : v1_1;
            ms_vertPtr++;
        }
        ms_numBatchedDrops++;
    }

    static inline void Render(LPDIRECT3DDEVICE pDevice)
    {
        if (!ms_enabled || ms_numDrops <= 0)
            return;

        IDirect3DVertexBuffer* vbuf = ms_vertexBuf;
#if(DIRECT3D_VERSION < 0x0900)
        vbuf->Lock(0, 0, (BYTE**)&ms_vertPtr, 0);
#else
        vbuf->Lock(0, 0, (void**)&ms_vertPtr, 0);
#endif
        ms_numBatchedDrops = 0;
        for (auto& drop : ms_drops)
            if (drop.active)
                AddToRenderList(&drop);
        vbuf->Unlock();
        
#ifndef DISABLESTATEBLOCK
#if(DIRECT3D_VERSION < 0x0900)
        DWORD pStateBlock = NULL;
        pDevice->CreateStateBlock(D3DSBT_ALL, &pStateBlock);
        pDevice->CaptureStateBlock(pStateBlock);

        pDevice->CopyRects(ms_bbuf, 0, 0, ms_surf, 0);
#else
        LPDIRECT3DSTATEBLOCK9 pStateBlock = NULL;
        pDevice->CreateStateBlock(D3DSBT_ALL, &pStateBlock);
        pStateBlock->Capture();

        pDevice->StretchRect(ms_bbuf, NULL, ms_surf, NULL, D3DTEXF_LINEAR);
#endif
#endif

        pDevice->SetTexture(0, ms_maskTex);
        pDevice->SetTexture(1, ms_tex);

        pDevice->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
        pDevice->SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, 1);
        pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
        pDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TEXTURE);
        pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
        pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TEXTURE);
        pDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_MODULATE);
#ifndef SNOWDROPS
        pDevice->SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
#else
        pDevice->SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_TEXTURE | D3DTA_COMPLEMENT);
#endif
        pDevice->SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_CURRENT);
        pDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        pDevice->SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
#ifndef DISABLERENDERSTATES
        pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        pDevice->SetRenderState(D3DRS_LIGHTING, 0);
        pDevice->SetRenderState(D3DRS_ZENABLE, 0);
        pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, 1);
        pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, 0);
        pDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
        pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
#if(DIRECT3D_VERSION >= 0x0900)
        pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, 1);
#endif
        pDevice->SetRenderState(D3DRS_COLORWRITEENABLE, 0xFFFFFFFF);
#endif

        pDevice->SetPixelShader(NULL);
        pDevice->SetVertexShader(NULL);

#if(DIRECT3D_VERSION < 0x0900)
        pDevice->SetVertexShader(DROPFVF);
        pDevice->SetStreamSource(0, vbuf, sizeof(VertexTex2));
        pDevice->SetIndices(ms_indexBuf, 0);
        pDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, ms_numBatchedDrops * 4, 0, ms_numBatchedDrops * 2);
#else
        pDevice->SetFVF(DROPFVF);
        pDevice->SetStreamSource(0, vbuf, 0, sizeof(VertexTex2));
        pDevice->SetIndices(ms_indexBuf);
        pDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, ms_numBatchedDrops * 4, 0, ms_numBatchedDrops * 2);
#endif
        pDevice->SetTexture(1, NULL);
        pDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        pDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

#ifndef DISABLESTATEBLOCK
#if(DIRECT3D_VERSION < 0x0900)
        pDevice->ApplyStateBlock(pStateBlock);
        pDevice->DeleteStateBlock(pStateBlock);
#else
        pStateBlock->Apply();
        pStateBlock->Release();
#endif
#endif
    }
};

void WaterDrop::Fade()
{
    auto delta = WaterDrops::GetTimeStepInMilliseconds() * 100.0f;
    this->time += delta;
    if (this->time >= this->ttl) {
        WaterDrops::ms_numDrops--;
        this->active = 0;
    }
    else if (this->fades)
        this->alpha = (int8_t)(255.0f - time / ttl * 255.0f);
}

class CallbackHandler
{
public:
    static inline void CreateThreadAutoClose(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize, LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId)
    {
        CloseHandle(CreateThread(lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter, dwCreationFlags, lpThreadId));
    }

    static inline void RegisterCallback(std::function<void()>&& fn)
    {
        fn();
    }

    static inline void RegisterCallback(std::wstring_view module_name, std::function<void()>&& fn)
    {
        if (module_name.empty() || GetModuleHandleW(module_name.data()) != NULL)
        {
            fn();
        }
        else
        {
            RegisterDllNotification();
            GetCallbackList().emplace(module_name, std::forward<std::function<void()>>(fn));
        }
    }

    static inline void RegisterCallback(std::function<void()>&& fn, bool bPatternNotFound, ptrdiff_t offset = 0x1100, uint32_t* ptr = nullptr)
    {
        if (!bPatternNotFound)
        {
            fn();
        }
        else
        {
            auto mh = GetModuleHandle(NULL);
            IMAGE_NT_HEADERS* ntHeader = (IMAGE_NT_HEADERS*)((DWORD)mh + ((IMAGE_DOS_HEADER*)mh)->e_lfanew);
            if (ptr == nullptr)
                ptr = (uint32_t*)((DWORD)mh + ntHeader->OptionalHeader.BaseOfCode + ntHeader->OptionalHeader.SizeOfCode - offset);
            std::thread([](std::function<void()>&& fn, uint32_t* ptr, uint32_t val)
                {
                    while (*ptr == val)
                        std::this_thread::yield();

                    fn();
                }, fn, ptr, *ptr).detach();
        }
    }

    static inline void RegisterCallback(std::function<void()>&& fn, hook::pattern pattern)
    {
        if (!pattern.empty())
        {
            fn();
        }
        else
        {
            auto* ptr = new ThreadParams{ fn, pattern };
            CreateThreadAutoClose(0, 0, (LPTHREAD_START_ROUTINE)&ThreadProc, (LPVOID)ptr, 0, NULL);
        }
    }

private:
    static inline void call(std::wstring_view module_name)
    {
        if (GetCallbackList().count(module_name.data()))
        {
            GetCallbackList().at(module_name.data())();
            GetCallbackList().erase(module_name.data());
        }

        //if (GetCallbackList().empty()) //win7 crash in splinter cell
        //    UnRegisterDllNotification();
    }

    static inline void invoke_all()
    {
        for (auto&& fn : GetCallbackList())
            fn.second();
    }

private:
    struct Comparator {
        bool operator() (const std::wstring& s1, const std::wstring& s2) const {
            std::wstring str1(s1.length(), ' ');
            std::wstring str2(s2.length(), ' ');
            std::transform(s1.begin(), s1.end(), str1.begin(), tolower);
            std::transform(s2.begin(), s2.end(), str2.begin(), tolower);
            return  str1 < str2;
        }
    };

    static std::map<std::wstring, std::function<void()>, Comparator>& GetCallbackList()
    {
        static std::map<std::wstring, std::function<void()>, Comparator> functions;
        return functions;
    }

    struct ThreadParams
    {
        std::function<void()> fn;
        hook::pattern pattern;
    };

    typedef NTSTATUS(NTAPI* _LdrRegisterDllNotification) (ULONG, PVOID, PVOID, PVOID);
    typedef NTSTATUS(NTAPI* _LdrUnregisterDllNotification) (PVOID);

    typedef struct _LDR_DLL_LOADED_NOTIFICATION_DATA {
        ULONG Flags;                    //Reserved.
        PUNICODE_STRING FullDllName;    //The full path name of the DLL module.
        PUNICODE_STRING BaseDllName;    //The base file name of the DLL module.
        PVOID DllBase;                  //A pointer to the base address for the DLL in memory.
        ULONG SizeOfImage;              //The size of the DLL image, in bytes.
    } LDR_DLL_LOADED_NOTIFICATION_DATA, LDR_DLL_UNLOADED_NOTIFICATION_DATA, *PLDR_DLL_LOADED_NOTIFICATION_DATA, *PLDR_DLL_UNLOADED_NOTIFICATION_DATA;

    typedef union _LDR_DLL_NOTIFICATION_DATA {
        LDR_DLL_LOADED_NOTIFICATION_DATA Loaded;
        LDR_DLL_UNLOADED_NOTIFICATION_DATA Unloaded;
    } LDR_DLL_NOTIFICATION_DATA, *PLDR_DLL_NOTIFICATION_DATA;

private:
    static inline void CALLBACK LdrDllNotification(ULONG NotificationReason, PLDR_DLL_NOTIFICATION_DATA NotificationData, PVOID Context)
    {
        static constexpr auto LDR_DLL_NOTIFICATION_REASON_LOADED = 1;
        if (NotificationReason == LDR_DLL_NOTIFICATION_REASON_LOADED)
        {
            call(NotificationData->Loaded.BaseDllName->Buffer);
        }
    }

    static inline void RegisterDllNotification()
    {
        LdrRegisterDllNotification = (_LdrRegisterDllNotification)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "LdrRegisterDllNotification");
        if (LdrRegisterDllNotification && !cookie)
            LdrRegisterDllNotification(0, LdrDllNotification, 0, &cookie);
    }

    static inline void UnRegisterDllNotification()
    {
        LdrUnregisterDllNotification = (_LdrUnregisterDllNotification)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "LdrUnregisterDllNotification");
        if (LdrUnregisterDllNotification && cookie)
            LdrUnregisterDllNotification(cookie);
    }

    static inline DWORD WINAPI ThreadProc(LPVOID ptr)
    {
        auto paramsPtr = static_cast<CallbackHandler::ThreadParams*>(ptr);
        auto params = *paramsPtr;
        delete paramsPtr;

        HANDLE hTimer = NULL;
        LARGE_INTEGER liDueTime;
        liDueTime.QuadPart = -30 * 10000000LL;
        hTimer = CreateWaitableTimer(NULL, TRUE, NULL);
        SetWaitableTimer(hTimer, &liDueTime, 0, NULL, NULL, 0);

        while (params.pattern.clear().empty())
        {
            Sleep(0);

            if (WaitForSingleObject(hTimer, 0) == WAIT_OBJECT_0)
            {
                CloseHandle(hTimer);
                return 0;
            }
        };

        params.fn();

        return 0;
    }
private:
    static inline _LdrRegisterDllNotification   LdrRegisterDllNotification;
    static inline _LdrUnregisterDllNotification LdrUnregisterDllNotification;
    static inline void* cookie;
public:
    static inline std::once_flag flag;
};

bool IsUALPresent()
{
    ModuleList dlls;
    dlls.Enumerate(ModuleList::SearchLocation::LocalOnly);
    for (auto& e : dlls.m_moduleList)
    {
        if (GetProcAddress(std::get<HMODULE>(e), "DirectInput8Create") != NULL && GetProcAddress(std::get<HMODULE>(e), "DirectSoundCreate8") != NULL && GetProcAddress(std::get<HMODULE>(e), "InternetOpenA") != NULL)
            return true;
    }
    return false;
}
