#include "xrd.h"

//#define USE_D3D_HOOK

static LPDIRECT3DDEVICE9* pDev;
uint32_t* TheGameFlowManagerStatus = (uint32_t*)0x008654A4;
bool bSpecialZones = true;

bool Rain_AmIinATunnel(void* Rain)
{
    int check = *(int*)(((int)Rain) + 0x4D04);
    return check == 0;
}

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

// near the fountain in City Core / South Market next to casinos, 4 points covering a square around it
bMatrix4 CasinoSplashZone = 
{
    -352.73, -572.26, 0.0, 0.0, // top left
    -345.18, -520.17, 0.0, 0.0, // top right
    -304.03, -523.80, 0.0, 0.0, // bot right
    -304.75, -574.04, 0.0, 0.0  // bot left
};

bVector3* gCameraPos;

bool bCameraInSplashZone(bVector3* pos, bMatrix4* zone)
{
    if (!bSpecialZones)
        return false;

    if (((*pos).x > (*zone).v0.x) &&  // top left
        ((*pos).x > (*zone).v1.x) &&  // top right
        ((*pos).x < (*zone).v2.x) &&  // bot right
        ((*pos).x < (*zone).v3.x) &&  // bot left

        ((*pos).y > (*zone).v0.y) &&  // top left
        ((*pos).y > (*zone).v3.y) &&  // bot left
        ((*pos).y < (*zone).v1.y) &&  // top right
        ((*pos).y < (*zone).v2.y)     // bot right
        )
        return true;

    return false;
}

void __stdcall OnScreenRain_Update_Hook(void* View)
{
    Camera* cam = *(Camera**)(((int)View) + 0x40);
    
    if (WaterDrops::fTimeStep && !*WaterDrops::fTimeStep && (WaterDrops::ms_numDrops || WaterDrops::ms_numDropsMoving))
        WaterDrops::Clear();

    gCameraPos = &(*cam).CurrentKey.Position;
    
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

injector::hook_back<void(__cdecl*)(int unk1)> hb_PreRVM;
void __cdecl PreRVMHook(int unk1)
{
    if ((*TheGameFlowManagerStatus == 6))
    {
        WaterDrops::Render(*pDev);
    }

    return hb_PreRVM.fun(unk1);
}

void Init()
{
    CIniReader iniReader("");
    WaterDrops::MinSize = iniReader.ReadInteger("MAIN", "MinSize", 4);
    WaterDrops::MaxSize = iniReader.ReadInteger("MAIN", "MaxSize", 15);
    WaterDrops::MaxDrops = iniReader.ReadInteger("MAIN", "MaxDrops", 2000);
    WaterDrops::MaxDropsMoving = iniReader.ReadInteger("MAIN", "MaxMovingDrops", 500);
    WaterDrops::bRadial = iniReader.ReadInteger("MAIN", "RadialMovement", 1) == 0;
    WaterDrops::bGravity = iniReader.ReadInteger("MAIN", "EnableGravity", 1) != 0;
    WaterDrops::fSpeedAdjuster = iniReader.ReadFloat("MAIN", "SpeedAdjuster", 1.0f);
    bSpecialZones = iniReader.ReadInteger("MAIN", "SpecialZones", 1) != 0;
    
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

    pattern = hook::pattern("D9 9E 48 4D 00 00");
    struct RainIntensityHook
    {
        void operator()(injector::reg_pack& regs)
        {
            float f = 0.0f;
            _asm {fstp dword ptr[f]}
            *(float*)(regs.esi + 0x4D48) = f;

            if (Rain_AmIinATunnel((void*)(regs.esi)))
                WaterDrops::ms_rainIntensity = 0.0f;
            else
                WaterDrops::ms_rainIntensity = *(float*)(regs.esi + 0x1F0);

            if (bCameraInSplashZone(gCameraPos, &CasinoSplashZone))
                WaterDrops::ms_rainIntensity += 0.5f; 

        }
    }; injector::MakeInline<RainIntensityHook>(pattern.get_first(0), pattern.get_first(6)); //0x00613A23

    pattern = hook::pattern("6A 00 6A 07 50 FF 92 E4 00 00 00 8B 45 08"); // 0x005CC0AE
    hb_PreRVM.fun = injector::MakeCALL(pattern.get_first(0x2B), PreRVMHook, true).get(); // 0x005CC0D9

    //OnScreenRain::Update(View*)
    pattern = hook::pattern("55 8B EC 83 E4 F0 83 EC 24 A1 ? ? ? ? 53 56 8B 75 08"); // 0x006140F0
    injector::MakeJMP(pattern.get_first(0), OnScreenRain_Update_Hook);

    pattern = hook::pattern("55 8B EC 83 E4 F0 81 EC 44 01 00 00 A1 ? ? ? ? 85 C0"); // 0x00613A70
    TheGameFlowManagerStatus = *pattern.count(1).get(0).get<uint32_t*>(0x2D);
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