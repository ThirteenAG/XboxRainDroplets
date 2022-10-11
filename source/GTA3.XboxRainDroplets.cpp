#define DX8
#include "xrd.h"

void Init()
{
    static IDirect3DDevice8* pDev2 = (IDirect3DDevice8*)(0x662EF0);

    struct RainDropletsHook3
    {
        void operator()(injector::reg_pack& regs)
        {
            WaterDrops::ms_rainIntensity = 1.0f;
            auto pDevice = *(LPDIRECT3DDEVICE8*)pDev2;
            WaterDrops::Process(pDevice);
            WaterDrops::Render(pDevice);
            WaterDrops::ms_rainIntensity = 1.0f;
        }
    }; injector::MakeInline<RainDropletsHook3>(0x48E603);
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