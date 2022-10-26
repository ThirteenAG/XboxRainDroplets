#include "xrd.h"

void Init()
{
    CIniReader iniReader("");
    WaterDrops::MAXDROPS = iniReader.ReadInteger("MAIN", "MaxDrops", 2000);
    WaterDrops::MAXDROPSMOVING = iniReader.ReadInteger("MAIN", "MaxMovingDrops", 500);
    WaterDrops::bRadial = iniReader.ReadInteger("MAIN", "RadialMovement", 0) != 0;
    WaterDrops::bGravity = iniReader.ReadInteger("MAIN", "EnableGravity", 1) != 0;
    WaterDrops::fSpeedAdjuster = iniReader.ReadFloat("MAIN", "SpeedAdjuster", 1.0f);
    
    static LPDIRECT3DDEVICE9* pDev = (LPDIRECT3DDEVICE9*)0xC97C28;
    static auto matrix = (RwMatrix*)((0xB6F97C + 0x20));
    static injector::hook_back<void(*)()> CMotionBlurStreaksRender;
    auto CMotionBlurStreaksRenderHook = []()
    {
        CMotionBlurStreaksRender.fun();
        WaterDrops::ms_rainIntensity = 1.0f;
        WaterDrops::right = matrix->right;
        WaterDrops::up = matrix->up;
        WaterDrops::at = matrix->at;
        WaterDrops::pos = matrix->pos;
        WaterDrops::Process(*pDev);
        WaterDrops::Render(*pDev);
    }; CMotionBlurStreaksRender.fun = injector::MakeCALL(0x726AD0, static_cast<void(*)()>(CMotionBlurStreaksRenderHook), true).get();
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