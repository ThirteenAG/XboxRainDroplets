// Direct3D 8, the API this game uses
#define XRD_ENABLE_D3D8
#include "xrd/xrd.h"

inline void RegisterWidescreenResetCallback(const wchar_t* moduleName)
{
    static bool registered = false;
    if (registered) return;

    const HMODULE module = GetModuleHandleW(moduleName);
    if (!module) return;
    using RegisterCallback = BOOL (__cdecl*)(void (__cdecl*)());
    const auto registerCallback = reinterpret_cast<RegisterCallback>(GetProcAddress(module, "RegisterBeforeResetCallback"));
    if (!registerCallback) return;

    registered = registerCallback(+[]() { WaterDrops::Reset(); }) != FALSE;
}

// ---------------------------------------------------------------------------
// GTA Vice City 1.0. What the drops answer to is the original effect, see the
// Neo water drops of skygfx: the same functions of the game are hooked and the
// same particle types are turned into drops. How the drops are drawn is not in
// this file, that is the renderer in source/xrd, which WaterDrops drives.
// ---------------------------------------------------------------------------

// the particle types of this game, see tSplashParticles in the original
enum tSplashParticles
{
    PARTICLE_WATER_SPARK = 2,
    PARTICLE_BLOOD = 6,
    PARTICLE_BLOOD_SMALL = 7,
    PARTICLE_BLOOD_SPURT = 8,
    PARTICLE_WATER = 12,
    PARTICLE_SPLASH = 26,
    PARTICLE_RAINDROP = 34,
    PARTICLE_RAINDROP_SMALL = 35,
    PARTICLE_RAIN_SPLASH = 36,
    PARTICLE_RAIN_SPLASH_BIGGROW = 37,
    PARTICLE_RAIN_SPLASHUP = 38,
    PARTICLE_WATERSPRAY = 39,
    PARTICLE_WATERDROP = 40,
    PARTICLE_BLOODDROP = 41,
    PARTICLE_CAR_SPLASH = 46,
    PARTICLE_BOAT_SPLASH = 47,
    PARTICLE_BOAT_THRUSTJET = 48,
    PARTICLE_WATER_HYDRANT = 49,
    PARTICLE_WATER_CANNON = 50,
    PARTICLE_PED_SPLASH = 52,
    PARTICLE_RAINDROP_2D = 80
};

// the camera modes, the drops are not visible in the first few
enum
{
    CAM_TOPDOWN1 = 1,
    CAM_TOPDOWN2 = 2,
    CAM_FIRSTPERSON = 16,
    CAM_TOPDOWNPED = 37
};

// one of the two functions that add a particle takes the colour of the particle
struct RwRGBA
{
    uint8_t r, g, b, a;
};

// the length of a splash, in frames of the game, and the distance from the
// camera the original effect lets it go
constexpr auto SPLASH_DURATION = 14;
constexpr auto SPLASH_DISTANCE = 20.0f;

// ---------------------------------------------------------------------------
// The game, the addresses are the ones of GTA Vice City 1.0
// ---------------------------------------------------------------------------

float& CTimer__ms_fTimeStep = *(float*)0x975424;	// CTimer::ms_fTimeStep
// The game counts a frame in frames of 50 Hz, the effect wants seconds per
// frame, which is what this holds, see where it is set.
static float timeStepSeconds = 0.0f;
float& CWeather__Rain = *(float*)0x975340;		// CWeather::Rain
bool& CCutsceneMgr__ms_running = *(bool*)0xA10AB2;	// CCutsceneMgr::ms_running
uint8_t* TheCamera = (uint8_t*)0x7E4688;		// TheCamera
uint8_t* Scene = (uint8_t*)0x8100B8;			// Scene, its camera is the second member
void** ppRwD3DDevice = (void**)0x7897A8;		// RwD3DDevice

static auto CCullZones__CamNoRain = (bool(__cdecl*)())0x57E0E0;
static auto CCullZones__PlayerNoRain = (bool(__cdecl*)())0x57E0C0;
static auto FindPlayerVehicle = (bool(__cdecl*)())0x4BC1E0;

struct CPad;

static auto CPad__GetPad = (CPad * (__cdecl*)(int))0x4AB060;
static bool CPad__GetLookBehindForCar(CPad* pad) { return ((bool(__thiscall*)(CPad*))0x4AAC30)(pad); }
static bool CPad__GetLookLeft(CPad* pad) { return ((bool(__thiscall*)(CPad*))0x4AAC90)(pad); }
static bool CPad__GetLookRight(CPad* pad) { return ((bool(__thiscall*)(CPad*))0x4AAC60)(pad); }

// CParticleObject::ms_aAudioHydrants, the eight hydrants of the map spray as well
struct AudioHydrant
{
    int entity;
    void* particleObject;
};
static AudioHydrant* audioHydrants = (AudioHydrant*)0x70799C;

// CParticleObject is a CPlaceable, its matrix is the first thing it has and the
// position of that matrix is the position of the object
constexpr auto PLACEABLE_POSITION = 0x30;

static bool NoRain()
{
    return CCullZones__CamNoRain() || CCullZones__PlayerNoRain();
}

// the mode of the camera that is in use
static short GetCamMode()
{
    const uint8_t active = *(TheCamera + 0x76);
    const uint8_t* cam = TheCamera + 0x188 + active * 0x1CC;

    return *(short*)(cam + 0xC);
}

// A camera the drops are not visible in: looking straight down, or looking
// around inside a car, is one where they would sit in the middle of the view,
// so the original effect stops them there.
static bool CameraSeesDrops()
{
    const short mode = GetCamMode();

    if (mode == CAM_TOPDOWN1 || mode == CAM_TOPDOWN2 || mode == CAM_TOPDOWNPED)
        return false;

    if (mode == CAM_FIRSTPERSON && FindPlayerVehicle())
    {
        CPad* pad = CPad__GetPad(0);

        if (pad && (CPad__GetLookBehindForCar(pad) || CPad__GetLookLeft(pad) || CPad__GetLookRight(pad)))
            return false;
    }

    return true;
}

// the modelling matrix of the camera the game draws the world with, it is the
// matrix of the frame the camera is attached to
static RwMatrix* GetCameraMatrix()
{
    const uint8_t* camera = *(uint8_t**)(Scene + 4);
    if (!camera)
        return nullptr;

    const uint8_t* frame = *(uint8_t**)(camera + 4);
    if (!frame)
        return nullptr;

    return (RwMatrix*)(frame + 0x10);
}

static bool IsParticleBlood(int id)
{
    switch (id)
    {
        case PARTICLE_BLOOD:
        case PARTICLE_BLOOD_SMALL:
        case PARTICLE_BLOOD_SPURT:
        case PARTICLE_BLOODDROP:
            return true;
    }

    return false;
}

static bool IsParticleSplash(int id)
{
    switch (id)
    {
        case PARTICLE_BOAT_SPLASH:
        case PARTICLE_CAR_SPLASH:
        case PARTICLE_PED_SPLASH:
        case PARTICLE_RAIN_SPLASH:
        case PARTICLE_RAIN_SPLASHUP:
        case PARTICLE_RAIN_SPLASH_BIGGROW:
        case PARTICLE_SPLASH:
            return true;
    }

    return false;
}

static float GetParticleDistance(int id)
{
    switch (id)
    {
        case PARTICLE_WATER_SPARK:		return 5.0f;
        case PARTICLE_BLOOD:			return 5.0f;
        case PARTICLE_BLOOD_SMALL:		return 5.0f;
        case PARTICLE_BLOOD_SPURT:		return 5.0f;
        case PARTICLE_WATER:			return 20.0f;
        case PARTICLE_SPLASH:			return 10.0f;
        case PARTICLE_RAINDROP:			return 5.0f;
        case PARTICLE_RAINDROP_SMALL:		return 5.0f;
        case PARTICLE_RAIN_SPLASH:		return 5.0f;
        case PARTICLE_RAIN_SPLASH_BIGGROW:	return 5.0f;
        case PARTICLE_RAIN_SPLASHUP:		return 5.0f;
        case PARTICLE_WATERSPRAY:		return 20.0f;
        case PARTICLE_WATERDROP:		return 20.0f;
        case PARTICLE_BLOODDROP:		return 5.0f;
        case PARTICLE_CAR_SPLASH:		return 12.0f;
        case PARTICLE_BOAT_SPLASH:		return 40.0f;
        case PARTICLE_BOAT_THRUSTJET:		return 20.0f;
        case PARTICLE_WATER_HYDRANT:		return 10.0f;
        case PARTICLE_WATER_CANNON:		return 20.0f;
        case PARTICLE_PED_SPLASH:		return 10.0f;
        case PARTICLE_RAINDROP_2D:		return 5.0f;
        default:				return 20.0f;
    }
}

// A particle the game adds. The ones that are water or blood put drops on the
// screen, the ones that are a splash keep them coming around the place the
// splash happened at.
static void AddDroplets(int particleType, RwV3d const& posn)
{
    RwV3d dist;
    RwV3dSub(&dist, (RwV3d*)&posn, &WaterDrops::ms_lastPos);
    const float len = RwV3dLength(&dist);

    if (len > GetParticleDistance(particleType))
        return;

    if (IsParticleSplash(particleType))
    {
        WaterDrops::RegisterSplash((RwV3d*)&posn, SPLASH_DISTANCE, SPLASH_DURATION);
        return;
    }

    const bool isBlood = IsParticleBlood(particleType);
    if (isBlood && !WaterDrops::bBloodDrops)
        return;

    WaterDrops::FillScreenMoving(1.0f / (len / 2.0f), isBlood);
}

// the hydrants of the map spray while the camera is near them
static void SprayHydrants()
{
    for (int i = 0; i < 8; i++)
    {
        AudioHydrant* hydrant = &audioHydrants[i];
        if (!hydrant->particleObject)
            continue;

        RwV3d dist;
        RwV3dSub(&dist, (RwV3d*)((uint8_t*)hydrant->particleObject + PLACEABLE_POSITION), &WaterDrops::ms_lastPos);

        if (RwV3dDotProduct(&dist, &dist) <= 40.0f)
            WaterDrops::FillScreenMoving(1.0f);
    }
}

// ---------------------------------------------------------------------------
// The functions of the game the drops hang on
// ---------------------------------------------------------------------------

// CRenderer::RenderEffects, the last thing the world is drawn with
static injector::hook_back<void(*)()> renderEffects;
static void RenderEffectsHook()
{
    renderEffects.fun();

    const RwMatrix* matrix = GetCameraMatrix();
    if (!matrix)
        return;

    WaterDrops::right = matrix->right;
    WaterDrops::up = matrix->up;
    WaterDrops::at = matrix->at;
    WaterDrops::pos = matrix->pos;

    // the rain of the game, the drops of the effect are gone where it does not
    // rain at all
    if (NoRain() || CWeather__Rain <= 0.0f)
        WaterDrops::ms_rainIntensity = 0.0f;
    else if (CameraSeesDrops())
        WaterDrops::ms_rainIntensity = CWeather__Rain;
    else
        WaterDrops::ms_rainIntensity = 0.0f;

    // CTimer::ms_fTimeStep counts in frames of 50 Hz, a second is 50 of them,
    // and the effect wants seconds per frame
    timeStepSeconds = CTimer__ms_fTimeStep / 50.0f;

    RegisterWidescreenResetCallback(L"GTAVC.WidescreenFix.asi");
    Xrd::Init(XRD_DEVICE_RENDERER, *ppRwD3DDevice);
    WaterDrops::Process();

    if (!CCutsceneMgr__ms_running)
    {
        WaterDrops::Render();
        SprayHydrants();
    }
}

// Run during game initialization, after the original lifecycle call.
static void InitialiseWaterDrops()
{
    auto device = static_cast<IDirect3DDevice8*>(*ppRwD3DDevice);
    if (!device || FAILED(device->TestCooperativeLevel())) return;
    RegisterWidescreenResetCallback(L"GTAVC.WidescreenFix.asi");
    if (!Xrd::Init(XRD_DEVICE_RENDERER, device)) return;
    WaterDrops::Init();

}

// the game starts a new game or a new save, the drops of the old one are gone
#define RESET_HOOK(n) \
    static injector::hook_back<void(*)()> resetCall##n; \
    static void ResetHook##n() { resetCall##n.fun(); WaterDrops::Reset(); }

static injector::hook_back<void(*)()> resetCall1;
static void ResetHook1() { resetCall1.fun(); WaterDrops::Reset(); InitialiseWaterDrops(); }
static injector::hook_back<void(*)()> resetCall2;
static void ResetHook2() { resetCall2.fun(); WaterDrops::Reset(); InitialiseWaterDrops(); }
RESET_HOOK(3)
RESET_HOOK(4)
RESET_HOOK(5)

// CPlaceable::AddParticle, in the two forms the game has of it
static injector::hook_back<void(__cdecl*)(int, RwV3d const&, RwV3d const&, void*, float, int, int, int, int)> addParticle1;
static void __cdecl AddParticleHook1(int particleType, RwV3d const& posn, RwV3d const& direction, void* entity, float size, int rotationSpeed, int rotation, int startFrame, int lifeSpan)
{
    addParticle1.fun(particleType, posn, direction, entity, size, rotationSpeed, rotation, startFrame, lifeSpan);
    AddDroplets(particleType, posn);
}

static injector::hook_back<void(__cdecl*)(int, RwV3d const&, RwV3d const&, void*, float, RwRGBA const&, int, int, int, int)> addParticle2;
static void __cdecl AddParticleHook2(int particleType, RwV3d const& posn, RwV3d const& direction, void* entity, float size, RwRGBA const& color, int rotationSpeed, int rotation, int startFrame, int lifeSpan)
{
    addParticle2.fun(particleType, posn, direction, entity, size, color, rotationSpeed, rotation, startFrame, lifeSpan);
    AddDroplets(particleType, posn);
}

// The game jumps to the code of a splash from here and keeps the object the
// splash belongs to in ebp, which is the argument the call below gets, see the
// original effect.
uintptr_t splashBreak = 0;

static void __cdecl RegisterSplashFromEbp(void* entity)
{
    WaterDrops::RegisterSplash((RwV3d*)((uint8_t*)entity + PLACEABLE_POSITION), SPLASH_DISTANCE, SPLASH_DURATION);
}

static void(__cdecl* pRegisterSplashFromEbp)(void*) = RegisterSplashFromEbp;

static void __declspec(naked) SplashHook()
{
    __asm
    {
        push	ebp
        call	dword ptr[pRegisterSplashFromEbp]
        pop	ebp
        mov	eax, splashBreak
        jmp	eax
    }
}

void Init()
{
    WaterDrops::ReadIniSettings();
    // the effect moves with the time of the game, which is a value of its own,
    // see timeStepSeconds
    WaterDrops::fTimeStep = &timeStepSeconds;

    // the game draws its own drops of the old effect as well, the effect here
    // replaces them
    injector::MakeNOP(0x560D63, 5, true);
    injector::MakeNOP(0x560EE3, 5, true);

    renderEffects.fun = injector::MakeCALL(0x4A604F, RenderEffectsHook, true).get();

    static void(*resetHooks[5])() = { ResetHook1, ResetHook2, ResetHook3, ResetHook4, ResetHook5 };
    static const uintptr_t resetCalls[5] = { 0x4A4DD6, 0x4A48EA, 0x42BCD6, 0x42C0BC, 0x42C318 };	// CGame::Initialise, CGame::ReInitGameObjectVariables, CGameLogic::Update

    resetCall1.fun = injector::MakeCALL(resetCalls[0], resetHooks[0], true).get();
    resetCall2.fun = injector::MakeCALL(resetCalls[1], resetHooks[1], true).get();
    resetCall3.fun = injector::MakeCALL(resetCalls[2], resetHooks[2], true).get();
    resetCall4.fun = injector::MakeCALL(resetCalls[3], resetHooks[3], true).get();
    resetCall5.fun = injector::MakeCALL(resetCalls[4], resetHooks[4], true).get();

    splashBreak = (uintptr_t)injector::GetBranchDestination(0x4E8721).get<void>();
    injector::MakeRelativeOffset(0x4E8721 + 1, (void*)SplashHook, 4, true);

    // the particles of the game, the ones commented out in the original are
    // the rain, which the effect already handles on its own
    addParticle1.fun = injector::MakeCALL(0x004FF238, AddParticleHook1, true).get();	// blood spurt
    injector::MakeCALL(0x00525AE4, AddParticleHook1, true);					// blood small
    injector::MakeCALL(0x00527D3F, AddParticleHook1, true);					// blood spurt
    injector::MakeCALL(0x00527D5F, AddParticleHook1, true);					// blood spurt
    injector::MakeCALL(0x00527D8A, AddParticleHook1, true);					// blood spurt
    injector::MakeCALL(0x00527DAA, AddParticleHook1, true);					// blood spurt
    injector::MakeCALL(0x00527EB8, AddParticleHook1, true);					// blood
    injector::MakeCALL(0x0052A4B3, AddParticleHook1, true);					// blood
    injector::MakeCALL(0x0057B794, AddParticleHook1, true);					// car splash
    injector::MakeCALL(0x005B4A1C, AddParticleHook1, true);					// blood small
    injector::MakeCALL(0x005C41A2, AddParticleHook1, true);					// blood small
    injector::MakeCALL(0x005C9E85, AddParticleHook1, true);					// blood
    injector::MakeCALL(0x005CA073, AddParticleHook1, true);					// boat splash
    injector::MakeCALL(0x005CBD73, AddParticleHook1, true);					// blood
    injector::MakeCALL(0x005CE46F, AddParticleHook1, true);					// blood small
    injector::MakeCALL(0x005CE55B, AddParticleHook1, true);					// blood small
    injector::MakeCALL(0x005CF648, AddParticleHook1, true);					// blood small
    injector::MakeCALL(0x005CF88E, AddParticleHook1, true);					// blood small
    injector::MakeCALL(0x005D3410, AddParticleHook1, true);					// blood
    injector::MakeCALL(0x005D343A, AddParticleHook1, true);					// blood
    injector::MakeCALL(0x005D3464, AddParticleHook1, true);					// blood
    injector::MakeCALL(0x005D3509, AddParticleHook1, true);					// blood
    injector::MakeCALL(0x005D35A3, AddParticleHook1, true);					// blood
    injector::MakeCALL(0x005D368B, AddParticleHook1, true);					// blood small
    injector::MakeCALL(0x005D36F1, AddParticleHook1, true);					// blood
    injector::MakeCALL(0x005D3757, AddParticleHook1, true);					// blood
    injector::MakeCALL(0x005D3A40, AddParticleHook1, true);					// blood spurt
    injector::MakeCALL(0x005D3A6A, AddParticleHook1, true);					// blood spurt
    injector::MakeCALL(0x005D3A94, AddParticleHook1, true);					// blood spurt

    addParticle2.fun = injector::MakeCALL(0x004E2C50, AddParticleHook2, true).get();	// boat splash
    injector::MakeCALL(0x004E2F5D, AddParticleHook2, true);					// boat splash
    injector::MakeCALL(0x004E30A3, AddParticleHook2, true);					// boat splash
    injector::MakeCALL(0x004E3353, AddParticleHook2, true);					// boat splash
    injector::MakeCALL(0x004E3516, AddParticleHook2, true);					// boat splash
    injector::MakeCALL(0x004E365C, AddParticleHook2, true);					// boat splash
    injector::MakeCALL(0x004E599C, AddParticleHook2, true);					// car splash
    injector::MakeCALL(0x004E5B6D, AddParticleHook2, true);					// car splash
    injector::MakeCALL(0x004E5D3E, AddParticleHook2, true);					// car splash
    injector::MakeCALL(0x004E5EE5, AddParticleHook2, true);					// car splash
    injector::MakeCALL(0x004E60BE, AddParticleHook2, true);					// car splash
    injector::MakeCALL(0x004E6222, AddParticleHook2, true);					// car splash
    injector::MakeCALL(0x004E6380, AddParticleHook2, true);					// car splash
    injector::MakeCALL(0x004E64B9, AddParticleHook2, true);					// car splash
    injector::MakeCALL(0x004E6761, AddParticleHook2, true);					// car splash
    injector::MakeCALL(0x004E6928, AddParticleHook2, true);					// car splash
    injector::MakeCALL(0x004E6AEF, AddParticleHook2, true);					// car splash
    injector::MakeCALL(0x004E6C98, AddParticleHook2, true);					// car splash
    injector::MakeCALL(0x004E6E43, AddParticleHook2, true);					// car splash
    injector::MakeCALL(0x004E6F7C, AddParticleHook2, true);					// car splash
    injector::MakeCALL(0x004E70AF, AddParticleHook2, true);					// car splash
    injector::MakeCALL(0x004E71BD, AddParticleHook2, true);					// car splash
    injector::MakeCALL(0x004E7566, AddParticleHook2, true);					// car splash
    injector::MakeCALL(0x004E769C, AddParticleHook2, true);					// car splash
    injector::MakeCALL(0x004E77D2, AddParticleHook2, true);					// car splash
    injector::MakeCALL(0x004E78F5, AddParticleHook2, true);					// car splash
    injector::MakeCALL(0x004E7B20, AddParticleHook2, true);					// splash
    injector::MakeCALL(0x004E7C80, AddParticleHook2, true);					// splash
    injector::MakeCALL(0x004E7DE0, AddParticleHook2, true);					// splash
    injector::MakeCALL(0x004E7F2D, AddParticleHook2, true);					// splash
    injector::MakeCALL(0x004E8023, AddParticleHook2, true);					// car splash
    injector::MakeCALL(0x005047D7, AddParticleHook2, true);					// car splash
    injector::MakeCALL(0x0050489F, AddParticleHook2, true);					// car splash
    injector::MakeCALL(0x00509CB1, AddParticleHook2, true);					// rain splash big grow
    injector::MakeCALL(0x005620A0, AddParticleHook2, true);					// car splash
    injector::MakeCALL(0x00562BB5, AddParticleHook2, true);					// waterdrop
    injector::MakeCALL(0x00563177, AddParticleHook2, true);					// rain splash
    injector::MakeCALL(0x005632D9, AddParticleHook2, true);					// rain splash
    injector::MakeCALL(0x0058F514, AddParticleHook2, true);					// car splash
    injector::MakeCALL(0x0058FAE1, AddParticleHook2, true);					// car splash
    injector::MakeCALL(0x00590D3C, AddParticleHook2, true);					// car splash
    injector::MakeCALL(0x0059134C, AddParticleHook2, true);					// car splash
    injector::MakeCALL(0x0059A500, AddParticleHook2, true);					// rain splash big grow
    injector::MakeCALL(0x0059A97C, AddParticleHook2, true);					// ped splash
    injector::MakeCALL(0x005A1D80, AddParticleHook2, true);					// boat splash
    injector::MakeCALL(0x005A1E76, AddParticleHook2, true);					// car splash
    injector::MakeCALL(0x005A365B, AddParticleHook2, true);					// car splash
    injector::MakeCALL(0x005A3767, AddParticleHook2, true);					// boat splash
    injector::MakeCALL(0x005A41EB, AddParticleHook2, true);					// car splash
    injector::MakeCALL(0x005A42F7, AddParticleHook2, true);					// boat splash
    injector::MakeCALL(0x005A47CF, AddParticleHook2, true);					// waterdrop
    injector::MakeCALL(0x005D3989, AddParticleHook2, true);					// blooddrop
}

extern "C" __declspec(dllexport) void InitializeASI()
{
    std::call_once(CallbackHandler::flag, []()
    {
        CallbackHandler::RegisterCallback(Init);
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
