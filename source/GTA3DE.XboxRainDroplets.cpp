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
}

namespace CCutsceneMgr
{
    GameRef<bool> ms_running;
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
    char unk[0x8];
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
    return CCullZones::CamNoRain() || CCullZones::PlayerNoRain() || NoDrops();
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

    WaterDrops::ReadIniSettings();
    static DXGI_FORMAT gFormat = DXGI_FORMAT_R10G10B10A2_UNORM;
    WaterDrops::SetXUVScale(0.125f, 0.875f);

    auto pattern = hook::pattern("40 38 2D ? ? ? ? 0F 85 ? ? ? ? 40 38 2D ? ? ? ? 0F 85");
    CTimer::m_CodePause.SetAddress(ResolveDisplacement(pattern));
    CTimer::m_UserPause.SetAddress(ResolveDisplacement(pattern, 13));

    pattern = hook::pattern("F3 0F 5C 3D ? ? ? ? 0F 28 C7");
    CWeather::Rain.SetAddress(ResolveDisplacement(pattern));

    pattern = hook::pattern("0F B6 15 ? ? ? ? F3 44 0F 10 0D");
    CCutsceneMgr::ms_running.SetAddress(ResolveDisplacement(pattern));

    pattern = hook::pattern("F6 05 ? ? ? ? ? 74 ? F6 05 ? ? ? ? ? 0F 85 ? ? ? ? 48 8B 05");
    CCullZones::CurrentFlags_Camera.SetAddress(ResolveDisplacement(pattern));

    pattern = hook::pattern("F6 05 ? ? ? ? ? 0F 85 ? ? ? ? 48 8B 05");
    CCullZones::CurrentFlags_Player.SetAddress(ResolveDisplacement(pattern));

    pattern = hook::pattern("8B 05 ? ? ? ? 83 F8 09");
    gGameState.SetAddress(ResolveDisplacement(pattern));

    pattern = hook::pattern("48 8D 0D ? ? ? ? E8 ? ? ? ? 0F B6 05 ? ? ? ? 48 69 C8 78 01 00 00 4A 8B 8C 21");
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