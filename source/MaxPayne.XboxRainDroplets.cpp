#define DX8
#define SNOWDROPS
#include "xrd.h"

IDirect3DDevice8* pDevice = nullptr;

auto getFirstCamera = (int(*)())nullptr;
auto getNextCamera = (int(*)())nullptr;
float** flt_1007A828 = nullptr;

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
    
    auto pattern = hook::pattern("8B F1 8B 4E 0C FF 15");
    struct RenderHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.esi = regs.ecx;
            regs.ecx = *(uint32_t*)(regs.esi + 0x0C);
            
            WaterDrops::ms_rainIntensity = **flt_1007A828 == 1.0f ? 0.0f : 1.0f;

            auto right = *(RwV3d*)0x8ABA68;
            auto up = *(RwV3d*)0x008ABA74;
            auto at = *(RwV3d*)0x008ABA80;
            auto pos = *(RwV3d*)0x008ABA98;

            WaterDrops::up = { at.x, at.y, at.z };
            WaterDrops::at = { right.x, right.y, right.z };
            WaterDrops::right = { up.x, up.y, up.z };
            WaterDrops::pos = { pos.x, pos.y, pos.z };

            WaterDrops::Process(pDevice);
            WaterDrops::Render(pDevice);
            WaterDrops::ms_rainIntensity = 0.0f;
        }
    }; injector::MakeInline<RenderHook>(pattern.get_first(0));
}

void InitE2_D3D8_DRIVER_MFC()
{
    auto pattern = hook::module_pattern(GetModuleHandle(L"e2_d3d8_driver_mfc"), "8B 86 ? ? ? ? 85 C0 74 0F 8B 50 38 8B 48 34");
    struct GetDeviceHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.eax = *(uint32_t*)(regs.esi + 0x11C);
            pDevice = (IDirect3DDevice8*)(*(uint32_t*)(regs.esi + 0x0D0));
        }
    }; injector::MakeInline<GetDeviceHook>(pattern.get_first(0), pattern.get_first(6));

    pattern = hook::module_pattern(GetModuleHandle(L"e2_d3d8_driver_mfc"), "8B 86 ? ? ? ? 8B 10 8D 8E ? ? ? ? 51 50 FF 52 38");
    struct ResetHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.eax = *(uint32_t*)(regs.esi + 0x0D0);
            WaterDrops::Reset();
        }
    }; injector::MakeInline<ResetHook>(pattern.get_first(0), pattern.get_first(6));

    pattern = hook::module_pattern(GetModuleHandle(L"e2_d3d8_driver_mfc"), "D9 1D ? ? ? ? D9 86 ? ? ? ? C7 05 ? ? ? ? ? ? ? ? D8 0D ? ? ? ? D9 1D ? ? ? ? D9 86 ? ? ? ? D8 0D ? ? ? ? D9 1D ? ? ? ? D9 86 ? ? ? ? D8 0D ? ? ? ? D9 1D ? ? ? ? D9 86 ? ? ? ? C7 05 ? ? ? ? ? ? ? ? D8 0D ? ? ? ? D9 1D ? ? ? ? 8A 86 ? ? ? ? C0 E8 05 A8 01 75 04 8B CE FF D7 8B 8E ? ? ? ? 89 0D ? ? ? ? 8A 96 ? ? ? ? C0 EA 05 F6 C2 01 75 04 8B CE FF D7 8B 86 ? ? ? ? A3 ? ? ? ? 8A 8E ? ? ? ? C0 E9 05 F6 C1 01 75 04 8B CE FF D7 8B 96");
    flt_1007A828 = pattern.get_first<float*>(2);
}

void InitE2MFC()
{
    auto pattern = hook::module_pattern(GetModuleHandle(L"E2MFC"), "8B 0D ? ? ? ? 85 C9 75 03 33 C0 C3 8B 41 04 8B 00 A3 ? ? ? ? 3B 41 04 75 03 33 C0 C3 8B 40 0C C3");
    getFirstCamera = (int(*)())pattern.get_first();
    pattern = hook::module_pattern(GetModuleHandle(L"E2MFC"), "56 8B 35 ? ? ? ? 85 F6 75 04 33 C0 5E C3 A1 ? ? ? ? 8B 48 08 8B 15 ? ? ? ? 3B CA 74 10 8B 01 3B C2");
    getNextCamera = (int(*)())pattern.get_first();
}

extern "C" __declspec(dllexport) void InitializeASI()
{
    std::call_once(CallbackHandler::flag, []()
    {
        CallbackHandler::RegisterCallback(Init);
        CallbackHandler::RegisterCallback(L"E2MFC.dll", InitE2MFC);
        CallbackHandler::RegisterCallback(L"E2_D3D8_DRIVER_MFC.dll", InitE2_D3D8_DRIVER_MFC);
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
