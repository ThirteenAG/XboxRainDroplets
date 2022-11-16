#include "xrd.h"

void Init()
{
    CIniReader iniReader("");
    WaterDrops::MAXDROPS = iniReader.ReadInteger("MAIN", "MaxDrops", 2000);
    WaterDrops::MAXDROPSMOVING = iniReader.ReadInteger("MAIN", "MaxMovingDrops", 500);
    WaterDrops::bRadial = iniReader.ReadInteger("MAIN", "RadialMovement", 0) != 0;
    WaterDrops::bGravity = iniReader.ReadInteger("MAIN", "EnableGravity", 1) != 0;
    WaterDrops::fSpeedAdjuster = iniReader.ReadFloat("MAIN", "SpeedAdjuster", 1.0f);
    
    WaterDrops::ms_rainIntensity = 0.0f;

    static auto ppDevice = *hook::get_pattern<uint32_t>("68 ? ? ? ? 68 ? ? ? ? 8B D7 83 CA 10", 1);

    //auto pattern = hook::pattern("E8 ? ? ? ? F6 00 10 0F 84 ? ? ? ? E8 ? ? ? ? D9 E8");
    //struct RenderPhaseCallbackHook
    //{
    //    void operator()(injector::reg_pack& regs)
    //    {
    //        auto pDevice = *(LPDIRECT3DDEVICE9*)ppDevice;
    //        WaterDrops::ms_rainIntensity = 1.0f;
    //
    //        WaterDrops::Process(pDevice);
    //        WaterDrops::Render(pDevice);
    //        WaterDrops::ms_rainIntensity = 1.0f;
    //    }
    //}; injector::MakeInline<RenderPhaseCallbackHook>(pattern.get_first(0));

    auto pattern = hook::pattern("8B 91 ? ? ? ? 50 FF D2 A1 ? ? ? ? 50");
    struct EndSceneHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.edx = *(uint32_t*)(regs.ecx + 0xA8);
            
            auto pDevice = *(LPDIRECT3DDEVICE9*)ppDevice;
            WaterDrops::ms_rainIntensity = 1.0f;

            WaterDrops::Process(pDevice);
            WaterDrops::Render(pDevice);
            WaterDrops::ms_rainIntensity = 1.0f;
        }
    }; injector::MakeInline<EndSceneHook>(pattern.get_first(0), pattern.get_first(6));
    
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
    
    pattern = hook::pattern("8B 08 8B 51 40 68 ? ? ? ? 50 FF D2 85 C0");
    struct ResetHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.ecx = *(uint32_t*)(regs.eax);
            regs.edx = *(uint32_t*)(regs.ecx + 0x40);
            WaterDrops::Reset();
        }
    }; injector::MakeInline<ResetHook>(pattern.get_first(0));
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