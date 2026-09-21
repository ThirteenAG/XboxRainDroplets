// Direct3D 9, the API this game uses
#define XRD_ENABLE_D3D9
#include "xrd/xrd.h"

// one of the two functions that add a particle takes the colour of the particle
struct RwRGBA
{
    uint8_t r, g, b, a;
};

float& CTimer__ms_fTimeStep = *(float*)0xB7CB5C;

// The game counts a frame in frames of 50 Hz, the effect wants seconds per
// frame, which is what this holds, see where it is set.
static float timeStepSeconds = 0.0f;
//CCamera& TheCamera = *(CCamera*)0xB6F028;
float& CWeather__Rain = *(float*)(0xC81324);
float& CWeather__UnderWaterness = *(float*)(0xC8132C);
bool& CCutsceneMgr__ms_running = *(bool*)(0xB5F851);
int* CGame__currArea = (int*)0xB72914;
int* CEntryExitManager__ms_exitEnterState = (int*)0x96A7CC;
auto CCullZones__CamNoRain = (bool(__cdecl*)())0x72DDB0;
auto CCullZones__PlayerNoRain = (bool(__cdecl*)())0x72DDC0;
auto FindPlayerVehicle = (bool(__cdecl*)(uint32_t, uint8_t))0x56E0D0;
uint8_t* TheCamera = (uint8_t*)0xB6F028;

// the camera modes the drops are not visible in
enum
{
    CAM_TOPDOWN = 1,
    CAM_FIRSTPERSON = 16,
    CAM_TWOPLAYER_TOPDOWN = 54
};

bool NoDrops()
{
    return CWeather__UnderWaterness > 0.339731634f || *CEntryExitManager__ms_exitEnterState != 0;
}

bool NoRain()
{
    return CCullZones__CamNoRain() || CCullZones__PlayerNoRain() || *CGame__currArea != 0 || NoDrops();
}

// the mode of the camera that is in use
static short GetCamMode()
{
    const uint8_t active = *(TheCamera + 0x59);
    const uint8_t* cam = TheCamera + 0x174 + active * 0x238;

    return *(short*)(cam + 0xC);
}

// A camera the drops are not visible in: looking straight down, or the first
// person camera of a pedestrian, is one where they would sit in the middle of
// the view, so the original effect stops them there.
static bool CameraSeesDrops()
{
    const short mode = GetCamMode();

    if (mode == CAM_TOPDOWN || mode == CAM_TWOPLAYER_TOPDOWN)
        return false;

    if (mode == CAM_FIRSTPERSON && FindPlayerVehicle(-1, 0) == 0)
        return false;

    return true;
}

void Init()
{
    WaterDrops::ReadIniSettings();
    // the effect moves with the time of the game, which is a value of its own,
    // see timeStepSeconds
    WaterDrops::fTimeStep = &timeStepSeconds;
    
    static LPDIRECT3DDEVICE9* pDev = (LPDIRECT3DDEVICE9*)0xC97C28;
    static auto matrix = (RwMatrix*)((0xB6F97C + 0x20));
    static injector::hook_back<void(*)()> CMotionBlurStreaksRender;
    auto CMotionBlurStreaksRenderHook = []()
    {
        CMotionBlurStreaksRender.fun();
        WaterDrops::ms_rainIntensity = 1.0f;
        WaterDrops::right = matrix->right;
        WaterDrops::up = matrix->up;
        WaterDrops::at = matrix->at;
        WaterDrops::pos = matrix->pos;

        //when you put camera underwater, droplets disappear instantly instead of fading out
        if (NoDrops()) {
            WaterDrops::Clear();
            return;
        }

        if (NoRain() || !CameraSeesDrops())
            WaterDrops::ms_rainIntensity = 0.0f;
        else
            WaterDrops::ms_rainIntensity = CWeather__Rain;


        // CTimer::ms_fTimeStep counts in frames of 50 Hz, a second is 50 of
        // them, and the effect wants seconds per frame
        timeStepSeconds = CTimer__ms_fTimeStep / 50.0f;

        Xrd::Init(XRD_DEVICE_RENDERER, *pDev);
        WaterDrops::Process();

        if (WaterDrops::ms_numDrops <= 0 || CCutsceneMgr__ms_running || !CameraSeesDrops())
        {

        }
        else
        {
            WaterDrops::Render();
        }

    }; CMotionBlurStreaksRender.fun = injector::MakeCALL(0x726AD0, static_cast<void(*)()>(CMotionBlurStreaksRenderHook), true).get();

    static injector::hook_back<void(__fastcall*)(void* _this, int edx, int id, RwV3d* point)> CAEFireAudioEntityAddAudioEvent;
    auto CAEFireAudioEntityAddAudioEventHook = [](void* _this, int edx, int id, RwV3d* point)
    {
        RwV3d dist;
        RwV3dSub(&dist, point, &WaterDrops::ms_lastPos);
        if (RwV3dLength(&dist) <= 10.0f)
            WaterDrops::RegisterSplash(point, 20.0f, 20);
        
        return CAEFireAudioEntityAddAudioEvent.fun(_this, edx, id, point);
    }; CAEFireAudioEntityAddAudioEvent.fun = injector::MakeCALL(0x4AAE2D, static_cast<void(__fastcall*)(void*, int, int, RwV3d*)>(CAEFireAudioEntityAddAudioEventHook), true).get(); //water_hydrant
    injector::MakeCALL(0x4AAE4B, static_cast<void(__fastcall*)(void*, int, int, RwV3d*)>(CAEFireAudioEntityAddAudioEventHook), true); //water_fountain
    injector::MakeCALL(0x4AAE69, static_cast<void(__fastcall*)(void*, int, int, RwV3d*)>(CAEFireAudioEntityAddAudioEventHook), true); //water_fnt_tme
    
    //FxManager_c::CreateFxSystem
    static injector::hook_back<void*(__fastcall*)(void*, int, char*, RwV3d*, RwMatrix*, char)> CreateFxSystem;
    auto CreateFxSystemHook = [](void* _this, int edx, char* name, RwV3d* point, RwMatrix* m, char flag) -> void*
    {
        RwV3d dist;
        RwV3dSub(&dist, point, &WaterDrops::ms_lastPos);
        if (RwV3dLength(&dist) <= 10.0f)
            WaterDrops::RegisterSplash(point, 10.0f, 1);
        
        return CreateFxSystem.fun(_this, edx, name, point, m, flag);
    };
    auto f = static_cast<void*(__fastcall*)(void*, int, char*, RwV3d*, RwMatrix*, char)>(CreateFxSystemHook);
    CreateFxSystem.fun = injector::MakeCALL(0x4A10C9, f, true).get();// "water_splash_big"
    injector::MakeCALL(0x4A1139, f, true); // "water_splash"
    injector::MakeCALL(0x4A11A9, f, true); // "water_splsh_sml"
    //injector::MakeCALL(0x68AEBA, f, true); // "water_swim"
    //injector::MakeCALL(0x68AF15, f, true); // "water_swim"
    //injector::MakeCALL(0x68AF66, f, true); // "water_swim"
    //injector::MakeCALL(0x68AFB3, f, true); // "water_swim"
    
    //AddParticle
    struct Fx_c {
        void* prt_blood;
        void* prt_boatsplash;
        void* prt_bubble;
        void* prt_cardebris;
        void* prt_collisionsmoke;
        void* prt_gunshell;
        void* prt_sand;
        void* prt_sand2;
        void* prt_smoke_huge;
        void* prt_smokeII_3_expand;
        void* prt_spark;
        void* prt_spark_2;
        void* prt_splash;
        void* prt_wake;
        void* prt_watersplash;
        void* prt_wheeldirt;
        void* prt_glass;
        //...
    };
    static Fx_c &g_fx = *(Fx_c*)0xA9AE00;
    static injector::hook_back<void*(__fastcall*)(void*, int, RwV3d*, RwV3d*, float, void*, float, float, float, unsigned char)> AddParticle;
    auto AddParticleHook = [](void* _this, int edx, RwV3d* position, RwV3d* velocity, float arg2, void* prtMult, float arg4, float brightness, float arg6, unsigned char arg7) -> void*
    {
            RwV3d dist;
            RwV3dSub(&dist, position, &WaterDrops::ms_lastPos);
            float pd = 20.0f;
            bool isBlood = false;
            if (_this == g_fx.prt_blood) { pd = 5.0; isBlood = true; }
            else if (_this == g_fx.prt_boatsplash) { pd = 40.0; }
            else if (_this == g_fx.prt_splash) { pd = 15.0; }
            else if (_this == g_fx.prt_wake) { pd = 10.0; }
            else if (_this == g_fx.prt_watersplash) { pd = 30.0; }
    
            float len = RwV3dLength(&dist);
            if (len <= pd)
                WaterDrops::FillScreenMoving(1.0f / (len / 2.0f), isBlood);
        
        return AddParticle.fun(_this, edx, position, velocity, arg2, prtMult, arg4, brightness, arg6, arg7);
    };
    auto f2 = static_cast<void*(__fastcall*)(void*, int, RwV3d*, RwV3d*, float, void*, float, float, float, unsigned char)>(AddParticleHook);
    //injector::MakeCALL(0x72AD55, f2, true); // "prt_splash"
    AddParticle.fun = injector::MakeCALL(0x7294A6, f2, true).get(); // "prt_watersplash"
    //injector::MakeCALL(0x6DE21A, f2, true); // "prt_splash"
    injector::MakeCALL(0x6DD6C5, f2, true); // "prt_boatsplash"
    injector::MakeCALL(0x6DD589, f2, true); // "prt_boatsplash"
    injector::MakeCALL(0x6C3ABE, f2, true); // "prt_wake"
    injector::MakeCALL(0x6C3939, f2, true); // "prt_watersplash"
    injector::MakeCALL(0x6AA86F, f2, true); // "prt_watersplash"
    injector::MakeCALL(0x68ADE2, f2, true); // "prt_wake"
    injector::MakeCALL(0x5E7649, f2, true); // "prt_watersplash"
    //injector::MakeCALL(0x5E74DC, f2, true); // "prt_splash"
    //injector::MakeCALL(0x5E37AF, f2, true); // "prt_splash"
    //injector::MakeCALL(0x5E3782, f2, true); // "prt_splash"
    injector::MakeCALL(0x49FEEE, f2, true); // "prt_boatsplash"
    //injector::MakeCALL(0x49F01F, f2, true); // "prt_blood"
    injector::MakeCALL(0x49EC92, f2, true); // "prt_blood"
    
    //chainsaw
    static injector::hook_back<void(__fastcall*)(void* _this, int edx, int eventID)> CAEPedWeaponAudioEntityAddAudioEvent;
    auto CAEPedWeaponAudioEntityAddAudioEventHook = [](void* _this, int edx, int eventID)
    {
        WaterDrops::FillScreenMoving(1.0f, true);
        return CAEPedWeaponAudioEntityAddAudioEvent.fun(_this, edx, eventID);
    }; CAEPedWeaponAudioEntityAddAudioEvent.fun = injector::MakeCALL(0x61CD73, static_cast<void(__fastcall*)(void*, int, int)>(CAEPedWeaponAudioEntityAddAudioEventHook), true).get();
    
    //harvester
    //static injector::hook_back<void(__cdecl*)(CEntity*)> WorldAdd;
    //auto WorldAddHook = [](CEntity* entity)
    //{
    //    WorldAdd.fun(entity);
    //    
    //    if(config->neoWaterDrops){
    //        RwV3d dist;
    //        RwV3dSub(&dist, (RwV3d*)&entity->GetPosition(), &WaterDrops::ms_lastPos);
    //        if(RwV3dLength(&dist) <= 20.0f)
    //            WaterDrops::FillScreenMoving(0.5f, true);
    //    }
    //};
    //WorldAdd.fun = injector::MakeCALL(0x6A9BE2, static_cast<void(__cdecl*)(CEntity*)>(WorldAddHook), true).get();

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