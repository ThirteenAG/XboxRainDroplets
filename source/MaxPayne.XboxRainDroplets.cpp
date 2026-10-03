// Direct3D 8, the API this game uses
#define XRD_ENABLE_D3D8
#include "xrd/xrd.h"
#include <unordered_map>

// It snows where the levels place snowfall particle systems, snowstorm_*, *_snowfall_* and the
// others of data\database\particles\particles.txt. Each one is a box the snow falls in, and the
// camera is in the snow while it is among the falling particles of one of them. The flakes are
// drawn over the game view once it is rendered, before the HUD.

IDirect3DDevice8* pDevice = nullptr;

namespace P_BaseObject
{
    // Points are transformed as p * M by three rows of axes, then the position
    constexpr ptrdiff_t OBJECT_TO_WORLD = 0x5C;

    void(__fastcall* calculateObjectToWorldMatrix)(void* _this, void* edx) = nullptr;

    const float(&GetObjectToWorld(uint8_t* pObject))[4][3]
    {
        calculateObjectToWorldMatrix(pObject, nullptr); // when it isn't up to date
        return *(float(*)[4][3])(pObject + OBJECT_TO_WORLD);
    }
}

namespace X_LevelRuntimeCamera
{
    constexpr ptrdiff_t CAMERA = 0x132; // P_Camera, what X_LevelRuntimeCamera::getCamera returns

    bool(__fastcall* roomsToRenderWithSkybox)(void* _this, void* edx) = nullptr;
}

namespace Snow
{
    // A snowfall particle system of the level
    struct Volume
    {
        float fIntensity;
        RwV3d Min;         // of its particles as of its last update
        RwV3d Max;
        uint32_t nUpdate;  // frame of that update
    };

    std::unordered_map<uint8_t*, Volume> Volumes;
    uint32_t nFrame = 0;

    // How hard it snows in a particle system, 0 for the ones that aren't snow falling, like the
    // snow the cars of the tenements throw up
    float GetIntensity(const char* szName)
    {
        std::string name(szName);
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        if (name.find("snow") == std::string::npos || name.starts_with("mercedes_"))
            return 0.0f;
        if (name.starts_with("snowstorm") || name.starts_with("snow_blizzard"))
            return 1.0f;
        if (name.starts_with("medium_") || name.find("fast") != std::string::npos || name.starts_with("nightmare"))
            return 0.7f;
        return 0.4f; // slow_snowfall_*, outside_snow, and the twister and whirl of snow
    }

    void Track(uint8_t* pSystem, float fIntensity)
    {
        Volumes[pSystem] = { fIntensity, { FLT_MAX, FLT_MAX, FLT_MAX }, { -FLT_MAX, -FLT_MAX, -FLT_MAX }, nFrame - 1000 };
    }

    // How hard it snows at the camera
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
    Snow::Volume* pDumpingVolume = nullptr;

    SafetyHookInline shBeginParticleDump = {};
    void __fastcall beginParticleDump(uint8_t* _this, void* edx)
    {
        shBeginParticleDump.unsafe_fastcall(_this, edx);

        auto it = Snow::Volumes.find(_this);
        pDumping = it != Snow::Volumes.end() ? _this : nullptr;
        pDumpingVolume = pDumping ? &it->second : nullptr;
        if (pDumpingVolume)
        {
            pDumpingVolume->Min = { FLT_MAX, FLT_MAX, FLT_MAX };
            pDumpingVolume->Max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
            pDumpingVolume->nUpdate = Snow::nFrame;
        }
    }

    // P_ParticleBuffer::ParticleData starts with the position, which the snow has in the world
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
        Snow::Volumes.erase(pSystem);
    }

    SafetyHookInline shDestructor = {};
    void __fastcall destructor(uint8_t* _this, void* edx)
    {
        Forget(_this);
        shDestructor.unsafe_fastcall(_this, edx);
    }
}

// The particle systems of particles.txt are handed out by name
namespace X_ParticleSystemImplementation
{
    SafetyHookInline shGetParticleSystem = {};
    uint8_t* __stdcall getParticleSystem(const char* szName, const char* szMaterial)
    {
        auto pSystem = shGetParticleSystem.unsafe_stdcall<uint8_t*>(szName, szMaterial);
        if (pSystem)
        {
            P_ParticleSystemBase::Forget(pSystem);
            if (float fIntensity = szName ? Snow::GetIntensity(szName) : 0.0f; fIntensity > 0.0f)
                Snow::Track(pSystem, fIntensity);
        }
        return pSystem;
    }

    SafetyHookInline shFreeParticleSystem = {};
    void __stdcall freeParticleSystem(uint8_t* pSystem)
    {
        P_ParticleSystemBase::Forget(pSystem);
        shFreeParticleSystem.unsafe_stdcall(pSystem);
    }
}

namespace MaxPayne_GameMode
{
    enum
    {
        TIME_UPDATE = 0x57,            // X_TimeUpdate, what X_ModeBase::getTimeUpdate returns
        VIEW_RENDERED = 0xEE,          // set when renderMode renders the game view
        LEVEL_RUNTIME_CAMERA = 0x1074, // X_LevelRuntimeCamera*
        PAUSED = 0x12CE,               // quickload/quicksave prompts
    };

    // X_TimeUpdate::getRelativeTime
    constexpr ptrdiff_t RELATIVE_TIME = TIME_UPDATE + 4;

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

        bool bReady = pDevice && P_BaseObject::calculateObjectToWorldMatrix;

        auto pLevelCamera = *(uint8_t**)(_this + LEVEL_RUNTIME_CAMERA);
        if (bReady && *(_this + VIEW_RENDERED) && pLevelCamera)
        {
            auto& camera = P_BaseObject::GetObjectToWorld(pLevelCamera + X_LevelRuntimeCamera::CAMERA);
            RwV3d pos = { camera[3][0], camera[3][1], camera[3][2] };

            WaterDrops::right = ToEffect(camera[0], -1.0f);
            WaterDrops::up = ToEffect(camera[1]);
            WaterDrops::at = ToEffect(camera[2]);
            WaterDrops::pos = ToEffect(camera[3]);

            // cuts of the camera don't sweep the flakes away
            RwV3d move;
            RwV3dSub(&move, &WaterDrops::pos, &WaterDrops::ms_lastPos);
            if (RwV3dDotProduct(&move, &move) > 3.0f * 3.0f)
                WaterDrops::ms_lastPos = WaterDrops::pos;

            // indoors, where no room in sight has the sky, a snow box reaching in doesn't count
            bool bOutdoors = X_LevelRuntimeCamera::roomsToRenderWithSkybox(pLevelCamera, nullptr);
            WaterDrops::ms_rainIntensity = bOutdoors ? Snow::GetIntensityAt(pos) : 0.0f;

            // the flakes follow the time of the game, so they stop with it and slow down in bullet time
            fTimeStep = *(float*)(_this + RELATIVE_TIME);
            WaterDrops::isPaused = *(_this + PAUSED) != 0;

            // flakes of snow instead of drops of rain, the ini read can reset this
            WaterDrops::SetSnow(true);

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

        ++Snow::nFrame;
    }
}

// MaxPayne_GameMode::renderMode
constexpr auto RenderModePattern = "6A FF 68 ? ? ? ? 64 A1 00 00 00 00 50 64 89 25 00 00 00 00 83 EC 20 53 56 8B F1 68 ? ? ? ? 8D 4C 24 20 FF 15 ? ? ? ? 33 DB 8B CE 89 5C 24 30 88 9E EE 00 00 00";

void Init()
{
    WaterDrops::ReadIniSettings();
    WaterDrops::fTimeStep = &MaxPayne_GameMode::fTimeStep;

    auto pattern = hook::pattern(RenderModePattern);
    MaxPayne_GameMode::shRenderMode = safetyhook::create_inline(pattern.get_first(), MaxPayne_GameMode::renderMode); //0x44EC60

    pattern = hook::pattern("56 57 8B F1 33 FF 8B 86 EC 05 00 00 85 C0 74 28 8B 8E F0 05 00 00 2B C8 C1 F9 02 3B F9 73 19 8B D0 8B 04 BA 8B 48 6C E8");
    X_LevelRuntimeCamera::roomsToRenderWithSkybox = (decltype(X_LevelRuntimeCamera::roomsToRenderWithSkybox))pattern.get_first(); //0x67A9B0

    pattern = hook::pattern("6A FF 68 ? ? ? ? 64 A1 00 00 00 00 50 64 89 25 00 00 00 00 83 EC 0C 53 55 56 57 68 ? ? ? ? 8D 4C 24 14 FF 15 ? ? ? ? 8A 0D");
    X_ParticleSystemImplementation::shGetParticleSystem = safetyhook::create_inline(pattern.get_first(), X_ParticleSystemImplementation::getParticleSystem); //0x673530

    pattern = hook::pattern("64 A1 00 00 00 00 8A 0D ? ? ? ? 6A FF 68 ? ? ? ? 50 64 89 25 00 00 00 00 83 EC 0C 53 55 B8 01 00 00 00 84 C8 56 57 75 30 8A D9 0A D8 88 1D");
    X_ParticleSystemImplementation::shFreeParticleSystem = safetyhook::create_inline(pattern.get_first(), X_ParticleSystemImplementation::freeParticleSystem); //0x673B80
}

safetyhook::MidHook GetDeviceHook = {};
safetyhook::MidHook ResetHook = {};
void InitE2_D3D8_DRIVER_MFC()
{
    auto pattern = hook::module_pattern(GetModuleHandle(L"e2_d3d8_driver_mfc"), "8B 86 ? ? ? ? 85 C0 74 0F 8B 50 38 8B 48 34");
    GetDeviceHook = safetyhook::create_mid(pattern.get_first(6), [](SafetyHookContext& regs)
    {
        pDevice = (IDirect3DDevice8*)(*(uint32_t*)(regs.esi + 0x0D0));
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

extern "C" __declspec(dllexport) void InitializeASI()
{
    std::call_once(CallbackHandler::flag, []()
    {
        // the executable is unpacked while it starts
        CallbackHandler::RegisterCallbackAtGetSystemTimeAsFileTime(Init, hook::pattern(RenderModePattern));
        CallbackHandler::RegisterCallback(L"E2MFC.dll", InitE2MFC);
        CallbackHandler::RegisterCallback(L"E2_D3D8_DRIVER_MFC.dll", InitE2_D3D8_DRIVER_MFC);
        CallbackHandler::RegisterModuleUnloadCallback(L"E2_D3D8_DRIVER_MFC.dll", []() { GetDeviceHook.reset(); ResetHook.reset(); });
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
