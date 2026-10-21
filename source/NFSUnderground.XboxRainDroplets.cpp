// Direct3D 9, the API this game uses
#define XRD_ENABLE_D3D9
#include "xrd/xrd.h"

// Need for Speed Underground has no rain at all, so the plugin makes it up, as the one of
// Driver: Parallel Lines does: when a race starts, it is decided at random whether it
// rains in it (RainChance of the settings), and how hard. It rains for the whole race, from
// the flyby of the camera to the finish line, and stops when the race is over, in its
// replays and in the front end.

static IDirect3DDevice9** pDevice = nullptr;
static int32_t* pGameState = nullptr;      // 3: the front end, 6: in a race
static int32_t** pRaceCoordinator = nullptr;
static uint8_t* pPlayerView = nullptr;     // eView of the first player, its Camera at +0x40

// The states of RaceCoordinator, as the game names them (RCSTATE_...)
enum RCState
{
    RCSTATE_NONE, RCSTATE_LOADING, RCSTATE_CAMERA_FLYBY, RCSTATE_PRERACE, RCSTATE_RACE_IN_PROGRESS,
    RCSTATE_INSTANT_REPLAY, RCSTATE_AUTO_REPLAY, RCSTATE_PAUSE, RCSTATE_POSTRACE,
};

constexpr int32_t GameStateRacing = 6;

// The rain of the current race
static bool bRaceHasRain = false;
static float fRaceRainIntensity = 0.0f;

static void UpdateRaceRain()
{
    static int32_t lastState = -1;
    int32_t state = (pRaceCoordinator && *pRaceCoordinator) ? **pRaceCoordinator : RCSTATE_NONE;
    bool racing = *pGameState == GameStateRacing;
    if (!racing)
        state = RCSTATE_NONE;

    // A race starts with the flyby of the camera, or with the countdown when there is no
    // flyby (a restarted race goes from the pause straight to it): the flyby is followed by
    // the countdown of the same race, which doesn't decide again
    if (state != lastState)
    {
        bool start = (state == RCSTATE_CAMERA_FLYBY || state == RCSTATE_PRERACE) && lastState != RCSTATE_CAMERA_FLYBY;
        if (start)
        {
            int chance = std::clamp(XrdSettings().Int("MAIN.RainChance"), 0, 100);
            bRaceHasRain = WaterDrops::GetRandomInt(99) < chance;
            fRaceRainIntensity = (1.0f - 0.65f) + WaterDrops::GetRandomFloat(0.65f);
        }
        if (state == RCSTATE_NONE || state == RCSTATE_POSTRACE)
            bRaceHasRain = false;
        lastState = state;
    }

    bool raining = bRaceHasRain && (state == RCSTATE_CAMERA_FLYBY || state == RCSTATE_PRERACE ||
        state == RCSTATE_RACE_IN_PROGRESS || state == RCSTATE_PAUSE);
    WaterDrops::ms_rainIntensity = raining ? fRaceRainIntensity : 0.0f;
    // the glass stops with the game while it is paused
    WaterDrops::isPaused = state == RCSTATE_PAUSE;
}

static RwV3d Normalized(float x, float y, float z)
{
    float length = sqrtf(x * x + y * y + z * z);
    if (length == 0.0f)
        return { 1.0f, 0.0f, 0.0f };
    return { x / length, y / length, z / length };
}

// The camera of the first player's view: its matrix (the view matrix, bMatrix4) at +0, its
// position at +0x40 and the direction it looks in at +0x50, as Camera::SetCameraMatrix
// fills them. The columns of the view matrix are the camera's left and up.
static bool ReadCamera()
{
    auto camera = pPlayerView ? *reinterpret_cast<float**>(pPlayerView + 0x40) : nullptr;
    if (!camera)
        return false;
    auto left = Normalized(camera[0], camera[4], camera[8]);
    auto up = Normalized(camera[1], camera[5], camera[9]);
    WaterDrops::right = { -left.x, -left.y, -left.z };
    WaterDrops::up = { -up.x, -up.y, -up.z };
    WaterDrops::at = { camera[0x50 / 4], camera[0x50 / 4 + 1], camera[0x50 / 4 + 2] };
    WaterDrops::pos = { camera[0x40 / 4], camera[0x40 / 4 + 1], camera[0x40 / 4 + 2] };
    return true;
}

// eDisplayFrame, right after the world of the views is drawn and before the front end
// (the HUD, the pause menu) is drawn over it: the drops go in between
static void OnWorldDrawn()
{
    if (!pDevice || !*pDevice)
        return;

    if (*pGameState != GameStateRacing || !ReadCamera())
    {
        // the front end and the loading screens: nothing on the lens
        UpdateRaceRain();
        if (WaterDrops::ms_numDrops || WaterDrops::ms_numDropsMoving)
            WaterDrops::Clear();
        return;
    }

    UpdateRaceRain();
    Xrd::Init(XRD_DEVICE_RENDERER, *pDevice);
    WaterDrops::Process();
    WaterDrops::Render();
}

void Init()
{
    WaterDrops::ReadSettings();

    // the device, read where the game resets it, which the effect follows
    auto pattern = hook::pattern("A1 ? ? ? ? 8B 10 68 ? ? ? ? 50 FF 52 40");
    pDevice = *pattern.get_first<IDirect3DDevice9**>(1);
    static auto ResetHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
    {
        WaterDrops::Reset();
    });

    pattern = hook::pattern("83 3D ? ? ? ? 06 ? ? A1");
    pGameState = *pattern.get_first<int32_t*>(2);

    pattern = hook::pattern("A1 ? ? ? ? 85 C0 74 ? ? ? 83 F8 ? 74 ? 83 F8 ? 75 ? 8A 43");
    pRaceCoordinator = *pattern.get_first<int32_t**>(1);

    // the first player's view, which the world of a race is drawn into
    pattern = hook::pattern("68 ? ? ? ? B9 01 00 00 00 8D 54 24 ? E8");
    pPlayerView = *pattern.get_first<uint8_t*>(1);

    // the time step of the frame in seconds, EAXSound::Update(RealTimeElapsed) in MainLoop
    pattern = hook::pattern("52 E8 ? ? ? ? 8B 2D ? ? ? ? BF");
    WaterDrops::fTimeStep = *pattern.get_first<float*>(8);

    // eDisplayFrame: BeginScene, the front end's background (1), the world, the front end;
    // the call that draws the world ends 15 bytes in (12 in the Russian 1.4)
    pattern = hook::pattern("6A 01 E8 ? ? ? ? 83 C4 04 E8 ? ? ? ? A1 ? ? ? ? 33 F6");
    void* worldDrawn = !pattern.empty() ? pattern.get_first(15) : nullptr;
    if (!worldDrawn)
    {
        pattern = hook::pattern("6A 01 E8 ? ? ? ? E8 ? ? ? ? 33 F6 56 E8");
        worldDrawn = pattern.get_first(12);
    }
    static auto WorldDrawnHook = safetyhook::create_mid(worldDrawn, [](SafetyHookContext& regs)
    {
        OnWorldDrawn();
    });
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
