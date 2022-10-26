#define DX8
#include "xrd.h"

void Init()
{
    CIniReader iniReader("");
    WaterDrops::MAXDROPS = iniReader.ReadInteger("MAIN", "MaxDrops", 2000);
    WaterDrops::MAXDROPSMOVING = iniReader.ReadInteger("MAIN", "MaxMovingDrops", 500);
    WaterDrops::bRadial = iniReader.ReadInteger("MAIN", "RadialMovement", 0) != 0;
    WaterDrops::bGravity = iniReader.ReadInteger("MAIN", "EnableGravity", 1) != 0;
    WaterDrops::fSpeedAdjuster = iniReader.ReadFloat("MAIN", "SpeedAdjuster", 1.0f);
    
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