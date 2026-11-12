// Direct3D 8, the API this game uses
#define XRD_ENABLE_D3D8
#include "xrd/xrd.h"
#include <set>

// The engine of the game: LS3DF.dll, or LSV10.dll and LSV11.dll where an install
// keeps several versions of the game next to each other.
static constexpr const wchar_t* aEngineNames[] = { L"LS3DF.dll", L"LSV11.dll", L"LSV10.dll" };

static HMODULE GetLS3DF()
{
    for (auto name : aEngineNames)
    {
        if (auto hModule = GetModuleHandleW(name))
            return hModule;
    }
    return nullptr;
}

void Init()
{
    WaterDrops::ReadSettings(true);

    WaterDrops::ms_StaticRain = true;
    WaterDrops::ms_rainIntensity = 0.0f;
    static std::set<uint32_t> m;

    // The menus that are open: 1.1 and 1.2 count their windows, 1.0 runs every
    // menu in a loop of its own, which is counted on the way in and out.
    static uint32_t nMenuLoops = 0;
    static uint32_t* pMenuWindows = &nMenuLoops;

    auto pattern = hook::pattern("25 00 C0 02 00 0C 01 A3");
    if (!pattern.empty()) // 1.0
    {
        struct MenuLoopEnter
        {
            void operator()(injector::reg_pack& regs)
            {
                regs.eax = (regs.eax & 0x2C000) | 1;
                nMenuLoops++;
            }
        }; injector::MakeInline<MenuLoopEnter>(pattern.get_first(0), pattern.get_first(7));

        pattern = hook::pattern("8B 8C 24 10 01 00 00 3B C7 89 0D");
        struct MenuLoopLeave
        {
            void operator()(injector::reg_pack& regs)
            {
                regs.ecx = *(uint32_t*)(regs.esp + 0x110);
                nMenuLoops--;
            }
        }; injector::MakeInline<MenuLoopLeave>(pattern.get_first(0), pattern.get_first(7));
    }
    else // 1.1 and 1.2
        pMenuWindows = *hook::pattern("A3 ? ? ? ? 8B C6 5E 83 C4 14 C3").get_first<uint32_t*>(1);

    static auto hEngine = GetLS3DF();

    pattern = hook::pattern(hEngine, "A1 ? ? ? ? 51 51 6A 00 89 0D ? ? ? ? 8B 10 50 FF 92 FC 00 00 00");
    static auto pDev = pattern.get_first(1);

    pattern = hook::pattern(hEngine, "C6 05 ? ? ? ? ? E8 ? ? ? ? 6A 00 6A 00 6A 02 FF 96 40 01 00 00");
    static auto byte_101C4D14 = *pattern.get_first<uint8_t*>(2);
    struct RainDropletsHook
    {
        void operator()(injector::reg_pack& regs)
        {
            *byte_101C4D14 = 1;
            auto pDevice = **(LPDIRECT3DDEVICE8**)pDev;
            Xrd::Init(XRD_DEVICE_RENDERER, pDevice);
            WaterDrops::Process();
            WaterDrops::Render();
            WaterDrops::ms_rainIntensity = 0.0f;
            WaterDrops::isPaused = false;
        }
    }; injector::MakeInline<RainDropletsHook>(pattern.get_first(0), pattern.get_first(7));

    pattern = hook::pattern(hEngine, "A1 ? ? ? ? 83 EC 3C 85 C0 53");
    static auto dword_101C59C0 = *pattern.get_first<uint32_t*>(1);
    struct ResetHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.eax = *dword_101C59C0;
            WaterDrops::Reset();
        }
    }; injector::MakeInline<ResetHook>(pattern.get_first(0));

    pattern = hook::pattern(hEngine, "8B B3 ? ? ? ? 8B 83 ? ? ? ? 3B F0");
    struct WeatherCheck
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.esi = *(uint32_t*)(regs.ebx + 0x2A4);
            if (!m.empty())
            {
                WaterDrops::ms_rainIntensity = *(float*)(*m.begin() + 0x1E8);
                WaterDrops::isPaused = *pMenuWindows != 0;
            }
        }
    }; injector::MakeInline<WeatherCheck>(pattern.get_first(0), pattern.get_first(6));

    pattern = hook::pattern(hEngine, "D9 99 ? ? ? ? A1 ? ? ? ? 3B C8");
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
            //WaterDrops::right = *(RwV3d*)(regs.esi + 0x10);
            //WaterDrops::up = *(RwV3d*)(regs.esi + 0x20);
            //WaterDrops::at = *(RwV3d*)(regs.esi + 0x30);
            //WaterDrops::pos = *(RwV3d*)(regs.esi + 0x40);

            auto up = *(RwV3d*)(regs.esi + 0x20);
            auto right = *(RwV3d*)(regs.esi + 0x10);
            auto at = *(RwV3d*)(regs.esi + 0x30);
            auto pos = *(RwV3d*)(regs.esi + 0x40);

            RwMatrix dst;
            dst.right = { at.z, at.x, at.y };
            dst.up = { right.z, right.x, right.y };
            dst.at = { up.z, up.x, up.y };
            dst.pos = { pos.z, pos.x, pos.y };

            // The first row of the frame points to the right of the screen and the
            // effect takes right as pointing to the left of it, the way RenderWare
            // has it, or the drops drift the wrong way when the camera turns.
            WaterDrops::right = { -dst.up.x, -dst.up.y, -dst.up.z };
            WaterDrops::up = dst.right;
            WaterDrops::at = dst.at;
            WaterDrops::pos = dst.pos;
        }
    }; injector::MakeInline<Cam>(pattern.get_first(0), pattern.get_first(6));

    pattern = hook::pattern("C7 44 24 ? ? ? ? ? 8B 49 6C 50 6A 57");
    struct HydrantHook
    {
        void operator()(injector::reg_pack& regs)
        {
            *(float*)(regs.esp + 0x24) = 1.0f;
            RwV3d vec = *(RwV3d*)(regs.esp + 0x30);
            RwV3d prt_pos = { vec.z, vec.x, vec.y };
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
}

extern "C" __declspec(dllexport) void InitializeASI()
{
    std::call_once(CallbackHandler::flag, []()
    {
        RegisterOldPluginCheck();
        for (auto name : aEngineNames)
        {
            CallbackHandler::RegisterCallback(name, +[]()
            {
                static std::once_flag flag;
                std::call_once(flag, Init);
            });
        }
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