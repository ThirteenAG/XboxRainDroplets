#include "xrd.h"

typedef struct MATH_tdst_Vector_
{
    float x;
    float y;
    float z;
} MATH_tdst_Vector;

typedef struct MATH_tdst_Matrix_
{
    __declspec(align(16)) float Ix;
    float Iy, Iz, Sx;
    float Jx, Jy, Jz, Sy;
    float Kx, Ky, Kz, Sz;
    MATH_tdst_Vector T;
    float w;
    LONG lType;
} __declspec(align(16)) MATH_tdst_Matrix;

uintptr_t* GDI_gpst_CurDD = 0;

void UpdateCameraFromJadeMatrix(const MATH_tdst_Matrix* pMatrix)
{
    if (!pMatrix) return;

    WaterDrops::right = { pMatrix->Ix, pMatrix->Iy, pMatrix->Iz };
    WaterDrops::up = { pMatrix->Jx,  pMatrix->Jy,  pMatrix->Jz };
    WaterDrops::at = { pMatrix->Kx,  pMatrix->Ky,  pMatrix->Kz };

    // Offset pos along the forward (K) axis so camera rotation
    // produces a position delta that WaterDrops interprets as movement.
    // This makes droplets slide when looking around with the mouse.
    const float kRotationSensitivity = 5.0f;
    WaterDrops::pos = {
        pMatrix->T.x + pMatrix->Kx * kRotationSensitivity,
        pMatrix->T.y + pMatrix->Ky * kRotationSensitivity,
        pMatrix->T.z + pMatrix->Kz * kRotationSensitivity
    };
}

void Init()
{
    WaterDrops::ReadIniSettings();

    auto pattern = hook::pattern("A1 ? ? ? ? 89 45 ? 8B 0D ? ? ? ? 89 0D ? ? ? ? 68");
    GDI_gpst_CurDD = *pattern.get_first<uintptr_t*>(1);

    pattern = hook::pattern("38 9E ? ? ? ? 74 ? B9 ? ? ? ? E8 ? ? ? ? 8B 15");
    static auto RenderHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
    {
        IDirect3DDevice9* pDevice = *(IDirect3DDevice9**)(regs.esi + 4);
        UpdateCameraFromJadeMatrix((MATH_tdst_Matrix*)(*GDI_gpst_CurDD + 0x158));
        WaterDrops::Process(pDevice);
        WaterDrops::Render(pDevice);
        WaterDrops::ms_rainIntensity = 0.0f;
    });

    pattern = hook::pattern("52 8D 44 24 ? 50 E8 ? ? ? ? 83 C4 ? 8D 4C 24");
    static auto AddRainFXHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
    {
        auto pst_Matrix = (MATH_tdst_Matrix*)(*GDI_gpst_CurDD + 0x158);
        auto pst_GlobalMatrix = (MATH_tdst_Matrix*)regs.edx;

        float rainPosX = pst_GlobalMatrix->T.x;
        float rainPosY = pst_GlobalMatrix->T.y;
        float rainPosZ = pst_GlobalMatrix->T.z;

        float camX = pst_Matrix->T.x;
        float camY = pst_Matrix->T.y;
        float camZ = pst_Matrix->T.z;

        float dx = camX - rainPosX;
        float dy = camY - rainPosY;
        float dz = camZ - rainPosZ;
        float horizDistSq = dx*dx + dy*dy;
        bool insideVolume = (horizDistSq <= 1.0f*1.0f);

        if (insideVolume)
        {
            WaterDrops::ms_rainIntensity = **(float**)(regs.esi + 0x198C);
        }
        else
        {
            WaterDrops::ms_rainIntensity = 0.0f;
        }
    });

    pattern = hook::pattern("89 46 ? 8B 46 ? 57");
    static auto DeviceResetHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
    {
        WaterDrops::Reset();
    });
}

extern "C" __declspec(dllexport) void InitializeASI()
{
    std::call_once(CallbackHandler::flag, []()
    {
        CallbackHandler::RegisterCallbackAtGetSystemTimeAsFileTime(Init, hook::pattern("38 9E ? ? ? ? 74 ? B9 ? ? ? ? E8 ? ? ? ? 8B 15"));
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