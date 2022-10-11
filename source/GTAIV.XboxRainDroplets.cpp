#include "xrd.h"

void Init()
{
    CIniReader iniReader("");
    WaterDrops::bRadial = iniReader.ReadInteger("MAIN", "RadialMovement", 0) != 0;
    WaterDrops::bGravity = iniReader.ReadInteger("MAIN", "EnableGravity", 1) != 0;
    
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
    static auto ppDevice = *hook::get_pattern<uint32_t>("83 C4 0C A1 ? ? ? ? A3", 4);

    pattern = hook::pattern("BF ? ? ? ? 39 3D ? ? ? ? 0F 85");
    struct RenderPhaseCallbackHook //bugged, renderer sometimes disappears
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.edi = 1;
    
            if (WaterDrops::fTimeStep && !*WaterDrops::fTimeStep && (WaterDrops::ms_numDrops || WaterDrops::ms_numDropsMoving))
                WaterDrops::Clear();
    
            auto pDevice = *(LPDIRECT3DDEVICE9*)ppDevice;
            WaterDrops::ms_rainIntensity = 1.0f;
    
            auto right = *(RwV3d*)(dw_1138E20 + 0x00);
            auto up = *(RwV3d*)(dw_1138E20 + 0x10);
            auto at = *(RwV3d*)(dw_1138E20 + 0x20);
            auto pos = *(RwV3d*)(dw_1138E20 + 0x30);
    
            WaterDrops::right.x = -right.x;
            WaterDrops::right.y = -right.y;
            WaterDrops::right.z = -right.z;
            WaterDrops::up = up;
            WaterDrops::at = at;
            WaterDrops::pos = pos;
        
            WaterDrops::Process(pDevice);
            WaterDrops::Render(pDevice);
            WaterDrops::ms_rainIntensity = 0.0f;
        }
    }; injector::MakeInline<RenderPhaseCallbackHook>(pattern.get_first(0));

    pattern = hook::pattern("0F 57 C0 B8 ? ? ? ? B9");
    struct ResetHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.eax = 0xFFFFFFFE;
            WaterDrops::Reset();
        }
    }; injector::MakeInline<ResetHook>(pattern.get_first(3));

    pattern = hook::pattern("D8 0D ? ? ? ? 83 C0 30");
    WaterDrops::fTimeStep = *pattern.get_first<float*>(-9);
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