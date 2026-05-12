#include "xrd.h"

namespace Manhunt2
{
    using GetD3DDeviceFn = LPDIRECT3DDEVICE(__cdecl*)();
    using ScreenSpaceEndFn = void(__cdecl*)();

    static constexpr uintptr_t kRenderHook = 0x005C5FE5; // CALL RslScreenSpaceEnd after RenderBloodDrops in CScene::Render.
    static constexpr uintptr_t kScreenSpaceEnd = 0x0060CEB0;
    static constexpr uintptr_t kResetD3DDevice = 0x00000000; // Not needed since we use TestCooperativeLevel to fix render device loss
    static constexpr uintptr_t kGetD3DDevice = 0x00404010;

    static constexpr uintptr_t kSceneCamera = 0x00789488;
    static constexpr uintptr_t kScenePlayer = 0x00789490;
    static constexpr uintptr_t kWeatherRain = 0x007960D0;
    static constexpr uintptr_t kCameraCovered = 0x0079617A;
    static inline bool bForceRain = false;
    static inline bool bIgnoreCameraCovered = true;
    static inline bool bEnableLookFilter = true;
    static inline float fIntensityScale = 1.0f;
    static inline float fMovementScale = 0.25f;
    static inline float fRainStrengthAtZ = 0.0f;
    static inline float fPitchDownFadeStart = 18.0f;
    static inline float fPitchDownFadeEnd = 30.0f;
    static inline RwV3d lastRawCameraPos = { 0.0f, 0.0f, 0.0f };
    static inline RwV3d scaledCameraPos = { 0.0f, 0.0f, 0.0f };
    static inline bool bCameraPositionInitialized = false;
    static inline bool bDeviceReady = true;

    static constexpr uintptr_t kCreateFxSystemCalls[] = {
        0x00000000,
    };

    static bool IsAddressSet(uintptr_t address)
    {
        return address != 0;
    }

    static LPDIRECT3DDEVICE GetD3DDevice()
    {
        return ((GetD3DDeviceFn)kGetD3DDevice)();
    }

    static void ScreenSpaceEnd()
    {
        ((ScreenSpaceEndFn)kScreenSpaceEnd)();
    }

    static void ResetWaterDrops()
    {
        WaterDrops::Reset();
        bCameraPositionInitialized = false;
    }

    static bool IsDeviceReady(LPDIRECT3DDEVICE pDevice)
    {
        if (!pDevice)
            return false;

        auto hr = pDevice->TestCooperativeLevel();
        if (hr == D3D_OK)
        {
            bDeviceReady = true;
            return true;
        }

        if (bDeviceReady)
        {
            ResetWaterDrops();
            bDeviceReady = false;
        }

        return false;
    }

    static void ScaleCameraMovement(RwMatrix& matrix)
    {
        if (!bCameraPositionInitialized)
        {
            lastRawCameraPos = matrix.pos;
            scaledCameraPos = matrix.pos;
            bCameraPositionInitialized = true;
            return;
        }

        RwV3d delta;
        RwV3dSub(&delta, &matrix.pos, &lastRawCameraPos);
        scaledCameraPos.x += delta.x * fMovementScale;
        scaledCameraPos.y += delta.y * fMovementScale;
        scaledCameraPos.z += delta.z * fMovementScale;
        lastRawCameraPos = matrix.pos;
        matrix.pos = scaledCameraPos;
    }

    static bool GetCameraMatrix(RwMatrix& matrix)
    {
        matrix.right = { 1.0f, 0.0f, 0.0f };
        matrix.up = { 0.0f, 0.0f, 1.0f };
        matrix.at = { 0.0f, 1.0f, 0.0f };
        matrix.pos = { 0.0f, 0.0f, 0.0f };

        auto camera = *(uintptr_t*)kSceneCamera;
        if (!camera)
            return true;

        auto cameraTransform = *(uintptr_t*)(camera + 0x100);
        if (!cameraTransform)
            return true;

        // camera + 0x100 is a CFrame. Local matrix starts at +0x40:
        // right +0x40, up +0x50, at +0x60, pos +0x70.
        matrix = *(RwMatrix*)(cameraTransform + 0x40);
        ScaleCameraMovement(matrix);

        matrix.at.z = fRainStrengthAtZ;
        return true;
    }

    static float GetLookFilter()
    {
        if (!bEnableLookFilter)
            return 1.0f;

        auto player = *(uintptr_t*)kScenePlayer;
        if (!player)
            return 1.0f;

        auto pitch = *(float*)(player + 0x1130);
        auto fadeStart = fPitchDownFadeStart;
        auto fadeEnd = fPitchDownFadeEnd;

        if (fadeEnd <= fadeStart)
            fadeEnd = fadeStart + 1.0f;

        if (pitch <= fadeStart)
            return 1.0f;
        if (pitch >= fadeEnd)
            return 0.0f;

        return 1.0f - ((pitch - fadeStart) / (fadeEnd - fadeStart));
    }

    static float GetRainIntensity()
    {
        auto rain = bForceRain ? 1.0f : *(float*)kWeatherRain;

        auto cameraCovered = *(bool*)kCameraCovered;
        if (!bIgnoreCameraCovered && cameraCovered)
            return 0.0f;

        rain *= fIntensityScale * GetLookFilter();
        return rain > 0.05f ? rain : 0.0f;
    }
}

injector::hook_back<void*(__fastcall*)(void* _this, void* edx, char* name, RwMatrix* pos, int a3, int a4)> hb_CreateFxSystem;
void* __fastcall CreateFxSystem(void* _this, void* edx, char* name, RwMatrix* pos, int a3, int a4)
{
    if (WaterDrops::bBloodDrops && name && pos)
    {
        std::string_view name_view(name);

        // Manhunt particle names. placeholder for manhunt 2 blooddroplets if we want them.
        if (name_view == "FXP001" || name_view == "FXP002" || name_view == "FXP003" ||
            name_view == "FXBTMET" || name_view == "FXBTMET2" || name_view == "FXRAT1")
        {
            RwV3d prt_pos = { pos->pos.x, pos->pos.y, pos->pos.z };
            auto len = WaterDrops::GetDistanceBetweenEmitterAndCamera(prt_pos);
            WaterDrops::FillScreenMoving(WaterDrops::GetDropsAmountBasedOnEmitterDistance(len, 50.0f, 100.0f), true);
        }
    }
    return hb_CreateFxSystem.fun(_this, edx, name, pos, a3, a4);
}

void Init()
{
    WaterDrops::ReadIniSettings();
    CIniReader iniReader("");
    Manhunt2::bForceRain = iniReader.ReadInteger("MANHUNT2", "ForceRain", 0) != 0;
    Manhunt2::bIgnoreCameraCovered = iniReader.ReadInteger("MANHUNT2", "IgnoreCameraCovered", 1) != 0;
    Manhunt2::bEnableLookFilter = iniReader.ReadInteger("MANHUNT2", "EnableLookFilter", 1) != 0;
    Manhunt2::fIntensityScale = iniReader.ReadFloat("MANHUNT2", "IntensityScale", 1.0f);
    Manhunt2::fMovementScale = iniReader.ReadFloat("MANHUNT2", "MovementScale", 0.25f);
    Manhunt2::fRainStrengthAtZ = iniReader.ReadFloat("MANHUNT2", "RainStrengthAtZ", 0.0f);
    Manhunt2::fPitchDownFadeStart = iniReader.ReadFloat("MANHUNT2", "PitchDownFadeStart", 18.0f);
    Manhunt2::fPitchDownFadeEnd = iniReader.ReadFloat("MANHUNT2", "PitchDownFadeEnd", 30.0f);

    if (Manhunt2::IsAddressSet(Manhunt2::kRenderHook))
    {
        struct RainDropletsHook
        {
            void operator()(injector::reg_pack& regs)
            {
                auto pDevice = Manhunt2::GetD3DDevice();
                if (Manhunt2::IsDeviceReady(pDevice))
                {
                    RwMatrix matrix;
                    Manhunt2::GetCameraMatrix(matrix);
                    WaterDrops::right = matrix.right;
                    WaterDrops::up = matrix.up;
                    WaterDrops::at = matrix.at;
                    WaterDrops::pos = matrix.pos;
                    WaterDrops::ms_rainIntensity = Manhunt2::GetRainIntensity();

                    WaterDrops::Process(pDevice);
                    WaterDrops::Render(pDevice);
                    WaterDrops::ms_rainIntensity = 0.0f;
                }

                Manhunt2::ScreenSpaceEnd();
            }
        }; injector::MakeInline<RainDropletsHook>(Manhunt2::kRenderHook);
    }

    if (Manhunt2::IsAddressSet(Manhunt2::kResetD3DDevice))
    {
        struct ResetHook
        {
            void operator()(injector::reg_pack& regs)
            {
                Manhunt2::ResetWaterDrops();
                Manhunt2::bDeviceReady = false;
            }
        }; injector::MakeInline<ResetHook>(Manhunt2::kResetD3DDevice);
    }

    for (auto call : Manhunt2::kCreateFxSystemCalls)
    {
        if (Manhunt2::IsAddressSet(call))
            hb_CreateFxSystem.fun = injector::MakeCALL(call, CreateFxSystem, true).get();
    }
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
        if (!IsUALPresent())
        {
            InitializeASI();
        }
    }
    return TRUE;
}
