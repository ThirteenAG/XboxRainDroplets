#define DX8
#include "xrd.h"

injector::hook_back<void*(__fastcall*)(void* _this, void* edx, char* name, RwMatrix* pos, int a3, int a4)> hb_CreateFxSystem;
void* __fastcall CreateFxSystem(void* _this, void* edx, char* name, RwMatrix* pos, int a3, int a4)
{
    std::string_view name_view(name);
    if (name_view == "FXP001" || name_view == "FXP002" || name_view == "FXP003" || name == "FXBTMET" || name_view == "FXBTMET2" || name_view == "FXRAT1")
    {
        RwV3d prt_pos = { pos->pos.x, pos->pos.y, pos->pos.z };
        auto len = WaterDrops::GetDistanceBetweenEmitterAndCamera(prt_pos);
        WaterDrops::FillScreenMoving(WaterDrops::GetDropsAmountBasedOnEmitterDistance(len, 50.0f, 100.0f), true);
    }
    return hb_CreateFxSystem.fun(_this, edx, name, pos, a3, a4);
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
    
    static auto bCameraCovered = (bool*)0x7B3275;
    static auto fPlayerInsideNess = (float*)0x7B3278;
    static auto OldWeatherType = (int16_t*)0x7B3238;
    static IDirect3DDevice8* pDev = (IDirect3DDevice8*)(0x824448);
    struct RainDropletsHook
    {
        void operator()(injector::reg_pack& regs)
        {
            if (*(uintptr_t*)0x79A938)
            {
                if ((*OldWeatherType == 2 || *OldWeatherType == 3) && !*fPlayerInsideNess && !*bCameraCovered)
                    WaterDrops::ms_rainIntensity = 1.0f;
                else
                    WaterDrops::ms_rainIntensity = 0.0f;
                auto matrix = (RwMatrix*)((*(uintptr_t*)(*(uintptr_t*)0x79A938 + 4) + 0x10));
                WaterDrops::right = matrix->right;
                WaterDrops::up.x = -matrix->up.x;
                WaterDrops::up.y = -matrix->up.y;
                WaterDrops::up.z = -matrix->up.z;
                WaterDrops::at = matrix->at;
                WaterDrops::pos = matrix->pos;
                auto pDevice = *(LPDIRECT3DDEVICE8*)pDev;
                WaterDrops::Process(pDevice);
                WaterDrops::Render(pDevice);
                WaterDrops::ms_rainIntensity = 0.0f;
            }
        }
    }; injector::MakeInline<RainDropletsHook>(0x476109);

    struct ResetHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.eax = *(uint32_t*)pDev;
            WaterDrops::Reset();
        }
    }; injector::MakeInline<ResetHook>(0x64169A);

    hb_CreateFxSystem.fun = injector::MakeCALL(0x494BA4, CreateFxSystem, true).get();
    hb_CreateFxSystem.fun = injector::MakeCALL(0x4B55AA, CreateFxSystem, true).get();
    hb_CreateFxSystem.fun = injector::MakeCALL(0x4F21C1, CreateFxSystem, true).get();
    hb_CreateFxSystem.fun = injector::MakeCALL(0x4F468E, CreateFxSystem, true).get();
    hb_CreateFxSystem.fun = injector::MakeCALL(0x5B0CF2, CreateFxSystem, true).get();
    hb_CreateFxSystem.fun = injector::MakeCALL(0x5CF969, CreateFxSystem, true).get();
    hb_CreateFxSystem.fun = injector::MakeCALL(0x5D1BC3, CreateFxSystem, true).get();
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