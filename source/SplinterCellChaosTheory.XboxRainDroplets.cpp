#include "xrd.h"

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
    
    pattern = hook::pattern("83 EC 08 55 56 8B F1 8B 46 7C");
    shAPlayerController__Tick = safetyhook::create_inline(pattern.get_first(), APlayerController__Tick);

    pattern = hook::pattern("8B 08 8D 9E 3C 4D 00 00");
    static auto CreateDeviceHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
    {
        WaterDrops::Reset();

        auto SafeRelease = [](auto ppT)
        {
            if (*ppT)
            {
                (*ppT)->Release();
                *ppT = NULL;
            }
        };

        SafeRelease(&WaterDrops::ms_vertexBuf);
        SafeRelease(&WaterDrops::ms_indexBuf);
    });

    pattern = hook::pattern("8B 10 50 FF 92 ? ? ? ? 8B 45 00 39 86 98 56 00 00");
    static auto RenderHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
    {
        IDirect3DDevice9* pDevice = (IDirect3DDevice9*)(regs.eax);
        WaterDrops::Process(pDevice);
        WaterDrops::Render(pDevice);
        WaterDrops::ms_rainIntensity = 0.0f;
    });
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