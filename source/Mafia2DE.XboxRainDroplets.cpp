// Direct3D 11, the API this game uses
#define XRD_ENABLE_D3D11
#include "xrd/xrd.h"

// Mafia II: Definitive Edition. Rain and snow both fall in it, by the season of
// the chapter, and both land on the lens: drops of rain, flakes of snow. The
// weather of the game says how hard it rains and snows, the camera the frame is
// rendered with says where the lens is, the physics of the world says whether it
// is under a roof, and the drops are drawn right before the HUD. A broken fire
// hydrant wets the lens too.

namespace Mafia2
{
    // The manager of the environment of the world, and on it the weather
    // manager. Its parameters are kept in a map of each of its categories,
    // std::map<int, parameter> at +48 + 8 * the category, and a parameter keeps
    // its name: RainIntensity and SnowIntensity, from 0 to 100, are what the
    // weather rains and snows with.
    uint8_t** Environment = nullptr;
    constexpr ptrdiff_t EnvironmentWeather = 0x20;
    constexpr ptrdiff_t WeatherCategories = 48;
    constexpr int Categories = 3;
    // a node of such a map: left, parent, right, whether it is the head, its id,
    // its name and its value
    constexpr ptrdiff_t NodeLeft = 0, NodeParent = 8, NodeRight = 16, NodeIsNil = 25;
    constexpr ptrdiff_t NodeName = 48, NodeValue = 64;

    // The viewport the world is rendered into, see the camera manager: the camera
    // assigned to it is the one the frame is rendered with, in a cutscene too.
    void* (*GetMainViewport)() = nullptr;
    constexpr ptrdiff_t ViewportCamera = 0x30;

    // The view matrix of a C_Camera, the inverse of its world matrix: three rows
    // of four floats, its right, its forward and its up in the world, each with
    // its part of the translation. The world has z up.
    constexpr ptrdiff_t CameraView = 0xDC;

    // The renderer, which the listeners of the HUD are called from: its device,
    // and on that the device and the context of Direct3D 11 and the swap chain.
    constexpr ptrdiff_t RendererDevice = 0xD0;
    constexpr ptrdiff_t DeviceContext = 0x2798;
    constexpr ptrdiff_t DeviceSwapChain = 0x27B0;
    constexpr ptrdiff_t SwapChainDXGI = 0x18;
    // the pass the HUD and the menus are drawn in, see Listeners
    constexpr int HudPass = 1;

    // The pause of the game: how many things keep it paused, the menu of the
    // pause among them, which blurs the picture the drops would be drawn over.
    uint8_t** Pause = nullptr;
    constexpr ptrdiff_t PauseCount = 0x8;

    // The ray the game casts through the physics of the world, against its main
    // scene and the scenes of the parts of the city the ray passes: the ground
    // under a character, the line of sight of the AI. It returns whether
    // something is hit, and what the flags of the query ask for of the nearest.
    struct RayQuery
    {
        float origin[3];
        float direction[3];
        float distance;
        uint32_t flags;
        uint32_t groups;
    };

    struct RayHit
    {
        uint32_t unused;
        float position[3];
        float normal[3];
        uint32_t face;
        float distance;
        uint32_t padding;
        void* object;
        int64_t material;
    };

    char (*RayCast)(void* queue, const RayQuery* query, RayHit* hit) = nullptr;
    // the distance of the nearest hit
    constexpr uint32_t RayHitDistance = 8;
    // the groups of shapes the camera collides with
    constexpr uint32_t CameraGroups = 0x6D820A;

    // The hydrants that are broken: a part of a thing that breaks, whose
    // definition has a jet of water of its own (HYDRANT: in it), is a hydrant,
    // and the game keeps the parts of them in a list it looks at every tenth of a
    // second, see HydrantTick. A part keeps the water of it: the gush it breaks
    // with, which keeps going, the jet that takes over from it while something
    // is over the hydrant and whether that one runs, and where they are, three
    // rows of four floats of a matrix in the world.
    constexpr ptrdiff_t HydrantsBegin = 0x8, HydrantsEnd = 0x10;
    constexpr ptrdiff_t PartHydrant = 0xB8;
    constexpr ptrdiff_t HydrantGush = 0x00, HydrantRuns = 0x20, HydrantJet = 0x28, HydrantMatrix = 0x34;
    // C_Effect::IsPlaying
    constexpr size_t EffectIsPlaying = 0x138 / 8;

    template <size_t Index, typename Ret, typename... Args>
    inline Ret Virtual(void* object, Args... args)
    {
        using Fn = Ret(*)(void*, Args...);
        return (*reinterpret_cast<Fn**>(object))[Index](object, args...);
    }
}

// The parameters of the weather by their name, see Mafia2::Environment. What is
// read of the game is read guarded, a map the weather is changing may be half
// way through it.
namespace Weather
{
    inline float* rain = nullptr;
    inline float* snow = nullptr;
    inline float sinceLookup = 1.0e9f;
    constexpr float LookupInterval = 1.0f;

    static float* FindIn(uint8_t* map, const char* name)
    {
        auto* head = *reinterpret_cast<uint8_t**>(map + 8);
        if (!head)
            return nullptr;

        uint8_t* stack[64];
        int depth = 0;
        int visited = 0;
        uint8_t* node = *reinterpret_cast<uint8_t**>(head + Mafia2::NodeParent);
        while ((node && !node[Mafia2::NodeIsNil]) || depth > 0)
        {
            while (node && !node[Mafia2::NodeIsNil])
            {
                if (depth == 64 || ++visited > 4096)
                    return nullptr;
                stack[depth++] = node;
                node = *reinterpret_cast<uint8_t**>(node + Mafia2::NodeLeft);
            }

            node = stack[--depth];
            const char* nodeName = *reinterpret_cast<const char**>(node + Mafia2::NodeName);
            if (nodeName && strcmp(nodeName, name) == 0)
                return reinterpret_cast<float*>(node + Mafia2::NodeValue);
            node = *reinterpret_cast<uint8_t**>(node + Mafia2::NodeRight);
        }

        return nullptr;
    }

    static float* Find(const char* name)
    {
        __try
        {
            if (!Mafia2::Environment || !*Mafia2::Environment)
                return nullptr;
            auto* weather = *reinterpret_cast<uint8_t**>(*Mafia2::Environment + Mafia2::EnvironmentWeather);
            if (!weather)
                return nullptr;
            for (int category = 0; category < Mafia2::Categories; category++)
            {
                auto* map = *reinterpret_cast<uint8_t**>(weather + Mafia2::WeatherCategories + 8 * category);
                if (!map)
                    continue;
                if (float* value = FindIn(map, name))
                    return value;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
        return nullptr;
    }

    static float Read(float* value)
    {
        __try
        {
            return value && std::isfinite(*value) ? *value : 0.0f;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return 0.0f;
        }
    }

    // the parameters are looked for again now and then: the weather of the next
    // chapter has maps of its own
    inline void Update(float dt)
    {
        sinceLookup += dt;
        if (sinceLookup < LookupInterval)
            return;
        sinceLookup = 0.0f;
        rain = Find("RainIntensity");
        snow = Find("SnowIntensity");
    }
}

// The address a rip relative operand at p points to, p being where its four bytes
// are and the instruction ending right after them.
template <typename T>
static T* Relative(uint8_t* p)
{
    return reinterpret_cast<T*>(p + 4 + *reinterpret_cast<int32_t*>(p));
}

RwV3d gCameraPos = {};

// The lens of the camera the frame is rendered with, out of its view matrix.
static bool UpdateCamera()
{
    if (!Mafia2::GetMainViewport)
        return false;

    auto* viewport = static_cast<uint8_t*>(Mafia2::GetMainViewport());
    if (!viewport)
        return false;

    auto* camera = *reinterpret_cast<uint8_t**>(viewport + Mafia2::ViewportCamera);
    if (!camera)
        return false;

    const float* v = reinterpret_cast<const float*>(camera + Mafia2::CameraView);
    const RwV3d right = { v[0], v[1], v[2] };
    const RwV3d forward = { v[4], v[5], v[6] };
    const RwV3d up = { v[8], v[9], v[10] };

    // the place of the camera, out of the translation of the inverse
    gCameraPos = {
        -(v[3] * right.x + v[7] * forward.x + v[11] * up.x),
        -(v[3] * right.y + v[7] * forward.y + v[11] * up.y),
        -(v[3] * right.z + v[7] * forward.z + v[11] * up.z),
    };

    if (!std::isfinite(gCameraPos.x) || !std::isfinite(gCameraPos.y) || !std::isfinite(gCameraPos.z))
        return false;

    // The effect takes the right of a camera the way RenderWare has it, pointing
    // to the left of the screen.
    WaterDrops::right = { -right.x, -right.y, -right.z };
    WaterDrops::at = forward;
    WaterDrops::up = up;
    WaterDrops::pos = gCameraPos;
    return true;
}

// Whether there is something above the lens that keeps the rain and the snow off
// it: a roof, a bridge, the top of a passage. Asked of the physics of the world
// with the ray of the game, straight up from the camera, against what the camera
// collides with, a few times a second.
namespace Cover
{
    constexpr float Height = 40.0f;
    constexpr float Interval = 0.2f;
    // A ray that starts inside a shape, a volume of the world the camera is in,
    // hits it where it starts: only what is above the camera is a ceiling, and the
    // ray goes on from right above such a hit.
    constexpr float Nearest = 0.3f;
    constexpr int Tries = 3;

    inline bool covered = false;
    inline float sinceCheck = Interval;

    // how far up the last ray hit, for the log of DebugLog, or -1
    inline float distance = -1.0f;

    static bool Cast(float start, Mafia2::RayHit& hit)
    {
        __try
        {
            Mafia2::RayQuery query = {
                { gCameraPos.x, gCameraPos.y, gCameraPos.z + start },
                { 0.0f, 0.0f, 1.0f },
                Height - start,
                Mafia2::RayHitDistance,
                Mafia2::CameraGroups,
            };
            hit = {};
            hit.material = -1;
            return Mafia2::RayCast(nullptr, &query, &hit) != 0;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    inline bool Check()
    {
        distance = -1.0f;
        if (!Mafia2::RayCast)
            return false;

        float start = 0.0f;
        for (int i = 0; i < Tries && start < Height; i++)
        {
            Mafia2::RayHit hit;
            if (!Cast(start, hit) || !std::isfinite(hit.distance))
                return false;

            const float up = start + hit.distance;
            if (up > Nearest)
            {
                distance = up;
                return true;
            }
            start = up + Nearest;
        }
        return false;
    }

    inline bool Update(float dt)
    {
        sinceCheck += dt;
        if (sinceCheck >= Interval)
        {
            sinceCheck = 0.0f;
            covered = Check();
        }
        return covered;
    }
}

// The jets of water of the broken hydrants, out of the list of the game, see
// HydrantTick: copied while the game looks at them, on its own thread, and the
// lens is wet by them when the drops are drawn. A list that was not looked at for
// a while is of hydrants that are gone, the game stops looking when none is left.
namespace Hydrants
{
    constexpr int Max = 16;
    // The water goes up from the hydrant and comes down round it: the lens is wet
    // in a column of that height over it, as far round it as the radius, from the
    // amount (a rate, see WaterDrops::FillScreenMovingRate) in the jet to nothing
    // at the radius.
    constexpr float Height = 9.0f;
    constexpr float Radius = 8.0f;
    constexpr float Amount = 0.35f;
    constexpr ULONGLONG Stale = 500;

    inline std::mutex mutex;
    inline RwV3d jets[Max] = {};
    inline int count = 0;
    // the broken hydrants the game has, running or not, for the log of DebugLog
    inline int broken = 0;
    inline ULONGLONG seen = 0;

    static int Read(uint8_t* hydrants, RwV3d* found, int& all)
    {
        int n = 0;
        all = 0;
        __try
        {
            auto** part = *reinterpret_cast<uint8_t***>(hydrants + Mafia2::HydrantsBegin);
            auto** end = *reinterpret_cast<uint8_t***>(hydrants + Mafia2::HydrantsEnd);
            for (; part && part != end && n < Max; ++part)
            {
                auto* hydrant = *part ? *reinterpret_cast<uint8_t**>(*part + Mafia2::PartHydrant) : nullptr;
                if (!hydrant)
                    continue;
                all++;
                auto* gush = *reinterpret_cast<void**>(hydrant + Mafia2::HydrantGush);
                const bool jet = hydrant[Mafia2::HydrantRuns] && *reinterpret_cast<void**>(hydrant + Mafia2::HydrantJet);
                if (!jet && !(gush && Mafia2::Virtual<Mafia2::EffectIsPlaying, bool>(gush)))
                    continue;
                const float* m = reinterpret_cast<const float*>(hydrant + Mafia2::HydrantMatrix);
                found[n++] = { m[3], m[7], m[11] };
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
        return n;
    }

    inline void Collect(uint8_t* hydrants)
    {
        RwV3d found[Max];
        int all = 0;
        const int n = Read(hydrants, found, all);

        std::lock_guard lock(mutex);
        std::copy(found, found + n, jets);
        count = n;
        broken = all;
        seen = GetTickCount64();
    }

    // how wet the lens is of the jets round it, as a rate
    inline float Wetness(const RwV3d& at)
    {
        std::lock_guard lock(mutex);
        if (GetTickCount64() - seen > Stale)
            count = 0;

        float wet = 0.0f;
        for (int i = 0; i < count; i++)
        {
            const float up = at.z - jets[i].z;
            if (up < -1.0f || up > Height)
                continue;
            const float distance = sqrtf((at.x - jets[i].x) * (at.x - jets[i].x) + (at.y - jets[i].y) * (at.y - jets[i].y));
            if (!(distance < Radius))
                continue;
            const float closeness = 1.0f - distance / Radius;
            wet = (std::max)(wet, Amount * closeness * closeness);
        }
        return wet;
    }

    inline int Count()
    {
        std::lock_guard lock(mutex);
        return count;
    }

    inline int Broken()
    {
        std::lock_guard lock(mutex);
        return GetTickCount64() - seen > Stale ? 0 : broken;
    }
}

// Rain or snow: the drops of the one the weather has more of, and the lens keeps
// the kind it has until the other is clearly the more of the two, so it does not
// switch back and forth while one turns into the other.
static float Precipitation()
{
    const float rain = std::clamp(Weather::Read(Weather::rain) * 0.01f, 0.0f, 1.0f);
    const float snow = std::clamp(Weather::Read(Weather::snow) * 0.01f, 0.0f, 1.0f);

    constexpr float Margin = 0.05f;
    static bool snowing = false;
    if (snow > rain + Margin)
        snowing = true;
    else if (rain > snow + Margin)
        snowing = false;

    WaterDrops::SetSnow(snowing);
    return snowing ? snow : rain;
}

static bool gDrawnThisFrame = false;

// What the plugin sees, once a second, into the log of DebugLog of the settings:
// what of the game it found, whether the HUD pass is reached, the weather, the
// cover and the camera.
namespace Diagnostics
{
    inline int hudPasses = 0, draws = 0;
    inline const char* stopped = "";

    inline void Write(float rain, float snow, bool covered, bool initialised)
    {
        static LARGE_INTEGER last = {};
        LARGE_INTEGER now, frequency;
        QueryPerformanceCounter(&now);
        QueryPerformanceFrequency(&frequency);
        if (last.QuadPart && double(now.QuadPart - last.QuadPart) / double(frequency.QuadPart) < 1.0)
            return;
        last = now;

        if (!WaterDrops::bDebugLog || WaterDrops::ms_debugLogPath.empty())
            return;

        if (FILE* f = _wfopen(WaterDrops::ms_debugLogPath.c_str(), L"a"))
        {
            fprintf(f, "mafia2: found rain %d snow %d viewport %d raycast %d hud passes %d draws %d stopped '%s' rain %.3f snow %.3f covered %d roof %.2f renderer %d snowdrops %d camera %.1f %.1f %.1f drops %d hydrants %d of %d wet %.3f\n",
                Weather::rain != nullptr, Weather::snow != nullptr, Mafia2::GetMainViewport != nullptr,
                Mafia2::RayCast != nullptr, hudPasses, draws, stopped, rain, snow, covered, Cover::distance, initialised,
                WaterDrops::bEnableSnow, gCameraPos.x, gCameraPos.y, gCameraPos.z, WaterDrops::ms_numDrops,
                Hydrants::Count(), Hydrants::Broken(), Hydrants::Wetness(gCameraPos));
            fclose(f);
        }

        hudPasses = draws = 0;
        stopped = "";
    }
}

// The drops of a frame, drawn into the target the HUD is about to be drawn into,
// over the world and its post processing and under the HUD and the menus.
static void DrawDrops(uint8_t* renderer)
{
    if (gDrawnThisFrame || !renderer)
        return;

    auto* device = *reinterpret_cast<uint8_t**>(renderer + Mafia2::RendererDevice);
    if (!device)
    {
        Diagnostics::stopped = "no device";
        return;
    }

    auto* swapChain = *reinterpret_cast<uint8_t**>(device + Mafia2::DeviceSwapChain);
    auto* context = *reinterpret_cast<ID3D11DeviceContext**>(device + Mafia2::DeviceContext);
    auto* dxgi = swapChain ? *reinterpret_cast<IDXGISwapChain**>(swapChain + Mafia2::SwapChainDXGI) : nullptr;
    if (!dxgi || !context)
    {
        Diagnostics::stopped = "no swap chain";
        return;
    }

    gDrawnThisFrame = true;

    // the drops wait for the game, and are drawn again as they were when it goes on
    if (Mafia2::Pause && *Mafia2::Pause && *reinterpret_cast<int*>(*Mafia2::Pause + Mafia2::PauseCount) > 0)
    {
        Diagnostics::stopped = "paused";
        return;
    }

    if (!UpdateCamera())
    {
        Diagnostics::stopped = "no camera";
        return;
    }

    const bool initialised = Xrd::Init(Xrd::RENDERER_D3D11, dxgi);
    const float dt = WaterDrops::GetTimeStep();
    Weather::Update(dt);
    const float precipitation = Precipitation();
    const bool covered = Cover::Update(dt);
    Diagnostics::Write(Weather::rain ? Weather::Read(Weather::rain) : -1.0f,
        Weather::snow ? Weather::Read(Weather::snow) : -1.0f, covered, initialised);

    if (!initialised)
    {
        Diagnostics::stopped = "renderer";
        return;
    }

    Diagnostics::draws++;
    WaterDrops::ms_rainIntensity = covered ? 0.0f : precipitation;

    // what the HUD is drawn into is what is bound right now
    ID3D11RenderTargetView* target = nullptr;
    context->OMGetRenderTargets(1, &target, nullptr);

    Xrd::RenderTarget renderTarget{};
    renderTarget.resource = target;
    Xrd::SetCommandContext(nullptr);
    Xrd::SetTarget(target ? &renderTarget : nullptr);
    Xrd::SetTargetState(Xrd::TARGET_STATE_RENDER_TARGET);

    WaterDrops::Process();
    // the water of a broken hydrant the camera is in
    if (const float wet = Hydrants::Wetness(gCameraPos); wet > 0.0f)
        WaterDrops::FillScreenMovingRate(wet);
    WaterDrops::Render();

    Xrd::SetTarget(nullptr);
    Xrd::SetTargetState(Xrd::TARGET_STATE_PRESENT);
    WaterDrops::ms_rainIntensity = 0.0f;

    if (target)
        target->Release();
}

// What the game does with its broken hydrants: whether their water runs, and
// the effect of it started or taken away. The jets are copied right after, see
// Hydrants.
SafetyHookInline shHydrantTick = {};
static void HydrantTick(uint8_t* hydrants)
{
    shHydrantTick.unsafe_call(hydrants);
    Hydrants::Collect(hydrants);
}

// The listeners of a pass of the renderer: the one of the HUD and of the menus,
// Sys2dRender, draws in the first pass, into the scene that is finished by then.
SafetyHookInline shListeners = {};
static char Listeners(uint8_t* renderer, void* context, void* listeners, int pass)
{
    if (pass == Mafia2::HudPass)
    {
        Diagnostics::hudPasses++;
        DrawDrops(renderer);
    }
    return shListeners.unsafe_call<char>(renderer, context, listeners, pass);
}

void Init()
{
    WaterDrops::ReadSettings();

    // the manager of the environment, out of the script function SetWeatherTemplate
    auto pattern = hook::pattern("48 83 EC 28 48 8B 05 ? ? ? ? 48 83 78 20 00 74 ? 48 8D 4C 24 40 E8 ? ? ? ? 48 8B 0D ? ? ? ? 48 8D 54 24 40 48 8B 49 20");
    if (!pattern.empty())
        Mafia2::Environment = Relative<uint8_t*>(pattern.get_first<uint8_t>(7));

    pattern = hook::pattern("48 83 EC 28 E8 ? ? ? ? 48 8B C8 BA 01 00 00 00 E8 ? ? ? ? 48 85 C0 74 ? 48 8B 80 A0 01 00 00");
    if (!pattern.empty())
        Mafia2::GetMainViewport = pattern.get_first<void*()>(0);

    // the pause of the game, where it is paused
    pattern = hook::pattern("48 8B 0D ? ? ? ? B2 01 E8 ? ? ? ? C6 43 08 01");
    if (!pattern.empty())
        Mafia2::Pause = Relative<uint8_t*>(pattern.get_first<uint8_t>(3));

    // the ray of the game, see Cover
    pattern = hook::pattern("40 55 41 54 41 55 41 56 41 57 48 81 EC 80 01 00 00 48 8D 6C 24 50");
    if (!pattern.empty())
        Mafia2::RayCast = pattern.get_first<char(void*, const Mafia2::RayQuery*, Mafia2::RayHit*)>(0);

    // the broken hydrants, see HydrantTick
    pattern = hook::pattern("41 56 41 57 48 83 EC 38 48 8B 41 10 4C 8B F9 48 2B 41 08 48 C1 F8 03");
    if (!pattern.empty())
        shHydrantTick = safetyhook::create_inline(pattern.get_first(), HydrantTick);

    // the listeners of the passes of the renderer, see Listeners
    pattern = hook::pattern("40 55 56 41 56 48 83 EC 20 83 BA ? ? ? ? 00 45 8B F1");
    if (!pattern.empty())
        shListeners = safetyhook::create_inline(pattern.get_first(), Listeners);

    // the swap chain presents the frame: the next one may draw the drops again
    pattern = hook::pattern("40 53 48 83 EC 40 48 8B 81 B0 27 00 00 48 8B D9 48 8B 48 18");
    if (!pattern.empty())
    {
        static auto PresentHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext&)
        {
            gDrawnThisFrame = false;
        });
    }

    // The buffers of the swap chain are given up and made again before it is
    // resized, and what the drops hold of them goes first.
    pattern = hook::pattern("40 53 55 56 57 48 81 EC A8 00 00 00 48 8B 42 20 45 33 DB 48 8B D9 48 8B FA 8B 48 18");
    if (!pattern.empty())
    {
        static auto ResizeHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext&)
        {
            WaterDrops::Reset();
            Xrd::Shutdown();
        });
    }
}

extern "C" __declspec(dllexport) void InitializeASI()
{
    std::call_once(CallbackHandler::flag, []()
    {
        CallbackHandler::RegisterCallbackAtGetSystemTimeAsFileTime(Init, hook::pattern("40 55 56 41 56 48 83 EC 20 83 BA ? ? ? ? 00 45 8B F1"));
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
