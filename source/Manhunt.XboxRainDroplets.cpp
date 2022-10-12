#define DX8
#include "xrd.h"

void Init()
{
    static auto bCameraCovered = (bool*)0x7B3275;
    static auto fPlayerInsideNess = (float*)0x7B3278;
    static auto OldWeatherType = (int16_t*)0x7B3238;
    static IDirect3DDevice8* pDev = (IDirect3DDevice8*)(0x824448);
    struct RainDropletsHook
    {
        void operator()(injector::reg_pack& regs)
        {
            if (*(uintptr_t*)0x79A938)
            {
                if ((*OldWeatherType == 2 || *OldWeatherType == 3) && !*fPlayerInsideNess && !*bCameraCovered)
                    WaterDrops::ms_rainIntensity = 1.0f;
                else
                    WaterDrops::ms_rainIntensity = 0.0f;
                auto matrix = (RwMatrix*)((*(uintptr_t*)(*(uintptr_t*)0x79A938 + 4) + 0x10));
                WaterDrops::right = matrix->right;
                WaterDrops::up.x = -matrix->up.x;
                WaterDrops::up.y = -matrix->up.y;
                WaterDrops::up.z = -matrix->up.z;
                WaterDrops::at = matrix->at;
                WaterDrops::pos = matrix->pos;
                auto pDevice = *(LPDIRECT3DDEVICE8*)pDev;
                WaterDrops::Process(pDevice);
                WaterDrops::Render(pDevice);
                WaterDrops::ms_rainIntensity = 0.0f;
            }
        }
    }; injector::MakeInline<RainDropletsHook>(0x476109);

    struct ResetHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.eax = *(uint32_t*)pDev;
            WaterDrops::Reset();
        }
    }; injector::MakeInline<ResetHook>(0x64169A);
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
        if (!IsUALPresent()) { InitializeASI(); }
    }
    return TRUE;
}