#include <injector\injector.hpp>
#include <injector\hooking.hpp>
#include <injector\calling.hpp>
#include <injector\utility.hpp>
#include <injector\assembly.hpp>
#include "xrd11.h"

void Init()
{
    WaterDrops::ReadIniSettings();

    WaterDrops::ms_rainIntensity = 0.0f;

    auto pattern = hook::pattern("E8 ? ? ? ? F3 0F 10 45 ? 56 51 8B CF");
    static auto BeforeUpdateRain = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
    {
        //auto right = *(RwV3d*)(regs.ebx + 0x6C);
        //auto up = *(RwV3d*)(regs.ebx + 0x7C);
        //auto at = *(RwV3d*)(regs.ebx + 0x8C);
        //auto pos = *(RwV3d*)(regs.ebx + 0x9C);
        //
        //WaterDrops::right = right;
        //WaterDrops::up = up;
        //WaterDrops::at = at;
        //WaterDrops::pos = pos;

        WaterDrops::ms_rainIntensity = 0.0f;
    });

    pattern = hook::pattern("F3 0F 11 44 24 ? F3 0F 11 45 ? F3 0F 11 04 24 E8 ? ? ? ? 8B 7D F0 8A 55 0B");
    static auto OnUpdateRain = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
    {
        WaterDrops::ms_rainIntensity = 1.0f;
        regs.xmm0.f32[0] = 0.0f; // disables original droplets
    });

    pattern = hook::pattern("E8 ? ? ? ? 83 C4 1C 8B CB");
    static auto BeforeRenderHUDPrimitives = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
    {
        WaterDrops::Process();
        WaterDrops::Render();
    });

    pattern = hook::pattern("8B 46 58 8D 95");
    static auto ResizeDXGIBuffers = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
    {
        auto pSwapChain = *(IDXGISwapChain**)(regs.esi + 0x58);
        WaterDrops::Reset();
        Sire::Shutdown();
        Sire::Init(Sire::SIRE_RENDERER_DX11, pSwapChain);
    });

    pattern = hook::pattern("89 85 ? ? ? ? 85 C0 79 07");
    static auto AfterD3D11CreateDeviceAndSwapChain = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
    {
        auto pSwapChain = *(IDXGISwapChain**)(regs.edi + 0x330);

        static bool bOnce = false;
        if (!bOnce)
        {
            bOnce = true;
            ID3D11Device* pDevice = nullptr;
            HRESULT ret = pSwapChain->GetDevice(__uuidof(ID3D11Device), (PVOID*)&pDevice);
            ID3D10Multithread* multithread = nullptr;
            if (SUCCEEDED(pDevice->QueryInterface(__uuidof(ID3D10Multithread), (void**)&multithread))) {
                multithread->SetMultithreadProtected(TRUE);
                multithread->Release();
            }
            pDevice->Release();
        }

        Sire::Init(Sire::SIRE_RENDERER_DX11, pSwapChain);
    });
}

extern "C" __declspec(dllexport) void InitializeASI()
{
    std::call_once(CallbackHandler::flag, []()
    {
        CallbackHandler::RegisterCallback(Init, hook::pattern("89 85 ? ? ? ? 85 C0 79 07"));
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