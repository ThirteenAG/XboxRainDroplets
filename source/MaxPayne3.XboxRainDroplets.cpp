#include <injector\injector.hpp>
#include <injector\hooking.hpp>
#include <injector\calling.hpp>
#include <injector\utility.hpp>
#include <injector\assembly.hpp>
#define XRD_ENABLE_D3D9
#define XRD_ENABLE_D3D10
#define XRD_ENABLE_D3D10_1
#define XRD_ENABLE_D3D11
#define XRD_ENABLE_D3D12
#include "xrd/xrd.h"
#include <atomic>
#include <mutex>
#include <cmath>

// The settings are described like those of every plugin (see XrdSettings in xrd/xrd.h), and Fusion Fix shows
// them in its Options menu as a menu host of ASI Settings, after its own pages.

namespace CTimer
{
    GameRef<bool> m_UserPause;
    GameRef<bool> m_CodePause;
}

// What the weather update decides about rain on the camera, handed from the game thread to the render thread.
// The game still runs its rain-on-lens checks, but on PC its own lens effect is gone: the rain request is set and
// then dropped unread.
struct LensRain
{
    float intensity;
    bool snow;
    bool cutscene;
    RwV3d right;
    RwV3d forward;
    RwV3d up;
    RwV3d pos;
};

static std::mutex lensRainMutex;
static LensRain lensRain{};
static std::atomic<uint32_t> lensRainUpdates = 0;

void Init()
{
    WaterDrops::ReadSettings(true);

    WaterDrops::ms_rainIntensity = 0.0f;

    static auto ppDevice = *hook::get_pattern<IDirect3DDevice9**>("68 ? ? ? ? 68 ? ? ? ? 8B D7 83 CA 10", 1);
    static auto ppSwapChain = *hook::get_pattern<IDXGISwapChain**>("A1 ? ? ? ? 8B 08 53 8D 54 24 14 52 50", 1);

    auto pattern = hook::pattern("A3 ? ? ? ? 8B 16 8B 82 ? ? ? ? 6A 03");
    static auto D3D11CreateDeviceAndSwapChain = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& ctx)
    {
        auto pSwapChain = *ppSwapChain;
        Xrd::Init(Xrd::RENDERER_D3D11, pSwapChain);
    });

    pattern = hook::pattern("A3 ? ? ? ? 8B 06 8B 90 ? ? ? ? 6A 06");
    static auto D3D9CreateDevice = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& ctx)
    {
        auto pDevice = *ppDevice;
        Xrd::Init(Xrd::RENDERER_D3D9, pDevice);
    });

    // The game's own pause, the user's (pause menu) and the code's. The clock keeps its last time step while
    // either is set, so the drops have to be held here.
    pattern = hook::pattern("84 C0 75 17 A0 ? ? ? ? 0A 05 ? ? ? ? 74 05");
    CTimer::m_UserPause.SetAddress(*pattern.get_first<bool*>(5));
    CTimer::m_CodePause.SetAddress(*pattern.get_first<bool*>(11));

    // The front end (main menu), which plays a scene of its own, rain included, behind the menu
    pattern = hook::pattern("80 3D ? ? ? ? 00 0F 94 C0 A2");
    static auto bFrontendActive = *pattern.get_first<bool*>(2);

    // The rain- and snow-on-lens checks of the weather update: rain or snow on the weather, all five probes from
    // around the camera up into the sky clear of cover, no cutscene running, and no camera inside a vehicle (set by
    // the scripts). Both are scaled by how far the camera is outside, the smoothed exposure the weather uses for
    // all of its rain and snow. Unlike the game, the drops don't wait for the camera to look up into the sky: a
    // lens gets wet looking ahead too.
    pattern = hook::pattern("F3 0F 10 86 ? ? ? ? 0F 2F C5 B3 01 77 0D F3 0F 10 86 ? ? ? ? 0F 2F C5 76 26 33 C0 EB 03 8D 49 00 80 BC 06 ? ? ? ? 00 75 13");
    static auto nRainOffset = *pattern.get_first<uint32_t>(4);
    static auto nSnowOffset = *pattern.get_first<uint32_t>(19);
    static auto nSkyProbesOffset = *pattern.get_first<uint32_t>(38);

    pattern = hook::pattern("E8 ? ? ? ? 85 C0 74 14 E8 ? ? ? ? 8B C8 E8 ? ? ? ? 84 C0 0F 85 ? ? ? ? 84 DB 0F 84 ? ? ? ? 80 3D ? ? ? ? 00");
    static auto GetCutsceneManager = injector::GetBranchDestination(pattern.get_first(0));
    static auto IsCutscenePlaying = injector::GetBranchDestination(pattern.get_first(16));
    static auto bCamInsideVehicle = *pattern.get_first<bool*>(39);

    // The frame of the camera that is rendered (gameplay, script or cutscene): right, forward, up and position,
    // 16 bytes apart; the weather probes the sky above the gameplay camera
    pattern = hook::pattern("83 EC 50 E8 ? ? ? ? F3 0F 10 08 F3 0F 10 50 04 F3 0F 10 58 08 F3 0F 10 05");
    static auto pFinalCameraMatrix = *(uintptr_t*)(injector::GetBranchDestination(pattern.get_first(3)).as_int() + 1) - 0x30;

    pattern = hook::pattern("8B CE E8 ? ? ? ? D9 05 ? ? ? ? 51 D9 1C 24 57 8B CE E8");
    static auto pRainExposure = *pattern.get_first<float*>(9);
    static auto LensRainHook = safetyhook::create_mid(pattern.get_first(18), [](SafetyHookContext& ctx)
    {
        auto weather = ctx.esi;
        auto camera = pFinalCameraMatrix;

        const float rain = *(float*)(weather + nRainOffset);
        const float snow = *(float*)(weather + nSnowOffset);

        bool bSkyClear = true;
        for (uint32_t i = 0; i < 5; i++)
        {
            if (*(uint8_t*)(weather + nSkyProbesOffset + i))
                bSkyClear = false;
        }

        auto pCutsceneManager = injector::cstd<void*()>::call(GetCutsceneManager);
        const bool bCutscene = pCutsceneManager && injector::thiscall<bool(void*)>::call(IsCutscenePlaying, pCutsceneManager);

        // The weather of the level says which of the two falls; with neither, the lens keeps the kind it has
        static bool bSnow = false;
        if (snow > rain)
            bSnow = true;
        else if (rain > snow)
            bSnow = false;

        float intensity = 0.0f;
        const float fall = bSnow ? snow : rain;
        if (fall > 0.0f && bSkyClear && !bCutscene && !*bCamInsideVehicle && !*bFrontendActive)
            intensity = fall * *pRainExposure;

        {
            std::lock_guard<std::mutex> lock(lensRainMutex);
            lensRain.intensity = intensity;
            lensRain.snow = bSnow;
            lensRain.cutscene = bCutscene;
            lensRain.right = *(RwV3d*)(camera + 0x00);
            lensRain.forward = *(RwV3d*)(camera + 0x10);
            lensRain.up = *(RwV3d*)(camera + 0x20);
            lensRain.pos = *(RwV3d*)(camera + 0x30);
        }
        lensRainUpdates++;
    });

    pattern = hook::pattern("68 ? ? ? ? E8 ? ? ? ? 8B 15 ? ? ? ? 8B 0D ? ? ? ? 8B 04 95 ? ? ? ? 83 C4 08 03 C1");
    static auto shsub_12D4470 = safetyhook::create_mid(*pattern.get_first<void*>(1), [](SafetyHookContext& ctx)
    {
        // A weather update that stopped without the game being paused leaves no rain behind
        static uint32_t lastUpdates = 0;
        static int32_t framesWithoutUpdate = 0;
        const uint32_t updates = lensRainUpdates;
        WaterDrops::isPaused = CTimer::m_UserPause || CTimer::m_CodePause;
        if (updates != lastUpdates || WaterDrops::isPaused)
            framesWithoutUpdate = 0;
        else if (framesWithoutUpdate < 10)
            framesWithoutUpdate++;
        lastUpdates = updates;

        bool bCutscene = false;
        bool bSnow = false;
        {
            std::lock_guard<std::mutex> lock(lensRainMutex);
            bCutscene = lensRain.cutscene;
            bSnow = lensRain.snow;
            // The first row of the matrix points to the right of the screen and
            // the effect takes right as pointing to the left of it, the way
            // RenderWare has it, or the drops drift the wrong way when the
            // camera turns.
            WaterDrops::right = { -lensRain.right.x, -lensRain.right.y, -lensRain.right.z };
            WaterDrops::up = lensRain.forward;
            WaterDrops::at = lensRain.up;
            WaterDrops::pos = lensRain.pos;
            WaterDrops::ms_rainIntensity = framesWithoutUpdate < 10 ? lensRain.intensity : 0.0f;
        }

        // Snow flakes or rain drops, as the level has it; SetSnow only does anything when that changes
        WaterDrops::SetSnow(bSnow);

        // A cutscene starts with a clean lens and keeps it clean while it plays
        if (bCutscene)
        {
            WaterDrops::Clear();
            return;
        }

        #ifdef DEBUG
        WaterDrops::ms_rainIntensity = 1.0f;
        #endif // DEBUG
        WaterDrops::Process();
        WaterDrops::Render();
    });

    pattern = hook::pattern("FF D2 8B 06 8B 90 ? ? ? ? 8B CE FF D2 A1 ? ? ? ? 8B 08");
    static auto D3D9onReset = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& ctx)
    {
        WaterDrops::Reset();
    });

    pattern = hook::pattern("D9 05 ? ? ? ? 8B 10 51");
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
