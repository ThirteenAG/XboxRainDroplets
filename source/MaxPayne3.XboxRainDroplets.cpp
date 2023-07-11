#include <injector\injector.hpp>
#include <injector\hooking.hpp>
#include <injector\calling.hpp>
#include <injector\utility.hpp>
#ifdef _M_IX86
#include <injector\assembly.hpp>
#endif

#include "xrd11.h"
#define FUSIONDXHOOK_INCLUDE_D3D9     1
#define FUSIONDXHOOK_INCLUDE_D3D10    1
#define FUSIONDXHOOK_INCLUDE_D3D10_1  1
#define FUSIONDXHOOK_INCLUDE_D3D11    1
#define FUSIONDXHOOK_USE_MINHOOK      1
#define DELAYED_BIND 2000ms
#include "FusionDxHook.h"


//static auto ppDevice = *hook::get_pattern<uint32_t>("68 ? ? ? ? 68 ? ? ? ? 8B D7 83 CA 10", 1);
uint32_t jmpaddr;
void __declspec(naked) sub_12D4470()
{
    //static auto pDevice = *(LPDIRECT3DDEVICE9*)ppDevice;

    //if (pDevice)
    {
        #ifdef DEBUG
        WaterDrops::ms_rainIntensity = 1.0f; // todo: disable
        #endif // DEBUG
        WaterDrops::Process();
        WaterDrops::Render();
        WaterDrops::ms_rainIntensity = 0.0f;
    }

    _asm jmp jmpaddr
}

void Init()
{
    WaterDrops::ReadIniSettings();

    WaterDrops::ms_rainIntensity = 0.0f;

    FusionDxHook::Init();

    auto pattern = hook::pattern("83 EC 08 F3 0F 10 05 ? ? ? ? 8D 04 24 50");
    jmpaddr = (uint32_t)pattern.get_first();

    //this enables something for next hook to work
    pattern = hook::pattern("38 1D ? ? ? ? 75 21 E8");
    injector::WriteMemory<uint8_t>(pattern.get_first(6), 0xEB, true);

    static bool bAmbientCheck = false;

    pattern = hook::pattern("8D 9B ? ? ? ? 8A 08 3A 0A");
    struct test
    {
        void operator()(injector::reg_pack& regs)
        {
            std::string_view str((char*)regs.eax);

            if (str == "Ambient_RoofCorner_Drip_S" || str == "Ambient_RoofLine_Drip_Long_S" || str == "Ambient_RoofLine_Splash_Long_S")
                bAmbientCheck = true;
        }
    }; injector::MakeInline<test>(pattern.get_first(0), pattern.get_first(6));

    pattern = hook::pattern("68 ? ? ? ? E8 ? ? ? ? 8B 15 ? ? ? ? 8B 0D ? ? ? ? 8B 04 95 ? ? ? ? 83 C4 08 03 C1");
    injector::WriteMemory(pattern.get_first(1), sub_12D4470, true);

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

    FusionDxHook::D3D9::onEndSceneEvent += [](LPDIRECT3DDEVICE9 pDevice)
    {
        Sire::Init(Sire::SIRE_RENDERER_DX9, pDevice);
    };

    FusionDxHook::D3D9::onResetEvent += [](LPDIRECT3DDEVICE9 pDevice)
    {
        WaterDrops::Reset();
    };

    FusionDxHook::D3D10::onPresentEvent += [](IDXGISwapChain* pSwapChain)
    {
        //Sire::Init(Sire::SIRE_RENDERER_DX10, pSwapChain);
    };

    FusionDxHook::D3D11::onPresentEvent += [](IDXGISwapChain* pSwapChain)
    {
        Sire::Init(Sire::SIRE_RENDERER_DX11, pSwapChain);
    };

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