#include "xrd.h"


void Init()
{
    CIniReader iniReader("");
    WaterDrops::MinSize = iniReader.ReadInteger("MAIN", "MinSize", 4);
    WaterDrops::MaxSize = iniReader.ReadInteger("MAIN", "MaxSize", 15);
    WaterDrops::MaxDrops = iniReader.ReadInteger("MAIN", "MaxDrops", 2000);
    WaterDrops::MaxDropsMoving = iniReader.ReadInteger("MAIN", "MaxMovingDrops", 500);
    WaterDrops::bRadial = iniReader.ReadInteger("MAIN", "RadialMovement", 0) != 0;
    WaterDrops::bGravity = iniReader.ReadInteger("MAIN", "EnableGravity", 1) != 0;
    WaterDrops::fSpeedAdjuster = iniReader.ReadFloat("MAIN", "SpeedAdjuster", 1.0f);
    
    WaterDrops::ms_rainIntensity = 0.0f;

    //static auto ppDevice = *hook::get_pattern<uint32_t>("", 1);
    static auto pCamMatrix = *hook::get_pattern<uint32_t>("BF ? ? ? ? F3 A5 BE", 1);

    auto pattern = hook::pattern("8B 81 ? ? ? ? 8B 08 50 FF 91 ? ? ? ? C3");
    struct EndSceneHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.eax = *(uint32_t*)(regs.ecx + 0x164);
        
            auto pDevice = (LPDIRECT3DDEVICE9)(regs.eax);
            WaterDrops::ms_rainIntensity = 1.0f;
    
            auto right = *(RwV3d*)(pCamMatrix + 0x00);
            auto up = *(RwV3d*)(pCamMatrix + 0x10);
            auto at = *(RwV3d*)(pCamMatrix + 0x20);
            auto pos = *(RwV3d*)(pCamMatrix + 0x30);
            
            WaterDrops::right = { -right.x, -right.z, -right.y };
            WaterDrops::up = { up.x, up.z, up.y };
            WaterDrops::at = { at.x, at.z, at.y };
            WaterDrops::pos = { pos.x, pos.z, pos.y };


            WaterDrops::Process(pDevice);
            WaterDrops::Render(pDevice);
            WaterDrops::ms_rainIntensity = 1.0f;
        }
    }; injector::MakeInline<EndSceneHook>(pattern.get_first(0), pattern.get_first(6));
    
    pattern = hook::pattern("8B 83 ? ? ? ? 6A 0E 59");
    struct ResetHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.eax = *(uint32_t*)(regs.ebx + 0x164);
            WaterDrops::Reset();
        }
    }; injector::MakeInline<ResetHook>(pattern.get_first(0), pattern.get_first(6));
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