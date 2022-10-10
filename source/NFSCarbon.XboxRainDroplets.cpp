#include "xrd.h"

//#define USE_D3D_HOOK

bool(__thiscall* View_AmIinATunnel)(void* View, int viewnum) = (bool(__thiscall*)(void*, int))0x007365E0;

static LPDIRECT3DDEVICE9* pDev;
static uint32_t dword_AB0FA0;

uint32_t* TheGameFlowManagerStatus_A99BBC = (uint32_t*)0x00A99BBC;
uint32_t* dword_B4D964 = (uint32_t*)0xB4D964;

// TODO: fix the "pulsating" drops and make it look more "rainy", also make it better at higher speeds
class NFSWaterDrops : public WaterDrops
{
public:
    enum {
        MAXDROPS = 6000,
        MAXDROPSMOVING = 700
    };

    static inline void NewTrace(WaterDropMoving* moving)
    {
        if (ms_numDrops < MAXDROPS) {
            moving->dist = 0.0f;
            PlaceNew(moving->drop->x, moving->drop->y, (float)(SC(MINSIZE)), 500.0f, 1, moving->drop->r, moving->drop->g, moving->drop->b);
        }
    }

    static void MoveDrop(WaterDropMoving* moving)
    {
        WaterDrop* drop = moving->drop;
        if (!ms_movingEnabled)
            return;
        if (!drop->active) {
            moving->drop = NULL;
            ms_numDropsMoving--;
            return;
        }

        if (!ms_noCamTurns && (ms_vec.z <= 0.0f || ms_distMoved <= 0.0f))
            return;
        float d = abs(ms_vec.z * 0.2f);
        float dx, dy, sum;
        dx = drop->x - ms_fbWidth * 0.5f + ms_vec.x;
        dy = drop->y - ms_fbHeight * 0.5f - ms_vec.y;
        sum = fabs(dx) + fabs(dy);
        if (sum >= 0.001f) {
            dx *= (1.0f / sum);
            dy *= (1.0f / sum);
        }
        moving->dist += ((d + ms_vecLen));
        NewTrace(moving);
        drop->x += (dx * d) - ms_vec.x;
        drop->y += (dy * d) + ms_vec.y;

        if (drop->x < 0.0f || drop->y < 0.0f ||
            drop->x > ms_fbWidth || drop->y > ms_fbHeight) {
            moving->drop = NULL;
            ms_numDropsMoving--;
        }
    }

    static inline void ProcessMoving()
    {
        WaterDropMoving* moving;
        if (!ms_movingEnabled)
            return;
        for (moving = ms_dropsMoving; moving < &ms_dropsMoving[MAXDROPSMOVING]; moving++)
            if (moving->drop)
                MoveDrop(moving);
    }

    static inline void Process(LPDIRECT3DDEVICE9 pDevice)
    {
        fps.update();
        if (!ms_initialised)
            InitialiseRender(pDevice);
        CalculateMovement();
        SprayDrops();
        ProcessMoving();
        Fade();
    }
};

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
    if ((*TheGameFlowManagerStatus_A99BBC == 6))
    {
        Camera* cam = *(Camera**)(((int)View) + 0x40);

        if (NFSWaterDrops::fTimeStep && !NFSWaterDrops::ms_rainIntensity && !*NFSWaterDrops::fTimeStep && (NFSWaterDrops::ms_numDrops || NFSWaterDrops::ms_numDropsMoving))
            NFSWaterDrops::Clear();

        cam->GetUpVec();

        NFSWaterDrops::up.x = UpVector.x;
        NFSWaterDrops::up.y = UpVector.y;
        NFSWaterDrops::up.z = UpVector.z;

        cam->GetLeftVec();

        NFSWaterDrops::right.x = LeftVector.x;
        NFSWaterDrops::right.y = LeftVector.y;
        NFSWaterDrops::right.z = LeftVector.z;


        NFSWaterDrops::at.x = (*cam).CurrentKey.RotNoise2Value.x;
        NFSWaterDrops::at.y = (*cam).CurrentKey.RotNoise2Value.y;
        NFSWaterDrops::at.z = (*cam).CurrentKey.RotNoise2Value.z;


        NFSWaterDrops::pos.x = (*cam).CurrentKey.PosNoise2Value.x;
        NFSWaterDrops::pos.y = (*cam).CurrentKey.PosNoise2Value.y;
        NFSWaterDrops::pos.z = (*cam).CurrentKey.PosNoise2Value.z;

        NFSWaterDrops::Process(*pDev);
        NFSWaterDrops::ms_rainIntensity = 0.0f;
    }
}

void Init()
{
#ifdef USE_D3D_HOOK
    //vtable gets overwritten at startup, so no point in patching it right away
    NFSWaterDrops::bPatchD3D = false;

    //resetting rain
    NFSWaterDrops::ProcessCallback2 = []()
    {
        NFSWaterDrops::ms_rainIntensity = 0.0f;
    };

    //hooking create to get EndScene and Reset
    auto pattern = hook::pattern("E8 ? ? ? ? 6A 2B 6A 2B A3");
    static injector::hook_back<IDirect3D9*(WINAPI*)(UINT)> Direct3DCreate9;
    auto Direct3DCreate9Hook = [](UINT SDKVersion) -> IDirect3D9*
    {
        auto pID3D9 = Direct3DCreate9.fun(SDKVersion);
        auto pVTable = (UINT_PTR*)(*((UINT_PTR*)pID3D9));
        if (!NFSWaterDrops::RealD3D9CreateDevice)
            NFSWaterDrops::RealD3D9CreateDevice = (CreateDevice_t)pVTable[IDirect3D9VTBL::CreateDevice];
        injector::WriteMemory(&pVTable[IDirect3D9VTBL::CreateDevice], &NFSWaterDrops::d3dCreateDevice, true);
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
            if (*NFSWaterDrops::pEndScene == (uint32_t)NFSWaterDrops::RealD3D9EndScene)
                injector::WriteMemory(NFSWaterDrops::pEndScene, &NFSWaterDrops::d3dEndScene, true);

            if (*NFSWaterDrops::pReset == (uint32_t)NFSWaterDrops::RealD3D9Reset)
                injector::WriteMemory(NFSWaterDrops::pReset, &NFSWaterDrops::d3dReset, true);

            NFSWaterDrops::ms_rainIntensity = float(**dword_B4AFFC / 20);
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
            NFSWaterDrops::Reset();
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
                NFSWaterDrops::ms_rainIntensity = 0;
            else
                NFSWaterDrops::ms_rainIntensity = *(float*)(regs.esi + 0x28C);
       }
   }; injector::MakeInline<RainIntensityHook>(pattern.get_first(0), pattern.get_first(6)); //0x007B3BBB

    pattern = hook::pattern("FF 91 FC 00 00 00 8B 44 24 14 89 46 14 8B 76 44 8B 0E 57 56 FF 91 00 01 00 00 8B 0D ? ? ? ?"); //0x00731118
    struct RainDropletsHook
    {
        void operator()(injector::reg_pack& regs)
        {
            if ((*TheGameFlowManagerStatus_A99BBC == 6))
            {
                NFSWaterDrops::Render(*pDev);
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
    NFSWaterDrops::fTimeStep = *pattern.get_first<float*>(1);

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