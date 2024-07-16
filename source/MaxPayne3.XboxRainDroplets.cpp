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

    static auto ppDevice = *hook::get_pattern<IDirect3DDevice9**>("68 ? ? ? ? 68 ? ? ? ? 8B D7 83 CA 10", 1);
    static auto ppSwapChain = *hook::get_pattern<IDXGISwapChain**>("A1 ? ? ? ? 8B 08 53 8D 54 24 14 52 50", 1);

    //this enables something for next hook to work
    auto pattern = hook::pattern("38 1D ? ? ? ? 75 21 E8");
    injector::WriteMemory<uint8_t>(pattern.get_first(6), 0xEB, true);

    static bool bAmbientCheck = false;

    pattern = hook::pattern("8D 9B ? ? ? ? 8A 08 3A 0A");
    struct AmbientCheckHook
    {
        void operator()(injector::reg_pack& regs)
        {
            std::string_view str((char*)regs.eax);

            if (str == "Ambient_RoofCorner_Drip_S" || str == "Ambient_RoofLine_Drip_Long_S" || str == "Ambient_RoofLine_Splash_Long_S")
                bAmbientCheck = true;
        }
    }; injector::MakeInline<AmbientCheckHook>(pattern.get_first(0), pattern.get_first(6));

    pattern = hook::pattern("A3 ? ? ? ? 8B 16 8B 82 ? ? ? ? 6A 03");
    static auto D3D11CreateDeviceAndSwapChain = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& ctx)
    {
        auto pSwapChain = *ppSwapChain;
        Sire::Init(Sire::SIRE_RENDERER_DX11, pSwapChain);
    });

    pattern = hook::pattern("A3 ? ? ? ? 8B 06 8B 90 ? ? ? ? 6A 06");
    static auto D3D9CreateDevice = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& ctx)
    {
        auto pDevice = *ppDevice;
        Sire::Init(Sire::SIRE_RENDERER_DX9, pDevice);
    });

    pattern = hook::pattern("68 ? ? ? ? E8 ? ? ? ? 8B 15 ? ? ? ? 8B 0D ? ? ? ? 8B 04 95 ? ? ? ? 83 C4 08 03 C1");
    static auto shsub_12D4470 = safetyhook::create_mid(*pattern.get_first<void*>(1), [](SafetyHookContext& ctx)
    {
        #ifdef DEBUG
        WaterDrops::ms_rainIntensity = 1.0f;
        #endif // DEBUG
        WaterDrops::Process();
        WaterDrops::Render();
        WaterDrops::ms_rainIntensity = 0.0f;
    });

    pattern = hook::pattern("F3 0F 11 6E ? F3 0F 11 46 ? 5E 5B");
    struct CameraHook
    {
        void operator()(injector::reg_pack& regs)
        {
            auto right = *(RwV3d*)(regs.esi + 0x20);
            auto up = *(RwV3d*)(regs.esi + 0x30);
            auto at = *(RwV3d*)(regs.esi + 0x40);
            auto pos = *(RwV3d*)(regs.esi + 0x50);

            WaterDrops::right = right;
            WaterDrops::up = up;
            WaterDrops::at = at;
            WaterDrops::pos = pos;
        }
    }; injector::MakeInline<CameraHook>(pattern.get_first(5));

    pattern = hook::pattern("FF D2 8B 06 8B 90 ? ? ? ? 8B CE FF D2 A1 ? ? ? ? 8B 08");
    static auto D3D9onReset = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& ctx)
    {
        WaterDrops::Reset();
    });

    pattern = hook::pattern("C6 86 ? ? ? ? ? EB 1C 8B 0D ? ? ? ? 57 E8");
    struct InteriornessHook1
    {
        void operator()(injector::reg_pack& regs)
        {
            *(uint8_t*)(regs.esi + 0x21BD8) = 1;
            if (bAmbientCheck)
            {
                WaterDrops::ms_rainIntensity = 1.0f;
                bAmbientCheck = false;
            }
        }
    }; injector::MakeInline<InteriornessHook1>(pattern.get_first(0), pattern.get_first(7));

    pattern = hook::pattern("C6 86 ? ? ? ? ? 85 FF 74 2E 80 BF ? ? ? ? ? 74 1B");
    struct InteriornessHook2
    {
        void operator()(injector::reg_pack& regs)
        {
            *(uint8_t*)(regs.esi + 0x21BD8) = 0;
            WaterDrops::ms_rainIntensity = 0.0f;
        }
    }; injector::MakeInline<InteriornessHook2>(pattern.get_first(0), pattern.get_first(7));

    pattern = hook::pattern("D9 05 ? ? ? ? 8B 10 51");
    WaterDrops::fTimeStep = *pattern.get_first<float*>(2);
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