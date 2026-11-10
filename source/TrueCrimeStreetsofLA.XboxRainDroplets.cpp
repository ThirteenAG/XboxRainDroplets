// Direct3D 8, the API this game uses
#define XRD_ENABLE_D3D8
#include "xrd/xrd.h"

// The world's y axis points down in this game: up is -y.

// The view the world is drawn with: the camera's world matrix (right, up, forward and position rows, 16 bytes each)
constexpr ptrdiff_t ViewCameraMatrix = 0x220;
// The world: the game state at +8 (0 and 1 play, 2 and above menus)
constexpr ptrdiff_t WorldGameMode = 8;
// The game controller: set while a scripted scene camera is in charge
constexpr ptrdiff_t ControllerSceneCamera = 0x1D0;

uint8_t* View = nullptr;
uint8_t* World = nullptr;
uint8_t** Controller = nullptr;
uint8_t** Player = nullptr;
float* RainAmount = nullptr;
IDirect3DDevice8** Device = nullptr;

// The world's ray test: from, to, hit position, hit normal, two objects to ignore, collision mask, closest hit, kind
void* RayWorld = nullptr;
void* (__fastcall* CheckRay)(void*, void*, const float*, const float*, float*, float*, void*, void*, int, int, int) = nullptr;

// The water jet of a hydrant that was knocked over: an emitter on the hydrant that keeps adding the geyser particles
// while it lasts. At the start of its update it moves along with the hydrant (+0xC0): its world matrix is at +0x10,
// the position at +0x40. Elapsed time at +0xC4, duration at +0xCC, rate at +0xD0; the emitters with +0xD4 set add
// other particles than water.
struct Jet
{
    uintptr_t emitter = 0;
    RwV3d pos{};
    float strength = 0.0f;
    LARGE_INTEGER seen{};
};
std::array<Jet, 16> Jets;

constexpr float JetHeight = 4.0f;       // how high the water goes above the hydrant
constexpr float JetSoaked = 2.0f;       // closer than this the camera is in the spray, whichever way it looks
constexpr float JetReach = 10.0f;       // farther than this nothing reaches the camera
constexpr float JetFacing = 0.25f;      // the least the camera has to face the jet from outside the spray

double SecondsSince(const LARGE_INTEGER& then)
{
    static LARGE_INTEGER frequency = []()
    {
        LARGE_INTEGER value{};
        QueryPerformanceFrequency(&value);
        return value;
    }();
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    return double(now.QuadPart - then.QuadPart) / double(frequency.QuadPart);
}

void SeeJet(uintptr_t emitter)
{
    if (*reinterpret_cast<int32_t*>(emitter + 4) < 0 || *reinterpret_cast<uint8_t*>(emitter + 0xD4))
        return;

    const float* p = reinterpret_cast<const float*>(emitter + 0x40);
    const float elapsed = *reinterpret_cast<float*>(emitter + 0xC4);
    const float duration = *reinterpret_cast<float*>(emitter + 0xCC);
    float strength = std::clamp(*reinterpret_cast<float*>(emitter + 0xD0), 0.0f, 1.0f);
    // The game thins the jet out over the second half of its time
    if (duration > 0.0f && duration - elapsed < duration * 0.5f)
        strength *= std::clamp((duration - elapsed) / (duration * 0.5f), 0.0f, 1.0f);
    if (!std::isfinite(strength) || !std::isfinite(p[0]) || !std::isfinite(p[1]) || !std::isfinite(p[2]))
        return;

    Jet* slot = nullptr;
    for (auto& jet : Jets)
    {
        if (jet.emitter == emitter)
        {
            slot = &jet;
            break;
        }
        if (!slot && (!jet.emitter || SecondsSince(jet.seen) > 1.0))
            slot = &jet;
    }
    if (!slot)
        return;

    slot->emitter = emitter;
    slot->pos = { p[0], p[1], p[2] };
    slot->strength = strength;
    QueryPerformanceCounter(&slot->seen);
}

// The water the jets near the camera throw at the lens
void SprayJets()
{
    for (auto& jet : Jets)
    {
        if (!jet.emitter)
            continue;
        // An emitter the game stopped updating is gone
        if (SecondsSince(jet.seen) > 0.5)
        {
            jet.emitter = 0;
            continue;
        }

        // The nearest point of the column of water above the hydrant
        const RwV3d& cam = WaterDrops::pos;
        const float height = std::clamp(jet.pos.y - cam.y, 0.0f, JetHeight);
        const RwV3d nearest = { jet.pos.x, jet.pos.y - height, jet.pos.z };
        RwV3d to = { nearest.x - cam.x, nearest.y - cam.y, nearest.z - cam.z };
        const float distance = sqrtf(RwV3dDotProduct(&to, &to));
        if (!(distance < JetReach))
            continue;

        float amount = jet.strength;
        if (distance > JetSoaked)
        {
            RwV3d dir = { to.x / distance, to.y / distance, to.z / distance };
            const float facing = RwV3dDotProduct(&dir, &WaterDrops::at);
            if (facing < JetFacing)
                continue;
            amount *= (1.0f - (distance - JetSoaked) / (JetReach - JetSoaked)) * facing;
        }
        WaterDrops::FillScreenMovingRate(amount);
    }
}

// Rain doesn't reach a camera under a roof, a bridge or a tunnel: a ray straight up from the camera
bool CameraCovered()
{
    static bool covered = false;
    static int frame = 0;
    if (frame++ % 6)
        return covered;

    const RwV3d& cam = WaterDrops::pos;
    alignas(16) float from[4]{ cam.x, cam.y, cam.z, 1.0f };
    alignas(16) float to[4]{ cam.x, cam.y - 40.0f, cam.z, 1.0f };
    alignas(16) float hit[4]{}, normal[4]{};
    covered = CheckRay(RayWorld, nullptr, from, to, hit, normal, Player ? *Player : nullptr, nullptr, 0xFF00, 1, 0) != nullptr;
    return covered;
}

void Init()
{
    WaterDrops::ReadSettings();

    // After the world, its particles and the rain are drawn, before the HUD
    auto pattern = hook::pattern("56 53 68 ? ? ? ? 6A 04 B9 ? ? ? ? E8 ? ? ? ? 53 B9");
    View = *pattern.get_first<uint8_t*>(3);
    World = *pattern.get_first<uint8_t*>(10);
    auto render = pattern.get_first(19);

    // The amount of rain, 0 to 1: missions set it, the world draws the rain with it
    RainAmount = *hook::pattern("B9 ? ? ? ? E8 ? ? ? ? 8B 0D ? ? ? ? B8 71 6E B0 36").get_first<float*>(1);
    Device = *hook::pattern("A1 ? ? ? ? 8B 08 6A 02 50 FF 91 94 00 00 00").get_first<IDirect3DDevice8**>(1);
    Controller = *hook::pattern("55 8B EC 83 E4 F0 83 EC 64 A1 ? ? ? ? 8B 88 70 03 00 00").get_first<uint8_t**>(10);
    Player = *hook::pattern("8B 0D ? ? ? ? 8B ? ? ? ? ? 32 DB").get_first<uint8_t**>(2);

    pattern = hook::pattern("B9 ? ? ? ? E8 ? ? ? ? 85 C0 74 ? 8B C8");
    RayWorld = *pattern.get_first<void*>(1);
    CheckRay = reinterpret_cast<decltype(CheckRay)>(injector::GetBranchDestination(pattern.get_first(5)).as_int());

    static auto RenderHook = safetyhook::create_mid(render, [](SafetyHookContext&)
    {
        IDirect3DDevice8* device = *Device;
        if (!device)
            return;

        const bool scene = Controller && *Controller && *(*Controller + ControllerSceneCamera);
        if (scene)
        {
            WaterDrops::Clear();
            return;
        }
        // Menus and the pause screen: the drops wait
        if (*reinterpret_cast<int32_t*>(World + WorldGameMode) >= 2)
            return;

        const float* m = reinterpret_cast<const float*>(View + ViewCameraMatrix);
        // The drops take right as the left of the screen; the camera's y row points down the screen like the world's
        WaterDrops::right = { -m[0], -m[1], -m[2] };
        WaterDrops::up = { -m[4], -m[5], -m[6] };
        WaterDrops::at = { m[8], m[9], m[10] };
        WaterDrops::pos = { m[12], m[13], m[14] };

        const float rain = *RainAmount;
        if (rain > 0.0f && std::isfinite(rain) && !CameraCovered())
            WaterDrops::ms_rainIntensity = std::clamp(rain / 0.45f, 0.28f, 1.0f);
        else
            WaterDrops::ms_rainIntensity = 0.0f;

        if (!Xrd::Init(XRD_DEVICE_RENDERER, device))
            return;
        SprayJets();
        WaterDrops::Process();
        WaterDrops::Render();
    });

    // The game releases its device and makes a new one on a resolution change: everything made on the old one goes
    // first
    static auto ReleaseHook = safetyhook::create_mid(hook::pattern("56 8B 35 ? ? ? ? 85 F6 74 ? EB ? ? ? ? F6 46 04 02 74 ? 8B 06 8B CE FF 50 0C").get_first(),
        [](SafetyHookContext&)
    {
        WaterDrops::Reset();
    });

    // The update of the hydrant water jets
    static auto JetHook = safetyhook::create_mid(hook::pattern("55 8B EC 83 E4 F0 83 EC 54 53 56 57 8B F1 E8 ? ? ? ? 8B 46 04 85 C0 0F 88").get_first(19),
        [](SafetyHookContext& regs)
    {
        SeeJet(regs.esi);
    });
}

extern "C" __declspec(dllexport) void InitializeASI()
{
    std::call_once(CallbackHandler::flag, []()
    {
        RegisterOldPluginCheck();
        CallbackHandler::RegisterCallbackAtGetSystemTimeAsFileTime(Init, hook::pattern("BF 94 00 00 00 8B C7"));
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
