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

    // The velocity the air has given this drop, in the space of the lens: x to
    // the right, y up, z along the view. The air that comes at a lens that is
    // driven forward pushes the drop to negative z, which the dome of the lens
    // turns into a run outwards from the middle of the picture, see MoveDrop.
    // This is the m_Velocity of the lens rain of Forza Horizon 4.
    float vel[3] = { 0.0f, 0.0f, 0.0f };
    // how the shape of the drop is turned on the glass, in radians
    float rotation = 0.0f;
    // Whether the drop leaves water behind it when the turning camera drags it
    // sideways over the glass. Not every drop does, see NewTrace.
    bool trail = false;
    // How fast this drop runs down the glass, in pixels of the frame a second,
    // and the speed it runs at in the end: zero for the drops gravity does not
    // move, see GravityShare.
    float fall = 0.0f;
    float fallTop = 0.0f;

    // the velocity the drop travels over the screen with, in pixels a second
    float shapeX = 0.0f, shapeY = 0.0f;
    // how long a piece of a trail of water is along the path, see NewTrace
    float trailLength = 0.0f;
    // How far the drop has run while it left water, and what its wander from side
    // to side and the width of its trail are made of, see WaterDrops::Meander.
    float meander = 0.0f;
    float meanderSeed = 0.0f;

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

    // A drop of blood, see WaterDrops::BloodHoldSeconds: it sticks where it
    // landed and fades away, it does not run like water.
    bool blood = false;

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
    static inline auto MaxSize = 19;
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

    // -----------------------------------------------------------------------
    // the air over the lens
    //
    // This is the lens rain of Forza Horizon 4, taken from a GPU capture of the
    // game: its simulation shader, its constants and the state of its drops over
    // ten frames. Every drop is a particle on the dome of the lens:
    //
    //   - it lands at rest, anywhere, and lives one second, drawn with the
    //     remaining part of its life as its opacity,
    //   - the air that comes at the lens pushes it, the harder the bigger the
    //     drop is and the more squarely the lens faces the air at its place,
    //     and a little friction, of the same size scale, holds it back,
    //   - the dome of the lens turns the air that comes at it head on into a run
    //     straight outwards from the middle of the picture, and air that comes
    //     from the side or from above into a run sideways or down,
    //   - it is gone when it leaves the picture or when its life is over.
    //
    // So a drop sits where it landed while the camera stands, and a camera that
    // drives sees every drop start from rest and run outwards faster and faster,
    // the big ones well ahead of the small ones.
    //
    // The air is what the camera moves through, see CalculateMovement, and a
    // camera that is turned is swung through the air as well: Forza has no such
    // camera, and this is where the turns of the mouse come in, see LookArm.
    // -----------------------------------------------------------------------
    // the air over the lens, in units of the game a second, in the space of the
    // lens (x right, y up, z along the view): what the camera moves through
    static inline float ms_air[3] = { 0.0f, 0.0f, 0.0f };
    // the part of it that comes from the turning of the camera, see LookArm
    static inline float ms_lookAir[2] = { 0.0f, 0.0f };
    // how fast the camera goes along its view, in units of the game a second
    static inline float ms_forwardSpeed = 0.0f;
    // where the camera looked and which way was right on the frame before, so
    // that a turn of it can be measured, see CalculateMovement
    static inline RwV3d ms_lastFwd;
    static inline RwV3d ms_lastRight;
    static inline bool ms_haveLastFwd = false;

    // Forza's g_SizeToMaginuteScalar: the push of the air and the friction of
    // the glass both scale with the size of a drop times this. A flake of snow
    // is pushed and held a third as hard as a drop of rain of its size.
    static constexpr float SizeToMagnitude = 3.12f;
    static constexpr float SnowSizeToMagnitude = 1.0f;
    // Forza's flakes of snow are three to five times the size of its drops of
    // rain, of every size in between, and there are far fewer of them on the
    // lens: this is the share of the rain of an amount that lands as snow.
    static constexpr float SnowMinScale = 3.6f;
    static constexpr float SnowMaxScale = 2.7f;
    static constexpr float SnowShare = 0.15f;
    // The lens turns the velocity of a drop into its run over the picture, in
    // halves of the picture a second: the velocity along the view outwards from
    // the middle at this share of it, and the velocity across the view sideways
    // and up or down at the second. Read off the lens textures of Forza.
    static constexpr float LensRadialGain = 0.5f;
    static constexpr float LensLateralGain = 0.9f;
    // How far the dome of the lens is tilted at the edge of the picture: its
    // normal there leans over to this sine, and the push of the air follows how
    // squarely the lens faces it, but never drops below MinAlignment of it.
    static constexpr float LensTilt = 0.87f;
    static constexpr float MinAlignment = 0.55f;
    // How long a drop stays on the glass, in seconds: Forza's RainDropLife of
    // the chase camera (WaterOnLensExterior in GlobalCarAttributes.xml), and of
    // its snow. A drop fades from the moment it lands over all of it. Forza
    // steps its drops in time measured in lives, so the push of the air and the
    // friction of the glass act over that time as well, see MoveDrop.
    static constexpr float LifeMinSeconds = 3.0f;
    static constexpr float LifeMaxSeconds = 3.0f;
    static constexpr float SnowLifeSeconds = 2.5f;
    // Forza lands RainDropParticlesPerSecond (700) times the intensity of the
    // rain a second, whatever the speed of the car or where its camera looks,
    // pushed out from the middle of the picture, of which about a third land on
    // it: this is that third, the drops a second the heaviest rain lands on the
    // picture. The intensity goes through Forza's curve first, see RainCurve.
    static constexpr float WeatherDropsPerSecond = 250.0f;
    // Blood is thick: it sticks where it lands, the air and the turning camera
    // do not move it and it leaves no trail. It stays whole for BloodHoldSeconds
    // to that and BloodHoldSpread more, and then fades over BloodFadeSeconds.
    static constexpr float BloodHoldSeconds = 2.0f;
    static constexpr float BloodHoldSpread = 1.5f;
    static constexpr float BloodFadeSeconds = 1.5f;
    // A drop that lands is this much bigger for the first moment, and shrinks
    // back to its size over SplashSeconds: the splash of it.
    static constexpr float SplashScale = 1.4f;
    static constexpr float SplashSeconds = 0.0157f;
    // One drop in eight is one of the big ones, see SpawnDrops.
    static constexpr float BigDropShare = 0.13f;
    // A camera that turns is swung through the air: the lens sits on an arm,
    // and a turn moves it sideways by the arm times the angle. This is that arm,
    // in units of the game, and it is long, because a drop on a lens has to be
    // shoved hard to move at all: see MoveDrop.
    // (Forza's drops move in time measured in their lives of three seconds,
    // which takes nine times the push to move them as far in a second of real
    // time: the arm is that much longer than it was when the drops moved in
    // seconds.)
    static constexpr float LookArm = 108.0f;
    // The speed along its view, in units of the game a second, from which a
    // camera that turns swings its lens through the air less, and from which
    // not at all: a camera that drives is turned by its car, see
    // CalculateMovement. A camera swung round a player who stands or walks goes
    // sideways and not along its view, so it keeps all of its swing.
    static constexpr float LookFadeStart = 3.0f;
    static constexpr float LookFadeEnd = 10.0f;
    // A drop that the turning camera shoved sideways comes to rest again over
    // this long, in seconds, once the camera stops: the glass holds it.
    static constexpr float LookSettle = 0.12f;
    // EnableGravity of the ini. Forza's drops never run down the lens; with
    // gravity on, a share of the drops does, picked at random from all of them,
    // and slowly: they pick up speed over GravityRise seconds to a speed of
    // their own between the two below, in pixels a second at 480 lines.
    static constexpr float GravityShare = 0.35f;
    static constexpr float GravitySlowest = 8.0f;
    static constexpr float GravityFastest = 22.0f;
    static constexpr float GravityRise = 0.4f;

    // The normal of the dome of the lens at a place of the picture, -1 to 1
    // each way with y up, and how squarely it faces the air.
    static inline float LensAlignment(float px, float py, const float* air)
    {
        const float length = sqrtf(air[0] * air[0] + air[1] * air[1] + air[2] * air[2]);

        if (length <= 0.0f)
            return MinAlignment;

        const float nx = LensTilt * std::clamp(px, -1.0f, 1.0f);
        const float ny = LensTilt * std::clamp(py, -1.0f, 1.0f);
        const float nz = sqrtf((std::max)(0.0f, 1.0f - nx * nx - ny * ny));
        const float facing = fabsf(nx * air[0] + ny * air[1] + nz * air[2]) / length;
        return (std::max)(facing, MinAlignment);
    }

    static inline int32_t ms_splashDuration;
    static inline RwV3d ms_splashPoint;
    static inline float ms_splashDistance;
    static inline float ms_splashRemovalDistance;

    static inline bool sprayWater = false;
    static inline bool sprayBlood = false;
    static inline bool ms_StaticRain = false;
    // Which way is forward on the frame: the games hand the matrix of their
    // camera over, and in some of them forward is in the up vector of it and up
    // in its at vector. That is known for every game and its plugin says so, see
    // ReadIniSettings; it is no setting of the ini.
    static inline bool bRadial = false;
    static inline bool bForwardIsUp = false;
    // EnableGravity of the ini. The drops on the lens of Forza do not run down it,
    // only the air moves them (see the air over the lens), so nothing reads this
    // any more; it is kept for the games and menus that set it.
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
    static constexpr float TraceLifeMin = 0.4f;
    static constexpr float TraceLifeMax = 1.4f;
    // Every backend holds a vertex buffer of a fixed size, which is the 64000
    // vertices of 16000 quads below, and one drop of the rain is one quad of it.
    // The drops a bead leaves behind it are drops of the rain, so the pool is what
    // bounds the water of the effect: see MaxDrops, which ResizePools keeps inside
    // this.
    static constexpr int32_t MaxQuads = 16000;
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

    // A drop of clear water gathers the light of the frame around it: next to
    // a tail light or a lamp it glows in its colour, the way Forza's drops do at
    // night. The water a drop leaves is a film and gathers none.
    static inline bool IsLens(const WaterDrop* drop)
    {
        return !drop->isTrace && drop->r == drop->g && drop->g == drop->b && GatheredLight();
    }

    // A game that is not raining spawns no drops at all, which is what the effect is for and
    // also what makes it impossible to look at while the weather of a game is being worked on.
    // The ini can hold the rain on, whatever the game reports, which is what ForceRain is for.
    static inline bool bForceRain = false;
    static inline float fSpeedAdjuster = 1.0f;

    // How blurred a drop is, from none (a drop of clear water, which shows the
    // frame around it in every detail, the way the effect has always drawn it)
    // to one (a drop that is out of focus, a soft disc that shows the light of
    // the whole of its surroundings, which is what the drops on the camera of
    // a racing game look like). DropBlur in the ini. The renderers that draw
    // the drops with a shader of Direct3D 9 and above do this, the rest draw
    // the drops clear.
    static inline float fDropBlur = 1.0f;

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

    // forwardIsUp: the game hands forward over in the up vector of its camera,
    // see bRadial
    static inline void ReadIniSettings(bool forwardIsUp = false)
    {
        bForwardIsUp = forwardIsUp;
        bRadial = forwardIsUp;

        CIniReader iniReader("");
        MinSize = iniReader.ReadInteger("MAIN", "MinSize", 4);
        MaxSize = iniReader.ReadInteger("MAIN", "MaxSize", 19);
        MaxDrops = iniReader.ReadInteger("MAIN", "MaxDrops", 3000);
        MaxDropsMoving = iniReader.ReadInteger("MAIN", "MaxMovingDrops", 6000);
        bGravity = iniReader.ReadInteger("MAIN", "EnableGravity", 1) != 0;
        bRefractions = iniReader.ReadInteger("MAIN", "Refractions", 1) != 0;
        fSpeedAdjuster = iniReader.ReadFloat("MAIN", "SpeedAdjuster", 1.0f);
        fMoveStep = iniReader.ReadFloat("MAIN", "MoveStep", 0.1f);
        bBloodDrops = iniReader.ReadInteger("MAIN", "BloodDrops", 1) != 0;
        bEnableSnow = iniReader.ReadInteger("BONUS", "EnableSnow", 0) != 0;
        bForceRain = iniReader.ReadInteger("MAIN", "ForceRain", 0) != 0;
        fDropBlur = std::clamp(iniReader.ReadFloat("MAIN", "DropBlur", 1.0f), 0.0f, 1.0f);

        static std::once_flag flag;
        std::call_once(flag, [&]()
        {
            if (std::filesystem::exists(iniReader.GetIniPath()))
            {
                static filewatch::FileWatch<std::string> watch(iniReader.GetIniPath().string(), [&](const std::string& path, const filewatch::Event change_type)
                {
                    if (change_type == filewatch::Event::modified)
                    {
                        ReadIniSettings(bForwardIsUp);
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

        // Which way is forward and which way is up on the frame. The games hand
        // the matrix of their camera over, and in some of them the two are the
        // other way round, which their plugins say, see bRadial.
        RwV3d& fwd = bRadial ? up : at;
        RwV3d& upAxis = bRadial ? at : up;

        ms_vec.x = -RwV3dDotProduct(&right, &ms_posDelta);
        ms_vec.y = RwV3dDotProduct(&upAxis, &ms_posDelta);
        ms_vec.z = RwV3dDotProduct(&fwd, &ms_posDelta);
        // The drift the camera gives the drops is in pixels of the frame, and it
        // has always been measured for the 480 lines the effect comes from: on a
        // screen of more lines it is scaled with everything else, or the drops of
        // a large screen would crawl.
        RwV3dScale(&ms_vec, &ms_vec, 10.0f * Scale());
        ms_vecLen = sqrt(ms_vec.y * ms_vec.y + ms_vec.x * ms_vec.x);

        // The air over the lens, see ms_air. A stall of the game is one long frame,
        // and the travel of that frame spread over it is a breeze: the frame is
        // capped the way the rain of a frame is, see FrameSeconds.
        const float dt = FrameSeconds();

        if (dt > 0.0f)
        {
            // The games hand the right vector of their camera over the way
            // RenderWare has it, pointing to the left of the screen, which is
            // what the effect has always taken it as; the right of the screen
            // is the other way.
            RwV3d screenRight = { -right.x, -right.y, -right.z };

            // the travel of the camera in units of the game a second, in the
            // space of the lens, and the air comes the other way
            const float side = RwV3dDotProduct(&screenRight, &ms_posDelta) / dt;
            const float lift = RwV3dDotProduct(&upAxis, &ms_posDelta) / dt;
            const float forward = RwV3dDotProduct(&fwd, &ms_posDelta) / dt;

            // Forza scales the air by how much the camera faces the way it goes:
            // all of it looking ahead, half looking out to the side and none
            // looking back, so a camera that looks back or backs up has no air
            // come at its lens.
            const float travel = sqrtf(side * side + lift * lift + forward * forward);
            const float facing = travel > 0.001f ? (forward / travel + 1.0f) * 0.5f : 1.0f;

            float airX = -side * facing;
            float airY = -lift * facing;
            float airZ = -forward * facing;

            // A camera that turns is swung through the air, see LookArm, and the
            // drops go the way the view turns, which is what a camera swung round
            // its player does with them. The camera of a game that is swung round
            // its player travels as well and that travel is what is measured
            // above, so of the two only the stronger one counts.
            float lookX = 0.0f;
            float lookY = 0.0f;

            if (ms_haveLastFwd)
            {
                // how far the view turned to the right and up, in radians of a
                // small turn: the way the forward axis moved along the others
                const RwV3d turned = { fwd.x - ms_lastFwd.x, fwd.y - ms_lastFwd.y, fwd.z - ms_lastFwd.z };
                const float yaw = turned.x * screenRight.x + turned.y * screenRight.y + turned.z * screenRight.z;
                const float pitch = turned.x * upAxis.x + turned.y * upAxis.y + turned.z * upAxis.z;

                // Driving, the camera of a game follows its car round the corners,
                // and Forza's drops feel nothing of that but the air in the view of
                // the camera, which is measured above: a turn swings the lens
                // through the air only for a camera that does not drive, and the
                // swing fades out as it picks up speed along its view, see
                // LookFadeStart.
                const float driving = std::clamp((fabsf(forward) - LookFadeStart) / (LookFadeEnd - LookFadeStart), 0.0f, 1.0f);
                lookX = yaw * LookArm / dt * (1.0f - driving);
                lookY = -pitch * LookArm / dt * (1.0f - driving);

                if (fabsf(lookX) > fabsf(airX))
                    airX = lookX;
                else
                    lookX = 0.0f;

                if (fabsf(lookY) > fabsf(airY))
                    airY = lookY;
                else
                    lookY = 0.0f;
            }

            // Air that is not a number, out of a camera the game has not set up
            // yet, would be taken into every drop and never leave it again.
            const auto finite = [](float v) { return std::isfinite(v) ? v : 0.0f; };
            ms_air[0] = finite(airX);
            ms_air[1] = finite(airY);
            ms_air[2] = finite(airZ);
            ms_lookAir[0] = finite(lookX);
            ms_lookAir[1] = finite(lookY);
            ms_forwardSpeed = finite(forward);
        }

        ms_lastFwd = fwd;
        ms_lastRight = right;
        ms_haveLastFwd = true;

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
    static inline float ms_snowRemainder = 0.0f;

    // the number of drops an amount stands for in one frame of sixty a second
    static inline float DropsOfAmount(float amount, bool weather = false)
    {
        // A splash of a game, a spray or a hit, is the drops it asks for.
        if (!weather)
            return amount * 20.0f;

        // the rain of the weather: the amount is the intensity of the rain, see
        // SprayDrops and WeatherDropsPerSecond
        return WeatherDropsPerSecond / 60.0f * amount;
    }

    // The rain of one frame: the amount is spread over the time the frame took,
    // see above. A game that keeps a spray going hands this over every frame,
    // and a splash of the camera is one burst of FillScreenMoving instead.
    static inline void FillScreenMovingRate(float amount, bool isBlood = false, bool weather = false)
    {
        if (!ms_initialised || ms_fbWidth <= 0 || ms_fbHeight <= 0)
            return;

        if (ms_StaticRain)
            amount = 1.0f;

        // see FillScreenMoving: the amount is not trusted to be a sane count
        if (!(amount > 0.0f))
            return;

        float& remainder = isBlood ? ms_bloodRemainder : ms_spawnRemainder;
        const float perFrame = DropsOfAmount(amount, weather);
        remainder = (std::min)(remainder + perFrame * FrameSeconds() * 60.0f, perFrame * 4.0f + 1.0f);

        if (remainder < 1.0f)
            return;

        const float whole = floorf(remainder);
        remainder -= whole;

        if (isBlood)
            SpawnDrops((int32_t)whole, 0xFF, 0x00, 0x00, false, true);
        else
            SpawnDrops((int32_t)whole, bEnableSnow ? SnowShade : 0xFF, bEnableSnow ? SnowShade : 0xFF, bEnableSnow ? SnowShade : 0xFF, weather);
    }

    // The intensity of the rain on Forza's lens out of the intensity of its
    // weather: nothing below 0.04, then up to a half along a line from there to
    // (0.5, 0.5), and the intensity itself above that.
    static inline float RainCurve(float rain)
    {
        if (!(rain >= 0.04f))
            return 0.0f;

        if (rain < 0.5f)
            return (rain - 0.04f) * 1.0869565f;

        return rain;
    }

    static inline void SprayDrops()
    {
        // A rain intensity that is negative or not a number at all is not rain. The
        // check for "not zero" is one that a not a number passes, and the branch
        // below turns the intensity into a number of drops.
        if (!NoRain() && (ms_rainIntensity > 0.0f || bForceRain) && ms_enabled)
        {
            // Forza's lens takes the rain at its intensity and nothing else: not
            // where the camera looks, which one game hands over as its view and
            // the next as its up, see RainCurve
            const float intensity = RainCurve(bForceRain ? 1.0f : std::clamp(ms_rainIntensity, 0.0f, 1.0f));

            if (intensity > 0.0f)
                FillScreenMovingRate(intensity, false, true);
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

    // A number of drops of the rain, placed at random over the glass. Forza lands
    // its drops anywhere at rest, and of two kinds: seven in eight are small, from
    // the smallest size the ini asks for to about twice that, and the rest are big,
    // from about two thirds of the largest size to the largest. They live a few
    // seconds, see LifeMinSeconds.
    static inline void SpawnDrops(int32_t n, int R, int G, int B, bool weather = false, bool blood = false)
    {
        if (!ms_initialised || ms_fbWidth <= 0 || ms_fbHeight <= 0)
            return;

        const int32_t room = RoomForNewDrops();

        if (n > room)
            n = room;

        const float smallest = (float)SC(MinSize);
        const float biggest = (float)(std::max)(SC(MaxSize), SC(MinSize));
        const float smallTop = smallest + (biggest - smallest) * 0.32f;
        const float bigBottom = smallest + (biggest - smallest) * 0.62f;

        // fewer flakes of snow than drops of rain, see SnowShare
        if (bEnableSnow)
        {
            ms_snowRemainder += n * SnowShare;
            n = (int32_t)ms_snowRemainder;
            ms_snowRemainder -= (float)n;
        }

        for (int32_t i = 0; i < n; i++)
        {
            // Forza lands its drops evenly over the picture (out of a table of
            // points spread evenly over it); what keeps the middle of its lens
            // thinner at speed is the run of the drops outwards, see MoveDrop
            // and the capture of the game measured in tests/XrdTestD3D11.cpp.
            float x = GetRandomFloat((float)ms_fbWidth);
            float y = GetRandomFloat((float)ms_fbHeight);
            (void)weather;

            const bool big = GetRandomFloat(1.0f) < BigDropShare;
            float size = big ? bigBottom + GetRandomFloat(biggest - bigBottom) : smallest + GetRandomFloat(smallTop - smallest);

            if (bEnableSnow)
                size = smallest * SnowMinScale + GetRandomFloat((std::max)(0.0f, biggest * SnowMaxScale - smallest * SnowMinScale));

            const float life = bEnableSnow ? SnowLifeSeconds
                : LifeMinSeconds + (LifeMaxSeconds > LifeMinSeconds ? GetRandomFloat(LifeMaxSeconds - LifeMinSeconds) : 0.0f);
            if (blood)
            {
                // blood sticks where it lands, see BloodHoldSeconds
                const float hold = BloodHoldSeconds + GetRandomFloat(BloodHoldSpread);
                WaterDrop* splat = PlaceNew(x, y, size, (hold + BloodFadeSeconds) * 2000.0f, 1, R, G, B);

                if (splat)
                {
                    splat->blood = true;
                    splat->trail = false;
                    splat->fallTop = 0.0f;
                }

                continue;
            }

            WaterDrop* drop = PlaceNew(x, y, size, life * 2000.0f, 1, R, G, B);

            // A flake of snow sticks where it lands: it is ice, the air and the
            // turning camera do not move it, it melts away where it is.
            if (drop && !bEnableSnow)
                NewDropMoving(drop);
        }
    }

    // Deposited water keeps the parent's colour, atlas shape, turn and opacity.
    // It stays on the glass and fades independently, without producing more
    // traces. MoveDrop distributes these deposits along the path.
    // Whether the water a drop leaves is drawn as a ribbon: a thin film along the
    // path of the drop, which the shaders of Direct3D 9 and above draw. The
    // other renderers draw it as small drops, the way the effect always has.
    static inline bool TrailRibbons()
    {
        const auto api = Xrd::GetRenderer();
        return GatheredLight() && ms_atlasUsed && (api != Xrd::RENDERER_D3D8 || Xrd::DrawsWithD3D9Shaders());
    }

    // The width of the film a drop leaves, as a share of the drop, right where
    // the drop is, and how much one piece of the ribbon overlaps the next, see
    // MoveDrop. No film is wider than TrailWidest of the largest size of the ini.
    // Behind the drop the film drains in a moment to TrailThin of that width:
    // a running drop is a bead with a thin wet line behind it, not a tail as
    // thick as itself. TrailNarrowing is how long that takes, in seconds.
    static constexpr float TrailWidthShare = 0.5f;
    static constexpr float TrailWidest = 0.45f;
    static constexpr float TrailThin = 0.35f;
    static constexpr float TrailNarrowing = 0.08f;
    static constexpr float TrailOverlap = 3.5f;
    // A drop that leaves water behind it loses that water: it shrinks by this
    // share of the way it runs while it leaves any, and once it is down to
    // TrailSmallest of the smallest size of the ini it is too small to leave
    // more and runs on as a small drop, or stops.
    static constexpr float TrailDrain = 0.08f;
    static constexpr float TrailSmallest = 1.5f;

    // A drop that runs over glass never runs straight: it catches on the glass
    // and takes in the small drops in its way, and wanders from side to side, so
    // its trail is a rivulet that meanders gently and never a ruled line. The
    // wander is how far the drop is off the line of its run after running a
    // distance s, in pixels: a long wave and a short one whose lengths and sizes
    // every drop has of its own, so no two trails wander alike.
    static inline float Meander(const WaterDrop* drop, float s)
    {
        const float seed = drop->meanderSeed;
        const float size = (std::max)(drop->size, 1.0f);
        const float longWave = size * (5.0f + 4.0f * seed);
        const float shortWave = size * (1.8f + 0.8f * fmodf(seed * 7.31f, 1.0f));
        const float longSway = size * (0.12f + 0.13f * fmodf(seed * 3.17f, 1.0f));
        const float shortSway = size * (0.03f + 0.03f * fmodf(seed * 5.53f, 1.0f));
        return longSway * sinf(s / longWave * 6.2831853f + seed * 40.0f) +
            shortSway * sinf(s / shortWave * 6.2831853f + seed * 90.0f);
    }

    // How wide a rivulet is where the drop that left it has run a distance s, as
    // a share of its usual width: it thins out and swells again along its length,
    // and here and there the water gathers a little. Made of the same seed as the
    // wander, so the width belongs to the place on the trail and does not flicker.
    static inline float TrailSwell(const WaterDrop* drop, float s)
    {
        const float seed = drop->meanderSeed;
        const float size = (std::max)(drop->size, 1.0f);
        const float slow = sinf(s / (size * (3.0f + 2.0f * fmodf(seed * 2.71f, 1.0f))) * 6.2831853f + seed * 17.0f);
        const float quick = sinf(s / (size * (1.1f + 0.6f * fmodf(seed * 4.43f, 1.0f))) * 6.2831853f + seed * 29.0f);
        const float bead = powf((std::max)(0.0f, sinf(s / (size * (4.0f + 3.0f * fmodf(seed * 6.11f, 1.0f))) * 6.2831853f + seed * 53.0f)), 8.0f);
        return std::clamp(1.0f + 0.25f * slow + 0.12f * quick + 0.4f * bead, 0.6f, 1.5f);
    }

    // The water a drop leaves behind it on the glass at a place of its path. As a
    // ribbon it is a thin film along the way the drop went, segment pixels of the
    // path long and overlapping the next piece, so the pieces read as one smooth
    // line; otherwise it is a small drop of the colour and the shape of the drop.
    static inline void NewTrace(WaterDrop* drop, float x, float y, float velocityX, float velocityY, float segment)
    {
        const bool ribbon = TrailRibbons();
        const float size = ribbon ? (std::max)(2.0f * Scale(),
            (std::min)(drop->size * TrailWidthShare, TrailWidest * (float)SC(MaxSize)) * TrailSwell(drop, drop->meander))
            : (std::max)((float)SC(MinSize), drop->size * TraceShare);
        auto* trace = PlaceNew(x, y, size, drop->traceTtl, true, drop->r, drop->g, drop->b, true);

        if (!trace)
            return;

        // the water of a drop is as visible as the drop is, and no more: a drop that
        // has almost faded out leaves water that fades from where the drop is now
        trace->alpha = drop->alpha;
        trace->alpha0 = drop->alpha;
        trace->shapeX = velocityX;
        trace->shapeY = velocityY;

        if (ribbon)
        {
            trace->uv_index = AtlasShapes;
            trace->rotation = atan2f(velocityY, velocityX);
            trace->trailLength = (std::max)(segment, 1.0f) * TrailOverlap;
            // a film of water is fainter than a drop
            trace->alpha = trace->alpha0 = (uint8_t)(drop->alpha * 0.85f);
        }
        else
        {
            trace->uv_index = drop->uv_index;
            trace->rotation = drop->rotation;
        }
    }

    // The speed, in pixels a second at 480 lines, at which a drop that the
    // turning camera drags over the glass starts to leave water behind it.
    // Forza leaves none: its drops only ever run outwards. A drag sideways is
    // where the water of this effect comes from, and only drops of some size
    // that are dragged hard leave it.
    static constexpr float TrailSpeed = 12.0f;
    // the air across the view under which a drop the turning camera shoved
    // settles again, in units of the game a second, see MoveDrop
    static constexpr float SettleBelow = 2.0f;
    // how big the water a drop leaves is, as a share of the drop
    static constexpr float TraceShare = 0.55f;

    // How far a drop travels in this frame, and the water it leaves on the way.
    // This is the simulation shader of the lens rain of Forza Horizon 4, see the
    // air over the lens above, in the units of this effect.
    static void MoveDrop(WaterDropMoving* moving, float dt, float settle)
    {
        WaterDrop* drop = moving->drop;
        if (!ms_movingEnabled)
            return;
        if (!drop->active)
        {
            DetachMoving(drop);
            return;
        }

        const float halfW = ms_fbWidth * 0.5f;
        const float halfH = ms_fbHeight * 0.5f;

        // where on the lens the drop is, -1 to 1 each way with y up
        const float px = (drop->x - halfW) / halfW;
        const float py = (halfH - drop->y) / halfH;

        // The size of the drop the way Forza measures it, half its width in
        // halves of the height of the picture, times the scale of the push.
        const float k = drop->size * 0.5f / halfH * (bEnableSnow ? SnowSizeToMagnitude : SizeToMagnitude);

        // Forza steps its drops in time measured in their lives, the frame time
        // divided by RainDropLife: see LifeMinSeconds. Gravity and the settling
        // of a shoved drop, which are the effect's own, stay in seconds.
        const float life = bEnableSnow ? SnowLifeSeconds : LifeMinSeconds;
        const float simDt = dt / life;

        // The dome of the lens turns the velocity into a run over the picture:
        // along the view outwards from the middle, across it sideways and up. The
        // drop runs with the velocity it came into the frame with and the air
        // changes it for the next, which is the order Forza steps its drops in: a
        // replay of the drops of a capture of the game through this lands them
        // where the game had them a few frames later, to a hundredth.
        const float r = sqrtf(px * px + py * py);
        const float outX = r > 0.0001f ? px / r : 0.0f;
        const float outY = r > 0.0001f ? py / r : 0.0f;
        const float runX = LensLateralGain * drop->vel[0] - LensRadialGain * drop->vel[2] * outX;
        const float runY = LensLateralGain * drop->vel[1] - LensRadialGain * drop->vel[2] * outY;

        // the friction of the glass, which takes away from every part of the
        // velocity of the drop and never turns it round
        for (float& v : drop->vel)
        {
            if (v > 0.0f)
                v = (std::max)(0.0f, v - k * simDt);
            else if (v < 0.0f)
                v = (std::min)(0.0f, v + k * simDt);
        }

        // the push of the air, the harder the more squarely the lens faces it
        const float push = LensAlignment(px, py, ms_air) * k * simDt;
        drop->vel[0] += ms_air[0] * push;
        drop->vel[1] += ms_air[1] * push;
        drop->vel[2] += ms_air[2] * push;

        // A drop that the turning camera shoved comes to rest again once the
        // camera stops turning, see LookSettle, unless the air of the travel of
        // the camera goes on pushing it that way: a camera that looks out to the
        // side of a car that drives keeps its drops running.
        if (ms_lookAir[0] == 0.0f && fabsf(ms_air[0]) < SettleBelow)
            drop->vel[0] *= settle;
        if (ms_lookAir[1] == 0.0f && fabsf(ms_air[1]) < SettleBelow)
            drop->vel[1] *= settle;


        // the run down the glass of a drop gravity moves, see GravityShare
        if (bGravity && drop->fallTop > 0.0f)
            drop->fall = (std::min)(drop->fallTop, drop->fall + drop->fallTop / GravityRise * dt);
        else
            drop->fall = 0.0f;

        float travelX = runX * halfW * simDt;
        float travelY = -runY * halfH * simDt + drop->fall * dt;

        // The water a drop leaves when the turning camera drags it sideways: by
        // the distance it travels, a part of its own size apart, and only from
        // the drops that leave any, see TrailSpeed.
        const float sideways = LensLateralGain * sqrtf(drop->vel[0] * drop->vel[0] + drop->vel[1] * drop->vel[1]) * halfH / life;
        // a drop that is dragged sideways leaves water, and so does one that
        // runs down the glass, see GravityShare
        const bool leaves = drop->trail && (sideways > TrailSpeed * Scale() || drop->fall > 0.0f);

        // and a drop that leaves water wanders from side to side as it runs, see
        // Meander
        const float run = sqrtf(travelX * travelX + travelY * travelY);

        if (leaves && run > 0.0001f && std::isfinite(run))
        {
            const float before = Meander(drop, drop->meander);
            drop->meander += run;
            const float aside = Meander(drop, drop->meander) - before;
            const float alongX = travelX / run;
            const float alongY = travelY / run;
            travelX -= alongY * aside;
            travelY += alongX * aside;
        }

        if (dt > 0.0f)
        {
            drop->shapeX = travelX / dt;
            drop->shapeY = travelY / dt;
        }

        const float distance = sqrtf(travelX * travelX + travelY * travelY);
        // A ribbon is laid down a little more often than it is wide; the small
        // drops of the other renderers a part of their own size apart.
        const float footprint = TrailRibbons() ? (std::max)(2.0f * Scale(), drop->size * 0.15f)
            : (std::max)((float)SC(MinSize), drop->size * TraceShare) * 0.4f;
        const float spacing = (std::max)(std::isfinite(fMoveStep) ? fMoveStep : 0.1f, (std::max)(0.25f, footprint));

        if (distance > 0.0001f && std::isfinite(distance))
        {
            const float remainder = fmodf(moving->dist, spacing);
            const float total = remainder + distance;
            const float crossed = floorf(total / spacing);
            // Spread a bounded number of deposits across a fast sweep. Never
            // carry a spawn backlog into stationary frames after a camera cut.
            const int count = (int)(std::min)(crossed, 8.0f);
            if (leaves && drop->alpha > 0 && ms_numDrops < (int)ms_drops.size() - 1)
                for (int i = 0; i < count; ++i)
                {
                    const float step = crossed > 8.0f ? (i + 0.5f) * distance / count
                        : spacing - remainder + i * spacing;
                    const float t = std::clamp(step / distance, 0.0f, 1.0f);
                    NewTrace(drop, drop->x + travelX * t, drop->y + travelY * t,
                        drop->shapeX, drop->shapeY, crossed > 8.0f ? distance / count : spacing);
                }
            moving->dist = fmodf(total, spacing);
        }

        // the water it left is water the drop no longer has, see TrailDrain
        if (leaves && distance > 0.0f && std::isfinite(distance))
        {
            const float smallest = TrailSmallest * (float)SC(MinSize);
            drop->size = (std::max)(smallest, drop->size - distance * TrailDrain);

            if (drop->size <= smallest)
            {
                drop->trail = false;
                drop->fallTop = 0.0f;
            }
        }

        drop->x += travelX;
        drop->y += travelY;

        // A drop that left the picture is gone, the way Forza kills a drop whose
        // middle leaves the lens.
        if (drop->x < 0.0f || drop->y < 0.0f || drop->x > (float)ms_fbWidth || drop->y > (float)ms_fbHeight)
            Expire(drop);
    }

    static inline void ProcessMoving()
    {
        if (!ms_movingEnabled)
            return;
        const float elapsed = GetFrameTimeSeconds();
        if (!(elapsed > 0.0f) || !std::isfinite(elapsed))
            return;
        // Bound recovery after a stall. The coefficient is shared by every moving
        // drop, so compute the exponential once per frame.
        const float dt = (std::min)(elapsed, 0.1f);
        const float settle = expf(-dt / LookSettle);
        for (auto& moving : ms_dropsMoving)
            if (moving.drop)
                MoveDrop(&moving, dt, settle);
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
        // one of the shapes of the atlas, or the whole of the fallback mask
        drop.uv_index = ms_atlasUsed ? GetRandomInt((bEnableSnow ? SnowShapes : AtlasShapes) - 1) : -1;
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

        // A drop lands at rest, turned any way, see the air over the lens. Some
        // of the drops of some size leave water behind them when the turning
        // camera drags them sideways, see TrailSpeed; the water a trail is made
        // of leaves nothing itself, and a flake of snow leaves no water at all.
        drop.vel[0] = drop.vel[1] = drop.vel[2] = 0.0f;
        drop.fall = 0.0f;
        drop.fallTop = 0.0f;

        if (!isTrace && !bEnableSnow && GetRandomFloat(1.0f) < GravityShare)
            drop.fallTop = (GravitySlowest + GetRandomFloat(GravityFastest - GravitySlowest)) * Scale();
        drop.rotation = GetRandomFloat(6.2831853f) - 3.1415927f;
        const float wet = (drop.size - (float)SC(MinSize)) / (float)(std::max)(1, SC(MaxSize) - SC(MinSize));
        drop.trail = !isTrace && !bEnableSnow && GetRandomFloat(1.0f) < 0.35f + 0.45f * std::clamp(wet, 0.0f, 1.0f);

        // How long the water a drop leaves stays on the glass, rolled for every
        // drop on its own, so the tails of one shower are of every length.
        // in seconds of its own, not a share of the life of the drop: a drop
        // stays for seconds, its film dries in a moment
        drop.traceTtl = (TraceLifeMin + GetRandomFloat(TraceLifeMax - TraceLifeMin)) * 2000.0f;
        drop.shapeX = drop.shapeY = 0.0f;
        drop.meander = 0.0f;
        drop.meanderSeed = GetRandomFloat(1.0f);
        drop.blood = false;

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
            SpawnDrops((int32_t)DropsOfAmount(amount), 0xFF, 0x00, 0x00, false, true);
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

        // A screen full of the drops of the rain, landed the way any of them
        // lands (see SpawnDrops): of its sizes, for its life, and moved by the air
        // like the rest, or flakes of snow when it snows.
        const int shade = bEnableSnow ? SnowShade : 0xFF;
        SpawnDrops(n, shade, shade, shade);
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

        // the camera the air was measured against is gone with the drops
        ms_air[0] = ms_air[1] = ms_air[2] = 0.0f;
        ms_lookAir[0] = ms_lookAir[1] = 0.0f;
        ms_forwardSpeed = 0.0f;
        ms_haveLastFwd = false;
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
    // how many texels the mask is across, see the shaders
    static inline int32_t ms_maskSize = 256;
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
        // the drops on the lens are of the other kind and of the other atlas:
        // the lens starts over with the shapes of the new one
        Clear();
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
            ReadIniSettings(bForwardIsUp);
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

            // Forza draws its flakes of snow with shapes of their own, clusters of
            // small specks of ice (source/resources/lenssnow.png, the ten flakes of
            // its texture array of them, laid out the way the drops of the rain are).
            if (LoadMask(bEnableSnow ? IDR_SNOWDROPMASK : IDR_DROPMASK, pixels, width, height))
            {
                ms_maskTex = Xrd::CreateTexture(width, height, pixels.data());
                ms_maskSize = width;
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
                ms_maskSize = MaskSize;
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
    // the frame it shows, and the colour it is drawn in.
    //
    // A drop of rain on a lens is a lens of its own, and what it shows is the
    // picture turned round both ways and squeezed into it: Forza refracts its
    // frame through the normal of the drop with an index of 0.8 and a scale of 7,
    // which comes to about all of the picture, upside down and mirrored, across
    // a drop. That is RefractionReach, in widths and heights of the picture from
    // the middle of the drop to the edge of its quad; the sampler clamps what
    // reaches past the frame, the way Forza's does.
    static constexpr float RefractionReach = 1.75f;
    // The scale Forza bends the frame through a drop with, and through a flake
    // of snow, whose dome is half as steep: see source/shaders/d3d10/drops.hlsl.
    // The renderers without that shader show the window of RefractionReach.
    static constexpr float Refraction = 7.0f;
    // Forza's RefractionScalar of the snow (its composite in the capture).
    static constexpr float SnowRefraction = 3.0f;
    // the atlas of the shapes of Forza's drops, source/resources/lensdrops.png:
    // fourteen shapes, four along each side
    static constexpr int AtlasTiles = 4;
    static constexpr int AtlasShapes = 14;
    // the shapes of the flakes of snow in their own atlas, see Init
    static constexpr int SnowShapes = 10;
    // how much of its tile a shape takes, see tools of the atlas and the shaders
    static constexpr float ShapeShare = 0.6f;
    // A flake of snow refracts with a scale of 3 instead of 7, through a shape
    // that is half as steep: what it shows is the picture around it, turned
    // round and a little squeezed, see the lens map of Forza's snow.
    // On the renderers without the shaders of Direct3D 9 and above a flake shows
    // the frame from this close round it, mirrored: each speck of it shows the
    // frame a little displaced, the way a bead of water bends it, and the shade
    // of the atlas darkens its rim (see source/resources/lenssnow.png). Forza's
    // flakes are clear water with a touch of frost, see SnowMilk.
    static constexpr float SnowRefractionReach = 0.07f;
    // How milky a flake is on those renderers, see Xrd::SetSceneFrost: the frost
    // of Forza's flakes is a few hundredths of their thickness.
    static constexpr float SnowMilk = 0.05f;
    // How much a flake of snow is frosted: what it shows is blurred by this
    // much more than a drop of rain (two levels of the chain for every one),
    // and it is a little darker, see SnowShade.
    static constexpr float SnowFrost = 0.35f;
    static constexpr int SnowShade = 235;

    // What a soft drop of rain shows on the renderers whose shaders draw it, see
    // source/shaders/d3d10/drops.hlsl: the picture round it, upside down and
    // mirrored the way a drop of water turns it, from LensWindow of the width
    // and the height of the picture on either side of it, blurred. Forza bends
    // its frame through the slope of every drop and blurs the result at a
    // quarter of the resolution; the mirrored window is what that comes to, a
    // picture that goes over the drop smoothly from one side to the other.
    static constexpr float LensWindow = 0.4f;

    static inline bool LensShaders()
    {
        const auto api = Xrd::GetRenderer();

        // a game of Direct3D 8 run through a wrapper of Direct3D 9 has its drops
        // drawn by the backend of Direct3D 9, see Xrd::DrawsWithD3D9Shaders
        if (api == Xrd::RENDERER_D3D8)
            return Xrd::DrawsWithD3D9Shaders();

        return api == Xrd::RENDERER_D3D9 || api == Xrd::RENDERER_D3D10 || api == Xrd::RENDERER_D3D10_1 ||
            api == Xrd::RENDERER_D3D11 || api == Xrd::RENDERER_D3D12;
    }

    static inline void AddDropQuad(float x, float y, float size, float uvsize, uint32_t color, int uv_index, bool lens,
        float rotation = 0.0f, float velocityX = 0.0f, float velocityY = 0.0f)
    {
        // the corners of the shape in the atlas: a tile of AtlasTiles along each
        // side, or the whole of the fallback mask
        float u0 = 0.0f, v0 = 0.0f, tileSize = 1.0f;

        if (uv_index >= 0)
        {
            tileSize = 1.0f / AtlasTiles;
            u0 = (uv_index % AtlasTiles) * tileSize;
            v0 = (uv_index / AtlasTiles) * tileSize;
        }

        const float uv[8] = { u0, v0, u0, v0 + tileSize, u0 + tileSize, v0 + tileSize, u0 + tileSize, v0 };
        static float xy[] = {
            -1.0f, -1.0f, -1.0f,  1.0f,
            1.0f,  1.0f,  1.0f, -1.0f
        };

        (void)uvsize;

        // where the middle of the drop is in the frame, for the refraction
        const float cu = (x + ms_xOff) / ms_fbWidth;
        const float cv = (y + ms_yOff) / ms_fbHeight;

        // A drop of clear water gathers the light of the frame around it, see
        // source/shaders/d3d10/drops.hlsl. The atlas coordinate is what says so:
        // every drop samples its tile at a coordinate between zero and one, so a
        // coordinate below zero is one the shader can read as the mark that the
        // drop is a lens, and it moves it back up before it samples. Every
        // renderer that draws the drops without that shader gets the atlas
        // coordinate untouched and is left exactly as it was. The water a drop
        // left behind it does not gather any light: a film of water on the glass
        // is not a lens, see IsLens.
        //
        // The shape is turned the way the drop landed, and the picture in it is
        // not: it is the frame refracted, which stays the right way round on the
        // screen whichever way the shape of the drop is turned.
        // the shape takes ShapeShare of its tile of the atlas, the rest is room
        // for the blur of a soft drop, so the quad is that much larger
        const float scale = size * 0.5f / (uv_index >= 0 ? ShapeShare : 1.0f);
        const float c = cosf(rotation);
        const float sn = sinf(rotation);

        // A drop that travels over the glass is drawn out along its way: one
        // that crosses its own size in a sixtieth of a second is almost twice as
        // long as it is wide, and none is drawn out further than that. Some of
        // what it gains in length it loses in width.
        const float speed = sqrtf(velocityX * velocityX + velocityY * velocityY);
        const float stretch = 1.0f + (std::min)(1.2f, speed / 60.0f * 0.9f / (std::max)(size, 1.0f));
        const float ax = speed > 0.001f ? velocityX / speed : 1.0f;
        const float ay = speed > 0.001f ? velocityY / speed : 0.0f;

        for (int i = 0; i < 4; i++)
        {
            Xrd::Vertex vertex{};
            const float rx = xy[i * 2] * c - xy[i * 2 + 1] * sn;
            const float ry = xy[i * 2] * sn + xy[i * 2 + 1] * c;
            const float along = (rx * ax + ry * ay) * stretch;
            const float across = (-rx * ay + ry * ax) / sqrtf(stretch);
            const float ox = along * ax - across * ay;
            const float oy = along * ay + across * ax;
            vertex.x = x + ox * scale * ms_xScale + ms_xOff;
            vertex.y = y + oy * scale + ms_yOff;
            vertex.z = 0.0f;
            vertex.color = color;
            vertex.u0 = uv[i * 2] - (lens ? AtlasLightMarker : 0.0f);
            vertex.v0 = uv[i * 2 + 1];

            const float reach = bEnableSnow ? SnowRefractionReach : (LensShaders() ? LensWindow : RefractionReach);
            vertex.u1 = cu - ox * reach;
            vertex.v1 = cv - oy * reach;

            ms_vertices.push_back(vertex);
        }
    }

    // How big a drop is drawn: the size it has, and for the first moment after it
    // landed the splash of it, see SplashScale.
    static inline float DrawnSize(const WaterDrop* drop)
    {
        // a flake of snow lands without a splash
        if (drop->isTrace || bEnableSnow)
            return drop->size;

        const float age = drop->time / 2000.0f;

        if (age >= SplashSeconds)
            return drop->size;

        const float t = std::clamp(age / SplashSeconds, 0.0f, 1.0f);
        const float s = t * t * (3.0f - 2.0f * t);
        return drop->size * (SplashScale + (1.0f - SplashScale) * s);
    }

    // One drop of the rain, drawn as one quad of the atlas of the drop shapes: see
    // AddDropQuad. The drops a drop left behind it are drops of the rain like any
    // other, so this is all there is to drawing one, and the water of the effect can
    // never take more of the vertex buffer of a backend than the pool is large.
    // A piece of a ribbon of water: a strip of the given length along the angle
    // and of the given width, over the tile of the atlas the shaders read as a
    // film, see source/shaders/d3d10/drops.hlsl. Along the strip is the u of the
    // tile, across it the v.
    static inline void AddStripQuad(float x, float y, float length, float width, float angle, uint32_t color)
    {
        static const float xy[] = { -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f, -1.0f };
        const float tileSize = 1.0f / AtlasTiles;
        const float u0 = (AtlasShapes % AtlasTiles) * tileSize;
        const float v0 = (AtlasShapes / AtlasTiles) * tileSize;
        const float uv[8] = { u0, v0, u0, v0 + tileSize, u0 + tileSize, v0 + tileSize, u0 + tileSize, v0 };
        const float ax = cosf(angle), ay = sinf(angle);

        for (int i = 0; i < 4; i++)
        {
            Xrd::Vertex vertex{};
            const float along = xy[i * 2] * length * 0.5f;
            const float across = xy[i * 2 + 1] * width * 0.5f;
            const float ox = along * ax - across * ay;
            const float oy = along * ay + across * ax;
            vertex.x = x + ox * ms_xScale + ms_xOff;
            vertex.y = y + oy + ms_yOff;
            vertex.z = 0.0f;
            vertex.color = color;
            // the film is marked in v, see source/shaders/d3d10/drops.hlsl
            vertex.u0 = uv[i * 2];
            vertex.v0 = uv[i * 2 + 1] - AtlasLightMarker;
            vertex.u1 = (x + ms_xOff) / ms_fbWidth;
            vertex.v1 = (y + ms_yOff) / ms_fbHeight;
            ms_vertices.push_back(vertex);
        }
    }

    static inline void AddToRenderList(WaterDrop* drop)
    {
        if (drop->isTrace && drop->uv_index == AtlasShapes && TrailRibbons())
        {
            // Right behind its drop the film is as wide as the drop left it, and
            // under the drop, which is drawn over it, so the two run into each
            // other as one body of water; a moment later it has drained into a
            // thin wet line, see TrailThin. A piece is never shorter than it is
            // wide, so its round ends are never cut off by its quad.
            const float age = drop->time / 2000.0f;
            const float thinning = TrailThin + (1.0f - TrailThin) * expf(-age / TrailNarrowing);
            const float width = (std::max)(1.2f * Scale(), drop->size * thinning);
            AddStripQuad(drop->x, drop->y, (std::max)(drop->trailLength, width), width, drop->rotation,
                Xrd::ColorARGB(drop->alpha, drop->r, drop->g, drop->b));
            ms_numBatchedDrops++;
            return;
        }

        AddDropQuad(drop->x, drop->y, DrawnSize(drop), drop->uvsize,
            Xrd::ColorARGB(drop->alpha, drop->r, drop->g, drop->b), drop->uv_index, IsLens(drop),
            drop->rotation, drop->shapeX, drop->shapeY);

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

        // The water the drops left goes under the drops, so a drop sits on the
        // end of its own trail as one body of water instead of under its film.
        for (auto& drop : ms_drops)
            if (drop.active && drop.isTrace)
                AddToRenderList(&drop);

        for (auto& drop : ms_drops)
            if (drop.active && !drop.isTrace)
                AddToRenderList(&drop);

        if (ms_numBatchedDrops <= 0)
            return;

        Xrd::SetMaskTexture(ms_maskTex);
        Xrd::SetProjection(Xrd::PROJECTION_SCREEN);
        Xrd::SetSceneUVScale(ms_UVXOffset, ms_UVXScale, ms_UVYOffset, ms_UVYScale);
        Xrd::SetSceneSampling(true);
        // A flake of snow is frosted water: Forza blurs what it shows a level of
        // the chain further than a drop of rain and softens its edge, whatever
        // DropBlur asks for the rain, see SnowFrost.
        Xrd::SetSceneComplement(false);
        Xrd::SetSceneFrost(bEnableSnow ? SnowMilk : 0.0f);
        // A flake of snow is drawn crisp: Forza blurs what a speck shows by its
        // frost, which is a few hundredths of it, so a speck refracts the frame
        // sharply, see SnowRefraction.
        Xrd::SetSceneBlur(bEnableSnow ? 0.0f : fDropBlur, ms_atlasUsed ? (float)AtlasTiles : 1.0f,
            bEnableSnow ? SnowRefraction : Refraction, (float)ms_maskSize);
        Xrd::Render(ms_vertices.data(), (int32_t)ms_vertices.size(), Xrd::PRIMITIVE_TRIANGLES);
    }
};

// How a drop ages. Its time is counted in the units the effect has always
// counted it in, two thousand to the second, and the life it was given is in
// them as well. What is left of its life is how visible it is, the way Forza
// draws its drops: a drop fades from the moment it lands until it is gone.
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
    else if (this->fades && this->blood)
    {
        // blood is whole until it fades over the end of its life
        const float left = (this->ttl - this->time) / (WaterDrops::BloodFadeSeconds * 2000.0f);
        this->alpha = (uint8_t)(this->alpha0 * std::clamp(left, 0.0f, 1.0f));
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
