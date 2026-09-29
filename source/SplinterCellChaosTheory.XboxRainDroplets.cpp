// Direct3D 9, the API this game uses
#define XRD_ENABLE_D3D9
#include "xrd/xrd.h"

struct FVector
{
    float X, Y, Z;
};

struct FRotator
{
    int Pitch, Yaw, Roll;
};

struct CameraData
{
    FVector Location;
    FRotator Rotation;
};

uint8_t(__fastcall* ULevel__IsInRainVolume)(void* uLevel, void* edx, FVector* a2) = nullptr;

uintptr_t gCurrentPlayerController = 0;
SafetyHookInline shULevel__Tick = {};
int __fastcall ULevel__Tick(void* uLevel, void* edx, int a2, float a3)
{
    static float TimeStep = 0.0f;
    TimeStep = a3;
    WaterDrops::fTimeStep = &TimeStep;

    if (gCurrentPlayerController)
    {
        auto curCam = (CameraData*)(gCurrentPlayerController + 0xE8);

        if (ULevel__IsInRainVolume(uLevel, 0, &curCam->Location))
        {
            WaterDrops::ms_rainIntensity = 1.0f;

            constexpr float UnrealToRadians = (2.0f * 3.14159265359f) / 65536.0f;

            float SR = sinf(curCam->Rotation.Roll * UnrealToRadians);
            float CR = cosf(curCam->Rotation.Roll * UnrealToRadians);
            float SP = sinf(curCam->Rotation.Pitch * UnrealToRadians);
            float CP = cosf(curCam->Rotation.Pitch * UnrealToRadians);
            float SY = sinf(curCam->Rotation.Yaw * UnrealToRadians);
            float CY = cosf(curCam->Rotation.Yaw * UnrealToRadians);

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
            matrix.pos.x = curCam->Location.X;
            matrix.pos.y = curCam->Location.Y;
            matrix.pos.z = curCam->Location.Z;
            matrix.pad3 = 0;

            // Apply to WaterDrops
            WaterDrops::right.x = -matrix.right.x;
            WaterDrops::right.y = -matrix.right.y;
            WaterDrops::right.z = -matrix.right.z;
            WaterDrops::up = matrix.up;
            WaterDrops::at.x = matrix.at.x * 2.0f;
            WaterDrops::at.y = matrix.at.y * 2.0f;
            WaterDrops::at.z = matrix.at.z * 2.0f;
            WaterDrops::pos = matrix.pos;
        }
    }
    gCurrentPlayerController = 0;
    return shULevel__Tick.unsafe_fastcall<int>(uLevel, edx, a2, a3);
}

SafetyHookInline shAPlayerController__Tick = {};
int __fastcall APlayerController__Tick(void* PlayerController, int a2, int a3, int a4)
{
    static void* prevPlayerController = nullptr;
    gCurrentPlayerController = (uintptr_t)PlayerController;

    if (PlayerController != prevPlayerController)
    {
        prevPlayerController = PlayerController;
        WaterDrops::Clear();
    }

    return shAPlayerController__Tick.unsafe_fastcall<int>(PlayerController, a2, a3, a4);
}

void Init()
{
    WaterDrops::ReadIniSettings();

    auto pattern = hook::pattern("53 56 57 8B F9 8B 87 DC 39 00 00 33 F6 85 C0 7E ? 8B 5C 24 10");
    ULevel__IsInRainVolume = (decltype(ULevel__IsInRainVolume))pattern.count(2).get(0).get<void>(0);

    pattern = hook::pattern("A1 ? ? ? ? 83 EC 24 85 C0");
    shULevel__Tick = safetyhook::create_inline(pattern.get_first(), ULevel__Tick);

    pattern = hook::pattern("8B 88 ? ? ? ? 8B 6E");
    shAPlayerController__Tick = safetyhook::create_inline(pattern.get_first(-10), APlayerController__Tick);

    // This is the game's shared resource teardown, called before both Reset
    // and device destruction. Releasing at CreateDevice is too late for Reset.
    pattern = hook::pattern("53 55 56 57 8B F1 E8 ? ? ? ? 8B 86 3C 4D 00 00 8B 08 33 ED 55 50 FF 91 70 01 00 00");
    static auto ReleaseDeviceResourcesHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext&)
    {
        WaterDrops::Reset();
        // The game may release/recreate the device when Reset fails. Forget the
        // old pointer even if the allocator reuses its address on recreation.
        Xrd::Shutdown();
    });

    // All three shader/HDR paths return through these epilogues in the
    // post-process dispatcher. The old hooks ran inside the combine pass (or
    // before HDR history was copied), feeding our output into later game passes.
    // ESI still holds the renderer here, before the epilogue restores it.
    pattern = hook::pattern("5E 88 1D ? ? ? ? 5B 83 C4 10 C2 08 00");
    pattern.count(3);
    static std::array<SafetyHookMid, 3> PostProcessHooks;
    for (size_t i = 0; i < PostProcessHooks.size(); ++i)
    {
        PostProcessHooks[i] = safetyhook::create_mid(pattern.get(i).get<void>(0), [](SafetyHookContext& regs)
        {
            auto* renderDevice = *reinterpret_cast<uint8_t**>(regs.esi + 0x1A4);
            auto* pDevice = *reinterpret_cast<IDirect3DDevice9**>(renderDevice + 0x4D3C);
            Xrd::Init(XRD_DEVICE_RENDERER, pDevice);
            WaterDrops::Process();
            WaterDrops::Render();
            WaterDrops::ms_rainIntensity = 0.0f;
        });
    }
}

extern "C" __declspec(dllexport) void InitializeASI()
{
    std::call_once(CallbackHandler::flag, []()
    {
        CallbackHandler::RegisterCallbackAtGetSystemTimeAsFileTime(Init, hook::pattern("53 56 57 8B F9 8B 87 DC 39 00 00 33 F6 85 C0 7E ? 8B 5C 24 10"));
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