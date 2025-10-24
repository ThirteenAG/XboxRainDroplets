#include <injector\injector.hpp>
#include <injector\hooking.hpp>
#include <injector\calling.hpp>
#include <injector\utility.hpp>
#include <injector\assembly.hpp>
#include "xrd11.h"

void Init()
{
    WaterDrops::ReadIniSettings();

    WaterDrops::CreateRenderTargetFromBackBuffer = false;
    WaterDrops::ms_rainIntensity = 0.0f;

    auto pattern = hook::pattern("F3 0F 7E 86 ? ? ? ? 8D 4C 40 ? 66 0F D6 04 8E 8B 96 ? ? ? ? 8D 04 8E 89 50 ? 8B 86 ? ? ? ? 40 99 B9 ? ? ? ? F7 F9 8B 4E ? 81 C1 ? ? ? ? 89 96 ? ? ? ? 8B 11 F2 0F 10 82 ? ? ? ? 66 0F 5A C0 F3 0F 11 86 ? ? ? ? F3 0F 10 86 ? ? ? ? 0F 2E C1 9F F6 C4 ? 7B ? 8B 01 F2 0F 10 88 ? ? ? ? 0F 5A C0 F2 0F 5C CA 66 0F 2F C8 76 ? 8B 46 ? 83 B8 ? ? ? ? ? 8D 88 ? ? ? ? 75 ? 8D 8E ? ? ? ? 8B 86 ? ? ? ? F3 0F 7E 01 8D 14 40 66 0F D6 84 96 ? ? ? ? 8B 49 ? 8D 84 96 ? ? ? ? 89 48 ? 8B 86 ? ? ? ? 40 99 B9 ? ? ? ? F7 F9 89 96 ? ? ? ? 8B 56 ? 8B 82 ? ? ? ? F2 0F 10 80 ? ? ? ? 66 0F 5A C0 F3 0F 11 86 ? ? ? ? 8B CE E8 ? ? ? ? 32 C0 88 46 ? 88 86 ? ? ? ? E9");
    static auto CameraHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
    {
        struct FVector
        {
            float X, Y, Z;
        };

        struct FRotator
        {
            int Pitch, Yaw, Roll;
        };

        auto gCamPos = (FVector*)(regs.esi + 0xE0);
        auto gCamRot = (FRotator*)(regs.esi + 0x6C);
        WaterDrops::fTimeStep = (float*)(regs.esi + 0x00);

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
    });

    pattern = hook::pattern("E8 ? ? ? ? F3 0F 10 45 ? 56 51 8B CF");
    static auto BeforeUpdateRain = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
    {
        WaterDrops::ms_rainIntensity -= 0.05f;
        if (WaterDrops::ms_rainIntensity < 0.0f)
            WaterDrops::ms_rainIntensity = 0.0f;

        #ifdef DEBUG
        WaterDrops::ms_rainIntensity = 1.0f;
        #endif // DEBUG
    });

    pattern = hook::pattern("F3 0F 11 44 24 ? F3 0F 11 45 ? F3 0F 11 04 24 E8 ? ? ? ? 8B 7D F0 8A 55 0B");
    static auto OnUpdateRain = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
    {
        WaterDrops::ms_rainIntensity = 1.0f;
        regs.xmm0.f32[0] = 0.0f; // disables original droplets
    });

    pattern = hook::pattern("E8 ? ? ? ? 83 C4 1C 8B CB");
    static auto BeforeRenderHUDPrimitives = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
    {
        WaterDrops::Process();
        WaterDrops::Render();
    });

    pattern = hook::pattern("8B 46 58 8D 95");
    static auto ResizeDXGIBuffers = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
    {
        auto pSwapChain = *(IDXGISwapChain**)(regs.esi + 0x58);
        WaterDrops::Reset();
        Sire::Shutdown();
        Sire::Init(Sire::SIRE_RENDERER_DX11, pSwapChain);
    });

    pattern = hook::pattern("89 85 ? ? ? ? 85 C0 79 07");
    static auto AfterD3D11CreateDeviceAndSwapChain = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
    {
        auto pSwapChain = *(IDXGISwapChain**)(regs.edi + 0x330);

        //static bool bOnce = false;
        //if (!bOnce)
        //{
        //    bOnce = true;
        //    ID3D11Device* pDevice = nullptr;
        //    HRESULT ret = pSwapChain->GetDevice(__uuidof(ID3D11Device), (PVOID*)&pDevice);
        //    ID3D10Multithread* multithread = nullptr;
        //    if (SUCCEEDED(pDevice->QueryInterface(__uuidof(ID3D10Multithread), (void**)&multithread))) {
        //        multithread->SetMultithreadProtected(TRUE);
        //        multithread->Release();
        //    }
        //    pDevice->Release();
        //}

        Sire::Init(Sire::SIRE_RENDERER_DX11, pSwapChain);
    });
}

extern "C" __declspec(dllexport) void InitializeASI()
{
    std::call_once(CallbackHandler::flag, []()
    {
        CallbackHandler::RegisterCallback(Init, hook::pattern("89 85 ? ? ? ? 85 C0 79 07"));
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