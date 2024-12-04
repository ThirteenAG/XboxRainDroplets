#include "xrd11.h"
#include <injector/injector.hpp>
#include <safetyhook.hpp>
#include <utility/Scan.hpp>

#define FUSIONDXHOOK_INCLUDE_D3D12    1
#define FUSIONDXHOOK_USE_SAFETYHOOK   0
#include "FusionDxHook.h"

namespace CWeather
{
    GameRef<float> Rain;
    GameRef<float> UnderWaterness;
}

namespace CCutsceneMgr
{
    GameRef<bool> ms_running;
}

namespace CGame
{
    GameRef<int> currArea;
}

namespace CEntryExitManager
{
    GameRef<int> ms_exitEnterState;
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
    char unk[0x830];
    RwMatrix m_mCameraMatrix;
};

GameRef<CCamera> TheCamera;
GameRef<int> gGameState;

bool NoDrops()
{
    return CWeather::UnderWaterness > 0.339731634f || CEntryExitManager::ms_exitEnterState != 0;
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
                SprayDroplets((RwV3d*)a3, 10.0f, false);
            }
            else if (name == "water_splash")
            {
                SprayDroplets((RwV3d*)a3, 20.0f, false);
            }
            else if (name == "water_splash_big")
            {
                SprayDroplets((RwV3d*)a3, 30.0f, false);
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
                SprayDroplets(position, 10.0f, true);
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

    WaterDrops::ReadIniSettings();
    static DXGI_FORMAT gFormat = DXGI_FORMAT_R10G10B10A2_UNORM;
    WaterDrops::SetXUVScale(0.125f, 0.875f);

    auto pattern = hook::pattern("44 38 3D ? ? ? ? 0F 85 ? ? ? ? 44 38 3D ? ? ? ? 0F 85");
    CTimer::m_CodePause.SetAddress(ResolveDisplacement(pattern));        //0x14522BCF3
    CTimer::m_UserPause.SetAddress(ResolveDisplacement(pattern, 13));    //0x14521F7EB

    pattern = hook::pattern("89 2D ? ? ? ? 89 0D");
    CEntryExitManager::ms_exitEnterState.SetAddress(ResolveDisplacement(pattern)); //0x1451B5B2C

    pattern = hook::pattern("F3 0F 10 05 ? ? ? ? 41 0F 2F C0 76");
    CWeather::Rain.SetAddress(ResolveDisplacement(pattern)); //0x14531B1D8

    pattern = hook::pattern("F3 0F 10 05 ? ? ? ? 0F 2F 05 ? ? ? ? F3 44 0F 10 05");
    CWeather::UnderWaterness.SetAddress(ResolveDisplacement(pattern)); //0x14572BEE8

    pattern = hook::pattern("44 88 25 ? ? ? ? 48 89 88");
    CCutsceneMgr::ms_running.SetAddress(ResolveDisplacement(pattern)); //0x14572A430

    pattern = hook::pattern("83 3D ? ? ? ? ? 75 ? F6 05");
    CGame::currArea.SetAddress(ResolveDisplacement(pattern)); //0x14572B450

    pattern = hook::pattern("F6 05 ? ? ? ? ? 75 ? BB A7 00 00 00");
    CCullZones::CurrentFlags_Camera.SetAddress(ResolveDisplacement(pattern)); //0x14531AA40

    pattern = hook::pattern("F6 05 ? ? ? ? ? 75 ? F6 05 ? ? ? ? ? 75 ? BB A7 00 00 00");
    CCullZones::CurrentFlags_Player.SetAddress(ResolveDisplacement(pattern)); //0x14531AA3C

    pattern = hook::pattern("83 3D ? ? ? ? ? 75 ? E8 ? ? ? ? 8B 05 ? ? ? ? 48 8D 0D");
    gGameState.SetAddress(ResolveDisplacement(pattern)); //0x145300FF8

    pattern = hook::pattern("48 8D 0D ? ? ? ? E8 ? ? ? ? 80 3D ? ? ? ? ? 48 8D 1D");
    TheCamera.SetAddress(ResolveDisplacement(pattern)); //0x1453E23D0

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
    static auto FD3D11ViewportPresentCheckedHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs) //0x141F5A4F5
    {
        auto pSwapChain = *(IDXGISwapChain**)(regs.rbx + 0x78);
        PresentHook(pSwapChain);
    });

    pattern = hook::pattern("48 8B 01 44 8B C3 8B D5");
    static auto FD3D12ViewportPresentCheckedHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs) //0x141FD5E67
    {
        auto pSwapChain = (IDXGISwapChain*)(regs.rcx);
        Sire::SetCommandQueue(FusionDxHook::D3D12::GetCommandQueueFromSwapChain(pSwapChain));
        PresentHook(pSwapChain);
    });

    pattern = hook::pattern("89 44 24 ? 41 FF 52 ? BA 00 00 00 80");
    static auto onBeforeResizeHookD3D11 = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs) //0x141F5A0BC
    {
        //IDXGISwapChain* pSwapChain = (IDXGISwapChain*)regs.rcx;
        //UINT Width = regs.r8;
        //UINT Height = regs.r9;
        gFormat = (DXGI_FORMAT)regs.rax;

        WaterDrops::Reset();
        Sire::Shutdown();
    });

    pattern = hook::pattern("44 89 74 24 ? 89 44 24 ? 41 FF 52");
    static auto onBeforeResizeHookD3D12 = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs) //0x141FEA673
    {
        //IDXGISwapChain* pSwapChain = (IDXGISwapChain*)regs.rcx;
        //UINT Width = regs.r8;
        //UINT Height = regs.r9;
        gFormat = (DXGI_FORMAT)regs.rax;

        WaterDrops::Reset();
        Sire::Shutdown();
    });

    pattern = hook::pattern("E8 ? ? ? ? 41 8D 5D ? 89 5D ? 41 3B DC");
    FxManager_c::shCreateFxSystem = safetyhook::create_inline(ResolveDisplacement(pattern), FxManager_c::CreateFxSystem); //0x141A7D8B0

    pattern = hook::pattern("E8 ? ? ? ? 48 85 C0 74 ? 48 8B C8 E8 ? ? ? ? 48 8B CE");
    UParticleSystemComponent::shResetParticles = safetyhook::create_inline(ResolveDisplacement(pattern), UParticleSystemComponent::ResetParticles); //0x140AEF6D0

    pattern = hook::pattern("E8 ? ? ? ? 49 83 EC 01 0F 85 ? ? ? ? 44 8B 64 24");
    FxSystem_c::shAddParticle = safetyhook::create_inline(ResolveDisplacement(pattern), FxSystem_c::AddParticle); //0x140AF06A0
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