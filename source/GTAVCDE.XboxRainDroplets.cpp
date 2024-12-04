#include "xrd11.h"
#include <injector/injector.hpp>
#include <safetyhook.hpp>
#include <utility/Scan.hpp>

#define FUSIONDXHOOK_INCLUDE_D3D12    1
#define FUSIONDXHOOK_USE_SAFETYHOOK   0
#include "FusionDxHook.h"

enum tParticleType
{
    PARTICLE_SPARK = 0,
    PARTICLE_SPARK_SMALL,
    PARTICLE_WATER_SPARK,
    PARTICLE_WHEEL_DIRT,
    PARTICLE_SAND,
    PARTICLE_WHEEL_WATER,
    PARTICLE_BLOOD,
    PARTICLE_BLOOD_SMALL,
    PARTICLE_BLOOD_SPURT,
    PARTICLE_DEBRIS,
    PARTICLE_DEBRIS2,
    PARTICLE_FLYERS,
    PARTICLE_WATER,
    PARTICLE_FLAME,
    PARTICLE_FIREBALL,
    PARTICLE_GUNFLASH,
    PARTICLE_GUNFLASH_NOANIM,
    PARTICLE_GUNSMOKE,
    PARTICLE_GUNSMOKE2,
    PARTICLE_CIGARETTE_SMOKE,
    PARTICLE_SMOKE,
    PARTICLE_SMOKE_SLOWMOTION,
    PARTICLE_DRY_ICE,
    PARTICLE_TEARGAS,
    PARTICLE_GARAGEPAINT_SPRAY,
    PARTICLE_SHARD,
    PARTICLE_SPLASH,
    PARTICLE_CARFLAME,
    PARTICLE_STEAM,
    PARTICLE_STEAM2,
    PARTICLE_STEAM_NY,
    PARTICLE_STEAM_NY_SLOWMOTION,
    PARTICLE_GROUND_STEAM,
    PARTICLE_ENGINE_STEAM,
    PARTICLE_RAINDROP,
    PARTICLE_RAINDROP_SMALL,
    PARTICLE_RAIN_SPLASH,
    PARTICLE_RAIN_SPLASH_BIGGROW,
    PARTICLE_RAIN_SPLASHUP,
    PARTICLE_WATERSPRAY,
    PARTICLE_EXPLOSION_MEDIUM,
    PARTICLE_EXPLOSION_LARGE,
    PARTICLE_EXPLOSION_MFAST,
    PARTICLE_EXPLOSION_LFAST,
    PARTICLE_CAR_SPLASH,
    PARTICLE_BOAT_SPLASH,
    PARTICLE_BOAT_THRUSTJET,
    PARTICLE_WATER_HYDRANT,
    PARTICLE_WATER_CANNON,
    PARTICLE_EXTINGUISH_STEAM,
    PARTICLE_PED_SPLASH,
    PARTICLE_PEDFOOT_DUST,
    PARTICLE_CAR_DUST,
    PARTICLE_HELI_DUST,
    PARTICLE_HELI_ATTACK,
    PARTICLE_ENGINE_SMOKE,
    PARTICLE_ENGINE_SMOKE2,
    PARTICLE_CARFLAME_SMOKE,
    PARTICLE_FIREBALL_SMOKE,
    PARTICLE_PAINT_SMOKE,
    PARTICLE_TREE_LEAVES,
    PARTICLE_CARCOLLISION_DUST,
    PARTICLE_CAR_DEBRIS,
    PARTICLE_BIRD_DEBRIS,
    PARTICLE_HELI_DEBRIS,
    PARTICLE_EXHAUST_FUMES,
    PARTICLE_RUBBER_SMOKE,
    PARTICLE_BURNINGRUBBER_SMOKE,
    PARTICLE_BULLETHIT_SMOKE,
    PARTICLE_GUNSHELL_FIRST,
    PARTICLE_GUNSHELL,
    PARTICLE_GUNSHELL_BUMP1,
    PARTICLE_GUNSHELL_BUMP2,
    PARTICLE_ROCKET_SMOKE,
    PARTICLE_THROWN_FLAME,
    PARTICLE_SWIM_SPLASH,
    PARTICLE_SWIM_WAKE,
    PARTICLE_SWIM_WAKE2,
    PARTICLE_HELI_WATER_DROP,
    PARTICLE_BALLOON_EXP,
    PARTICLE_AUDIENCE_FLASH,
    PARTICLE_TEST,
    PARTICLE_BIRD_FRONT,
    PARTICLE_SHIP_SIDE,
    PARTICLE_BEASTIE,
    PARTICLE_RAINDROP_2D,
    PARTICLE_FERRY_CHIM_SMOKE,
    PARTICLE_MULTIPLAYER_HIT,
    PARTICLE_HYDRANT_STEAM,
    PARTICLE_FLOOR_HIT,
    PARTICLE_BLOODDROP,
    PARTICLE_HEATHAZE,
    PARTICLE_HEATHAZE_IN_DIST,
    PARTICLE_WATERDROP,

    MAX_PARTICLES,
    PARTICLE_FIRST = PARTICLE_SPARK,
    PARTICLE_LAST = PARTICLE_HYDRANT_STEAM
};

namespace CWeather
{
    GameRef<float> Rain;
}

namespace CCutsceneMgr
{
    GameRef<bool> ms_running;
}

namespace CGame
{
    GameRef<int> currArea;
}

namespace CTimer
{
    GameRef<bool> m_CodePause;
    GameRef<bool> m_UserPause;
}

namespace CCullZones
{
    GameRef<uint32_t> CurrentFlags_Camera;
    bool CamNoRain()
    {
        return (CurrentFlags_Camera & 8) != 0;
    }

    GameRef<uint32_t> CurrentFlags_Player;
    bool PlayerNoRain()
    {
        return (CurrentFlags_Player & 8) != 0;
    }
}

struct CCamera
{
    char unk[0x860];
    RwMatrix m_mCameraMatrix;
};

GameRef<CCamera> TheCamera;
GameRef<int> gGameState;

bool NoDrops()
{
    return false;
}

bool NoRain()
{
    return CCullZones::CamNoRain() || CCullZones::PlayerNoRain() || CGame::currArea != 0 || NoDrops();
}

void SprayDroplets(RwV3d* position, float pd = 20.0f, bool isBlood = false)
{
    RwV3d dist;
    RwV3dSub(&dist, position, &WaterDrops::ms_lastPos);
    float len = RwV3dLength(&dist);
    if (len <= pd)
        WaterDrops::FillScreenMoving((1.0f / (len / 2.0f)) * 100.0f, isBlood);
}

thread_local std::string cachedPrt;
thread_local std::map<void*, std::string> prtMap;

namespace FxManager_c
{
    SafetyHookInline shCreateFxSystem{};
    void CreateFxSystem(void* a1, void* name, int a3, int id)
    {
        shCreateFxSystem.fastcall(a1, name, a3, id);
        cachedPrt = *(const char**)name; // FString
    }
}

bool bGotoAddParticle = false;
namespace UParticleSystemComponent
{
    SafetyHookInline shResetParticles{};
    void* ResetParticles(void* UParticleSystemComponent, void* a2, void* a3, void* a4)
    {
        auto res = shResetParticles.fastcall<void*>(UParticleSystemComponent, a2, a3, a4);
        prtMap[res] = std::move(cachedPrt);

        bGotoAddParticle = false;
        RwV3d* position = (RwV3d*)a3;
        if (position->x == 0.0f && position->y == 0.0f && position->z == 0.0f)
        {
            bGotoAddParticle = true;
        }
        else
        {
            std::string_view name = prtMap[res];
            if (name == "water_swim")
            {
                SprayDroplets((RwV3d*)a3, 10.0f, false);
            }
            if (name == "water_splsh_sml")
            {
                SprayDroplets((RwV3d*)a3, 2.0f, false);
            }
            else if (name == "water_splash")
            {
                SprayDroplets((RwV3d*)a3, 5.0f, false);
            }
            else if (name == "water_splash_big")
            {
                SprayDroplets((RwV3d*)a3, 20.0f, false);
            }
            else if (name == "water_hydrant")
            {
                WaterDrops::RegisterSplash((RwV3d*)a3, 10.0f, 600, 100.0f);
            }
        }

        return res;
    }
}

namespace FxSystem_c
{
    SafetyHookInline shAddParticle{};
    void AddParticle(void* a1, RwV3d* position, void* a3, float a4, int* a5)
    {
        //if (bGotoAddParticle)
        {
            std::string_view name = prtMap[a1];

            if (name == "prt_blood" || name == "prt_blood_Fountain")
            {
                SprayDroplets(position, 3.0f, true);
            }
            else if (name == "boat_prop")
            {
                SprayDroplets(position, 20.0f, false);
            }
            else if (name == "prt_watersplash")
            {
                SprayDroplets(position, 20.0f, false);
            }
            else if (name == "prt_watercannon_splash")
            {
                SprayDroplets(position, 7.0f, false);
            }
        }

        return shAddParticle.fastcall(a1, position, a3, a4, a5);
    }
}

uintptr_t ResolveDisplacement(hook::pattern& pattern, ptrdiff_t offset = 0)
{
    return utility::resolve_displacement((uintptr_t)pattern.get_first(offset)).value_or(0);
}

void Init()
{
    FusionDxHook::Init(); // For DX12 compat, no hooks applied

    WaterDrops::ReadIniSettings(true);
    static DXGI_FORMAT gFormat = DXGI_FORMAT_R10G10B10A2_UNORM;
    WaterDrops::SetXUVScale(0.125f, 0.875f);

    auto pattern = hook::pattern("44 38 3D ? ? ? ? 0F 85 ? ? ? ? 44 38 3D ? ? ? ? 0F 85 ? ? ? ? 44 38 3D");
    CTimer::m_CodePause.SetAddress(ResolveDisplacement(pattern));
    CTimer::m_UserPause.SetAddress(ResolveDisplacement(pattern, 13));

    pattern = hook::pattern("F3 0F 10 0D ? ? ? ? 0F 2F 0D ? ? ? ? 0F 86");
    CWeather::Rain.SetAddress(ResolveDisplacement(pattern));

    pattern = hook::pattern("44 88 2D ? ? ? ? 48 89 88");
    CCutsceneMgr::ms_running.SetAddress(ResolveDisplacement(pattern));

    pattern = hook::pattern("8B 05 ? ? ? ? 85 C0 74 ? 83 F8 0D");
    CGame::currArea.SetAddress(ResolveDisplacement(pattern));

    pattern = hook::pattern("F6 05 ? ? ? ? ? 75 ? F6 05 ? ? ? ? ? 0F 84");
    CCullZones::CurrentFlags_Camera.SetAddress(ResolveDisplacement(pattern));

    pattern = hook::pattern("F6 05 ? ? ? ? ? 0F 84 ? ? ? ? 41 B9 04 00 00 00");
    CCullZones::CurrentFlags_Player.SetAddress(ResolveDisplacement(pattern));

    pattern = hook::pattern("83 3D ? ? ? ? ? 0F 84 ? ? ? ? 48 8B 0D ? ? ? ? 83 B9");
    gGameState.SetAddress(ResolveDisplacement(pattern));

    pattern = hook::pattern("48 8D 35 ? ? ? ? F3 0F 58 C8");
    TheCamera.SetAddress(ResolveDisplacement(pattern));

    static auto PresentHook = [](IDXGISwapChain* pSwapChain)
    {
        if (gGameState < 9)
            return;
        
        Sire::Init(Sire::SIRE_RENDERER_DX11, pSwapChain);
        Sire::SetTextureFormat(Sire::GetCurrentRenderer(), gFormat);

        WaterDrops::right = TheCamera.get().m_mCameraMatrix.right;
        WaterDrops::up = TheCamera.get().m_mCameraMatrix.up;
        WaterDrops::at = TheCamera.get().m_mCameraMatrix.at;
        WaterDrops::pos = TheCamera.get().m_mCameraMatrix.pos;
        
        //when you put camera underwater, droplets disappear instantly instead of fading out
        if (NoDrops()) {
            WaterDrops::Clear();
            return;
        }
        
        if (NoRain())
            WaterDrops::ms_rainIntensity = 0.0f;
        else
            WaterDrops::ms_rainIntensity = CWeather::Rain;

        if (!CTimer::m_CodePause && !CTimer::m_UserPause)
        {
            WaterDrops::Process();

            if (WaterDrops::ms_numDrops > 0 && !CCutsceneMgr::ms_running)
            {
                WaterDrops::Render();
            }
        }
    };

    pattern = hook::pattern("48 8B 4B ? 48 8B 01 FF 50 ? 8B F0");
    static auto FD3D11ViewportPresentCheckedHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
    {
        auto pSwapChain = *(IDXGISwapChain**)(regs.rbx + 0x78);
        PresentHook(pSwapChain);
    });

    pattern = hook::pattern("48 8B 01 44 8B C3 8B D5");
    static auto FD3D12ViewportPresentCheckedHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
    {
        auto pSwapChain = (IDXGISwapChain*)(regs.rcx);
        Sire::SetCommandQueue(FusionDxHook::D3D12::GetCommandQueueFromSwapChain(pSwapChain));
        PresentHook(pSwapChain);
    });

    pattern = hook::pattern("89 44 24 ? 41 FF 52 ? BA 00 00 00 80");
    static auto onBeforeResizeHookD3D11 = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
    {
        //IDXGISwapChain* pSwapChain = (IDXGISwapChain*)regs.rcx;
        //UINT Width = regs.r8;
        //UINT Height = regs.r9;
        gFormat = (DXGI_FORMAT)regs.rax;

        WaterDrops::Reset();
        Sire::Shutdown();
    });

    pattern = hook::pattern("44 89 74 24 ? 89 44 24 ? 41 FF 52");
    static auto onBeforeResizeHookD3D12 = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
    {
        //IDXGISwapChain* pSwapChain = (IDXGISwapChain*)regs.rcx;
        //UINT Width = regs.r8;
        //UINT Height = regs.r9;
        gFormat = (DXGI_FORMAT)regs.rax;

        WaterDrops::Reset();
        Sire::Shutdown();
    });

    pattern = hook::pattern("E8 ? ? ? ? 49 8B 07 49 8B CF 48 8B 95");
    FxManager_c::shCreateFxSystem = safetyhook::create_inline(ResolveDisplacement(pattern), FxManager_c::CreateFxSystem);

    pattern = hook::pattern("E8 ? ? ? ? 48 8B D8 48 85 C0 74 ? 48 8B C8 E8 ? ? ? ? 48 8B 53");
    UParticleSystemComponent::shResetParticles = safetyhook::create_inline(ResolveDisplacement(pattern), UParticleSystemComponent::ResetParticles);

    pattern = hook::pattern("E8 ? ? ? ? FF C3 3B DF 0F 8C ? ? ? ? 44 0F 28 BC 24");
    FxSystem_c::shAddParticle = safetyhook::create_inline(ResolveDisplacement(pattern), FxSystem_c::AddParticle);

    pattern = hook::pattern("8D 47 ? 83 F8 24");
    static auto CParticleAddParticleHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
    {
        auto type = (tParticleType)(regs.rcx);
        auto position = (RwV3d*)(regs.rdx);
        
        if (type == PARTICLE_SPLASH || type == PARTICLE_BOAT_THRUSTJET)
        {
            SprayDroplets(position, 7.0f, false);
        }
    });
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