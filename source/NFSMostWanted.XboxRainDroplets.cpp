#include "xrd.h"

//#define USE_D3D_HOOK

static LPDIRECT3DDEVICE9* pDev;
uint32_t* TheGameFlowManagerStatus = (uint32_t*)0x00925E90;
bool(__cdecl* View_AmIinATunnel)(void* View, int viewnum) = (bool(__cdecl*)(void*, int))0x0073CFE0;

struct bVector2
{
    float x;
    float y;
};

struct bVector3
{
    float x;
    float y;
    float z;
    float pad;
};

struct bVector4
{
    float x;
    float y;
    float z;
    float w;
};

struct bMatrix4
{
    bVector4 v0;
    bVector4 v1;
    bVector4 v2;
    bVector4 v3;
};

struct WaveData3
{
    struct bVector3 frequency;
    struct bVector3 amplitude;
};

struct CameraParams
{
    bMatrix4 Matrix;
    bVector3 Position;
    bVector3 Direction;
    bVector3 Target;
    WaveData3 PosNoise[3];
    WaveData3 RotNoise[3];
    bVector3 PosNoise2Value;
    bVector3 RotNoise2Value;
    float FocalDistance;
    float DepthOfField;
    float DOFFalloff;
    float DOFMaxIntensity;
    float NearZ;
    float FarZ;
    float LB_height;
    float SimTimeMultiplier;
    bVector4 FadeColor;
    float TargetDistance;
    unsigned short FieldOfView;
    unsigned short PaddingAngle;
    bVector2 PaddingVector2;
};

bVector3* bNormalize(bVector3* dest, bVector3* v)
{
    float v2;
    float v3;
    float v4;
    float v5;

    v2 = sqrt(((v->z * v->z) + ((v->x * v->x) + (v->y * v->y))));
    if (v2 == 0.0)
    {
        dest->x = 1.0f;
        dest->y = 0.0f;
        dest->z = 0.0f;
    }
    else
    {
        v3 = (1.0f / v2);
        v4 = v->y;
        v5 = v->z;
        dest->x = v->x * v3;
        dest->y = v4 * v3;
        dest->z = v5 * v3;
    }
    return dest;
}

bVector3 UpVector;
bVector3 LeftVector;
bVector3 ForwardVector;

class Camera
{
public:
    CameraParams CurrentKey;
    CameraParams PreviousKey;
    CameraParams VelocityKey;
    BOOL bClearVelocity;
    char Padding_820[3];
    float ElapsedTime;
    int LastUpdateTime;
    int LastDisparateTime;
    int RenderDash;
    float NoiseIntensity;

    bVector3* GetForwardVec()
    {
        D3DXMATRIX v2;
        D3DXMatrixTranspose(&v2, (D3DXMATRIX*)&(CurrentKey.Matrix));
        return bNormalize(&ForwardVector, (bVector3*)v2.m[2]);
    }

    bVector3* GetUpVec()
    {
        D3DXMATRIX v2;
        D3DXMatrixTranspose(&v2, (D3DXMATRIX*)&(CurrentKey.Matrix));
        return bNormalize(&UpVector, (bVector3*)v2.m[1]);
    }

    bVector3* GetLeftVec()
    {
        D3DXMATRIX v2;
        D3DXMatrixTranspose(&v2, (D3DXMATRIX*)&(CurrentKey.Matrix));
        return bNormalize(&LeftVector, (bVector3*)v2.m[0]);
    }
};

void __stdcall OnScreenRain_Update_Hook(void* View)
{
    if ((*TheGameFlowManagerStatus == 6))
    {
        Camera* cam = *(Camera**)(((int)View) + 0x40);

        if (WaterDrops::fTimeStep && !*WaterDrops::fTimeStep && (WaterDrops::ms_numDrops || WaterDrops::ms_numDropsMoving))
            WaterDrops::Clear();

        cam->GetUpVec();

        WaterDrops::up.x = -UpVector.x;
        WaterDrops::up.y = -UpVector.y;
        WaterDrops::up.z = -UpVector.z;

        cam->GetLeftVec();

        WaterDrops::right.x = -LeftVector.x;
        WaterDrops::right.y = -LeftVector.y;
        WaterDrops::right.z = -LeftVector.z;

        WaterDrops::at.x = (*cam).CurrentKey.Direction.x;
        WaterDrops::at.y = (*cam).CurrentKey.Direction.y;
        WaterDrops::at.z = (*cam).CurrentKey.Direction.z;

        WaterDrops::pos.x = (*cam).CurrentKey.Position.x;
        WaterDrops::pos.y = (*cam).CurrentKey.Position.y;
        WaterDrops::pos.z = (*cam).CurrentKey.Position.z;

        WaterDrops::Process(*pDev);
        WaterDrops::ms_rainIntensity = 0.0f;
    }
}

injector::hook_back<void(__cdecl*)(int unk1, int unk2)> hb_PreRVM;
void __cdecl PreRVMHook(int unk1, int unk2)
{

    if ((*TheGameFlowManagerStatus == 6))
    {
        WaterDrops::Render(*pDev);
    }

    return hb_PreRVM.fun(unk1, unk2);
}

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
    pDev = *pattern.get_first<LPDIRECT3DDEVICE9*>(1);
    struct ResetHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.eax = *(uint32_t*)pDev;
            WaterDrops::Reset();
        }
    }; injector::MakeInline<ResetHook>(pattern.get_first(0));

    pattern = hook::pattern("D9 9E F0 33 00 00");
    struct RainIntensityHook
    {
        void operator()(injector::reg_pack& regs)
        {
            float f = 0.0f;
            _asm {fstp dword ptr[f]}
            *(float*)(regs.esi + 0x33F0) = f;

            if (View_AmIinATunnel(*(void**)(regs.esi + 0x288), 1))
                WaterDrops::ms_rainIntensity = 0.0f;
            else
                WaterDrops::ms_rainIntensity = *(float*)(regs.esi + 0x28C);
        }
    }; injector::MakeInline<RainIntensityHook>(pattern.get_first(0), pattern.get_first(6)); //0x73CCDB

    pattern = hook::pattern("6A 00 6A 07 50 FF 92 E4 00 00 00 8B 45 08"); // 0x006E7077
    hb_PreRVM.fun = injector::MakeCALL(pattern.get_first(0x1D), PreRVMHook, true).get(); // 0x006E7094

    //OnScreenRain::Update(View*)
    pattern = hook::pattern("55 8B EC 83 E4 F0 83 EC 24 A1 ? ? ? ? 53 56"); // 0x0073CDB0
    injector::MakeJMP(pattern.get_first(0), OnScreenRain_Update_Hook);
    pattern = hook::pattern("55 8B EC 83 E4 F0 81 EC D4 00 00 00 83 3D"); // 0x00757770
    TheGameFlowManagerStatus = *pattern.count(1).get(0).get<uint32_t*>(0xE);
#endif

    //hiding original droplets
    static int32_t ogDrops = 0;
    static auto pDrops = &ogDrops;
    pattern = hook::pattern("8B 0D ? ? ? ? 8B 01 33 FF 85 C0");
    injector::WriteMemory(pattern.get_first(2), &pDrops, true);

    //Sim::Internal::mLastFrameTime
    pattern = hook::pattern("D9 1D ? ? ? ? E8 ? ? ? ? 53 E8 ? ? ? ? D8 05 ? ? ? ? A1 ? ? ? ? 83 C4 04");
    WaterDrops::fTimeStep = *pattern.get_first<float*>(2);

    //View::AmIinATunnel(int viewnum)
    pattern = hook::pattern("8B 44 24 04 8B 40 68 85 C0 74 1C 8B 4C 24"); //0x0073CFE0
    View_AmIinATunnel = (bool(__cdecl*)(void*, int))pattern.get_first(0);
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