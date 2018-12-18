#include "xrd.h"

void Init()
{
    WaterDrops::ms_rainIntensity = 0.0f;

    auto pattern = hook::pattern("80 7E 0C 00 74 0B 8B CE E8 ? ? ? ? C6 46 0C 00");
    struct RainDropletsHook
    {
        void operator()(injector::reg_pack& regs)
        {
            WaterDrops::ms_rainIntensity = 1.0f;
        }
    }; injector::MakeInline<RainDropletsHook>(pattern.get_first(8));

    static auto dw_1138E20 = *hook::get_pattern<uint32_t>("B9 ? ? ? ? E8 ? ? ? ? 84 C0 75 13 6A 01 6A 01 68 F4 01 00 00 B9 ? ? ? ? E8 ? ? ? ? 57", 1);

    pattern = hook::pattern("8B 80 AC 11 00 00 8B 08 89 44 24 04 8B 91 A8 00 00 00 FF E2");
    struct EndSceneHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.eax = *(uint32_t*)(regs.eax + 0x11AC);

            auto pDevice = (LPDIRECT3DDEVICE9)regs.eax; //**(LPDIRECT3DDEVICE9**)injector::aslr_ptr(0x61E3C8).get();

            WaterDrops::right = *(RwV3d*)(dw_1138E20 + 0x00);
            WaterDrops::up =    *(RwV3d*)(dw_1138E20 + 0x10);
            WaterDrops::at =    *(RwV3d*)(dw_1138E20 + 0x20);
            WaterDrops::pos =   *(RwV3d*)(dw_1138E20 + 0x30);

            WaterDrops::Process(pDevice);
            WaterDrops::Render(pDevice);
            WaterDrops::ms_rainIntensity = 0.0f;
        }
    }; injector::MakeInline<EndSceneHook>(pattern.get_first(0), pattern.get_first(6));

    struct ResetHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.eax = 0xFFFFFFFE;
            WaterDrops::Reset();
        }
    }; injector::MakeInline<ResetHook>(injector::aslr_ptr(0x6463C4));
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

    }
    return TRUE;
}