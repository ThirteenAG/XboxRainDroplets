#include "xrd.h"

//#define USE_D3D_HOOK

void Init()
{
    CIniReader iniReader("");
    WaterDrops::bRadial = iniReader.ReadInteger("MAIN", "RadialMovement", 0) == 0;
    WaterDrops::bGravity = iniReader.ReadInteger("MAIN", "EnableGravity", 1) != 0;
    
#ifdef USE_D3D_HOOK
    //vtable gets overwritten at startup, so no point in patching it right away
    WaterDrops::bPatchD3D = false;

    //resetting rain
    WaterDrops::ProcessCallback2 = []()
    {
        WaterDrops::ms_rainIntensity = 0.0f;
    };

    //hooking create to get EndScene and Reset
    auto pattern = hook::pattern("E8 ? ? ? ? 6A 2B 6A 2B A3");
    static injector::hook_back<IDirect3D9*(WINAPI*)(UINT)> Direct3DCreate9;
    auto Direct3DCreate9Hook = [](UINT SDKVersion) -> IDirect3D9*
    {
        auto pID3D9 = Direct3DCreate9.fun(SDKVersion);
        auto pVTable = (UINT_PTR*)(*((UINT_PTR*)pID3D9));
        if (!WaterDrops::RealD3D9CreateDevice)
            WaterDrops::RealD3D9CreateDevice = (CreateDevice_t)pVTable[IDirect3D9VTBL::CreateDevice];
        injector::WriteMemory(&pVTable[IDirect3D9VTBL::CreateDevice], &WaterDrops::d3dCreateDevice, true);
        return pID3D9;
    }; Direct3DCreate9.fun = injector::MakeCALL(pattern.get_first(0), static_cast<IDirect3D9*(WINAPI*)(UINT)>(Direct3DCreate9Hook), true).get();

    //Patching after vtable is overwritten, using rain function. Also setting the rain intensity here.
    static auto dword_9196B8 = *hook::get_pattern<uint32_t**>("8B 15 ? ? ? ? D9 82 A0 36 00 00", 2);
    pattern = hook::pattern("C7 05 ? ? ? ? ? ? ? ? 5E 81 C4 8C 00 00 00 C3");
    static auto dword_982C80 = *pattern.get_first<uint32_t*>(2);
    struct RainDropletsHook
    {
        void operator()(injector::reg_pack& regs)
        {
            *dword_982C80 = 0;
            if (*WaterDrops::pEndScene == (uint32_t)WaterDrops::RealD3D9EndScene)
                injector::WriteMemory(WaterDrops::pEndScene, &WaterDrops::d3dEndScene, true);

            if (*WaterDrops::pReset == (uint32_t)WaterDrops::RealD3D9Reset)
                injector::WriteMemory(WaterDrops::pReset, &WaterDrops::d3dReset, true);

            WaterDrops::ms_rainIntensity = float(**dword_9196B8 / 20);
        }
    }; injector::MakeInline<RainDropletsHook>(pattern.get_first(0), pattern.get_first(10));
#else
    auto pattern = hook::pattern("A1 ? ? ? ? 8B 10 68 ? ? ? ? 50 FF 52 40");
    static auto pDev = *pattern.get_first<LPDIRECT3DDEVICE9*>(1);
    struct ResetHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.eax = *(uint32_t*)pDev;
            WaterDrops::Reset();
        }
    }; injector::MakeInline<ResetHook>(pattern.get_first(0));

    static auto dword_9196B8 = *hook::get_pattern<uint32_t**>("8B 15 ? ? ? ? D9 82 A0 36 00 00", 2);
    static auto dword_982D80 = *hook::get_pattern<uint32_t>("B1 01 C7 05 ? ? ? ? ? ? ? ? C7 05 ? ? ? ? ? ? ? ? 88 0D ? ? ? ? A3", 8);
    pattern = hook::pattern("C7 05 ? ? ? ? ? ? ? ? 5E 81 C4 8C 00 00 00 C3");
    static auto dword_982C80 = *pattern.get_first<uint32_t*>(2);
    struct RainDropletsHook
    {
        void operator()(injector::reg_pack& regs)
        {
            *dword_982C80 = 0;
            WaterDrops::ms_rainIntensity = float(**dword_9196B8 / 20);
            WaterDrops::right = *(RwV3d*)(dword_982D80 + 0x00);
            WaterDrops::up = *(RwV3d*)(dword_982D80 + 0x10);
            WaterDrops::at = *(RwV3d*)(dword_982D80 + 0x20);
            WaterDrops::pos = *(RwV3d*)(dword_982D80 + 0x60);
            WaterDrops::Process(*pDev);
            WaterDrops::Render(*pDev);
        }
    }; injector::MakeInline<RainDropletsHook>(pattern.get_first(0), pattern.get_first(10));
#endif

    //hiding original droplets
    static int32_t ogDrops = 0;
    static auto pDrops = &ogDrops;
    pattern = hook::pattern("8B 0D ? ? ? ? 8B 01 33 FF 85 C0");
    injector::WriteMemory(pattern.get_first(2), &pDrops, true);

    //disabling rain check so new droplets can fade out
    pattern = hook::pattern("A1 ? ? ? ? 8B 08 81 EC ? ? ? ? 85 C9 0F 84");
    injector::MakeNOP(pattern.get_first(15), 6, true);

    //Sim::Internal::mLastFrameTime
    pattern = hook::pattern("D9 1D ? ? ? ? E8 ? ? ? ? 53 E8 ? ? ? ? D8 05 ? ? ? ? A1 ? ? ? ? 83 C4 04");
    WaterDrops::fTimeStep = *pattern.get_first<float*>(2);
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