#define DX8
#include "xrd.h"

void Init()
{
    static IDirect3DDevice8* pDev = (IDirect3DDevice8*)(0x824448);
    struct RainDropletsHook
    {
        void operator()(injector::reg_pack& regs)
        {
            WaterDrops::ms_rainIntensity = 1.0f;
            auto pDevice = *(LPDIRECT3DDEVICE8*)pDev;
            if (*(uintptr_t*)0x79A938)
            {
                auto matrix = (RwMatrix*)((*(uintptr_t*)(*(uintptr_t*)0x79A938 + 4) + 0x10));
                WaterDrops::right = matrix->right;
                WaterDrops::up.x = -matrix->up.x;
                WaterDrops::up.y = -matrix->up.y;
                WaterDrops::up.z = -matrix->up.z;
                WaterDrops::at = matrix->at;
                WaterDrops::pos = matrix->pos;
                WaterDrops::Process(pDevice);
                WaterDrops::Render(pDevice);
                WaterDrops::ms_rainIntensity = 1.0f;
            }
        }
    }; injector::MakeInline<RainDropletsHook>(0x476109);
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