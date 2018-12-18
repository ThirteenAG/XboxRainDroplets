#include "xrd.h"

//#define USE_D3D_HOOK

void Init()
{
    auto pattern = hook::pattern("A1 ? ? ? ? 8B 10 68 ? ? ? ? 50 FF 52 40");
    static auto pDev = *pattern.get_first<LPDIRECT3DDEVICE9*>(1);
    struct ResetHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.eax = *(uint32_t*)pDev;
            WaterDrops::Reset();
        }
    }; injector::MakeInline<ResetHook>(pattern.get_first(0));

    static auto dword_982D80 = *hook::get_pattern<uint32_t>("BE ? ? ? ? 33 FF 8B 44 3C 0C", 1);
    pattern = hook::pattern("89 86 F0 01 00 00 ? ? 8B 4C 24 08");
    static auto dword_870910 = *pattern.get_first<uint32_t*>(2);
    struct RainDropletsHook
    {
        void operator()(injector::reg_pack& regs)
        {
            *(uint32_t*)(regs.esi + 0x1F0) = regs.eax;

            WaterDrops::ms_noCamTurns = true;
            WaterDrops::ms_rainIntensity = 1.0f;
            WaterDrops::right = *(RwV3d*)(dword_982D80 + 0x00);
            WaterDrops::up = *(RwV3d*)(dword_982D80 + 0x10);
            WaterDrops::at = *(RwV3d*)(dword_982D80 + 0x20);
            WaterDrops::pos = *(RwV3d*)(dword_982D80 + 0x60);
            WaterDrops::Process(*pDev);
            WaterDrops::Render(*pDev);
        }
    }; injector::MakeInline<RainDropletsHook>(pattern.get_first(0), pattern.get_first(6));
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