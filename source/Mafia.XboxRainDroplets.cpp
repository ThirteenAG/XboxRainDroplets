#include "xrd.h"
#include <set>

//#define USE_D3D_HOOK

void Init()
{
#ifdef USE_D3D_HOOK
    //setting rain
    WaterDrops::ProcessCallback1 = []()
    {
        WaterDrops::ms_rainIntensity = 1.0f;
    };

    //resetting rain
    WaterDrops::ProcessCallback2 = []()
    {
        WaterDrops::ms_rainIntensity = 0.0f;
    };

    //hooking create to get EndScene and Reset
    auto pattern = hook::pattern(GetModuleHandle(L"LS3DF.dll"), "E8 ? ? ? ? 85 C0 A3 ? ? ? ? 75 1F 68 ? ? ? ? E8 ? ? ? ? 83 C4 04");
    static injector::hook_back<IDirect3D9*(WINAPI*)(UINT)> Direct3DCreate8;
    auto Direct3DCreate8Hook = [](UINT SDKVersion) -> IDirect3D9*
    {
        auto pID3D8 = Direct3DCreate8.fun(SDKVersion);
        auto pVTable = (UINT_PTR*)(*((UINT_PTR*)pID3D8));
        if (!WaterDrops::RealD3D9CreateDevice)
            WaterDrops::RealD3D9CreateDevice = (CreateDevice_t)pVTable[IDirect3D8VTBL::CreateDevice];
        injector::WriteMemory(&pVTable[IDirect3D8VTBL::CreateDevice], &WaterDrops::d3d8CreateDevice, true);
        return pID3D8;
    }; Direct3DCreate8.fun = injector::MakeCALL(pattern.get_first(0), static_cast<IDirect3D9*(WINAPI*)(UINT)>(Direct3DCreate8Hook), true).get();
#else
    WaterDrops::ms_StaticRain = true;
    WaterDrops::ms_noCamTurns = true;
    WaterDrops::ms_rainIntensity = 0.0f;
    static std::set<uint32_t> m;

    auto pattern = hook::pattern(GetModuleHandle(L"LS3DF.dll"), "A1 ? ? ? ? 51 51 6A 00 89 0D ? ? ? ? 8B 10 50 FF 92 FC 00 00 00");
    static auto pDev = pattern.get_first(1);

    pattern = hook::pattern(GetModuleHandle(L"LS3DF.dll"), "C6 05 ? ? ? ? ? E8 ? ? ? ? 6A 00 6A 00 6A 02 FF 96 40 01 00 00");
    static auto byte_101C4D14 = *pattern.get_first<uint8_t*>(2);
    struct RainDropletsHook
    {
        void operator()(injector::reg_pack& regs)
        {
            *byte_101C4D14 = 1;
            auto pDevice = **(LPDIRECT3DDEVICE9**)pDev;
            class Direct3DDevice8 : public IUnknown
            {
                //...
            public:
                void* ProxyAddressLookupTable;
                void* const D3D;
                IDirect3DDevice9 *const ProxyInterface;
                //...
            };
            if (!((Direct3DDevice8*)pDevice)->ProxyInterface)
                if (MessageBox(0, L"Xbox Rain Droplets requires d3d8to9. Enable it and restart.", L"Mafia", MB_OK) == IDOK)
                    ExitProcess(0);
            WaterDrops::Process(((Direct3DDevice8*)pDevice)->ProxyInterface);
            WaterDrops::Render(((Direct3DDevice8*)pDevice)->ProxyInterface);
            WaterDrops::ms_rainIntensity = 0.0f;
        }
    }; injector::MakeInline<RainDropletsHook>(pattern.get_first(0), pattern.get_first(7));

    pattern = hook::pattern(GetModuleHandle(L"LS3DF.dll"), "A1 ? ? ? ? 83 EC 3C 85 C0 53");
    static auto dword_101C59C0 = *pattern.get_first<uint32_t*>(1);
    struct ResetHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.eax = *dword_101C59C0;
            WaterDrops::Reset();
        }
    }; injector::MakeInline<ResetHook>(pattern.get_first(0));

    pattern = hook::pattern(GetModuleHandle(L"LS3DF.dll"), "8B B3 ? ? ? ? 8B 83 ? ? ? ? 3B F0");
    struct WeatherCheck
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.esi = *(uint32_t*)(regs.ebx + 0x2A4);
            if (!m.empty())
                WaterDrops::ms_rainIntensity = *(float*)(*m.begin() + 0x1E8);
        }
    }; injector::MakeInline<WeatherCheck>(pattern.get_first(0), pattern.get_first(6));

    pattern = hook::pattern(GetModuleHandle(L"LS3DF.dll"), "D9 99 ? ? ? ? A1 ? ? ? ? 3B C8");
    struct RainCheckFSTP
    {
        void operator()(injector::reg_pack& regs)
        {
            float f = 0.0f;
            _asm {fstp dword ptr[f]}
            *(float*)(regs.ecx + 0x1E8) = f;
            m.emplace(regs.ecx);
        }
    }; injector::MakeInline<RainCheckFSTP>(pattern.get_first(0), pattern.get_first(6));

    pattern = hook::pattern("8B 4E 40 8B 56 44 8D 46 40");
    struct Cam
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.ecx = *(uint32_t*)(regs.esi + 0x40);
            regs.edx = *(uint32_t*)(regs.esi + 0x44);
            WaterDrops::right = *(RwV3d*)(regs.esi + 0x10);
            WaterDrops::up = *(RwV3d*)(regs.esi + 0x20);
            WaterDrops::at = *(RwV3d*)(regs.esi + 0x30);
            WaterDrops::pos = *(RwV3d*)(regs.esi + 0x40);
        }
    }; injector::MakeInline<Cam>(pattern.get_first(0), pattern.get_first(6));

    pattern = hook::pattern("C7 44 24 ? ? ? ? ? 8B 49 6C 50 6A 57");
    struct HydrantHook
    {
        void operator()(injector::reg_pack& regs)
        {
            *(float*)(regs.esp + 0x24) = 1.0f;
            RwV3d prt_pos = *(RwV3d*)(regs.esp + 0x30);
            RwV3d dist;
            RwV3dSub(&dist, &prt_pos, &WaterDrops::pos);
            if (RwV3dDotProduct(&dist, &dist) <= 20.0f)
                WaterDrops::RegisterSplash(&prt_pos, 3.0f, 14);
        }
    }; injector::MakeInline<HydrantHook>(pattern.get_first(0), pattern.get_first(8));

    pattern = hook::pattern("D9 46 18 D8 46 30 D9 5C 24 2C D9 46 1C");
    struct HydrantHook2
    {
        void operator()(injector::reg_pack& regs)
        {
            float f1 = *(float*)(regs.esi + 0x18);
            float f2 = *(float*)(regs.esi + 0x30);
            _asm {fld  dword ptr[f1]}
            _asm {fadd dword ptr[f2]}
            WaterDrops::ms_splashDuration += 1;
        }
    }; injector::MakeInline<HydrantHook2>(pattern.get_first(0), pattern.get_first(6));
#endif
}

extern "C" __declspec(dllexport) void InitializeASI()
{
    std::call_once(CallbackHandler::flag, []()
        {
            CallbackHandler::RegisterCallback(L"LS3DF.dll", Init);
        });
}

BOOL APIENTRY DllMain(HMODULE /*hModule*/, DWORD reason, LPVOID /*lpReserved*/)
{
    if (reason == DLL_PROCESS_ATTACH)
    {

    }
    return TRUE;
}