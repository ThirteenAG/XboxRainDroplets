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
#include <functional>
#include <format>
#include "inireader/IniReader.h"
#include "Hooking.Patterns.h"
#include "includes/ModuleList.hpp"
#include "includes/FileWatch.hpp"
#include "includes/callbacks.h"
#include "includes/gameref.hpp"
/// The games expect the hooking library from this header, the old one pulled it in
/// as well.
#include <injector\injector.hpp>
#include <injector\hooking.hpp>
#include <injector\calling.hpp>
#include <injector\utility.hpp>
#include <injector\assembly.hpp>

// ---------------------------------------------------------------------------
// The drops are drawn by the renderer in source/xrd, one small backend per
// graphics API: Direct3D 8, 9, 10, 10.1, 11 and 12, OpenGL and Vulkan. A
// project defines the API the game is before including this header, only those
// backends are compiled in.
//
// Direct3D 8 is the one API that cannot share a translation unit with Direct3D
// 9, so a Direct3D 8 game says XRD_ENABLE_D3D8 and nothing else: the headers,
// the device type and the renderer all follow from it. A binary that wants both
// versions at once (the wrapper) leaves that define out and adds
// source/xrd/xrdrender.d3d8.cpp to the project instead.
// ---------------------------------------------------------------------------
#include "xrd/xrdrender.h"

#if defined(XRD_ENABLE_D3D8)
#include "xrd/xrdrender.d3d8.h"
#endif

#if defined(XRD_ENABLE_D3D9)
#include "xrd/xrdrender.d3d9.h"
#endif

#if defined(XRD_ENABLE_D3D10) || defined(XRD_ENABLE_D3D10_1)
#include "xrd/xrdrender.d3d10.h"
#endif

#if defined(XRD_ENABLE_D3D11)
#include "xrd/xrdrender.d3d11.h"
#endif

#if defined(XRD_ENABLE_D3D12)
#include "xrd/xrdrender.d3d12.h"
#endif

#if defined(XRD_ENABLE_OPENGL)
#include "xrd/xrdrender.gl.h"
#endif

#if defined(XRD_ENABLE_VULKAN)
#include "xrd/xrdrender.vk.h"
#endif

// The drawing of an emulator that hands the frame of a game over where it is in
// between its world and its UI, which serves whichever API the emulator runs.
#if defined(XRD_ENABLE_THIN3D)
#include "xrd/xrdrender.thin3d.h"
#endif

// ---------------------------------------------------------------------------
// The device type and the renderer that go with the API the project named. A
// game is one or the other, so XRD_ENABLE_D3D8 is all a Direct3D 8 project has
// to say about it and everything else stays on Direct3D 9.
// ---------------------------------------------------------------------------
#if defined(XRD_ENABLE_D3D8)
#include <d3d8.h>
#include <d3dx8.h>
#include <d3dx8tex.h>
#pragma comment(lib, "legacy_stdio_definitions.lib")
#pragma comment(lib, "d3d8.lib")
#pragma comment(lib, "D3dx8.lib")
typedef LPDIRECT3DDEVICE8 LPDIRECT3DDEVICE;
#define XRD_DEVICE_RENDERER Xrd::RENDERER_D3D8
#else
#include <d3d9.h>
#if !defined(XRD_NO_D3DX)
// the Direct3D 9 helpers, the games use them for their matrices and textures
#include <d3dx9.h>
#include <d3dx9tex.h>
#pragma comment(lib, "d3dx9.lib")
#endif
typedef LPDIRECT3DDEVICE9 LPDIRECT3DDEVICE;
#define XRD_DEVICE_RENDERER Xrd::RENDERER_D3D9
#endif

// the masks of the drop shapes and the shaders of the refraction, embedded exactly
// like the original builds did: see Xrd::resources in xrdcommon.h, which is where
// the ids live, and source/resources/Dropmask.rc, which has to agree with them

// Windows decodes the PNG masks on its own, no library has to be shipped
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

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

inline float RwV3dLength(const RwV3d* in)
{
    return sqrtf(in->x * in->x + in->y * in->y + in->z * in->z);
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

// The world space snow and rain streaks of the consoles (the old snow.h). It
// needs the engine vector and matrix types above and nothing else, so it comes
// with this header everywhere.
#include "xrd/xrdsnow.h"

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
    // The alpha this drop fades down from, which is what it was handed when it was
    // placed and what Fade counts the alpha of the drop from. Water a drop leaves
    // is handed the alpha of the drop itself, so a drop that has almost faded out
    // can not leave water that shows up brighter than the drop does.
    uint8_t alpha0 = 0xFF;

    // How fast this bead runs down the glass, in pixels of a frame of the
    // effect's time. Zero is a bead the surface tension holds where it is: it
    // does not run, and the only thing that moves it is the camera.
    float slide = 0.0f;
    float shapeX = 0.0f, shapeY = 0.0f; // filtered velocity for elastic deformation

    // The water this bead leaves behind it, as drops of the rain of its own: a drop
    // of water running over a pane of glass leaves drops of water where it has been,
    // and what it left takes a place in the pool and is drawn by exactly the same
    // code as the bead it came from, with its shape and its colour, see NewTrace.
    // How long one of them stays on the glass is rolled when the bead is placed, and
    // it is what the length of the tail of this bead is: a bead whose water dries
    // quickly has a short tail and one whose water stays has a long one.
    float traceTtl = 0.0f;

    // The water a bead left behind it, see NewTrace. It is drawn like any drop
    // but it is not a lens: a film of water on the glass gathers no light.
    bool isTrace = false;

    bool active;
    bool fades;
    void Fade();
};

class WaterDropMoving
{
public:
    WaterDrop* drop;
    // Remaining screen-space travel since the last deposit, in pixels.
    float dist = 0.0f;
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
    // how many of the drops of the pool are the water of trails, see NewTrace
    static inline int32_t ms_numTraces;
    // where the search for a free place in the pool goes on from, see AcquireSlot
    static inline int32_t ms_nextFree;

    static inline bool ms_enabled;
    static inline bool ms_movingEnabled;

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
    static inline bool bInvertedRadial = false;
    static inline bool bGravity = true;
    static inline bool bBloodDrops = true;
    static inline bool bEnableSnow = false;

    // -----------------------------------------------------------------------
    // the water a bead leaves behind it
    //
    // A bead that travels over the glass leaves drops of water where it has been,
    // and what it left is a drop of the rain like any other: it takes a place in
    // the pool, it is drawn by the same code with the same shape and the same
    // colour as the bead it came from, and the only thing about it that is not a
    // drop is that it does not move. How much of a tail a bead has is how long the
    // water it left stays on the glass, and that is rolled for every bead of the
    // rain on its own, so the tails of one shower are of every length there is.
    // -----------------------------------------------------------------------
    // Requested deposit spacing in pixels. MoveDrop applies a small minimum
    // based on the footprint to avoid redundant overlapping deposits.
    static inline float fMoveStep = 0.1f;
    // The part of the life of a bead that one of the drops it leaves behind it
    // lasts, before the length of the tail of a bead is rolled for. The effect has
    // always divided the life of a drop by SC(4) for this, and that is kept,
    // because it is what the tails of the rain have always looked like.
    static constexpr float TraceLifeBase = 4.0f;
    // And what the tail of one bead is multiplied by, rolled for every bead of the
    // rain on its own: a bead whose water dries quickly leaves a short tail behind
    // it, a bead whose water stays leaves a long one that reaches back over the
    // glass, so the tails of one shower are of every length there is and no two
    // beads leave the same. See PlaceNew.
    static constexpr float TraceLifeMin = 0.35f;
    static constexpr float TraceLifeMax = 2.5f;
    // Every backend holds a vertex buffer of a fixed size, which is the 64000
    // vertices of 16000 quads below, and one drop of the rain is one quad of it.
    // The drops a bead leaves behind it are drops of the rain, so the pool is what
    // bounds the water of the effect: see MaxDrops, which ResizePools keeps inside
    // this.
    static constexpr int32_t MaxQuads = 16000;
    // One bead in two is held where it is by the surface tension and never runs
    // down the glass, and it is not the small ones: see PlaceNew.
    static constexpr float HangingShare = 0.5f;
    // The share of the pool the water of the trails may not fill: the rain has to
    // be able to go on in a downpour, and a drop of it may take the place of the
    // water of a trail when nothing else is free, see AcquireSlot.
    static constexpr float PoolReserveShare = 0.2f;

    // The pixels of the frame one pixel of the 480 lines the effect was made for
    // is. The sizes of the drops have always been scaled by it; the speeds they
    // run and drift at, the window of the frame they show and the life of their
    // water are scaled by it too, so the rain behaves the same on a screen of any
    // size instead of crawling on a large one.
    static inline float Scale()
    {
        return ms_scaling > 0.0f ? ms_scaling : 1.0f;
    }

    // How much of a second this frame is, capped so that a stall of the game does
    // not turn into a burst of rain: what the rain of a frame is measured by.
    static inline float FrameSeconds()
    {
        const float elapsed = GetFrameTimeSeconds();

        if (!(elapsed > 0.0f) || !std::isfinite(elapsed))
            return 0.0f;

        return (std::min)(elapsed, 0.1f);
    }

    // A drop of clear water is a lens: the shaders that draw the drops give it
    // the colour of the light of the frame around it, see source/shaders and
    // xrdrender.d3d8.h. That is only possible where the drops are drawn with a
    // shader at all, which is what GatheredLight below answers, and it is only
    // worth it for the drops of clear rain: a drop the game asked for in a colour
    // of its own is that colour.
    static inline bool bRefractions = true;

    // The light a drop gathers is measured out of the frame it is drawn into, and
    // against that frame, see source/shaders: it is the frame of the game the
    // effect is a part of. The plugins of an emulator draw the drops into the
    // frame of another machine instead, handed over in the middle of its own
    // frame, and that frame is neither as bright as the ones the effect measures
    // against nor always in the same place, so a drop of clear water over it
    // comes out white. Those hosts say so here and their drops are drawn the way
    // the fixed function renderers draw them.
    static inline bool bOwnFrame = true;

    // The renderers that draw the drops with a shader that gathers the light of
    // the frame around them. Direct3D 9 and above load embedded shader bytecode,
    // built from source/shaders, so they are known here; Direct3D 8 builds
    // one of its own, out of a device that may not be able to run it at all, and
    // its backend is what answers for it (see xrdrender.d3d8.h).
    static inline bool GatheredLight()
    {
        if (!bRefractions || !bOwnFrame || bEnableSnow)
            return false;

        const auto api = Xrd::GetRenderer();

        if (api == Xrd::RENDERER_D3D8)
            return Xrd::GathersLight();

        return api == Xrd::RENDERER_D3D9 || api == Xrd::RENDERER_D3D10 || api == Xrd::RENDERER_D3D10_1 ||
            api == Xrd::RENDERER_D3D11 || api == Xrd::RENDERER_D3D12;
    }

    static inline bool IsLens(const WaterDrop* drop)
    {
        return !drop->isTrace && drop->r == drop->g && drop->g == drop->b && GatheredLight();
    }

    // A game that is not raining spawns no drops at all, which is what the effect is for and
    // also what makes it impossible to look at while the weather of a game is being worked on.
    // The ini can hold the rain on, whatever the game reports, which is what ForceRain is for.
    static inline bool bForceRain = false;
    static inline float fSpeedAdjuster = 1.0f;

    // Kept because the games assign them, exactly like the original header did.
    static inline void(*ProcessCallback1)();
    static inline void(*ProcessCallback2)();

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

    // Adapters supply seconds (including the GTA adapters, which convert their
    // native 50 Hz timer before assigning fTimeStep).
    static inline float GetFrameTimeSeconds()
    {
        return GetTimeStep();
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

        EnsureDevice();

        // A game that is paused has stopped its clock, and the glass stops with
        // it: what is on it stays as it is until the game goes on.
        if (isPaused)
            return;

        ProcessGlobalEmitters();
        CalculateMovement();
        SprayDrops();
        ProcessMoving();
        Fade();
    }

    static inline void ReadIniSettings(bool invertedRadial = false)
    {
        bInvertedRadial = invertedRadial;

        CIniReader iniReader("");
        MinSize = iniReader.ReadInteger("MAIN", "MinSize", 4);
        MaxSize = iniReader.ReadInteger("MAIN", "MaxSize", 15);
        MaxDrops = iniReader.ReadInteger("MAIN", "MaxDrops", 3000);
        MaxDropsMoving = iniReader.ReadInteger("MAIN", "MaxMovingDrops", 6000);
        bRadial = iniReader.ReadInteger("MAIN", "RadialMovement", 0) != 0;
        bGravity = iniReader.ReadInteger("MAIN", "EnableGravity", 1) != 0;
        bRefractions = iniReader.ReadInteger("MAIN", "Refractions", 1) != 0;
        fSpeedAdjuster = iniReader.ReadFloat("MAIN", "SpeedAdjuster", 1.0f);
        fMoveStep = iniReader.ReadFloat("MAIN", "MoveStep", 0.1f);
        bBloodDrops = iniReader.ReadInteger("MAIN", "BloodDrops", 1) != 0;
        bEnableSnow = iniReader.ReadInteger("BONUS", "EnableSnow", 0) != 0;
        bForceRain = iniReader.ReadInteger("MAIN", "ForceRain", 0) != 0;

        if (invertedRadial)
            bRadial = !bRadial;

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
                        ReadIniSettings(bInvertedRadial);
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
                WaterDrops::FillScreenMovingRate(it.second);
        }
    }

    static inline void CalculateMovement()
    {
        RwV3dSub(&ms_posDelta, &pos, &ms_lastPos);

        if (fSpeedAdjuster)
        {
            right.x *= fSpeedAdjuster;
            right.y *= fSpeedAdjuster;
            right.z *= fSpeedAdjuster;
            up.x *= fSpeedAdjuster;
            up.y *= fSpeedAdjuster;
            up.z *= fSpeedAdjuster;
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
        // The drift the camera gives the drops is in pixels of the frame, and it
        // has always been measured for the 480 lines the effect comes from: on a
        // screen of more lines it is scaled with everything else, or the drops of
        // a large screen would crawl.
        RwV3dScale(&ms_vec, &ms_vec, 10.0f * Scale());
        ms_vecLen = sqrt(ms_vec.y * ms_vec.y + ms_vec.x * ms_vec.x);

        ms_enabled = true; //!istopdown && !carlookdirection;
        ms_movingEnabled = true; //!istopdown && !carlookdirection;

        float c = at.z;
        if (c > 1.0f) c = 1.0f;
        if (c < -1.0f) c = -1.0f;
        ms_rainStrength = (float)RAD2DEG(acos(c));
    }

    // -----------------------------------------------------------------------
    // the rain of a frame
    //
    // The rain is a rate and not a count: a frame adds the drops of its own share
    // of a second and carries what is left over into the next one, so twice the
    // frames a second is not twice the rain, and a light rain that adds less than
    // a drop a frame still adds its drops. The amount is what the games have
    // always handed over, and the number of drops it stood for in one frame of
    // sixty a second is what it still stands for.
    // -----------------------------------------------------------------------
    static inline float ms_spawnRemainder = 0.0f;
    static inline float ms_bloodRemainder = 0.0f;

    // the number of drops an amount stands for in one frame of sixty a second
    static inline float DropsOfAmount(float amount)
    {
        return (ms_vec.z <= 5.0f * Scale() ? 1.0f : 1.5f) * amount * 20.0f;
    }

    // The rain of one frame: the amount is spread over the time the frame took,
    // see above. A game that keeps a spray going hands this over every frame,
    // and a splash of the camera is one burst of FillScreenMoving instead.
    static inline void FillScreenMovingRate(float amount, bool isBlood = false)
    {
        if (!ms_initialised || ms_fbWidth <= 0 || ms_fbHeight <= 0)
            return;

        if (ms_StaticRain)
            amount = 1.0f;

        // see FillScreenMoving: the amount is not trusted to be a sane count
        if (!(amount > 0.0f))
            return;

        float& remainder = isBlood ? ms_bloodRemainder : ms_spawnRemainder;
        const float perFrame = DropsOfAmount(amount);
        remainder = (std::min)(remainder + perFrame * FrameSeconds() * 60.0f, perFrame * 4.0f + 1.0f);

        if (remainder < 1.0f)
            return;

        const float whole = floorf(remainder);
        remainder -= whole;

        if (isBlood)
            SpawnDrops((int32_t)whole, 0xFF, 0x00, 0x00);
        else
            SpawnDrops((int32_t)whole, 0xFF, 0xFF, 0xFF);
    }

    static inline void SprayDrops()
    {
        // A rain intensity that is negative or not a number at all is not rain. The
        // check for "not zero" is one that a not a number passes, and the branch
        // below turns the intensity into a number of drops.
        if (!NoRain() && (ms_rainIntensity > 0.0f || bForceRain) && ms_enabled)
        {
            auto tmp = (int32_t)(180.0f - ms_rainStrength);
            if (tmp < 40) tmp = 40;
            FillScreenMovingRate((tmp - 40.0f) / 150.0f * (bForceRain ? 1.0f : ms_rainIntensity) * 0.5f);
        }
        if (sprayWater)
            FillScreenMovingRate(0.5f, false);
        if (sprayBlood)
            FillScreenMovingRate(0.5f, true);
        if (ms_splashDuration >= 0)
        {
            RwV3d dist;
            RwV3dSub(&dist, &ms_splashPoint, &ms_lastPos);
            float f = RwV3dDotProduct(&dist, &dist);
            f = sqrt(f);
            if (f <= ms_splashDistance)
                FillScreenMovingRate(1.0f);
            else if (ms_splashRemovalDistance > 0.0f && f >= ms_splashRemovalDistance)
                ms_splashDuration = -1;
            ms_splashDuration--;
        }
    }

    // A number of drops of the rain, placed at random over the glass, of sizes
    // between the smallest and the largest the ini asks for, and with lives
    // between one and four seconds of the effect.
    static inline void SpawnDrops(int32_t n, int R, int G, int B)
    {
        if (!ms_initialised || ms_fbWidth <= 0 || ms_fbHeight <= 0)
            return;

        const int32_t room = RoomForNewDrops();

        if (n > room)
            n = room;

        const float smallest = (float)SC(MinSize);
        const float biggest = (float)(std::max)(SC(MaxSize), SC(MinSize));

        for (int32_t i = 0; i < n; i++)
        {
            const float x = GetRandomFloat((float)ms_fbWidth);
            const float y = GetRandomFloat((float)ms_fbHeight);
            const float size = smallest + GetRandomFloat(biggest - smallest);
            const float ttl = 2000.0f + GetRandomFloat(6000.0f);
            WaterDrop* drop = PlaceNew(x, y, size, ttl, 1, R, G, B);

            if (drop)
                NewDropMoving(drop);
        }
    }

    // Deposited water keeps the parent's colour, atlas shape and opacity. It
    // stays on the glass and fades independently, without producing more traces.
    // MoveDrop distributes these deposits along the path, with bounded density.
    static inline void NewTrace(WaterDrop* drop, float x, float y, float velocityX, float velocityY)
    {
        // A shrinking parent cannot leave a bead larger than itself.
        const float size = (std::min)((float)SC(MinSize), drop->size * 0.65f);
        auto* trace = PlaceNew(x, y, size, drop->traceTtl, true, drop->r, drop->g, drop->b, true);

        if (!trace)
            return;

        // the water of a drop is as visible as the drop is, and no more: a drop that
        // has almost faded out leaves water that fades from where the drop is now
        trace->alpha = drop->alpha;
        trace->alpha0 = drop->alpha;

        // what the glass holds does not run down it, and water that was left behind
        // is water of its own and not one of the drops that move
        trace->slide = 0.0f;
        trace->uv_index = drop->uv_index;
        // Deposited water follows the direction of travel, without becoming a mover.
        trace->shapeX = velocityX;
        trace->shapeY = velocityY;
    }

    static void MoveDrop(WaterDropMoving* moving, float dt, float response, float shrink)
    {
        WaterDrop* drop = moving->drop;
        if (!ms_movingEnabled)
            return;
        if (!drop->active)
        {
            DetachMoving(drop);
            return;
        }

        const float slide = bGravity ? drop->slide * (60.0f * dt) : 0.0f;

        float d = abs(ms_vec.z * 0.2f);
        float dx, dy, sum;
        dx = drop->x - ms_fbWidth * 0.5f + ms_vec.x;
        dy = drop->y - ms_fbHeight * 0.5f - (ms_vec.y + slide);
        sum = fabs(dx) + fabs(dy);
        if (sum >= 0.001f)
        {
            dx *= (1.0f / sum);
            dy *= (1.0f / sum);
        }

        // What the drop travels over the glass in this frame: the drift the camera
        // gives it and the slide of the drop itself.
        const float travelX = (dx * d) - ms_vec.x;
        const float travelY = (dy * d) + (ms_vec.y + slide);
        if (dt > 0.0f)
        {
            drop->shapeX += (travelX / dt - drop->shapeX) * response;
            drop->shapeY += (travelY / dt - drop->shapeY) * response;
        }

        // Deposit by actual distance, not camera speed or frame count. A small
        // footprint-dependent minimum avoids piling hundreds of nearly identical
        // drops on top of each other during a slow slide.
        const float distance = sqrtf(travelX * travelX + travelY * travelY);
        const float footprint = (std::min)((float)SC(MinSize), drop->size * 0.65f);
        const float spacing = (std::max)(std::isfinite(fMoveStep) ? fMoveStep : 0.1f,
            (std::max)(0.25f, (std::min)(1.5f, footprint * 0.12f)));
        if (distance > 0.0001f && std::isfinite(distance))
        {
            const float remainder = fmodf(moving->dist, spacing);
            const float total = remainder + distance;
            const float crossed = floorf(total / spacing);
            // Spread a bounded number of deposits across a fast sweep. Never
            // carry a spawn backlog into stationary frames after a camera cut.
            const int count = (int)(std::min)(crossed, 8.0f);
            if (drop->alpha > 0 && ms_numDrops < (int)ms_drops.size() - 1)
                for (int i = 0; i < count; ++i)
                {
                    const float step = crossed > 8.0f ? (i + 0.5f) * distance / count
                        : spacing - remainder + i * spacing;
                    const float t = std::clamp(step / distance, 0.0f, 1.0f);
                    NewTrace(drop, drop->x + travelX * t, drop->y + travelY * t,
                        drop->shapeX, drop->shapeY);
                }
            moving->dist = fmodf(total, spacing);
        }

        drop->x += travelX;
        drop->y += travelY;

        // Equivalent to the old 60 Hz shrink rate, integrated independently of FPS.
        drop->size *= shrink;

        // A drop that left the glass is gone: it would otherwise keep its place in
        // the pool until it faded, drawn where nobody sees it.
        if (drop->x < -(float)(SC(MaxSize)) || drop->y < -(float)(SC(MaxSize)) ||
            drop->x >(ms_fbWidth + SC(MaxSize)) || drop->y >(ms_fbHeight + SC(MaxSize)))
        {
            Expire(drop);
        }
    }

    static inline void ProcessMoving()
    {
        if (!ms_movingEnabled)
            return;
        const float elapsed = GetFrameTimeSeconds();
        if (!(elapsed > 0.0f) || !std::isfinite(elapsed))
            return;
        // Preserve 60 Hz tuning; bound recovery after a stall. These coefficients
        // are shared by every moving bead, so compute exponentials once per frame.
        const float dt = (std::min)(elapsed, 0.1f);
        const float response = 1.0f - expf(-dt / 0.08f);
        const float shrink = expf(-0.200334f * dt);
        for (auto& moving : ms_dropsMoving)
            if (moving.drop)
                MoveDrop(&moving, dt, response, shrink);
    }

    static inline void Fade()
    {
        for (auto& drop : ms_drops)
            if (drop.active)
                drop.Fade();
    }

    // the places of the pool the water of the trails leaves to the rain
    static inline int32_t TraceReserve()
    {
        return (std::max)(64, (int32_t)((float)ms_drops.size() * PoolReserveShare));
    }

    // A free place in the pool, searched from where the last search ended so that
    // a nearly full pool is not walked from its start for every drop. Nothing is
    // free for the water of a trail once the trails hold their share of the pool,
    // and a drop of the rain takes the place of the water of a trail when nothing
    // else is free: the rain has to go on in a downpour.
    static inline int32_t AcquireSlot(bool forTrace)
    {
        const int32_t count = (int32_t)ms_drops.size();

        if (count <= 0)
            return -1;

        if (forTrace && ms_numTraces >= count - TraceReserve())
            return -1;

        for (int32_t i = 0; i < count; i++)
        {
            const int32_t index = (ms_nextFree + i) % count;

            if (!ms_drops[index].active)
            {
                ms_nextFree = (index + 1) % count;
                return index;
            }
        }

        if (forTrace)
            return -1;

        for (int32_t i = 0; i < count; i++)
        {
            const int32_t index = (ms_nextFree + i) % count;
            WaterDrop& other = ms_drops[index];

            if (other.active && other.isTrace)
            {
                Expire(&other);
                ms_nextFree = (index + 1) % count;
                return index;
            }
        }

        return -1;
    }

    // A drop the effect is done with. It is taken out of the list of the drops that
    // move before its place in the pool is handed out again: an entry that still
    // pointed at it would move and age whichever drop takes the place next, which
    // is what a drop that runs at twice the speed looks like.
    static inline void Expire(WaterDrop* drop)
    {
        if (!drop->active)
            return;

        drop->active = 0;
        ms_numDrops--;

        if (drop->isTrace)
            ms_numTraces--;
        else
            DetachMoving(drop);
    }

    static inline WaterDrop* PlaceNew(float x, float y, float size, float ttl, bool fades, int R = 0xFF, int G = 0xFF, int B = 0xFF, bool isTrace = false)
    {
        if (NoDrops())
            return NULL;

        const int32_t index = AcquireSlot(isTrace);

        if (index < 0)
            return NULL;

        WaterDrop& drop = ms_drops[index];
        ms_numDrops++;

        if (isTrace)
            ms_numTraces++;

        drop.x = x;
        drop.y = y;
        drop.size = size;
        drop.uv_index = ms_atlasUsed ? GetRandomInt(3) : 4; //sizeof(uv) - 2 || uv[last]
        // A size past the range of the ini (a test, a splash of a game) would
        // otherwise turn the window of the frame the drop shows inside out.
        drop.uvsize = std::clamp((SC(MaxSize) - size + 1.0f) / (SC(MaxSize) - SC(MinSize) + 1.0f), 0.0f, 1.0f);
        drop.fades = fades;
        drop.active = 1;
        drop.isTrace = isTrace;
        drop.r = R;
        drop.g = G;
        drop.b = B;
        drop.alpha = 0xFF;
        drop.alpha0 = 0xFF;
        drop.time = 0.0f;
        drop.ttl = ttl;

        // Whether this bead runs down the glass at all is one bead in two,
        // and which one it is is decided here and by nothing else about the
        // bead: beads of every size are then seen both hanging where they
        // are and running down, which is what rain on a pane of glass looks
        // like. How fast a bead that does run then goes follows how much
        // water there is in it, and it keeps that speed: a bead that is
        // running is not a bead that changes its mind on every frame. The
        // water a bead leaves behind it never runs.
        drop.slide = 0.0f;

        if (bGravity && !isTrace && GetRandomFloat(1.0f) >= HangingShare)
        {
            // pixels of a frame of sixty a second at the 480 lines the effect
            // comes from, scaled to the frame, see Scale
            const float slowest = gravity / gdivmin * Scale();
            const float fastest = gravity / gdivmax * Scale();
            const float weight = drop.size / (float)(std::max)(1, SC(MaxSize));

            drop.slide = slowest + (fastest - slowest) * (0.3f + 0.7f * weight);
        }

        // The water a drop leaves is the path it has run, over the time that
        // water stays on the glass: it is rolled for every drop of the rain
        // on its own, so no two drops leave a tail of the same length. The
        // life of a bead has always been divided by four for this at the 480
        // lines the effect comes from, and it is the same share of the life
        // of the bead on a screen of any size.
        drop.traceTtl = (drop.ttl / 4.0f * TraceLifeBase)
            * (TraceLifeMin + GetRandomFloat(TraceLifeMax - TraceLifeMin));
        drop.shapeX = drop.shapeY = 0.0f;

        return &drop;
    }

    // A drop the effect is done with is taken out of the list of the drops that
    // move before its place in the pool is handed out again, see Expire.
    static inline void DetachMoving(WaterDrop* drop)
    {
        for (auto& moving : ms_dropsMoving)
        {
            if (moving.drop == drop)
            {
                moving.drop = nullptr;

                if (ms_numDropsMoving > 0)
                    ms_numDropsMoving--;
            }
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

    // How many drops the pools can still take. Everything that adds a batch of
    // them goes through this, so a count that came from a game can never ask for
    // more than the room there is.
    static inline int32_t RoomForNewDrops()
    {
        // The water of the trails does not count against the rain: a drop of the
        // rain takes the place of a drop of a trail when nothing else is free, see
        // AcquireSlot.
        const int32_t heads = ms_numDrops - ms_numTraces;
        int32_t room = int32_t(ms_drops.capacity()) - heads;
        const int32_t moving = int32_t(ms_dropsMoving.capacity()) - ms_numDropsMoving;

        if (moving < room)
            room = moving;

        return room - 1;
    }

    static inline void FillScreenMoving(float amount, bool isBlood = false)
    {
        // A game can ask for drops before the effect has a screen: a hook of a
        // plugin fires while a level is still loading, and a device that was just
        // handed over reports no size until its first frame is presented. Nothing
        // can be placed on a screen that is not there yet, and the pools are not
        // the ones the ini asked for until the first frame either.
        if (!ms_initialised || ms_fbWidth <= 0 || ms_fbHeight <= 0)
            return;

        if (ms_StaticRain)
            amount = 1.0f;

        // The amount comes from a game or from a patch of one, and a negative one,
        // an enormous one or one that is not a number at all may not become a loop:
        // a counter that is counted down never reaches zero again once it is below
        // zero, and a loop of four billion drops per frame is what a hang looks
        // like. It is capped at the room there is as well, the pools decide how
        // many drops fit.
        if (!(amount > 0.0f))
            return;

        if (isBlood)
            SpawnDrops((int32_t)DropsOfAmount(amount), 0xFF, 0x00, 0x00);
        else
            SpawnDrops((int32_t)DropsOfAmount(amount), 0xFF, 0xFF, 0xFF);
    }

    static inline void FillScreenMovingColor(float amount, int R = 0xFF, int G = 0xFF, int B = 0xFF)
    {
        // see FillScreenMoving: no screen, no drops
        if (!ms_initialised || ms_fbWidth <= 0 || ms_fbHeight <= 0)
            return;

        if (ms_StaticRain)
            amount = 1.0f;

        // see FillScreenMoving: the amount is not trusted to be a sane count
        if (!(amount > 0.0f))
            return;

        SpawnDrops((int32_t)DropsOfAmount(amount), R, G, B);
    }

    static inline void FillScreen(int n)
    {
        // A frame of zero pixels is what a device that was just handed over reports
        // until its first frame is there, and a modulo or a division by it is a
        // crash of the whole process. Nothing can be placed on it either.
        if (!ms_initialised || ms_fbWidth <= 0 || ms_fbHeight <= 0)
            return;

        // the count comes from a game, it may not reach past the pool. The count of
        // the pool is one past its last drop, so the count is capped at the count of
        // the pool and the drops are then taken by index: a walk that compares the
        // address of a drop with the address of the drop at the count of the pool
        // reads past the end of it, which is what an out of range error is.
        if (n < 0)
            n = 0;

        if (n > int32_t(ms_drops.size()))
            n = int32_t(ms_drops.size());

        // the drops that are gone are taken out of the list of the drops that
        // move as well, see Clear and DetachMoving: the places are handed out
        // again right below
        Clear();

        for (int32_t i = 0; i < n; i++)
        {
            float x = (float)(rand() % ms_fbWidth);
            float y = (float)(rand() % ms_fbHeight);

            // the range of the sizes is a modulo as well, and a scale of zero
            // (a frame smaller than the drops) makes it one
            const int32_t sizeRange = SC(MaxSize) - SC(MinSize);
            float time = sizeRange > 0 ? (float)(rand() % sizeRange + SC(MinSize)) : (float)SC(MinSize);

            PlaceNew(x, y, time, 2000.0f, 1);
        }
    }

    static inline void Clear()
    {
        for (auto& drop : ms_drops)
            drop.active = false;

        // The places in the pool are free again right away, so nothing may still
        // point into them, see DetachMoving.
        for (auto& moving : ms_dropsMoving)
            moving = {};

        ms_numDrops = 0;
        ms_numDropsMoving = 0;
        ms_numTraces = 0;
        ms_nextFree = 0;
        ms_spawnRemainder = 0.0f;
        ms_bloodRemainder = 0.0f;
    }

    static inline void Reset()
    {
        Clear();
        ms_splashDuration = -1;
        ms_splashDistance = 0;
        ms_splashPoint = { 0 };

        ms_fbWidth = 0;
        ms_fbHeight = 0;
        ms_initialised = false;
        ms_generation = 0;
        ms_renderPrepared = false;
        ms_prepareRetry = 0;
        ms_vertices.clear();

        // the mask belongs to the device that just went away, the next Init
        // loads it again, which is what the original code did as well
        ReleaseMask();

        Xrd::Reset();
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

    // -----------------------------------------------------------------------
    // Rendering
    //
    // The renderer in source/xrd does everything that depends on the graphics
    // API: it copies the frame the drops refract into a texture, sets the
    // states and draws the batch. What is left here is building the quads,
    // which is the same on every API.
    // -----------------------------------------------------------------------
    static inline Xrd::Texture* ms_maskTex = nullptr;
    static inline std::vector<Xrd::Vertex> ms_vertices;
    static inline int32_t ms_fbWidth = 0;
    static inline int32_t ms_fbHeight = 0;

    // The window the frame of the game is presented in. A frame the game draws into
    // a buffer of its own is stretched to that window, so a drop drawn round into
    // the buffer is an oval on screen: it is drawn with the inverse of that stretch
    // in x, see ComputeXScale. Zero means the game draws into the window itself and
    // nothing is stretched, which is what the games this effect comes from do.
    static inline int32_t ms_screenWidth = 0;
    static inline int32_t ms_screenHeight = 0;
    static inline float ms_xScale = 1.0f;
    static inline int32_t ms_numBatchedDrops = 0;
    static inline float ms_UVXOffset = 0.0f;
    static inline float ms_UVXScale = 1.0f;
    static inline float ms_UVYOffset = 0.0f;
    static inline float ms_UVYScale = 1.0f;
    static inline bool ms_initialised = false;
    static inline bool ms_renderPrepared = false;
    static inline ULONGLONG ms_prepareRetry = 0;
    static inline uint32_t ms_generation = 0;
    static inline bool ms_atlasUsed = true;
    static inline bool ms_iniRead = false;

    // How far the atlas coordinate of a drop that gathers light is moved down,
    // see AddToRenderList and VSMain in source/shaders/d3d10/drops.hlsl. Every
    // shader and backend that takes the mark back off has to agree on
    // it, which is what Xrd::AtlasLightMarker is for: the vertex shader of
    // Direct3D 10 and above moves it back up, the shader of Direct3D 9 reads the
    // sign of it, and the backend of Direct3D 8 takes it off on the way in.
    static constexpr float AtlasLightMarker = Xrd::AtlasLightMarker;

    // Switching the falling drops between rain and snow also switches the mask
    // they are drawn with, the one loaded from the resources.
    static inline void SetSnow(bool enabled)
    {
        if (bEnableSnow == enabled)
            return;

        bEnableSnow = enabled;
        ReleaseMask();
        ms_initialised = false;
    }

    // The shape of a drop has to be round as it is seen, not as it is drawn: the
    // frame of the game is stretched from its buffer to the window it is presented
    // in, and a circle of the buffer is an oval of the window unless the quad is
    // drawn narrower by the same ratio, see ms_xScale.
    static inline void ComputeXScale()
    {
        ms_xScale = 1.0f;

        if (ms_screenWidth <= 0 || ms_screenHeight <= 0 || ms_fbWidth <= 0 || ms_fbHeight <= 0)
            return;

        const float frame = (float)ms_fbWidth / (float)ms_fbHeight;
        const float screen = (float)ms_screenWidth / (float)ms_screenHeight;

        if (screen > 0.0f)
            ms_xScale = frame / screen;
    }

    static inline void SetXUVScale(float offset, float scale)
    {
        ms_UVXOffset = offset;
        ms_UVXScale = scale;
    }

    static inline void SetYUVScale(float offset, float scale)
    {
        ms_UVYOffset = offset;
        ms_UVYScale = scale;
    }

    static inline void ReleaseMask()
    {
        if (ms_maskTex)
        {
            Xrd::DestroyTexture(ms_maskTex);
            ms_maskTex = nullptr;
        }
    }

    // The masks ship as PNG resources, the very same files the Direct3D 8 and 9
    // builds always used, so the fallback shape is only needed when a resource
    // cannot be read.
    static inline bool LoadMask(int resourceId, std::vector<uint8_t>& pixels, int32_t& width, int32_t& height)
    {
        HMODULE hModule = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&LoadMask, &hModule);

        HRSRC hResource = FindResource(hModule, MAKEINTRESOURCE(resourceId), RT_RCDATA);
        if (!hResource)
            return false;

        HGLOBAL hLoaded = LoadResource(hModule, hResource);
        if (!hLoaded)
            return false;

        const void* pData = LockResource(hLoaded);
        const DWORD uSize = SizeofResource(hModule, hResource);
        if (!pData || !uSize)
            return false;

        IStream* pStream = nullptr;
        if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &pStream)))
            return false;

        ULONG uWritten = 0;
        pStream->Write(pData, uSize, &uWritten);

        LARGE_INTEGER start{};
        pStream->Seek(start, STREAM_SEEK_SET, nullptr);

        Gdiplus::GdiplusStartupInput input;
        ULONG_PTR token = 0;
        if (Gdiplus::GdiplusStartup(&token, &input, nullptr) != Gdiplus::Ok)
        {
            pStream->Release();
            return false;
        }

        bool bResult = false;

        {
            Gdiplus::Bitmap bitmap(pStream, FALSE);

            if (bitmap.GetLastStatus() == Gdiplus::Ok)
            {
                width = (int32_t)bitmap.GetWidth();
                height = (int32_t)bitmap.GetHeight();

                Gdiplus::Rect rect(0, 0, width, height);
                Gdiplus::BitmapData data{};

                if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &data) == Gdiplus::Ok)
                {
                    pixels.resize((size_t)width * height * 4);

                    const uint8_t* pSource = (const uint8_t*)data.Scan0;
                    for (int32_t y = 0; y < height; y++)
                    {
                        const uint8_t* pRow = pSource + (size_t)y * data.Stride;
                        uint8_t* pDestination = pixels.data() + (size_t)y * width * 4;

                        for (int32_t x = 0; x < width; x++)
                        {
                            // Windows hands out BGRA, the renderers expect RGBA
                            pDestination[x * 4 + 0] = pRow[x * 4 + 2];
                            pDestination[x * 4 + 1] = pRow[x * 4 + 1];
                            pDestination[x * 4 + 2] = pRow[x * 4 + 0];
                            pDestination[x * 4 + 3] = pRow[x * 4 + 3];
                        }
                    }

                    bitmap.UnlockBits(&data);
                    bResult = true;
                }
            }
        }

        Gdiplus::GdiplusShutdown(token);
        pStream->Release();

        return bResult;
    }

    // The moving drops hold pointers into the drops, so the two pools can only be
    // resized together, and the pointers can not survive it: growing a vector moves
    // its storage, and every pointer the moving drops hold into the old storage is
    // then a pointer into freed memory, which MoveDrop reads and writes through.
    // That is what it looks like when the heap hands the freed block out again for
    // the moving drops themselves: their entries turn into floats, and the next
    // dereference of an entry crashes. Nothing is worth keeping across a resize
    // either, the effect is built again from the ini right after this.
    static inline void ResizePools()
    {
        // Every drop of the effect, the drops the rain leaves behind it included,
        // is one quad of the vertex buffer of the backend, so the pool can never
        // be larger than the buffer: a pool of drops that does not fit in it is a
        // buffer that is written past its end, see MaxQuads.
        if (MaxDrops > MaxQuads)
            MaxDrops = MaxQuads;

        if (ms_drops.size() == (size_t)MaxDrops && ms_dropsMoving.size() == (size_t)MaxDropsMoving)
            return;

        Clear();

        ms_drops.resize(MaxDrops);
        ms_dropsMoving.resize(MaxDropsMoving);
    }

    static inline void Init()
    {
        if (!Xrd::IsActive())
            return;

        // the ini is read once, changes to it are picked up by the watcher that
        // ReadIniSettings installs
        if (!ms_iniRead)
        {
            ReadIniSettings(bInvertedRadial);
            ms_iniRead = true;
        }

        ResizePools();
        // a drop of the effect and the water it can leave behind it, see AddToRenderList
        ms_vertices.reserve((size_t)MaxQuads * 4);

        Xrd::Size size = Xrd::GetSize();
        ms_fbWidth = size.width;
        ms_fbHeight = size.height;
        ms_scaling = ms_fbHeight / 480.0f;

        if (!ms_maskTex)
        {
            std::vector<uint8_t> pixels;
            int32_t width = 0;
            int32_t height = 0;

            // Whether the atlas is what is drawn has to follow the mask that is
            // loaded here and not what was loaded before: a mask that could not
            // be read once would otherwise leave every drop with the round
            // fallback shape for good, even after the next one was read fine.
            ms_atlasUsed = false;

            if (LoadMask(bEnableSnow ? IDR_SNOWDROPMASK : IDR_DROPMASK, pixels, width, height))
            {
                ms_maskTex = Xrd::CreateTexture(width, height, pixels.data());
                ms_atlasUsed = ms_maskTex != nullptr;
            }

            if (!ms_maskTex)
            {
                static constexpr auto MaskSize = 128;
                pixels.resize(MaskSize * MaskSize * 4);

                for (int32_t y = 0; y < MaskSize; y++)
                {
                    const float yf = ((y + 0.5f) / MaskSize - 0.5f) * 2.0f;

                    for (int32_t x = 0; x < MaskSize; x++)
                    {
                        const float xf = ((x + 0.5f) / MaskSize - 0.5f) * 2.0f;
                        memset(&pixels[((size_t)y * MaskSize + x) * 4], xf * xf + yf * yf < 1.0f ? 0xFF : 0x00, 4);
                    }
                }

                ms_maskTex = Xrd::CreateTexture(MaskSize, MaskSize, pixels.data());
            }
        }

        ms_generation = Xrd::GetBackendGeneration();

        // The size is what everything of the effect is scaled, clipped and randomly
        // placed with, so it has to be there: a device that was just handed over
        // reports a size of zero until its first frame is presented, and with a
        // size of zero a modulo or a division by it is a crash. The next frame
        // tries again.
        ms_initialised = ms_fbWidth > 0 && ms_fbHeight > 0;
        PrepareRenderer();
    }

    static inline void PrepareRenderer()
    {
        if (!ms_initialised || !ms_maskTex || ms_renderPrepared)
            return;
        const auto now = GetTickCount64();
        if (now < ms_prepareRetry)
            return;
        Xrd::SetMaskTexture(ms_maskTex);
        ms_renderPrepared = Xrd::Prepare(MaxQuads * 4);
        // A temporarily unavailable target must not trigger expensive retries
        // every frame. Successful preparation has no recurring allocation cost.
        ms_prepareRetry = ms_renderPrepared ? 0 : now + 1000;
    }

    // The renderer of the game can be switched while it runs, and the backend is
    // built again when its device is replaced under it. Everything the effect
    // created belongs to the device of that backend, and the one that is there now
    // either does not know the handles (which is what a drop of solid black looks
    // like) or traps on them (which is what a crash in the driver looks like).
    // Everything is therefore dropped and built again with the device that is
    // current, which is what this is called for before the drops are used.
    static inline void EnsureDevice()
    {
        if (ms_initialised && ms_generation != Xrd::GetBackendGeneration())
        {
            ms_renderPrepared = false;
            ms_prepareRetry = 0;
            ms_maskTex = nullptr;
            ms_initialised = false;
            ms_fbWidth = 0;
            ms_fbHeight = 0;
        }

        if (!ms_initialised)
            Init();
        else
            PrepareRenderer();
    }

    static inline void Shutdown()
    {
        Reset();
    }

    // One quad of the effect: a shape of the atlas of drop shapes, the copy of
    // the frame around the quad that it samples, and the colour it is drawn in.
    // The smaller the quad is for the same shape and the same piece of the frame,
    // the more of the frame it shows, which is what makes a drop a lens.
    static inline void AddDropQuad(float x, float y, float size, float uvsize, uint32_t color, int uv_index, bool lens,
        float velocityX = 0.0f, float velocityY = 0.0f)
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

        float u1_1, u1_2;
        float v1_1, v1_2;
        float tmp;

        // pixels of the 480 lines the effect comes from, scaled to the frame: the
        // window of a drop is the same part of the picture on a screen of any size
        tmp = (uvsize * (300.0f - 40.0f) + 40.0f) * Scale();
        u1_1 = x + ms_xOff - tmp * ms_xScale;
        v1_1 = y + ms_yOff - tmp;
        u1_2 = x + ms_xOff + tmp * ms_xScale;
        v1_2 = y + ms_yOff + tmp;
        u1_1 = (u1_1 <= 0.0f ? 0.0f : u1_1) / ms_fbWidth;
        v1_1 = (v1_1 <= 0.0f ? 0.0f : v1_1) / ms_fbHeight;
        u1_2 = (u1_2 >= ms_fbWidth ? ms_fbWidth : u1_2) / ms_fbWidth;
        v1_2 = (v1_2 >= ms_fbHeight ? ms_fbHeight : v1_2) / ms_fbHeight;

        // A drop of clear water gathers the light of the frame around it, see
        // source/shaders/d3d10/drops.hlsl. The atlas coordinate is what says so:
        // every drop samples its tile at a coordinate between zero and one, so a
        // coordinate below zero is one the shader can read as the mark that the
        // drop is a lens, and it moves it back up before it samples. Every
        // renderer that draws the drops without that shader gets the atlas
        // coordinate untouched and is left exactly as it was. The water a bead
        // left behind it does not gather any light: a film of water on the glass
        // is not a lens, see IsLens.
        const float scale = size * 0.5f;
        const float speed = sqrtf(velocityX * velocityX + velocityY * velocityY);
        const float stretch = 1.0f + (std::min)(0.45f, speed * 0.012f / (std::max)(size, 1.0f));
        const float ax = speed > 0.001f ? velocityX / speed : 1.0f;
        const float ay = speed > 0.001f ? velocityY / speed : 0.0f;

        for (int i = 0; i < 4; i++)
        {
            Xrd::Vertex vertex{};
            const float along = (xy[i * 2] * ax + xy[i * 2 + 1] * ay) * stretch;
            const float across = (-xy[i * 2] * ay + xy[i * 2 + 1] * ax) / stretch;
            vertex.x = x + (along * ax - across * ay) * scale * ms_xScale + ms_xOff;
            vertex.y = y + (along * ay + across * ax) * scale + ms_yOff;
            vertex.z = 0.0f;
            vertex.color = color;
            vertex.u0 = uv[uv_index][i * 2] - (lens ? AtlasLightMarker : 0.0f);
            vertex.v0 = uv[uv_index][i * 2 + 1];
            vertex.u1 = i >= 2 ? u1_2 : u1_1;
            vertex.v1 = i % 3 == 0 ? v1_2 : v1_1;

            ms_vertices.push_back(vertex);
        }
    }

    // One drop of the rain, drawn as one quad of the atlas of the drop shapes: see
    // AddDropQuad. The drops a drop left behind it are drops of the rain like any
    // other, so this is all there is to drawing one, and the water of the effect can
    // never take more of the vertex buffer of a backend than the pool is large.
    static inline void AddToRenderList(WaterDrop* drop)
    {
        AddDropQuad(drop->x, drop->y, drop->size, drop->uvsize,
            Xrd::ColorARGB(drop->alpha, drop->r, drop->g, drop->b), drop->uv_index, IsLens(drop),
            drop->shapeX, drop->shapeY);

        ms_numBatchedDrops++;
    }

    static inline void Render()
    {
        if (!ms_enabled || ms_numDrops <= 0)
            return;

        EnsureDevice();

        if (!ms_initialised || !Xrd::IsActive())
            return;

        const Xrd::Size size = Xrd::GetSize();

        if (size.width != ms_fbWidth || size.height != ms_fbHeight || size.width <= 0 || size.height <= 0)
        {
            // the window changed size, everything is measured again on the next
            // frame, until then the drops would be in the wrong place
            Reset();
            return;
        }

        ComputeXScale();

        ms_vertices.clear();
        ms_numBatchedDrops = 0;

        for (auto& drop : ms_drops)
            if (drop.active)
                AddToRenderList(&drop);

        if (ms_numBatchedDrops <= 0)
            return;

        Xrd::SetMaskTexture(ms_maskTex);
        Xrd::SetProjection(Xrd::PROJECTION_SCREEN);
        Xrd::SetSceneUVScale(ms_UVXOffset, ms_UVXScale, ms_UVYOffset, ms_UVYScale);
        Xrd::SetSceneSampling(true);
        Xrd::SetSceneComplement(bEnableSnow);
        Xrd::Render(ms_vertices.data(), (int32_t)ms_vertices.size(), Xrd::PRIMITIVE_TRIANGLES);
    }
};

void WaterDrop::Fade()
{
    auto delta = WaterDrops::GetTimeStepInMilliseconds() * 100.0f;
    this->time += delta;
    if (this->time >= this->ttl)
    {
        // Spawning runs before the drops are moved on the next frame, so the place
        // this drop sits in can already be handed to another one by then, see
        // DetachMoving, which Expire takes care of.
        WaterDrops::Expire(this);
    }
    else if (this->fades)
        this->alpha = (uint8_t)(this->alpha0 * (1.0f - std::clamp(this->time / this->ttl, 0.0f, 1.0f)));
}

bool IsModuleUAL(HMODULE mod)
{
    if (GetProcAddress(mod, "IsUltimateASILoader") != NULL || (GetProcAddress(mod, "DirectInput8Create") != NULL && GetProcAddress(mod, "DirectSoundCreate8") != NULL && GetProcAddress(mod, "InternetOpenA") != NULL))
        return true;
    return false;
}

bool IsUALPresent()
{
    for (const auto& entry : std::stacktrace::current())
    {
        HMODULE hModule = NULL;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)entry.native_handle(), &hModule))
        {
            if (IsModuleUAL(hModule))
                return true;
        }
    }
    return false;
}

template<typename T>
std::array<uint8_t, sizeof(T)> to_bytes(const T& object)
{
    std::array<uint8_t, sizeof(T)> bytes;
    const uint8_t* begin = reinterpret_cast<const uint8_t*>(std::addressof(object));
    const uint8_t* end = begin + sizeof(T);
    std::copy(begin, end, std::begin(bytes));
    return bytes;
}

template<typename T, size_t N>
auto to_bytes(const T(&arr)[N]) -> std::array<uint8_t, N - 1>
{
    std::array<uint8_t, N - 1> bytes;
    const uint8_t* begin = reinterpret_cast<const uint8_t*>(arr);
    std::copy(begin, begin + N - 1, std::begin(bytes));
    return bytes;
}

template<typename T>
T& from_bytes(const std::array<uint8_t, sizeof(T)>& bytes, T& object)
{
    static_assert(std::is_trivially_copyable<T>::value, "not a TriviallyCopyable type");
    uint8_t* begin_object = reinterpret_cast<uint8_t*>(std::addressof(object));
    std::copy(std::begin(bytes), std::end(bytes), begin_object);
    return object;
}

template<class T, class T1>
T from_bytes(const T1& bytes)
{
    static_assert(std::is_trivially_copyable<T>::value, "not a TriviallyCopyable type");
    T object;
    uint8_t* begin_object = reinterpret_cast<uint8_t*>(std::addressof(object));
    std::copy(std::begin(bytes), std::end(bytes) - (sizeof(T1) - sizeof(T)), begin_object);
    return object;
}

template <size_t n>
std::string pattern_str(const std::array<uint8_t, n> bytes)
{
    std::string result;
    result.reserve(n * 3);
    for (size_t i = 0; i < n; i++)
    {
        result += std::format("{:02X} ", bytes[i]);
    }
    return result;
}

template <typename T>
std::string pattern_str(T t)
{
    if constexpr (std::is_same<T, char>::value)
        return std::format("{} ", t);
    else
        return std::format("{:02X} ", t);
}

template <typename T, typename... Rest>
std::string pattern_str(T t, Rest... rest)
{
    std::string prefix;
    if constexpr (std::is_same<T, char>::value)
        prefix = std::format("{} ", t);
    else
        prefix = std::format("{:02X} ", t);
    return prefix + pattern_str(rest...);
}

template <size_t count = 1, typename... Args>
hook::pattern find_pattern(Args... args)
{
    hook::pattern pattern;
    ((pattern = hook::pattern(args), !pattern.count_hint(count).empty()) || ...);
    return pattern;
}

template <size_t count = 1, typename... Args>
hook::pattern find_module_pattern(HMODULE hModule, Args... args)
{
    hook::pattern pattern;
    ((pattern = hook::module_pattern(hModule, args), !pattern.count_hint(count).empty()) || ...);
    return pattern;
}

template <size_t count = 1, typename... Args>
hook::pattern find_range_pattern(uintptr_t range_start, size_t range_size, Args... args)
{
    hook::pattern pattern;
    ((pattern = hook::range_pattern(range_start, range_size, args), !pattern.count_hint(count).empty()) || ...);
    return pattern;
}
