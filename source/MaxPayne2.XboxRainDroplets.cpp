#define DX8
#include "xrd.h"

IDirect3DDevice8* pDevice = nullptr;

int __stdcall sub_10001770(float a1)
{
    if (a1 == -1000.0f)
        WaterDrops::ms_rainIntensity = 1.0f;
    else
        WaterDrops::ms_rainIntensity = 0.0f;
    return (int)a1;
}

void __fastcall sub_10004820(float* _this, void* edx, float* a2)
{
    _this[0] = *_this * a2[0] + _this[1] * a2[3] + _this[2] * a2[6] + a2[9];
    _this[1] = *_this * a2[1] + _this[1] * a2[4] + _this[2] * a2[7] + a2[10];
    _this[2] = *_this * a2[2] + _this[1] * a2[5] + _this[2] * a2[8] + a2[11];

    auto right = RwV3d{ a2[0], a2[1], a2[2] };
    auto up = RwV3d{ a2[3], a2[4], a2[5] };
    auto at = RwV3d{ a2[6], a2[7], a2[8] };
    auto pos = RwV3d{ a2[9], a2[10], a2[11] };

    WaterDrops::right = { -at.x, -at.y, -at.z };
    WaterDrops::up = { right.x, right.y, right.z };
    WaterDrops::at = { up.x, up.y, up.z };
    WaterDrops::pos = { pos.x, pos.y, pos.z };
}

void Init()
{
    WaterDrops::bRadial = true;
    auto pattern = hook::pattern("C7 44 24 ? ? ? ? ? 75 07 8A 46 41 84 C0 74 09");
    struct RenderHook
    {
        void operator()(injector::reg_pack& regs)
        {
            *(uint32_t*)(regs.esp + 0x18) = 0;

            WaterDrops::Process(pDevice);
            WaterDrops::Render(pDevice);
            WaterDrops::ms_rainIntensity = 0.0f;
        }
    }; injector::MakeInline<RenderHook>(pattern.get_first(0), pattern.get_first(8));
}

void InitE2_D3D8_DRIVER_MFC()
{
    auto pattern = hook::module_pattern(GetModuleHandle(L"e2_d3d8_driver_mfc"), "8B 86 ? ? ? ? 85 C0 74 0F 8B 50 50");
    struct GetDeviceHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.eax = *(uint32_t*)(regs.esi + 0x14C);
            pDevice = (IDirect3DDevice8*)(*(uint32_t*)(regs.esi + 0x100));
        }
    }; injector::MakeInline<GetDeviceHook>(pattern.get_first(0), pattern.get_first(6));

    pattern = hook::module_pattern(GetModuleHandle(L"e2_d3d8_driver_mfc"), "8B 86 ? ? ? ? 8B 10 8D 8E ? ? ? ? 51 50 FF 52 38");
    struct ResetHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.eax = *(uint32_t*)(regs.esi + 0x100);
            WaterDrops::Reset();
        }
    }; injector::MakeInline<ResetHook>(pattern.get_first(0), pattern.get_first(6));
}

void InitE2MFC()
{

}

void InitX_GameObjectsMFC()
{
    auto pattern = hook::module_pattern(GetModuleHandle(L"X_GameObjectsMFC"), "8D 4C 24 08 E8 ? ? ? ? D9 44 24 0C");
    injector::MakeCALL(pattern.get_first(4), sub_10004820, true);
}

void InitX_ModesMFC()
{

}

void InitX_sndmfc()
{
    auto pattern = hook::module_pattern(GetModuleHandle(L"sndmfc.dll"), "E8 ? ? ? ? 89 86 ? ? ? ? 5E C2 08 00 8B 44 24 0C 50");
    injector::MakeCALL(pattern.get_first(0), sub_10001770, true);
}

extern "C" __declspec(dllexport) void InitializeASI()
{
    std::call_once(CallbackHandler::flag, []()
    {
        CallbackHandler::RegisterCallback(Init);
        CallbackHandler::RegisterCallback(L"E2MFC.dll", InitE2MFC);
        CallbackHandler::RegisterCallback(L"E2_D3D8_DRIVER_MFC.dll", InitE2_D3D8_DRIVER_MFC);
        CallbackHandler::RegisterCallback(L"X_GameObjectsMFC.dll", InitX_GameObjectsMFC);
        CallbackHandler::RegisterCallback(L"X_ModesMFC.dll", InitX_ModesMFC);
        CallbackHandler::RegisterCallback(L"sndmfc.dll", InitX_sndmfc);
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
