// Direct3D 8, the API this game uses
#define XRD_ENABLE_D3D8
#include "xrd/xrd.h"
#include <unordered_map>

// It rains where the levels place rain particle systems, rain_heavy_*, rain_medium_* and
// rain_medium_rain_low_* of data\database\particles\level_particles.h. Each one is a box the rain
// falls in, and the camera is in the rain while it is among the falling particles of one of them.
// The drops are drawn over the game view once it is rendered, after the pain and bullet time
// warp of the screen and before the HUD.

IDirect3DDevice8* pDevice = nullptr;

namespace P_BaseObject
{
    // Points are transformed as p * M by three rows of axes, then the position
    constexpr ptrdiff_t OBJECT_TO_WORLD = 0x68;

    void(__fastcall* calculateObjectToWorldMatrix)(void* _this, void* edx) = nullptr;

    const float(&GetObjectToWorld(uint8_t* pObject))[4][3]
    {
        calculateObjectToWorldMatrix(pObject, nullptr); // when it isn't up to date
        return *(float(*)[4][3])(pObject + OBJECT_TO_WORLD);
    }
}

namespace X_LevelRuntimeCamera
{
    uint8_t* (__fastcall* getCamera)(void* _this, void* edx) = nullptr;
    bool(__fastcall* roomsToRenderWithSkybox)(void* _this, void* edx) = nullptr;
}

namespace X_ModeBase
{
    void* (__fastcall* getTimeUpdate)(void* _this, void* edx) = nullptr;
}

namespace X_TimeUpdate
{
    float(__fastcall* getRelativeTime)(void* _this, void* edx) = nullptr;
}

namespace Rain
{
    // A rain particle system of the level
    struct Volume
    {
        float fIntensity;
        RwV3d Min;         // of its particles as of its last update
        RwV3d Max;
        uint32_t nUpdate;  // frame of that update
    };

    std::unordered_map<uint8_t*, Volume> Volumes;
    uint32_t nFrame = 0;

    // How hard it rains in a particle system, 0 for the ones that aren't rain falling. rain_hitting_*
    // are the splashes where rain hits the ground and other surfaces.
    float GetIntensity(const char* szName)
    {
        std::string name(szName);
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        if (!name.starts_with("rain_") || name.starts_with("rain_hitting"))
            return 0.0f;
        if (name.find("heavy") != std::string::npos)
            return 1.0f;
        if (name.find("low") != std::string::npos)
            return 0.4f;
        return 0.7f;
    }

    void Track(uint8_t* pSystem, float fIntensity)
    {
        Volumes[pSystem] = { fIntensity, { FLT_MAX, FLT_MAX, FLT_MAX }, { -FLT_MAX, -FLT_MAX, -FLT_MAX }, nFrame - 1000 };
    }

    // How hard it rains at the camera
    float GetIntensityAt(const RwV3d& camera)
    {
        float fIntensity = 0.0f;
        for (auto& [pSystem, volume] : Volumes)
        {
            // particle systems out of sight stop updating, the camera isn't among their particles
            if (nFrame - volume.nUpdate > 30)
                continue;

            if (camera.x >= volume.Min.x && camera.x <= volume.Max.x && camera.y >= volume.Min.y && camera.y <= volume.Max.y && camera.z >= volume.Min.z && camera.z <= volume.Max.z)
                fIntensity = (std::max)(fIntensity, volume.fIntensity);
        }
        return fIntensity;
    }
}

namespace P_ParticleSystemBase
{
    // Every update of a particle system hands its living particles over to be drawn, between
    // beginParticleDump and endParticleDump, with addParticle
    uint8_t* pDumping = nullptr;
    Rain::Volume* pDumpingVolume = nullptr;

    SafetyHookInline shBeginParticleDump = {};
    void __fastcall beginParticleDump(uint8_t* _this, void* edx)
    {
        shBeginParticleDump.unsafe_fastcall(_this, edx);

        auto it = Rain::Volumes.find(_this);
        pDumping = it != Rain::Volumes.end() ? _this : nullptr;
        pDumpingVolume = pDumping ? &it->second : nullptr;
        if (pDumpingVolume)
        {
            pDumpingVolume->Min = { FLT_MAX, FLT_MAX, FLT_MAX };
            pDumpingVolume->Max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
            pDumpingVolume->nUpdate = Rain::nFrame;
        }
    }

    // P_ParticleBuffer::ParticleData starts with the position, which the rain has in the world
    SafetyHookInline shAddParticle = {};
    void __fastcall addParticle(uint8_t* _this, void* edx, const float* pParticle)
    {
        if (_this == pDumping)
        {
            auto& volume = *pDumpingVolume;
            volume.Min = { (std::min)(volume.Min.x, pParticle[0]), (std::min)(volume.Min.y, pParticle[1]), (std::min)(volume.Min.z, pParticle[2]) };
            volume.Max = { (std::max)(volume.Max.x, pParticle[0]), (std::max)(volume.Max.y, pParticle[1]), (std::max)(volume.Max.z, pParticle[2]) };
        }
        shAddParticle.unsafe_fastcall(_this, edx, pParticle);
    }

    void Forget(uint8_t* pSystem)
    {
        if (pSystem == pDumping)
            pDumping = nullptr;
        Rain::Volumes.erase(pSystem);
    }

    SafetyHookInline shDestructor = {};
    void __fastcall destructor(uint8_t* _this, void* edx)
    {
        Forget(_this);
        shDestructor.unsafe_fastcall(_this, edx);
    }
}

// The particle systems of particles.txt and level_particles.h are handed out by name
namespace X_ParticleSystemImplementation
{
    SafetyHookInline shGetParticleSystem = {};
    uint8_t* __fastcall getParticleSystem(void* _this, void* edx, const char* szName, const char* szMaterial)
    {
        auto pSystem = shGetParticleSystem.unsafe_fastcall<uint8_t*>(_this, edx, szName, szMaterial);
        if (pSystem)
        {
            P_ParticleSystemBase::Forget(pSystem);
            if (float fIntensity = szName ? Rain::GetIntensity(szName) : 0.0f; fIntensity > 0.0f)
                Rain::Track(pSystem, fIntensity);
        }
        return pSystem;
    }

    SafetyHookInline shFreeParticleSystem = {};
    void __fastcall freeParticleSystem(void* _this, void* edx, uint8_t* pSystem)
    {
        P_ParticleSystemBase::Forget(pSystem);
        shFreeParticleSystem.unsafe_fastcall(_this, edx, pSystem);
    }
}

namespace MP_GameMode
{
    enum
    {
        VIEW_RENDERED = 0x148,         // set when renderMode renders the game view
        LEVEL_RUNTIME_CAMERA = 0x10A0, // X_LevelRuntimeCamera*
        PAUSED = 0x12CE,               // quickload/quicksave prompts
    };

    // MaxFX is Y up and its cameras look down Z, the effect is Z up, and the right of the
    // RenderWare camera it was made for points to the left
    RwV3d ToEffect(const float* v, float fSign = 1.0f)
    {
        return { v[0] * fSign, v[2] * fSign, v[1] * fSign };
    }

    float fTimeStep = 0.0f;

    SafetyHookInline shRenderMode = {};
    void __fastcall renderMode(uint8_t* _this, void* edx)
    {
        shRenderMode.unsafe_fastcall(_this, edx);

        bool bReady = pDevice && P_BaseObject::calculateObjectToWorldMatrix && X_LevelRuntimeCamera::getCamera &&
            X_LevelRuntimeCamera::roomsToRenderWithSkybox && X_ModeBase::getTimeUpdate && X_TimeUpdate::getRelativeTime;

        auto pLevelCamera = *(uint8_t**)(_this + LEVEL_RUNTIME_CAMERA);
        if (bReady && *(_this + VIEW_RENDERED) && pLevelCamera)
        {
            auto& camera = P_BaseObject::GetObjectToWorld(X_LevelRuntimeCamera::getCamera(pLevelCamera, nullptr));
            RwV3d pos = { camera[3][0], camera[3][1], camera[3][2] };

            WaterDrops::right = ToEffect(camera[0], -1.0f);
            WaterDrops::up = ToEffect(camera[1]);
            WaterDrops::at = ToEffect(camera[2]);
            WaterDrops::pos = ToEffect(camera[3]);

            // cuts of the camera don't sweep the drops away
            RwV3d move;
            RwV3dSub(&move, &WaterDrops::pos, &WaterDrops::ms_lastPos);
            if (RwV3dDotProduct(&move, &move) > 3.0f * 3.0f)
                WaterDrops::ms_lastPos = WaterDrops::pos;

            // indoors, where no room in sight has the sky, a rain box reaching in doesn't count
            bool bOutdoors = X_LevelRuntimeCamera::roomsToRenderWithSkybox(pLevelCamera, nullptr);
            WaterDrops::ms_rainIntensity = bOutdoors ? Rain::GetIntensityAt(pos) : 0.0f;

            // the drops follow the time of the game, so they stop with it and slow down in bullet time
            fTimeStep = X_TimeUpdate::getRelativeTime(X_ModeBase::getTimeUpdate(_this, nullptr), nullptr);
            WaterDrops::isPaused = *(_this + PAUSED) != 0;

            Xrd::Init(XRD_DEVICE_RENDERER, pDevice);
            WaterDrops::Process();
            if (WaterDrops::ms_numDrops > 0)
            {
                // the game view is done with its scene by now
                bool bScene = SUCCEEDED(pDevice->BeginScene());
                WaterDrops::Render();
                if (bScene)
                    pDevice->EndScene();
            }
        }

        ++Rain::nFrame;
    }
}

// MP_GameMode::renderMode
constexpr auto RenderModePattern = "6A FF 68 ? ? ? ? 64 A1 00 00 00 00 50 64 89 25 00 00 00 00 83 EC 18 53 56 57 8B F1 68 ? ? ? ? 8D 4C 24 1C FF 15 ? ? ? ? 8B CE C7 44 24 2C 00 00 00 00 C6 86 48 01 00 00 00";

void Init()
{
    WaterDrops::ReadIniSettings();
    WaterDrops::fTimeStep = &MP_GameMode::fTimeStep;

    auto pattern = hook::pattern(RenderModePattern);
    MP_GameMode::shRenderMode = safetyhook::create_inline(pattern.get_first(), MP_GameMode::renderMode); //0x451B20
}

safetyhook::MidHook GetDeviceHook = {};
safetyhook::MidHook ResetHook = {};
void InitE2_D3D8_DRIVER_MFC()
{
    auto pattern = hook::module_pattern(GetModuleHandle(L"e2_d3d8_driver_mfc"), "8B 86 ? ? ? ? 85 C0 74 0F 8B 50 50");
    GetDeviceHook = safetyhook::create_mid(pattern.get_first(6), [](SafetyHookContext& regs)
    {
        pDevice = (IDirect3DDevice8*)(*(uint32_t*)(regs.esi + 0x100));
    });

    pattern = hook::module_pattern(GetModuleHandle(L"e2_d3d8_driver_mfc"), "8B 86 ? ? ? ? 8B 10 8D 8E ? ? ? ? 51 50 FF 52 38");
    ResetHook = safetyhook::create_mid(pattern.get_first(6), [](SafetyHookContext& regs)
    {
        WaterDrops::Reset();
    });
}

void InitE2MFC()
{
    auto e2mfc = GetModuleHandle(L"e2mfc");
    P_BaseObject::calculateObjectToWorldMatrix = (decltype(P_BaseObject::calculateObjectToWorldMatrix))GetProcAddress(e2mfc, "?calculateObjectToWorldMatrix@P_BaseObject@@IAEXXZ");
    P_ParticleSystemBase::shBeginParticleDump = safetyhook::create_inline(GetProcAddress(e2mfc, "?beginParticleDump@P_ParticleSystemBase@@QAEXXZ"), P_ParticleSystemBase::beginParticleDump);
    P_ParticleSystemBase::shAddParticle = safetyhook::create_inline(GetProcAddress(e2mfc, "?addParticle@P_ParticleSystemBase@@QAEXABUParticleData@P_ParticleBuffer@@@Z"), P_ParticleSystemBase::addParticle);
    P_ParticleSystemBase::shDestructor = safetyhook::create_inline(GetProcAddress(e2mfc, "??1P_ParticleSystemBase@@UAE@XZ"), P_ParticleSystemBase::destructor);
}

void InitX_GameObjectsMFC()
{
    auto module = GetModuleHandle(L"X_GameObjectsMFC");
    X_LevelRuntimeCamera::getCamera = (decltype(X_LevelRuntimeCamera::getCamera))GetProcAddress(module, "?getCamera@X_LevelRuntimeCamera@@QAEPAVP_Camera@@XZ");
    X_LevelRuntimeCamera::roomsToRenderWithSkybox = (decltype(X_LevelRuntimeCamera::roomsToRenderWithSkybox))GetProcAddress(module, "?roomsToRenderWithSkybox@X_LevelRuntimeCamera@@QAE_NXZ");
    X_ParticleSystemImplementation::shGetParticleSystem = safetyhook::create_inline(GetProcAddress(module, "?getParticleSystem@X_ParticleSystemImplementation@@UAEPAVX_ParticleSystem@@PBD0@Z"), X_ParticleSystemImplementation::getParticleSystem);
    X_ParticleSystemImplementation::shFreeParticleSystem = safetyhook::create_inline(GetProcAddress(module, "?freeParticleSystem@X_ParticleSystemImplementation@@UAEXPAVX_ParticleSystem@@@Z"), X_ParticleSystemImplementation::freeParticleSystem);
}

void InitX_ModesMFC()
{
    X_ModeBase::getTimeUpdate = (decltype(X_ModeBase::getTimeUpdate))GetProcAddress(GetModuleHandle(L"X_ModesMFC"), "?getTimeUpdate@X_ModeBase@@QBEABVX_TimeUpdate@@XZ");
}

void InitX_HelpersMFC()
{
    X_TimeUpdate::getRelativeTime = (decltype(X_TimeUpdate::getRelativeTime))GetProcAddress(GetModuleHandle(L"X_HelpersMFC"), "?getRelativeTime@X_TimeUpdate@@QBEMXZ");
}

extern "C" __declspec(dllexport) void InitializeASI()
{
    std::call_once(CallbackHandler::flag, []()
    {
        CallbackHandler::RegisterCallbackAtGetSystemTimeAsFileTime(Init, hook::pattern(RenderModePattern));
        CallbackHandler::RegisterCallback(L"E2MFC.dll", InitE2MFC);
        CallbackHandler::RegisterCallback(L"E2_D3D8_DRIVER_MFC.dll", InitE2_D3D8_DRIVER_MFC);
        CallbackHandler::RegisterModuleUnloadCallback(L"E2_D3D8_DRIVER_MFC.dll", []() { GetDeviceHook.reset(); ResetHook.reset(); });
        CallbackHandler::RegisterCallback(L"X_GameObjectsMFC.dll", InitX_GameObjectsMFC);
        CallbackHandler::RegisterCallback(L"X_ModesMFC.dll", InitX_ModesMFC);
        CallbackHandler::RegisterCallback(L"X_HelpersMFC.dll", InitX_HelpersMFC);
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
