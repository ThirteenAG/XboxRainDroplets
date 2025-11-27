#define DIRECT3D_VERSION         0x0800
#include "xrd.h"

IDirect3DDevice8* pDevice = nullptr;

std::string currentLevelRoomName;
int __stdcall sub_10001770(float a1)
{
    if (a1 == -1000.0f)
    {
        if (currentLevelRoomName != "02_kitchen" && currentLevelRoomName != "StartRoom" && currentLevelRoomName != "memoriam")
            WaterDrops::ms_rainIntensity = 1.0f;
        else
            WaterDrops::ms_rainIntensity = 0.0f;
    }
    else
        WaterDrops::ms_rainIntensity = 0.0f;
    return (int)a1;
}

void __fastcall TransformPoint3D(float* point, void* edx, float* matrix)
{
    float x = point[0];
    float y = point[1];
    float z = point[2];

    float transformedX = x * matrix[0] + y * matrix[3] + z * matrix[6] + matrix[9];
    float transformedY = x * matrix[1] + y * matrix[4] + z * matrix[7] + matrix[10];
    float transformedZ = x * matrix[2] + y * matrix[5] + z * matrix[8] + matrix[11];

    point[0] = transformedX;
    point[1] = transformedY;
    point[2] = transformedZ;

    auto right = RwV3d{ matrix[0], matrix[1], matrix[2] };
    auto up = RwV3d{ matrix[3], matrix[4], matrix[5] };
    auto at = RwV3d{ matrix[6], matrix[7], matrix[8] };
    auto pos = RwV3d{ matrix[9], matrix[10], matrix[11] };

    WaterDrops::right = { at.x, at.y, at.z };
    WaterDrops::up = { up.x, up.y, up.z };
    WaterDrops::at = { right.x, right.y, right.z };
    WaterDrops::pos = { pos.x, pos.y, pos.z };
}

void Init()
{
    WaterDrops::ReadIniSettings();

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

    pattern = hook::pattern("84 C0 74 ? 8D 8E ? ? ? ? 57");
    static auto X_LevelRuntimeCameraroomsToRenderWithSkyboxHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
    {
        if ((regs.eax & 0xFF) == 0)
            WaterDrops::ms_rainIntensity = 0.0f;
    });
}

safetyhook::MidHook GetDeviceHook = {};
safetyhook::MidHook ResetHook = {};
void InitE2_D3D8_DRIVER_MFC()
{
    auto pattern = hook::module_pattern(GetModuleHandle(L"e2_d3d8_driver_mfc"), "8B 86 ? ? ? ? 85 C0 74 0F 8B 50 50");
    GetDeviceHook = safetyhook::create_mid(pattern.get_first(6), [](SafetyHookContext& regs)
    {
        pDevice = (IDirect3DDevice8*)(*(uint32_t*)(regs.esi + 0x100));
    });

    pattern = hook::module_pattern(GetModuleHandle(L"e2_d3d8_driver_mfc"), "8B 86 ? ? ? ? 8B 10 8D 8E ? ? ? ? 51 50 FF 52 38");
    ResetHook = safetyhook::create_mid(pattern.get_first(6), [](SafetyHookContext& regs)
    {
        WaterDrops::Reset();
    });
}

void InitE2MFC()
{

}

void InitX_GameObjectsMFC()
{
    auto pattern = hook::module_pattern(GetModuleHandle(L"X_GameObjectsMFC"), "8D 4C 24 08 E8 ? ? ? ? D9 44 24 0C");
    injector::MakeCALL(pattern.get_first(4), TransformPoint3D, true);

    pattern = hook::module_pattern(GetModuleHandle(L"X_GameObjectsMFC"), "84 C0 74 ? 8B 9C 24");
    static auto X_LevelRuntimeCameraroomsToRenderWithSkyboxHook = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& regs)
    {
        if ((regs.eax & 0xFF) == 0)
            WaterDrops::ms_rainIntensity = 0.0f;
    });
}

void InitX_ModesMFC()
{

}

void InitX_sndmfc()
{
    auto pattern = hook::module_pattern(GetModuleHandle(L"sndmfc.dll"), "E8 ? ? ? ? 89 86 ? ? ? ? 5E C2 08 00 8B 44 24 0C 50");
    injector::MakeCALL(pattern.get_first(0), sub_10001770, true);
}

SafetyHookInline shX_LevelRuntimeRoom__getLevelRoom = {};
void* __fastcall X_LevelRuntimeRoom__getLevelRoom(void* X_LevelRuntimeRoom, void* edx)
{
    auto sv = std::string_view(*(const char**)((uintptr_t)X_LevelRuntimeRoom + 0x14) + 0xA);
    currentLevelRoomName = sv;
    return shX_LevelRuntimeRoom__getLevelRoom.unsafe_fastcall<void*>(X_LevelRuntimeRoom, edx);
}

void InitX_LevelRuntimeMFC()
{
    shX_LevelRuntimeRoom__getLevelRoom = safetyhook::create_inline(GetProcAddress(GetModuleHandle(L"X_LevelRuntimeMFC"), "?getLevelRoom@X_LevelRuntimeRoom@@QBEPBVX_LevelRoom@@XZ"), X_LevelRuntimeRoom__getLevelRoom);
}

extern "C" __declspec(dllexport) void InitializeASI()
{
    std::call_once(CallbackHandler::flag, []()
    {
        CallbackHandler::RegisterCallback(Init);
        CallbackHandler::RegisterCallback(L"E2MFC.dll", InitE2MFC);
        CallbackHandler::RegisterCallback(L"E2_D3D8_DRIVER_MFC.dll", InitE2_D3D8_DRIVER_MFC);
        CallbackHandler::RegisterModuleUnloadCallback(L"E2_D3D8_DRIVER_MFC.dll", []() { GetDeviceHook.reset(); ResetHook.reset(); });
        CallbackHandler::RegisterCallback(L"X_GameObjectsMFC.dll", InitX_GameObjectsMFC);
        CallbackHandler::RegisterCallback(L"X_ModesMFC.dll", InitX_ModesMFC);
        CallbackHandler::RegisterCallback(L"sndmfc.dll", InitX_sndmfc);
        CallbackHandler::RegisterCallback(L"X_LevelRuntimeMFC.dll", InitX_LevelRuntimeMFC);
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
