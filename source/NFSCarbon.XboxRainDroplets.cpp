#include "xrd.h"

//#define USE_D3D_HOOK

bool(__thiscall* View_AmIinATunnel)(void* View, int viewnum) = (bool(__thiscall*)(void*, int))0x007365E0;

static LPDIRECT3DDEVICE9* pDev;
static uint32_t dword_AB0FA0;

uint32_t* TheGameFlowManagerStatus_A99BBC = (uint32_t*)0x00A99BBC;
uint32_t* dword_B4D964 = (uint32_t*)0xB4D964;

void __stdcall OnScreenRain_Update_Hook(void* View)
{
    if ((*TheGameFlowManagerStatus_A99BBC == 6))
    {
        void* CameraParams = *(void**)(((int)View) + 0x40);

        if (WaterDrops::fTimeStep && !WaterDrops::ms_rainIntensity && !*WaterDrops::fTimeStep && (WaterDrops::ms_numDrops || WaterDrops::ms_numDropsMoving))
            WaterDrops::Clear();

        // TODO: apply the values from the View properly!
        //WaterDrops::up = *(RwV3d*)((int)CameraParams);
        //WaterDrops::right = *(RwV3d*)((int)CameraParams + 0x10);
        WaterDrops::at = *(RwV3d*)((int)CameraParams + 0x50);
        WaterDrops::pos = *(RwV3d*)((int)CameraParams + 0x40);

        WaterDrops::Process(*pDev);
        WaterDrops::ms_rainIntensity = 0.0f;
    }
}

void Init()
{
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
    }; Direct3DCreate9.fun = injector::MakeCALL(pattern.get_first(0), static_cast<IDirect3D9*(WINAPI*)(UINT)>(Direct3DCreate9Hook), true).get(); //0x73088C

    //Patching after vtable is overwritten, using rain function. Also setting the rain intensity here.
    static auto dword_B4AFFC = *hook::get_pattern<uint32_t**>("8B 15 ? ? ? ? D9 82 A0 36 00 00", 2);
    pattern = hook::pattern("C7 05 ? ? ? ? ? ? ? ? B0 01 5E 81 C4 8C 00 00 00 C3");
    static auto dword_AB0BA4 = *pattern.get_first<uint32_t*>(2);
    struct RainDropletsHook
    {
        void operator()(injector::reg_pack& regs)
        {
            *dword_AB0BA4 = 0;
            if (*WaterDrops::pEndScene == (uint32_t)WaterDrops::RealD3D9EndScene)
                injector::WriteMemory(WaterDrops::pEndScene, &WaterDrops::d3dEndScene, true);

            if (*WaterDrops::pReset == (uint32_t)WaterDrops::RealD3D9Reset)
                injector::WriteMemory(WaterDrops::pReset, &WaterDrops::d3dReset, true);

            WaterDrops::ms_rainIntensity = float(**dword_B4AFFC / 20);
        }
    }; injector::MakeInline<RainDropletsHook>(pattern.get_first(0), pattern.get_first(10)); //0x722FA0
#else
    auto pattern = hook::pattern("A1 ? ? ? ? 8B 10 68 ? ? ? ? 50 FF 52 40");
    pDev = *pattern.get_first<LPDIRECT3DDEVICE9*>(1);
    struct ResetHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.eax = *(uint32_t*)pDev;
            WaterDrops::Reset();
        }
    }; injector::MakeInline<ResetHook>(pattern.get_first(0)); //0x72B3C5

    dword_AB0FA0 = *hook::get_pattern<uint32_t>("B1 01 C7 05 ? ? ? ? ? ? ? ? C7 05 ? ? ? ? ? ? ? ? 88 0D ? ? ? ? A3", 8);
    pattern = hook::pattern("C7 05 ? ? ? ? ? ? ? ? B0 01 5E 81 C4 8C 00 00 00 C3");

   pattern = hook::pattern("D9 9E F0 33 00 00"); 
   struct RainIntensityHook
   {
       void operator()(injector::reg_pack& regs)
       {
            _asm fstp dword ptr[esi + 0x33F0]

            if (View_AmIinATunnel(*(void**)(regs.esi + 0x288), 1))
                WaterDrops::ms_rainIntensity = 0;
            else
                WaterDrops::ms_rainIntensity = *(float*)(regs.esi + 0x28C);
       }
   }; injector::MakeInline<RainIntensityHook>(pattern.get_first(0), pattern.get_first(6)); //0x007B3BBB

    pattern = hook::pattern("FF 91 FC 00 00 00 8B 44 24 14 89 46 14 8B 76 44 8B 0E 57 56 FF 91 00 01 00 00 8B 0D ? ? ? ?"); //0x00731118
    struct RainDropletsHook
    {
        void operator()(injector::reg_pack& regs)
        {
            if ((*TheGameFlowManagerStatus_A99BBC == 6))
            {
                WaterDrops::Render(*pDev);
            }
            regs.eax = *(uint32_t*)pDev;
            regs.edx = *(uint32_t*)(regs.esi + 0x48);
            //regs.edx = *(uint32_t*)regs.ecx;
            //regs.esi = regs.ecx;
        }
    }; injector::MakeInline<RainDropletsHook>(pattern.get_first(-30), pattern.get_first(-22)); //0x007310FA

#endif

    //hiding original droplets
    //eRenderRainDrops
    pattern = hook::pattern("A1 ? ? ? ? 8B 08 81 EC 8C 00 00 00 85 C9 75 09"); // 0x00722CB0
    injector::MakeNOP(pattern.get_first(0xF), 2, true);

    //Sim::Internal::mLastFrameTime
    pattern = hook::pattern("A1 ? ? ? ? 6A 01 6A 1C C7 44 24");
    WaterDrops::fTimeStep = *pattern.get_first<float*>(1);

    //View::AmIinATunnel(int viewnum)
    pattern = hook::pattern("8B 41 6C 85 C0 74 1C 8B 4C 24 04"); //0x007365E0
    View_AmIinATunnel = (bool(__thiscall*)(void*, int))pattern.get_first(0);

    //OnScreenRain::Update(View*)
    pattern = hook::pattern("55 8B EC 83 E4 F0 83 EC 24 A1 ? ? ? ? 53 56"); // 0x007C5AD0
    injector::MakeJMP(pattern.get_first(0), OnScreenRain_Update_Hook);
    TheGameFlowManagerStatus_A99BBC = *pattern.count(1).get(0).get<uint32_t*>(0x68);
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

    }
    return TRUE;
}