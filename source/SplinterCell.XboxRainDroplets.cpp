// Direct3D 8, the API this game uses
#define XRD_ENABLE_D3D8
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
        auto curCam = (CameraData*)(gCurrentPlayerController + 0xD4);

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
int __fastcall APlayerController__Tick(void* PlayerController, int a2, float a3, int a4)
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

void InitEngine()
{
    ULevel__IsInRainVolume = (decltype(ULevel__IsInRainVolume))GetProcAddress(GetModuleHandle(L"Engine"), "?IsInRainVolume@ULevel@@UAEEAAVFVector@@@Z");
    shULevel__Tick = safetyhook::create_inline(GetProcAddress(GetModuleHandle(L"Engine"), "?Tick@ULevel@@UAEXW4ELevelTick@@M@Z"), ULevel__Tick);
    shAPlayerController__Tick = safetyhook::create_inline(GetProcAddress(GetModuleHandle(L"Engine"), "?Tick@APlayerController@@UAEHMW4ELevelTick@@@Z"), APlayerController__Tick);
}

// In a window the back buffer can be larger than the window, and only the part the
// viewport covers is shown: the drops are placed in that part and refract only it.
void SetVisibleArea(LPDIRECT3DDEVICE8 pDevice)
{
    static Xrd::RenderTarget visible;
    IDirect3DSurface8* pTarget = nullptr;
    D3DSURFACE_DESC desc{};
    D3DVIEWPORT8 viewport{};
    if (FAILED(pDevice->GetRenderTarget(&pTarget)) || !pTarget)
        return;
    const bool described = SUCCEEDED(pTarget->GetDesc(&desc));
    pTarget->Release();
    if (!described || !desc.Width || !desc.Height || FAILED(pDevice->GetViewport(&viewport)) || !viewport.Width || !viewport.Height)
        return;

    visible.size = { (int32_t)(viewport.X + viewport.Width), (int32_t)(viewport.Y + viewport.Height) };
    Xrd::SetTarget(&visible);
    WaterDrops::SetXUVScale(0.0f, (float)visible.size.width / (float)desc.Width);
    WaterDrops::SetYUVScale(0.0f, (float)visible.size.height / (float)desc.Height);
}

void InitD3DDrv()
{
    WaterDrops::ReadSettings();

    // UD3DRenderDevice::Unlock, right before EndScene: the frame is complete here.
    // The EndScene call itself may be hooked by other plugins, so only the load of the device is matched.
    auto pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "8D 8E 88 46 00 00 E8 ? ? ? ? 8B 86 84 46 00 00 50");
    static auto RenderHook = safetyhook::create_mid(pattern.get_first(11), [](SafetyHookContext& regs)
    {
        auto pDevice = *(LPDIRECT3DDEVICE8*)(regs.esi + 0x4684);
        Xrd::Init(XRD_DEVICE_RENDERER, pDevice);
        SetVisibleArea(pDevice);
        WaterDrops::Process();
        WaterDrops::Render();
        WaterDrops::ms_rainIntensity = 0.0f;
    });

    pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "8B 10 FF 52 ? 8B 06");
    static auto DeviceResetHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
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

    });
}

extern "C" __declspec(dllexport) void InitializeASI()
{
    std::call_once(CallbackHandler::flag, []()
    {
        RegisterOldPluginCheck();
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