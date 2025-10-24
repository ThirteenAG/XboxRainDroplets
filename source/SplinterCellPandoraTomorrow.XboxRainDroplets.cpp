#define DIRECT3D_VERSION 0x0800
#include "xrd.h"

struct FVector
{
    float X, Y, Z;
};

struct FRotator
{
    int Pitch, Yaw, Roll;
};

uint8_t(__fastcall* ULevel__IsInRainVolume)(void* uLevel, void* edx, FVector* a2) = nullptr;

FVector* gCamPos = nullptr;
FRotator* gCamRot = nullptr;
SafetyHookInline shULevel__Tick = {};
int __fastcall ULevel__Tick(void* uLevel, void* edx, int a2, float a3)
{
    if (gCamPos && gCamRot && ULevel__IsInRainVolume(uLevel, 0, gCamPos))
    {
        WaterDrops::ms_rainIntensity = 1.0f;

        constexpr float UnrealToRadians = (2.0f * 3.14159265359f) / 65536.0f;

        float SR = sinf(gCamRot->Roll * UnrealToRadians);
        float CR = cosf(gCamRot->Roll * UnrealToRadians);
        float SP = sinf(gCamRot->Pitch * UnrealToRadians);
        float CP = cosf(gCamRot->Pitch * UnrealToRadians);
        float SY = sinf(gCamRot->Yaw * UnrealToRadians);
        float CY = cosf(gCamRot->Yaw * UnrealToRadians);

        // Build RwMatrix from Unreal rotation matrix
        RwMatrix matrix;

        // Right vector (from M[1][0-2])
        matrix.right.x = SR * SP * CY - CR * SY;
        matrix.right.y = SR * SP * SY + CR * CY;
        matrix.right.z = -SR * CP;
        matrix.flags = 0;

        // Up vector (from M[2][0-2])
        matrix.up.x = -(CR * SP * CY + SR * SY);
        matrix.up.y = CY * SR - CR * SP * SY;
        matrix.up.z = CR * CP;
        matrix.pad1 = 0;

        // At vector (from M[0][0-2]) - Forward
        matrix.at.x = CP * CY;
        matrix.at.y = CP * SY;
        matrix.at.z = SP;
        matrix.pad2 = 0;

        // Position
        matrix.pos.x = gCamPos->X;
        matrix.pos.y = gCamPos->Y;
        matrix.pos.z = gCamPos->Z;
        matrix.pad3 = 0;

        // Apply to WaterDrops
        WaterDrops::right.x = -matrix.right.x;
        WaterDrops::right.y = -matrix.right.y;
        WaterDrops::right.z = -matrix.right.z;
        WaterDrops::up = matrix.up;
        WaterDrops::at = matrix.at;
        WaterDrops::pos = matrix.pos;

        if (WaterDrops::fSpeedAdjuster)
        {
            WaterDrops::right.x *= WaterDrops::fSpeedAdjuster;
            WaterDrops::right.y *= WaterDrops::fSpeedAdjuster;
            WaterDrops::right.z *= WaterDrops::fSpeedAdjuster;
            WaterDrops::up.x *= WaterDrops::fSpeedAdjuster;
            WaterDrops::up.y *= WaterDrops::fSpeedAdjuster;
            WaterDrops::up.z *= WaterDrops::fSpeedAdjuster;
        }
    }
    else
        WaterDrops::ms_rainIntensity = 0.0f;

    return shULevel__Tick.unsafe_fastcall<int>(uLevel, edx, a2, a3);
}

void InitEngine()
{
    auto pattern = hook::module_pattern(GetModuleHandle(L"Engine"), "55 8B EC 53 8B D9 56 57 33 F6");
    ULevel__IsInRainVolume = (decltype(ULevel__IsInRainVolume))pattern.get_first();

    pattern = hook::module_pattern(GetModuleHandle(L"Engine"), "55 8B EC 6A ? 68 ? ? ? ? 64 A1 ? ? ? ? 50 83 EC ? 53 56 57 A1 ? ? ? ? 33 C5 50 8D 45 ? 64 A3 ? ? ? ? 8B D9 8B 43 ? 8B 38");
    shULevel__Tick = safetyhook::create_inline(pattern.get_first(), ULevel__Tick);

    pattern = hook::module_pattern(GetModuleHandle(L"Engine"), "8B 4E ? F7 C1 ? ? ? ? 74 ? 8A 86");
    static auto CameraHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
    {
        gCamPos = (FVector*)(regs.esi + 0x14C);
        gCamRot = (FRotator*)(regs.esi + 0x14C + sizeof(FVector));
    });
}

void InitD3DDrv()
{
    WaterDrops::ReadIniSettings();

    auto pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "FF 92 ? ? ? ? 8B 03 8B CB FF 50 ? 8D 8D ? ? ? ? FF 15 ? ? ? ? 8D 8D ? ? ? ? FF 15 ? ? ? ? 8B 4D ? 64 89 0D ? ? ? ? 59 5F 5E 5B 8B 4D ? 33 CD E8 ? ? ? ? 8B E5 5D C2");
    static auto RenderHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
    {
        auto pDevice = *(LPDIRECT3DDEVICE8*)(*(uintptr_t*)(regs.ebx + 0x1C) + 0x4694);
        WaterDrops::Process(pDevice);
        WaterDrops::Render(pDevice);
        WaterDrops::ms_rainIntensity = 0.0f;
    });

    pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "FF 50 ? 8D 8D ? ? ? ? C7 86 ? ? ? ? ? ? ? ? FF 15 ? ? ? ? 8D 8D ? ? ? ? FF 15 ? ? ? ? 8B 4D ? 64 89 0D ? ? ? ? 59 5F 5E 8B 4D ? 33 CD E8 ? ? ? ? 8B E5 5D 8B E3 5B C2");
    static auto RenderHookNV = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
    {
        auto pDevice = *(LPDIRECT3DDEVICE8*)(*(uintptr_t*)(regs.esi + 0x1C) + 0x4694);
        WaterDrops::Process(pDevice);
        WaterDrops::Render(pDevice);
        WaterDrops::ms_rainIntensity = 0.0f;
    });

    pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "8B CE E8 ? ? ? ? 8B 07");
    static auto DeviceResetHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
    {
        WaterDrops::Reset();
    });
}

extern "C" __declspec(dllexport) void InitializeASI()
{
    std::call_once(CallbackHandler::flag, []()
    {
        CallbackHandler::RegisterCallback(L"Engine.dll", InitEngine);
        CallbackHandler::RegisterCallback(L"D3DDrv.dll", InitD3DDrv);
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