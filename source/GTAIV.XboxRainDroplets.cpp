#include "xrd.h"

//#define EFLC

void RegisterFountains()
{
    WaterDrops::RegisterGlobalEmitter({ -234.365f, 769.001f, 6.83f }, 10.0f); //middle park fountain
}

float* CWeatherRain = nullptr;
void __fastcall sub_B870A0(uint8_t* self, void* edx)
{
    if (self[0])
    {
        WaterDrops::FillScreenMoving(50.0f, true); //blood_cam
        self[0] = 0;
    }
    if (self[12])
    {
        WaterDrops::ms_rainIntensity = *CWeatherRain; //rain_drops
        self[12] = 0;
    }
    if (self[28])
    {
        WaterDrops::FillScreenMoving(1.0f); //water_splash_cam
        self[28] = 0;
    }
}

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

    RegisterFountains();

#ifdef EFLC
    CWeatherRain = *hook::get_pattern<float*>("F3 0F 11 05 ? ? ? ? A3 ? ? ? ? A3 ? ? ? ? A3 ? ? ? ? F3 0F 11 0D", 4);
#else
    CWeatherRain = *hook::get_pattern<float*>("F3 0F 11 05 ? ? ? ? E8 ? ? ? ? 84 C0 74 15 E8 ? ? ? ? 84 C0", 4);
#endif
    
    //enable original rain drops render when camera not looking up
#ifdef EFLC
    auto pattern = hook::pattern("76 15 B3 01");
#else
    auto pattern = hook::pattern("74 16 33 C0 80 7C 06");
    injector::MakeNOP(pattern.get_first(-9), 3);
    injector::WriteMemory<uint16_t>(pattern.get_first(-9), 0x01B0, true); //mov al,01
#endif
    injector::MakeNOP(pattern.get_first(0), 2);

    pattern = hook::pattern("56 8B F1 80 3E 00 74 08");
    injector::MakeJMP(pattern.get_first(0), sub_B870A0, true);

#ifdef EFLC
    static auto pCamMatrix = *hook::get_pattern<uint32_t>("B9 ? ? ? ? E8 ? ? ? ? 84 C0 75 13 6A 01 6A 01 68 F4 01 00 00 B9 ? ? ? ? E8 ? ? ? ? 57", 1);
#else
    static auto pCamMatrix = *hook::get_pattern<uint32_t>("7A 21 B9", 3);
#endif
    static auto ppDevice = *hook::get_pattern<uint32_t>("83 C4 0C A1 ? ? ? ? A3", 4);

    pattern = hook::pattern("A2 ? ? ? ? E8 ? ? ? ? 8B CE E8 ? ? ? ? 8B CE E8 ? ? ? ? 5E C2 04 00");
    static auto sub_8297D0 = injector::GetBranchDestination(pattern.get_first(19));
    struct CPostFXHook
    {
        void operator()(injector::reg_pack& regs)
        {
            injector::cstd<void()>::call(sub_8297D0);
    
            auto pDevice = *(LPDIRECT3DDEVICE9*)ppDevice;
            
            auto right = *(RwV3d*)(pCamMatrix + 0x00);
            auto up = *(RwV3d*)(pCamMatrix + 0x10);
            auto at = *(RwV3d*)(pCamMatrix + 0x20);
            auto pos = *(RwV3d*)(pCamMatrix + 0x30);
            
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
    }; injector::MakeInline<CPostFXHook>(pattern.get_first(19));
    
    pattern = hook::pattern("B8 ? ? ? ? B9 ? ? ? ? BF ? ? ? ? F3 AB");
    struct ResetHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.eax = 0xFFFFFFFE;
            WaterDrops::Reset();
        }
    }; injector::MakeInline<ResetHook>(pattern.get_first(0));

#ifdef EFLC
    pattern = hook::pattern("D8 0D ? ? ? ? 83 C0 30");
    WaterDrops::fTimeStep = *pattern.get_first<float*>(-9);
#else
    pattern = hook::pattern("F3 0F 10 05 ? ? ? ? 8B 02 51");
    WaterDrops::fTimeStep = *pattern.get_first<float*>(4);
#endif
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