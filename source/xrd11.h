#pragma once
#define WIN32_LEAN_AND_MEAN
#define _USE_MATH_DEFINES
#include <cmath>
#include <time.h>
#include <algorithm>
#include <filesystem>
#include <thread>
#include <mutex>
#include <map>
#include <iomanip>
#include <random>
#include <stacktrace>
#include "inireader/IniReader.h"
#include "Hooking.Patterns.h"
#include "includes/ModuleList.hpp"
#include "includes/FileWatch.hpp"

#include "sire.h"
#include "dropmask.h"

#include <subauth.h>

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
    static inline RwV3d ms_splashPoint;
    static inline float ms_splashDistance;
    static inline float ms_splashRemovalDistance;

    static inline bool sprayWater = false;
    static inline bool sprayBlood = false;
    static inline bool ms_StaticRain = false;
    static inline bool bRadial = false;
    static inline bool bGravity = true;
    static inline bool bBloodDrops = true;
    static inline bool bEnableSnow = false;
    static inline float fSpeedAdjuster = 1.0f;
    static inline int BackBufferMethod = 0;

    static inline RwV3d right;
    static inline RwV3d up;
    static inline RwV3d at;
    static inline RwV3d pos;

    static inline std::vector<std::pair<RwV3d, float>> ms_sprayLocations;

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

    static inline void Process()
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
            Init();

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
        BackBufferMethod = iniReader.ReadInteger("RENDER", "BackBufferMethod", 0);

        static std::once_flag flag;
        std::call_once(flag, [&]()
        {
            if (invertedRadial)
                bRadial = !bRadial;

            if (std::filesystem::exists(iniReader.GetIniPath()))
            {
                static filewatch::FileWatch<std::string> watch(iniReader.GetIniPath().string(), [&](const std::string& path, const filewatch::Event change_type)
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
        for (auto& it : ms_sprayLocations)
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

    static inline void NewDropMoving(WaterDrop* drop)
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

    // Rendering static inline 
    static inline SirePtr<Sire::tSireTexture2D> ms_tex = nullptr;
    static inline SirePtr<Sire::tSireTexture2D> ms_maskTex = nullptr;

    static inline std::vector<uint16_t> ms_indexBuf = {};

    static inline int32_t ms_fbWidth = 0;
    static inline int32_t ms_fbHeight = 0;
    static inline int32_t ms_numBatchedDrops;

    static inline int32_t ms_initialised = false;
    static inline bool ms_atlasUsed = true;

    static inline void Shutdown() {
        Reset();
    }

    static inline void Init()
    {
        if (!Sire::IsRendererActive())
            return;

        ReadIniSettings();

        auto windowSize = Sire::GetWindowSize();
        Sire::SetViewport(0.0f, 0.0f, static_cast<float>(windowSize.x), static_cast<float>(windowSize.y));

        ms_drops.resize(MaxDrops);
        ms_dropsMoving.resize(MaxDropsMoving);

        ms_indexBuf.resize(MaxDrops * 6 * sizeof(uint16_t));

        for (auto i = 0; i < MaxDrops; i++)
        {
            ms_indexBuf[i * 6 + 0] = i * 4 + 0;
            ms_indexBuf[i * 6 + 1] = i * 4 + 1;
            ms_indexBuf[i * 6 + 2] = i * 4 + 2;
            ms_indexBuf[i * 6 + 3] = i * 4 + 0;
            ms_indexBuf[i * 6 + 4] = i * 4 + 2;
            ms_indexBuf[i * 6 + 5] = i * 4 + 3;
        }

        ms_tex = Sire::CreateTexture(windowSize.x, windowSize.y, nullptr);

        ms_fbWidth = windowSize.x;
        ms_fbHeight = windowSize.y;
        ms_scaling = ms_fbHeight / 480.0f;

        ms_maskTex = Sire::CreateTexture(drop_mask.width, drop_mask.height, (uint8_t*)drop_mask.pixel_data);

        if (!ms_maskTex)
        {
            static constexpr auto MaskSize = 128;
            uint8_t* pixels = new uint8_t[MaskSize * MaskSize * 4];

            int32_t stride = MaskSize * 4;
            for (int y = 0; y < MaskSize; y++)
            {
                float yf = ((y + 0.5f) / MaskSize - 0.5f) * 2.0f;
                for (int x = 0; x < MaskSize; x++)
                {
                    float xf = ((x + 0.5f) / MaskSize - 0.5f) * 2.0f;
                    memset(&pixels[y * stride + x * 4], xf * xf + yf * yf < 1.0f ? 0xFF : 0x00, 4);
                }
            }

            ms_maskTex = Sire::CreateTexture(MaskSize, MaskSize, pixels);
            ms_atlasUsed = false;
            delete[] pixels;
        }

        ms_initialised = 1;
    }

    static inline void AddToRenderList(WaterDrop* drop)
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

        tmp = drop->uvsize * (300.0f - 40.0f) + 40.0f;
        u1_1 = drop->x + ms_xOff - tmp;
        v1_1 = drop->y + ms_yOff - tmp;
        u1_2 = drop->x + ms_xOff + tmp;
        v1_2 = drop->y + ms_yOff + tmp;
        u1_1 = (u1_1 <= 0.0f ? 0.0f : u1_1) / ms_fbWidth;
        v1_1 = (v1_1 <= 0.0f ? 0.0f : v1_1) / ms_fbHeight;
        u1_2 = (u1_2 >= ms_fbWidth ? ms_fbWidth : u1_2) / ms_fbWidth;
        v1_2 = (v1_2 >= ms_fbHeight ? ms_fbHeight : v1_2) / ms_fbHeight;

        scale = drop->size * 0.5f;

        Sire::SetColor4f(drop->r / 255.0f, drop->g / 255.0f, drop->b / 255.0f, drop->alpha / 255.0f);

        for (i = 0; i < 4; i++) {
            Sire::SetTexCoords4f(i >= 2 ? u1_2 : u1_1, i % 3 == 0 ? v1_2 : v1_1,
                uv[drop->uv_index][i * 2], uv[drop->uv_index][i * 2 + 1]);
            Sire::SetVertex2f(drop->x + xy[i * 2] * scale + ms_xOff, drop->y + xy[i * 2 + 1] * scale + ms_yOff);
        }
        ms_numBatchedDrops++;
    }

    static inline void Render()
    {
        if (!ms_enabled || ms_numDrops <= 0)
            return;

        if (!ms_initialised)
            return;

        if (!Sire::IsRendererActive())
            return;

        auto windowSize = Sire::GetWindowSize();
        if (windowSize.x != ms_fbWidth || windowSize.y != ms_fbHeight) {
            Reset();
            Sire::Shutdown();
            return;
        }

        auto backBuffer = Sire::GetBackBuffer(BackBufferMethod);
        Sire::CopyResource(ms_tex, backBuffer);

        Sire::SetRenderState(Sire::SIRE_BLEND_ALPHATESTENABLE, true);
        Sire::SetRenderState(Sire::SIRE_BLEND_SRCBLEND, Sire::SIRE_BLEND_SRC_ALPHA);
        Sire::SetRenderState(Sire::SIRE_BLEND_DESTBLEND, Sire::SIRE_BLEND_INV_SRC_ALPHA);
        Sire::SetRenderState(Sire::SIRE_BLEND_BLENDOP, Sire::SIRE_BLEND_OP_ADD);
        Sire::SetRenderState(Sire::SIRE_BLEND_SRCBLENDALPHA, Sire::SIRE_BLEND_ONE);
        Sire::SetRenderState(Sire::SIRE_BLEND_DESTBLENDALPHA, Sire::SIRE_BLEND_ZERO);
        Sire::SetRenderState(Sire::SIRE_BLEND_BLENDOPALPHA, Sire::SIRE_BLEND_OP_ADD);
        Sire::SetRenderState(Sire::SIRE_BLEND_WRITEMASK, Sire::SIRE_COLOR_WRITE_ENABLE_ALL);
        Sire::SetRenderState(Sire::SIRE_BLEND_CULLMODE, Sire::SIRE_CULL_NONE);
        Sire::SetRenderState(Sire::SIRE_BLEND_FILLMODE, Sire::SIRE_FILL_SOLID);
        Sire::SetRenderState(Sire::SIRE_BLEND_STENCILENABLE, FALSE);
        Sire::SetRenderState(Sire::SIRE_BLEND_COLORWRITEENABLE, 0xFFFFFFFF);

        Sire::SetProjectionMode(Sire::SIRE_PROJ_ORTHOGRAPHIC);
        Sire::SetTexture(ms_tex, ms_maskTex);
        Sire::Begin(Sire::SIRE_TRIANGLE);
        
        ms_numBatchedDrops = 0;
        for (auto& drop : ms_drops)
            if (drop.active)
                AddToRenderList(&drop);
        
        Sire::SetIndices(ms_indexBuf, ms_numBatchedDrops * 6);
        Sire::End();
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

    static inline void RegisterCallback(std::function<void()>&& fn, hook::pattern pattern)
    {
        if (!pattern.empty())
        {
            fn();
        }
        else
        {
            auto* ptr = new ThreadParams{ fn, pattern };
            CloseHandle(CreateThread(0, 0, (LPTHREAD_START_ROUTINE)&ThreadProc, (LPVOID)ptr, 0, NULL));
        }
    }

private:
    static inline void call(std::wstring_view module_name)
    {
        if (GetCallbackList().count(module_name.data()))
        {
            GetCallbackList().at(module_name.data())();
        }
    }

    static inline void invoke_all()
    {
        for (auto&& fn : GetCallbackList())
            fn.second();
    }

private:
    static inline std::map<std::wstring, std::function<void()>>& GetCallbackList()
    {
        return functions;
    }

    struct ThreadParams
    {
        std::function<void()> fn;
        hook::pattern pattern;
    };

    typedef NTSTATUS(NTAPI* _LdrRegisterDllNotification) (ULONG, PVOID, PVOID, PVOID);
    typedef NTSTATUS(NTAPI* _LdrUnregisterDllNotification) (PVOID);

    typedef struct _LDR_DLL_LOADED_NOTIFICATION_DATA
    {
        ULONG Flags;                    //Reserved.
        PUNICODE_STRING FullDllName;    //The full path name of the DLL module.
        PUNICODE_STRING BaseDllName;    //The base file name of the DLL module.
        PVOID DllBase;                  //A pointer to the base address for the DLL in memory.
        ULONG SizeOfImage;              //The size of the DLL image, in bytes.
    } LDR_DLL_LOADED_NOTIFICATION_DATA, LDR_DLL_UNLOADED_NOTIFICATION_DATA, * PLDR_DLL_LOADED_NOTIFICATION_DATA, * PLDR_DLL_UNLOADED_NOTIFICATION_DATA;

    typedef union _LDR_DLL_NOTIFICATION_DATA
    {
        LDR_DLL_LOADED_NOTIFICATION_DATA Loaded;
        LDR_DLL_UNLOADED_NOTIFICATION_DATA Unloaded;
    } LDR_DLL_NOTIFICATION_DATA, * PLDR_DLL_NOTIFICATION_DATA;

    typedef NTSTATUS(NTAPI* PLDR_MANIFEST_PROBER_ROUTINE)
        (
            IN HMODULE DllBase,
            IN PCWSTR FullDllPath,
            OUT PHANDLE ActivationContext
            );

    typedef NTSTATUS(NTAPI* PLDR_ACTX_LANGUAGE_ROURINE)
        (
            IN HANDLE Unk,
            IN USHORT LangID,
            OUT PHANDLE ActivationContext
            );

    typedef void(NTAPI* PLDR_RELEASE_ACT_ROUTINE)
        (
            IN HANDLE ActivationContext
            );

    typedef VOID(NTAPI* fnLdrSetDllManifestProber)
        (
            IN PLDR_MANIFEST_PROBER_ROUTINE ManifestProberRoutine,
            IN PLDR_ACTX_LANGUAGE_ROURINE CreateActCtxLanguageRoutine,
            IN PLDR_RELEASE_ACT_ROUTINE ReleaseActCtxRoutine
            );

private:
    static inline void CALLBACK LdrDllNotification(ULONG NotificationReason, PLDR_DLL_NOTIFICATION_DATA NotificationData, PVOID Context)
    {
        static constexpr auto LDR_DLL_NOTIFICATION_REASON_LOADED = 1;
        if (NotificationReason == LDR_DLL_NOTIFICATION_REASON_LOADED)
        {
            call(NotificationData->Loaded.BaseDllName->Buffer);
        }
    }

    static inline NTSTATUS NTAPI ProbeCallback(IN HMODULE DllBase, IN PCWSTR FullDllPath, OUT PHANDLE ActivationContext)
    {
        std::wstring str(FullDllPath);
        call(str.substr(str.find_last_of(L"/\\") + 1));

        HANDLE actx = NULL;
        ACTCTXW act = { 0 };

        act.cbSize = sizeof(act);
        act.dwFlags = ACTCTX_FLAG_RESOURCE_NAME_VALID | ACTCTX_FLAG_HMODULE_VALID;
        act.lpSource = FullDllPath;
        act.hModule = DllBase;
        act.lpResourceName = ISOLATIONAWARE_MANIFEST_RESOURCE_ID;

        // Reset pointer, crucial for x64 version
        *ActivationContext = 0;

        actx = CreateActCtxW(&act);

        // Report no manifest is present
        if (actx == INVALID_HANDLE_VALUE)
            return 0xC000008B; //STATUS_RESOURCE_NAME_NOT_FOUND;

        *ActivationContext = actx;

        return STATUS_SUCCESS;
    }

    static inline void RegisterDllNotification()
    {
        LdrRegisterDllNotification = (_LdrRegisterDllNotification)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "LdrRegisterDllNotification");
        if (LdrRegisterDllNotification)
        {
            if (!cookie)
                LdrRegisterDllNotification(0, LdrDllNotification, 0, &cookie);
        }
        else
        {
            LdrSetDllManifestProber = (fnLdrSetDllManifestProber)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "LdrSetDllManifestProber");
            if (LdrSetDllManifestProber)
            {
                LdrSetDllManifestProber(&ProbeCallback, NULL, &ReleaseActCtx);
            }
        }
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
    static inline fnLdrSetDllManifestProber     LdrSetDllManifestProber;
public:
    static inline std::once_flag flag;
    static inline std::map<std::wstring, std::function<void()>> functions;
};

bool IsModuleUAL(HMODULE mod)
{
    if (GetProcAddress(mod, "IsUltimateASILoader") != NULL || (GetProcAddress(mod, "DirectInput8Create") != NULL && GetProcAddress(mod, "DirectSoundCreate8") != NULL && GetProcAddress(mod, "InternetOpenA") != NULL))
        return true;
    return false;
}

bool IsUALPresent() {
    for (const auto& entry : std::stacktrace::current()) {
        HMODULE hModule = NULL;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)entry.native_handle(), &hModule)) {
            if (IsModuleUAL(hModule))
                return true;
        }
    }
    return false;
}