// Direct3D 9, the API this game uses
#define XRD_ENABLE_D3D9
#include "xrd/xrd.h"

// Far Cry is the same code for 32 and for 64 bit: the game itself is in the Cry
// modules the executable loads, and every one of them is reached through the
// interfaces of the engine, which CrySystem.dll hands out. The plugin only needs
// the system, and asks it for the rest by the places of its functions in the
// tables of those interfaces. The places are those of the shipped modules of both
// architectures, which are the same (CryCommon of the SDK has a few of them off by
// the functions of IProcess that I3DEngine overrides).
namespace Cry
{
    template <size_t Index, typename Ret, typename... Args>
    inline Ret Virtual(void* object, Args... args)
    {
        using Fn = Ret(__thiscall*)(void*, Args...);
        return (*reinterpret_cast<Fn**>(object))[Index](object, args...);
    }

    struct Vec3
    {
        float x, y, z;
    };

    // ISystem
    constexpr size_t SystemGetTimer = 23;
    constexpr size_t SystemGetConsole = 24;
    constexpr size_t SystemGet3DEngine = 26;
    constexpr size_t SystemGetViewCamera = 53;
    // IConsole, ICVar
    constexpr size_t ConsoleGetCVar = 20;
    constexpr size_t CVarGetFVal = 2;
    // ITimer
    constexpr size_t TimerGetFrameTime = 5;
    // I3DEngine
    constexpr size_t EngineLoadLevel = 8;
    constexpr size_t EngineGetWaterLevel = 21;
    constexpr size_t EngineCreateParticleEmitter = 24;
    constexpr size_t EngineDeleteParticleEmitter = 25;
    constexpr size_t EngineSetRainAmount = 125;
    constexpr size_t EngineGetVisAreaFromPos = 129;
    // IParticleEmitter, IParticleEffect
    constexpr size_t EmitterSetPos = 6;
    constexpr size_t EffectGetName = 5;
    // the effect a CParticleEmitter shows, m_pEffect, which is no function of
    // its interface
#ifdef _WIN64
    constexpr size_t EmitterEffect = 0x58;
#else
    constexpr size_t EmitterEffect = 0x44;
#endif

    // The camera the system renders with: its place, and the angles of the game in
    // degrees after the place it had before, see CCamera of the SDK.
    struct Camera
    {
        Vec3 position;
        Vec3 oldPosition;
        Vec3 anglesDeg;
    };
}

void* gSystem = nullptr;
IDirect3DDevice9* gDevice = nullptr;
Cry::Vec3 gCameraPos = {};
bool gHaveCamera = false;

// ---------------------------------------------------------------------------
// The water of the world around the camera
//
// Every effect of particles of the game is put in its place by
// CParticleEmitter::SetPos: an effect spawned once (a splash, an explosion) when
// it is spawned, and an effect an entity keeps going (a waterfall) every frame
// the entity is in sight. The ones of water near the camera wet the lens: a
// waterfall it stands under or next to, the spray of the foot of a waterfall,
// the spray of a vehicle driven through the water, and a splash of something
// falling into the water, of an explosion in it or of a bullet hitting it.
// ---------------------------------------------------------------------------
namespace Water
{
    enum class Kind
    {
        None,
        Fall,     // the water falling down from the top of a waterfall
        Spray,    // what keeps flying off something: the foot of a waterfall, a vehicle
        Splash,   // one splash
    };

    struct Source
    {
        const char* name;   // a part of the name of the effect
        Kind kind;
        float radius;       // in metres, how far from it the lens gets wet at all
        float amount;       // see below
    };

    // The amount of a fall or a spray is a rate (FillScreenMovingRate: 1 is twenty
    // drops a sixtieth of a second), that of a splash one burst (FillScreenMoving:
    // 1 is twenty drops). Both are at the source, and fall off to nothing at the
    // radius, see Wetness.
    constexpr Source Sources[] = {
        { "waterfall_collision", Kind::Spray, 9.0f, 0.12f },
        { "waterfall_breaks", Kind::Spray, 6.0f, 0.08f },
        { "waterfall", Kind::Fall, 5.0f, 0.25f },
        { "vehicle_water", Kind::Spray, 7.0f, 0.10f },
        { "Grenade_water", Kind::Splash, 16.0f, 3.0f },
        { "under_water_explosion", Kind::Splash, 16.0f, 3.0f },
        { "water_first_splash", Kind::Splash, 16.0f, 3.0f },
        { "water_particles", Kind::Splash, 12.0f, 1.5f },
        { "water_splash", Kind::Splash, 6.0f, 1.0f },
        { "hit_water", Kind::Splash, 3.0f, 0.15f },
    };

    // How far down from the top of a waterfall its water falls, in metres.
    constexpr float FallHeight = 30.0f;

    // the strongest rate and the strongest burst of the water of this frame
    inline float rate = 0.0f;
    inline float burst = 0.0f;

    inline const Source* Find(const char* name)
    {
        if (!name)
            return nullptr;
        for (const auto& source : Sources)
            if (strstr(name, source.name))
                return &source;
        return nullptr;
    }

    // How wet the lens gets of a source at a place, from nothing at its radius to
    // all of its amount at it. The water of a waterfall falls from the place of
    // its effect, so a camera next to the column of it, or under it, is in it.
    inline float Wetness(const Source& source, const Cry::Vec3& at)
    {
        const float dx = gCameraPos.x - at.x;
        const float dy = gCameraPos.y - at.y;
        float dz = gCameraPos.z - at.z;

        if (source.kind == Kind::Fall)
        {
            if (dz > 1.0f || dz < -FallHeight)
                return 0.0f;
            dz = 0.0f;
        }

        const float distance = sqrtf(dx * dx + dy * dy + dz * dz);
        if (!(distance < source.radius))
            return 0.0f;

        const float closeness = 1.0f - distance / source.radius;
        return source.amount * closeness * closeness;
    }

    inline void Emitted(void* emitter, const Cry::Vec3* pos)
    {
        if (!gHaveCamera || !emitter || !pos)
            return;

        auto* effect = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(emitter) + Cry::EmitterEffect);
        if (!effect)
            return;

        const auto* source = Find(Cry::Virtual<Cry::EffectGetName, const char*>(effect));
        if (!source)
            return;

        const float wet = Wetness(*source, *pos);
        if (source->kind == Kind::Splash)
            burst = (std::max)(burst, wet);
        else
            rate = (std::max)(rate, wet);
    }

    // The water of the frame on the lens, and the next frame starts dry.
    inline void Apply()
    {
        if (rate > 0.0f)
            WaterDrops::FillScreenMovingRate(rate);
        if (burst > 0.0f)
            WaterDrops::FillScreenMoving(burst);
        rate = burst = 0.0f;
    }

#ifdef _WIN64
    SafetyHookInline shSetPos = {};
    void EmitterSetPos(void* emitter, const Cry::Vec3* pos, const Cry::Vec3* dir, float scale)
    {
        shSetPos.unsafe_call(emitter, pos, dir, scale);
        Emitted(emitter, pos);
    }
#else
    SafetyHookInline shSetPos = {};
    void __fastcall EmitterSetPos(void* emitter, void* edx, const Cry::Vec3* pos, const Cry::Vec3* dir, float scale)
    {
        shSetPos.unsafe_thiscall(emitter, pos, dir, scale);
        Emitted(emitter, pos);
    }
#endif

    // SetPos is found in the table of an emitter the engine makes for it, and
    // which goes again right after.
    inline void Hook(void* engine)
    {
        if (shSetPos)
            return;

        auto* emitter = Cry::Virtual<Cry::EngineCreateParticleEmitter, void*>(engine);
        if (!emitter)
            return;

        shSetPos = safetyhook::create_inline((*reinterpret_cast<void***>(emitter))[Cry::EmitterSetPos], EmitterSetPos);
        Cry::Virtual<Cry::EngineDeleteParticleEmitter, void, void*>(engine, emitter);
    }
}

// ---------------------------------------------------------------------------
// The rain of the levels
//
// The game rains only on one map, the one storm of its own is on mp_airstrip.
// With LevelRainChance of the settings, a level that loads may rain: showers come
// and go while it is played, each of a strength of its own, and they are the rain
// of the game itself, e_rain_amount, which the engine draws the rain of the world
// with, and the drops follow, see DrawDrops.
// The engine draws its rain under the water too, so while the camera is under it
// the engine is given none, see SetRainAmount.
// A level with a storm of its own is left with it. Whether a level rains is
// decided when the engine loads it, see C3DEngine::LoadLevel, so a save loaded in
// it or a menu opened in it keeps its weather.
// ---------------------------------------------------------------------------
namespace Weather
{
    // How long a shower and the dry spell between two of them last, in seconds,
    // how strong a shower rains, in e_rain_amount, and how fast it comes and goes,
    // a second. The rain of the engine was made for the little of it the one storm
    // of the game sets, 0.15 at most: more of it covers the picture in streaks and
    // everything near the camera, the weapon in the hands included, in splashes.
    // A level that rains starts in a shower, and the dry spells are the shorter.
    constexpr float ShowerShortest = 90.0f, ShowerLongest = 240.0f;
    constexpr float DryShortest = 20.0f, DryLongest = 60.0f;
    constexpr float WeakestShower = 0.08f, StrongestShower = 0.15f;
    constexpr float Fade = 0.01f;

    // The rain on the lens of a rain of the game: e_rain_amount is no measure of
    // it, the strongest rain of the game, FullGameRain, is LensRainOfFull of the
    // rain of the effect.
    constexpr float FullGameRain = 0.15f;
    constexpr float LensRainOfFull = 0.6f;

    inline float LensRain(float gameRain)
    {
        return std::clamp(gameRain / FullGameRain, 0.0f, 1.0f) * LensRainOfFull;
    }

    inline bool rains = false;
    inline bool showering = false;
    inline float left = 0.0f;
    inline float target = 0.0f;
    inline float current = 0.0f;
    // what was written to e_rain_amount last, to tell a storm of the game from it
    inline float written = -1.0f;
    inline bool storm = false;
    // a level was loaded since the last frame, see LoadLevel
    inline bool loaded = true;

    inline float Between(float a, float b)
    {
        return a + WaterDrops::GetRandomFloat(b - a);
    }

    // A level was loaded: whether it rains in it is decided again. e_rain_amount
    // is kept from a level to the next one, so the shower of the level before is
    // taken back first.
    inline void NewLevel(void* engine)
    {
        if (written >= 0.0f && !storm)
            Cry::Virtual<Cry::EngineSetRainAmount, void, float>(engine, 0.0f);

        const int chance = std::clamp(WaterDrops::SettingInt("MAIN.LevelRainChance", 30), 0, 100);
        rains = chance > 0 && WaterDrops::GetRandomFloat(100.0f) < (float)chance;
        showering = true;
        left = Between(ShowerShortest, ShowerLongest);
        target = Between(WeakestShower, StrongestShower);
        current = target;
        written = -1.0f;
        storm = false;
    }

    // the rain of the game, whichever of the two it is, for this frame
    inline float Update(void* engine, float gameRain, float dt)
    {
        if (loaded)
        {
            loaded = false;
            NewLevel(engine);
            gameRain = 0.0f;
        }

        // a rain that is not the one written here is a storm of the game
        if (written >= 0.0f && fabsf(gameRain - written) > 0.001f)
            storm = true;
        if (!rains || storm)
            return gameRain;

        left -= dt;
        if (left <= 0.0f)
        {
            showering = !showering;
            left = showering ? Between(ShowerShortest, ShowerLongest) : Between(DryShortest, DryLongest);
            target = showering ? Between(WeakestShower, StrongestShower) : 0.0f;
        }

        const float step = Fade * dt;
        current = current < target ? (std::min)(target, current + step) : (std::max)(target, current - step);

        Cry::Virtual<Cry::EngineSetRainAmount, void, float>(engine, current);
        written = current;
        return current;
    }

    // Whether the camera is under the water, and the rain the game asked for while
    // it is: the engine is given none until the camera comes up again, and then the
    // rain that was asked for last.
    inline bool submerged = false;
    inline float asked = 0.0f;

#ifdef _WIN64
    SafetyHookInline shSetRainAmount = {};
    void SetRainAmount(void* engine, float amount)
    {
        asked = amount;
        shSetRainAmount.unsafe_call(engine, submerged ? 0.0f : amount);
    }

    inline void SetEngineRain(void* engine, float amount)
    {
        shSetRainAmount.unsafe_call(engine, amount);
    }
#else
    SafetyHookInline shSetRainAmount = {};
    void __fastcall SetRainAmount(void* engine, void* edx, float amount)
    {
        asked = amount;
        shSetRainAmount.unsafe_thiscall(engine, submerged ? 0.0f : amount);
    }

    inline void SetEngineRain(void* engine, float amount)
    {
        shSetRainAmount.unsafe_thiscall(engine, amount);
    }
#endif

    // The camera went under the water or came up out of it: rain is the rain of
    // the game right then.
    inline void Submerge(void* engine, bool underwater, float rain)
    {
        if (underwater == submerged || !shSetRainAmount)
            return;

        submerged = underwater;
        if (submerged)
            asked = rain;
        SetEngineRain(engine, submerged ? 0.0f : asked);
    }

#ifdef _WIN64
    SafetyHookInline shLoadLevel = {};
    bool LoadLevel(void* engine, const char* folder, const char* mission, bool editor)
    {
        loaded = true;
        return shLoadLevel.unsafe_call<bool>(engine, folder, mission, editor);
    }
#else
    SafetyHookInline shLoadLevel = {};
    bool __fastcall LoadLevel(void* engine, void* edx, const char* folder, const char* mission, bool editor)
    {
        loaded = true;
        return shLoadLevel.unsafe_thiscall<bool>(engine, folder, mission, editor);
    }
#endif

    inline void Hook(void* engine)
    {
        if (!shLoadLevel)
            shLoadLevel = safetyhook::create_inline((*reinterpret_cast<void***>(engine))[Cry::EngineLoadLevel], LoadLevel);
        if (!shSetRainAmount)
            shSetRainAmount = safetyhook::create_inline((*reinterpret_cast<void***>(engine))[Cry::EngineSetRainAmount], SetRainAmount);
    }
}

// The way the camera of the game looks, from its angles, exactly as the camera
// builds the matrix the renderer draws with (CCamera::Update and CryViewMatrix of
// the SDK): the rows of the rotation are its right, its forward and its up, in
// the world of the game, which has z up. A camera of no angles looks down -y.
void UpdateCamera(const Cry::Camera* camera)
{
    constexpr float DegToRad = 3.14159265358979f / 180.0f;
    const float p = camera->anglesDeg.x * DegToRad;
    const float r = camera->anglesDeg.y * DegToRad;
    const float y = -(camera->anglesDeg.z * DegToRad + 3.14159265358979f);

    // RotX(p) * RotY(r) * RotZ(y)
    const float cp = cosf(p), sp = sinf(p);
    const float cr = cosf(r), sr = sinf(r);
    const float cy = cosf(y), sy = sinf(y);
    const float m[3][3] = {
        { cr * cy, -cr * sy, sr },
        { sp * sr * cy + cp * sy, -sp * sr * sy + cp * cy, -sp * cr },
        { -cp * sr * cy + sp * sy, cp * sr * sy + sp * cy, cp * cr },
    };

    // The effect takes the right of a camera the way RenderWare has it, pointing
    // to the left of the screen.
    WaterDrops::right = { -m[0][0], -m[0][1], -m[0][2] };
    WaterDrops::at = { m[1][0], m[1][1], m[1][2] };
    WaterDrops::up = { m[2][0], m[2][1], m[2][2] };
    WaterDrops::pos = { camera->position.x, camera->position.y, camera->position.z };
}

// How far under the surface of the water the lens is under it, in metres, and
// how long it has to have been under it, in seconds, to come up covered with
// water, see DrawDrops.
constexpr float UnderwaterDepth = 0.05f;
constexpr float SurfacingSplashAfter = 0.25f;

// The drops of a frame, drawn over the world the 3D engine rendered and under the
// HUD and the menus the game draws after it, see Init3DEngine.
void DrawDrops()
{
    if (!gSystem || !gDevice)
        return;

    auto* camera = Cry::Virtual<Cry::SystemGetViewCamera, Cry::Camera*>(gSystem);
    auto* engine = Cry::Virtual<Cry::SystemGet3DEngine, void*>(gSystem);
    auto* timer = Cry::Virtual<Cry::SystemGetTimer, void*>(gSystem);
    auto* console = Cry::Virtual<Cry::SystemGetConsole, void*>(gSystem);

    // the system renders no world before a camera is placed, see CSystem::Render
    if (!camera || !engine || !timer || !console)
        return;
    const auto& at = camera->position;
    if (fabsf(at.x) < 0.01f && fabsf(at.y) < 0.01f && fabsf(at.z) < 0.01f)
        return;

    if (gDevice->TestCooperativeLevel() != D3D_OK)
        return;

    static float timeStep = 0.0f;
    timeStep = Cry::Virtual<Cry::TimerGetFrameTime, float>(timer);
    WaterDrops::fTimeStep = &timeStep;

    UpdateCamera(camera);
    gCameraPos = camera->position;
    gHaveCamera = true;
    Water::Hook(engine);
    Weather::Hook(engine);

    // The rain of the game is e_rain_amount, which the weather of a level sets.
    // It rains on a lens that is outdoors, out of every area of the portals (the
    // test the game makes before it lets the rain of a storm be set, see
    // ApplyStormToEnvironment), and above the water.
    static void* rainAmount = nullptr;
    if (!rainAmount)
        rainAmount = Cry::Virtual<Cry::ConsoleGetCVar, void*, const char*, bool>(console, "e_rain_amount", true);

    // under the water the engine has no rain, and the rain is the one the game
    // asked for
    const float gameRain = Weather::submerged ? Weather::asked
        : rainAmount ? Cry::Virtual<Cry::CVarGetFVal, float>(rainAmount) : 0.0f;
    const float rain = Weather::Update(engine, gameRain, timeStep);
    const bool outdoors = Cry::Virtual<Cry::EngineGetVisAreaFromPos, void*, const Cry::Vec3*>(engine, &at) == nullptr;
    const float waterLevel = Cry::Virtual<Cry::EngineGetWaterLevel, float, const Cry::Vec3*, Cry::Vec3*>(engine, &at, nullptr);
    const bool underwater = at.z < waterLevel - UnderwaterDepth;
    Weather::Submerge(engine, underwater, rain);

    // A lens that goes under the water is washed clean, and one that comes up out
    // of it after a while is covered with the water it brings up: a camera of a
    // swimmer bobs through the surface, and is not splashed every time it does.
    static float underwaterSeconds = 0.0f;
    if (underwater)
    {
        WaterDrops::Clear();
        underwaterSeconds += timeStep;
        Water::rate = Water::burst = 0.0f;
        return;
    }

    Xrd::Init(XRD_DEVICE_RENDERER, gDevice);

    if (underwaterSeconds >= SurfacingSplashAfter)
        WaterDrops::FillScreenMoving(1.0f);
    underwaterSeconds = 0.0f;

    WaterDrops::ms_rainIntensity = outdoors ? Weather::LensRain(rain) : 0.0f;
    WaterDrops::Process();
    Water::Apply();
    WaterDrops::Render();
    WaterDrops::ms_rainIntensity = 0.0f;
}

SafetyHookInline shCreateSystemInterface = {};
void* __cdecl CreateSystemInterface(void* initParams)
{
    auto* system = shCreateSystemInterface.unsafe_ccall<void*>(initParams);

    if (system && !gSystem)
        gSystem = system;

    return system;
}

// The renderer of the game makes a pure device where the driver offers one, and
// a pure device does not tell what state it is in: the drops could not put back
// what they change, and the renderer, which remembers what it set, would draw
// with the wrong state after them. The device is made an ordinary one.
SafetyHookInline shDeviceReset = {};
HRESULT STDMETHODCALLTYPE DeviceReset(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters)
{
    // what the drops hold of the device has to go before a reset
    WaterDrops::Reset();
    return shDeviceReset.unsafe_stdcall<HRESULT>(pDevice, pPresentationParameters);
}

SafetyHookInline shCreateDevice = {};
HRESULT STDMETHODCALLTYPE CreateDevice(IDirect3D9* pD3D, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface)
{
    // a new device replaces the one before it, and what the drops made on that one
    // goes with it
    if (gDevice)
    {
        WaterDrops::Reset();
        Xrd::Shutdown();
        gDevice = nullptr;
    }

    BehaviorFlags &= ~D3DCREATE_PUREDEVICE;
    auto hr = shCreateDevice.unsafe_stdcall<HRESULT>(pD3D, Adapter, DeviceType, hFocusWindow, BehaviorFlags,
        pPresentationParameters, ppReturnedDeviceInterface);

    if (SUCCEEDED(hr) && ppReturnedDeviceInterface && *ppReturnedDeviceInterface)
    {
        gDevice = *ppReturnedDeviceInterface;
        if (!shDeviceReset)
            shDeviceReset = safetyhook::create_inline((*reinterpret_cast<void***>(gDevice))[16], DeviceReset);
    }

    return hr;
}

SafetyHookInline shDirect3DCreate9 = {};
IDirect3D9* WINAPI Direct3DCreate9Hook(UINT SDKVersion)
{
    auto* pD3D = shDirect3DCreate9.unsafe_stdcall<IDirect3D9*>(SDKVersion);

    if (pD3D && !shCreateDevice)
        shCreateDevice = safetyhook::create_inline((*reinterpret_cast<void***>(pD3D))[16], CreateDevice);

    return pD3D;
}

void InitCrySystem()
{
    static std::once_flag flag;
    std::call_once(flag, []()
    {
        WaterDrops::ReadSettings();

        auto pCreateSystemInterface = GetProcAddress(GetModuleHandleW(L"CrySystem.dll"), "CreateSystemInterface");
        if (pCreateSystemInterface)
            shCreateSystemInterface = safetyhook::create_inline(pCreateSystemInterface, CreateSystemInterface);
    });
}

// C3DEngine::Draw, where its scene is rendered, whichever way it was (all of the
// ways end in the same place, before the frame is captured to a file): the drops
// go on the world there, before what is drawn over the world of the game after it.
// The Far Cry Widescreen Fix draws there as well (its anti-aliasing, its gamma
// and the borders of the cutscenes), from a hook of its own on the very same
// place, which it finds by the code that is there. The place is found here by the
// code before it, which neither hook changes, so the two find it in any order;
// the fix loads first, and a hook made over its hook runs before it, so the drops
// are drawn before its anti-aliasing, its gamma and its borders as well.
void Init3DEngine()
{
    static std::once_flag flag;
    std::call_once(flag, []()
    {
#ifdef _WIN64
        auto pattern = hook::module_pattern(GetModuleHandleW(L"Cry3DEngine.dll"), "80 38 00 0F 85 ? ? ? ? BA FF FF FF FF 48 8B CD E8");
        constexpr ptrdiff_t SceneRendered = 22;
#else
        auto pattern = hook::module_pattern(GetModuleHandleW(L"Cry3DEngine.dll"), "33 DB EB 08 6A FF 55 E8");
        constexpr ptrdiff_t SceneRendered = 12;
#endif
        if (pattern.empty())
            return;

        static auto SceneRenderedHook = safetyhook::create_mid(pattern.get_first(SceneRendered), [](SafetyHookContext&)
        {
            DrawDrops();
        });
    });
}

void InitD3D9()
{
    static std::once_flag flag;
    std::call_once(flag, []()
    {
        auto pDirect3DCreate9 = GetProcAddress(GetModuleHandleW(L"d3d9.dll"), "Direct3DCreate9");
        if (pDirect3DCreate9)
            shDirect3DCreate9 = safetyhook::create_inline(pDirect3DCreate9, Direct3DCreate9Hook);
    });
}

extern "C" __declspec(dllexport) void InitializeASI()
{
    std::call_once(CallbackHandler::flag, []()
    {
        CallbackHandler::RegisterCallback(L"CrySystem.dll", InitCrySystem);
        CallbackHandler::RegisterCallback(L"Cry3DEngine.dll", Init3DEngine);
        CallbackHandler::RegisterCallback(L"d3d9.dll", InitD3D9);
    });
}

BOOL APIENTRY DllMain(HMODULE /*hModule*/, DWORD reason, LPVOID /*lpReserved*/)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        if (!IsUALPresent()) { InitializeASI(); }
    }
    return TRUE;
}
