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
    WaterDrops::fMoveStep = iniReader.ReadFloat("MAIN", "MoveStep", 20.0f);
    
    WaterDrops::ms_rainIntensity = 0.0f;

    static auto ppDevice = *hook::get_pattern<uint32_t>("B8 ? ? ? ? 89 9F", 1);
    static auto pCamMatrix = *hook::get_pattern<uint32_t>("BF ? ? ? ? F3 A5 89 1D ? ? ? ? 8B 4D B8", 1);

    auto pattern = hook::pattern("A1 ? ? ? ? 8B 10 50");
    static auto dword_8AFA60 = *pattern.get_first<uint32_t*>(1);
    struct EndSceneHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.eax = *(uint32_t*)(dword_8AFA60) ;
        
            auto pDevice = *(LPDIRECT3DDEVICE9*)(ppDevice + 0x158);
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
    }; injector::MakeInline<EndSceneHook>(pattern.get_first(0));

    pattern = hook::pattern("8B 83 ? ? ? ? 83 EC 38");
    struct ResetHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.eax = *(uint32_t*)(regs.ebx + 0x158);
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