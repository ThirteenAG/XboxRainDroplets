// Direct3D 11, the API this game uses
#define XRD_ENABLE_D3D11
#include "xrd/xrd.h"

// Mafia: Definitive Edition. Only rain falls in it. The weather of the game says
// how hard it rains, the camera the frame is rendered with says where the lens
// is, the test the game makes of whether a place is inside and a ray of its
// physics straight up say whether the lens is under a roof, and the drops are
// drawn right before the HUD. The spray of a broken fire hydrant wets the lens
// too.
//
// The engine does not draw with Direct3D where it is told to: a frame is recorded
// into a stream of commands, which another thread plays to Direct3D afterwards.
// The drops are drawn by a command of their own put into that stream, see
// Callback, so they are drawn on that thread, in their place among the others.

namespace MafiaDE
{
    // The weather manager, out of the environment: the intensity of the rain it
    // rains with, from 0 to 1.
    uint8_t** Environment = nullptr;
    constexpr ptrdiff_t EnvironmentWeather = 0x28;
    constexpr ptrdiff_t WeatherRain = 0x318;

    // The manager of the cameras: its first function hands out a camera by its
    // kind, the one the game renders with is 1. The entity of a camera keeps its
    // world matrix, three rows of four floats, its right, its forward and its up
    // in the columns and its place in the last one. The world has z up.
    uint8_t** CameraManager = nullptr;
    constexpr int GameCamera = 1;
    constexpr ptrdiff_t CameraEntity = 0x208;
    constexpr ptrdiff_t EntityWorld = 0x60;

    // The manager of the sound of the world, which knows of every place whether it
    // is inside: IsInside(manager, position, for the camera).
    uint8_t** SoundManagers = nullptr;
    constexpr ptrdiff_t SoundManager = 0x8;
    constexpr size_t SoundIsInside = 25;

    // A ray of the physics of the world (Havok), the one the game casts to find
    // the ground under a character: whether it hits anything at all. The first
    // argument is not used. The physics, which is not there in the menus and
    // while a level loads, is the scene of the world of the game there.
    struct RayQuery
    {
        void* vtable;
        float from[3];
        float to[3];
        uint32_t layers;
        uint32_t flags;
        int32_t ignore;
        uint32_t padding;
    };

    bool (*RayHitsAny)(void* unused, const RayQuery* ray) = nullptr;
    uint8_t** Physics = nullptr;
    constexpr ptrdiff_t PhysicsWorld = 0x50, WorldScene = 0x18;
    // what a character can stand on: the ground, a floor, a roof
    constexpr uint32_t GroundLayers = 0x40080202;

    // The renderer: the state of the device it records the frame with, and on
    // that the context of the device the commands go to.
    constexpr ptrdiff_t RendererState = 0x80;
    // ICommandContext::Callback: a function of the object handed over is called
    // with a copy of the eight bytes handed over, when the stream is played.
    constexpr size_t ContextCallback = 0x208 / 8;

    // An effect of particles of the world: its id in the library of the effects,
    // its state, and the entity that keeps it, whose world matrix is the one of a
    // camera (see CameraEntity), its place in the last column.
    constexpr ptrdiff_t EffectId = 0x0C;
    constexpr ptrdiff_t EffectEntity = 0xA8;
    constexpr ptrdiff_t EffectFlags = 0xBC;
    // not started, or stopped
    constexpr uint32_t EffectStopped = 0x08;

    // the stage of the extensions of the renderer the HUD and the menus are drawn
    // in, see Extensions
    constexpr int HudStage = 2;

    // A texture of the engine: its kind, 1 for a texture, and the texture of
    // Direct3D it is.
    constexpr ptrdiff_t TextureKind = 0x8;
    constexpr ptrdiff_t TextureD3D11 = 0xA8;

    // The context of the device that plays the stream to Direct3D, which presents
    // the frame: its swap chain, and on that the one of DXGI.
    constexpr ptrdiff_t ImmediateSwapChain = 0x382F0;
    constexpr ptrdiff_t SwapChainDXGI = 0x20;

    template <size_t Index, typename Ret, typename... Args>
    inline Ret Virtual(void* object, Args... args)
    {
        using Fn = Ret(*)(void*, Args...);
        return (*reinterpret_cast<Fn**>(object))[Index](object, args...);
    }
}

// The address a rip relative operand at p points to, p being where its four bytes
// are and the instruction ending right after them.
template <typename T>
static T* Relative(uint8_t* p)
{
    return reinterpret_cast<T*>(p + 4 + *reinterpret_cast<int32_t*>(p));
}

// What a frame needs of the game, read while it is recorded and handed over to
// the command that draws the drops, which is played afterwards on the other
// thread: the frames after it are being recorded by then.
struct Frame
{
    RwV3d right, at, up, pos;
    float rain;
    // the water of the effects round the camera, a rate, see Water
    float water;
    bool camera;
    uint8_t* target;
};

constexpr uint64_t Frames = 4;
static Frame gFrames[Frames] = {};
static uint64_t gFrameCount = 0;

RwV3d gCameraPos = {};

// Read guarded, what is read of the game may be on its way to be made or to go.
namespace Game
{
    static float Rain()
    {
        __try
        {
            if (!MafiaDE::Environment || !*MafiaDE::Environment)
                return 0.0f;
            auto* weather = *reinterpret_cast<uint8_t**>(*MafiaDE::Environment + MafiaDE::EnvironmentWeather);
            if (!weather)
                return 0.0f;
            const float rain = *reinterpret_cast<float*>(weather + MafiaDE::WeatherRain);
            return std::isfinite(rain) ? std::clamp(rain, 0.0f, 1.0f) : 0.0f;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return 0.0f;
        }
    }

    // the lens of the camera the frame is rendered with, out of its world matrix
    static bool Camera(Frame& frame)
    {
        __try
        {
            if (!MafiaDE::CameraManager || !*MafiaDE::CameraManager)
                return false;
            auto* camera = MafiaDE::Virtual<0, uint8_t*, int>(*MafiaDE::CameraManager, MafiaDE::GameCamera);
            if (!camera)
                return false;
            auto* entity = *reinterpret_cast<uint8_t**>(camera + MafiaDE::CameraEntity);
            if (!entity)
                return false;

            const float* m = reinterpret_cast<const float*>(entity + MafiaDE::EntityWorld);
            frame.right = { m[0], m[4], m[8] };
            frame.at = { m[1], m[5], m[9] };
            frame.up = { m[2], m[6], m[10] };
            frame.pos = { m[3], m[7], m[11] };
            return std::isfinite(frame.pos.x) && std::isfinite(frame.pos.y) && std::isfinite(frame.pos.z);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    static bool Inside(const RwV3d& pos)
    {
        __try
        {
            if (!MafiaDE::SoundManagers || !*MafiaDE::SoundManagers)
                return false;
            auto* manager = *reinterpret_cast<uint8_t**>(*MafiaDE::SoundManagers + MafiaDE::SoundManager);
            if (!manager)
                return false;
            const float position[3] = { pos.x, pos.y, pos.z };
            return MafiaDE::Virtual<MafiaDE::SoundIsInside, bool, const float*, bool>(manager, position, true);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }
}

// Whether the lens is where the rain does not reach it: inside, where the game
// has rooms (a building, a passage), or under anything else, a ray straight up
// of the physics hitting something (a shed, a porch, a bridge). Asked of the game
// a few times a second, on its own thread, see Tick: the physics is not to be
// asked while it moves the world, which it may on the thread of the renderer.
namespace Cover
{
    constexpr ULONGLONG Interval = 200;
    constexpr float Height = 40.0f;
    // the ray starts this much over the camera, out of what the camera is in
    constexpr float Start = 0.05f;

    inline std::atomic<bool> covered = false;
    // for the log of DebugLog
    inline std::atomic<bool> inside = false, roof = false;
    inline ULONGLONG last = 0;

    static bool Roof(const RwV3d& pos)
    {
        __try
        {
            auto* game = MafiaDE::Physics ? *MafiaDE::Physics : nullptr;
            auto* world = game ? *reinterpret_cast<uint8_t**>(game + MafiaDE::PhysicsWorld) : nullptr;
            auto** scene = world ? *reinterpret_cast<uint8_t***>(world + MafiaDE::WorldScene) : nullptr;
            if (!MafiaDE::RayHitsAny || !scene || !*scene)
                return false;

            const MafiaDE::RayQuery ray = {
                nullptr,
                { pos.x, pos.y, pos.z + Start },
                { pos.x, pos.y, pos.z + Height },
                MafiaDE::GroundLayers,
                0,
                -1,
            };
            return MafiaDE::RayHitsAny(nullptr, &ray);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    // once a frame, on the thread of the game
    inline void Tick()
    {
        const ULONGLONG now = GetTickCount64();
        if (now - last < Interval)
            return;
        last = now;

        Frame frame;
        if (!Game::Camera(frame))
        {
            covered = false;
            return;
        }

        inside = Game::Inside(frame.pos);
        roof = Roof(frame.pos);
        covered = inside || roof;
    }
}

// The water of the effects of the world on the lens: the spray of a broken fire
// hydrant. A hydrant that breaks starts its spray (401, which runs for half a
// minute) with a second effect along with it (403), and the burst of water it
// breaks with (400). Every effect the game updates in a frame is looked at, see
// Updated, on the thread of the game: the ones of water are kept with where they
// are for a little while, the game does not update an effect behind the camera
// every frame, and the lens is wet by them in a column over them, from the amount
// (a rate, see WaterDrops::FillScreenMovingRate) at one to nothing at the radius.
namespace Water
{
    struct Source
    {
        uint32_t id;
        float radius;
        float height;
        float amount;
    };

    constexpr Source Sources[] = {
        { 401, 8.0f, 9.0f, 0.35f },
        { 403, 8.0f, 9.0f, 0.35f },
        { 400, 5.0f, 4.0f, 0.15f },
    };

    constexpr int Max = 32;
    constexpr ULONGLONG Hold = 500;

    struct Seen
    {
        const Source* source;
        RwV3d pos;
        ULONGLONG at;
    };

    inline std::mutex mutex;
    inline Seen seen[Max] = {};
    inline int count = 0;

    // Where the camera was in the frame recorded last, and the effects of any
    // kind updated near it, for the log of DebugLog: the water a game has that is
    // not of the list above is found out by its id there.
    inline RwV3d camera = {};
    constexpr float NearbyReach = 15.0f;
    constexpr int NearbyMax = 8;
    struct Nearby
    {
        uint32_t id, flags;
        float distance;
    };
    inline Nearby nearby[NearbyMax] = {};
    inline int nearbyCount = 0;

    inline const Source* Find(uint32_t id)
    {
        for (const auto& source : Sources)
            if (source.id == id)
                return &source;
        return nullptr;
    }

    static bool Read(uint8_t* effect, uint32_t& id, uint32_t& flags, RwV3d& pos)
    {
        __try
        {
            id = *reinterpret_cast<uint32_t*>(effect + MafiaDE::EffectId);
            flags = *reinterpret_cast<uint32_t*>(effect + MafiaDE::EffectFlags);
            auto* entity = *reinterpret_cast<uint8_t**>(effect + MafiaDE::EffectEntity);
            if (!entity)
                return false;
            const float* m = reinterpret_cast<const float*>(entity + MafiaDE::EntityWorld);
            pos = { m[3], m[7], m[11] };
            return std::isfinite(pos.x) && std::isfinite(pos.y) && std::isfinite(pos.z);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    // an effect the game updated this frame
    inline void Updated(uint8_t* effect)
    {
        uint32_t id = 0, flags = 0;
        RwV3d pos;
        if (!effect || !Read(effect, id, flags, pos) || (flags & MafiaDE::EffectStopped))
            return;

        const Source* source = Find(id);
        const float dx = pos.x - camera.x, dy = pos.y - camera.y, dz = pos.z - camera.z;
        const float distance = sqrtf(dx * dx + dy * dy + dz * dz);
        if (!source && !(WaterDrops::bDebugLog && distance < NearbyReach))
            return;

        std::lock_guard lock(mutex);

        if (WaterDrops::bDebugLog && distance < NearbyReach)
        {
            int i = 0;
            while (i < nearbyCount && nearby[i].id != id)
                i++;
            if (i == nearbyCount && nearbyCount < NearbyMax)
                nearby[nearbyCount++] = { id, flags, distance };
            else if (i < nearbyCount && distance < nearby[i].distance)
                nearby[i] = { id, flags, distance };
        }

        if (!source)
            return;

        // the same effect seen again, or a new one
        const ULONGLONG now = GetTickCount64();
        int slot = -1;
        for (int i = 0; i < count; i++)
        {
            if (seen[i].source == source && fabsf(seen[i].pos.x - pos.x) < 0.5f && fabsf(seen[i].pos.y - pos.y) < 0.5f &&
                fabsf(seen[i].pos.z - pos.z) < 0.5f)
            {
                slot = i;
                break;
            }
            if (slot < 0 && now - seen[i].at > Hold)
                slot = i;
        }
        if (slot < 0 && count < Max)
            slot = count++;
        if (slot >= 0)
            seen[slot] = { source, pos, now };
    }

    // how wet the lens is at a place of the effects of water round it, as a rate
    inline float Wetness(const RwV3d& at)
    {
        std::lock_guard lock(mutex);
        camera = at;
        const ULONGLONG now = GetTickCount64();

        float wet = 0.0f;
        for (int i = 0; i < count; i++)
        {
            if (now - seen[i].at > Hold)
                continue;
            const Source& source = *seen[i].source;
            const float up = at.z - seen[i].pos.z;
            if (up < -1.0f || up > source.height)
                continue;
            const float dx = at.x - seen[i].pos.x, dy = at.y - seen[i].pos.y;
            const float distance = sqrtf(dx * dx + dy * dy);
            if (!(distance < source.radius))
                continue;
            const float closeness = 1.0f - distance / source.radius;
            wet = (std::max)(wet, source.amount * closeness * closeness);
        }
        return wet;
    }

    // the effects near the camera since the last time, into the log, see Diagnostics
    inline void WriteNearby(FILE* f)
    {
        std::lock_guard lock(mutex);
        fprintf(f, "mafiade: effects near the camera:");
        for (int i = 0; i < nearbyCount; i++)
            fprintf(f, " %u (%.1f m, flags %x)", nearby[i].id, nearby[i].distance, nearby[i].flags);
        fprintf(f, "\n");
        nearbyCount = 0;
    }
}

// What the plugin sees, once a second, into the log of DebugLog of the settings:
// what of the game it found, whether the HUD stage is reached and the drops are
// drawn, the weather, the cover and the camera.
namespace Diagnostics
{
    inline int stages = 0, callbacks = 0, draws = 0;
    inline const char* stopped = "";

    inline void Write(const Frame& frame, bool covered)
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
            fprintf(f, "mafiade: found weather %d cameras %d sound %d stages %d callbacks %d draws %d stopped '%s' rain %.3f covered %d camera %d %.1f %.1f %.1f target %p drops %d water %.3f inside %d roof %d\n",
                MafiaDE::Environment != nullptr, MafiaDE::CameraManager != nullptr, MafiaDE::SoundManagers != nullptr,
                stages, callbacks, draws, stopped, frame.rain, covered, frame.camera,
                frame.pos.x, frame.pos.y, frame.pos.z, frame.target, WaterDrops::ms_numDrops, frame.water,
                (bool)Cover::inside, (bool)Cover::roof);
            Water::WriteNearby(f);
            fclose(f);
        }

        stages = callbacks = draws = 0;
        stopped = "";
    }
}

// The swap chain of the frame, which the context that plays the stream presents
// with, and whether the drops of that frame are drawn yet: the HUD stage may come
// more than once in a frame.
static IDXGISwapChain* gSwapChain = nullptr;
static bool gDrawnThisFrame = false;

// The view the drops draw into, made for the texture the HUD is drawn into, and
// kept: it holds the texture, so it goes before the swap chain is resized.
static std::mutex gTargetMutex;
static ID3D11Texture2D* gTargetTexture = nullptr;
static ID3D11RenderTargetView* gTargetView = nullptr;

static void ReleaseTarget()
{
    if (gTargetView)
        gTargetView->Release();
    if (gTargetTexture)
        gTargetTexture->Release();
    gTargetView = nullptr;
    gTargetTexture = nullptr;
}

// A texture of the engine is often made of a format of no type, which its views
// give one to; the drops draw into it the way the engine does.
static DXGI_FORMAT ViewFormat(DXGI_FORMAT format)
{
    switch (format)
    {
    case DXGI_FORMAT_R8G8B8A8_TYPELESS: return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_TYPELESS: return DXGI_FORMAT_B8G8R8A8_UNORM;
    case DXGI_FORMAT_R10G10B10A2_TYPELESS: return DXGI_FORMAT_R10G10B10A2_UNORM;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS: return DXGI_FORMAT_R16G16B16A16_FLOAT;
    default: return format;
    }
}

static ID3D11Texture2D* TextureOf(uint8_t* target)
{
    __try
    {
        if (*reinterpret_cast<int*>(target + MafiaDE::TextureKind) != 1)
            return nullptr;
        return *reinterpret_cast<ID3D11Texture2D**>(target + MafiaDE::TextureD3D11);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
}

static ID3D11RenderTargetView* ViewOf(uint8_t* target)
{
    auto* texture = target ? TextureOf(target) : nullptr;
    if (!texture)
        return nullptr;
    if (texture == gTargetTexture)
        return gTargetView;

    ReleaseTarget();

    ID3D11Device* device = nullptr;
    texture->GetDevice(&device);
    if (!device)
        return nullptr;

    D3D11_TEXTURE2D_DESC desc;
    texture->GetDesc(&desc);
    D3D11_RENDER_TARGET_VIEW_DESC view = {};
    view.Format = ViewFormat(desc.Format);
    view.ViewDimension = desc.SampleDesc.Count > 1 ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D;
    if (SUCCEEDED(device->CreateRenderTargetView(texture, &view, &gTargetView)))
    {
        gTargetTexture = texture;
        gTargetTexture->AddRef();
    }
    device->Release();
    return gTargetView;
}

// The drops of a frame, drawn into the texture the HUD is about to be drawn into,
// over the world and its post processing and under the HUD and the menus.
static void DrawDrops(const Frame& frame)
{
    Diagnostics::callbacks++;
    if (gDrawnThisFrame)
        return;
    gDrawnThisFrame = true;

    std::lock_guard lock(gTargetMutex);

    if (!gSwapChain || !frame.camera)
    {
        Diagnostics::stopped = gSwapChain ? "no camera" : "no swap chain";
        return;
    }

    if (!Xrd::Init(Xrd::RENDERER_D3D11, gSwapChain))
    {
        Diagnostics::stopped = "renderer";
        return;
    }

    auto* target = ViewOf(frame.target);
    if (!target)
    {
        Diagnostics::stopped = "no target";
        return;
    }

    // The effect takes the right of a camera the way RenderWare has it, pointing
    // to the left of the screen.
    WaterDrops::right = { -frame.right.x, -frame.right.y, -frame.right.z };
    WaterDrops::at = frame.at;
    WaterDrops::up = frame.up;
    WaterDrops::pos = frame.pos;
    gCameraPos = frame.pos;
    WaterDrops::ms_rainIntensity = frame.rain;

    Xrd::RenderTarget renderTarget{};
    renderTarget.resource = target;
    Xrd::SetCommandContext(nullptr);
    Xrd::SetTarget(&renderTarget);
    Xrd::SetTargetState(Xrd::TARGET_STATE_RENDER_TARGET);

    WaterDrops::Process();
    // the water of the effects round the camera, see Water
    if (frame.water > 0.0f)
        WaterDrops::FillScreenMovingRate(frame.water);
    WaterDrops::Render();

    Xrd::SetTarget(nullptr);
    Xrd::SetTargetState(Xrd::TARGET_STATE_PRESENT);
    WaterDrops::ms_rainIntensity = 0.0f;
    Diagnostics::draws++;
}

// The command of the drops in the stream, called with the number of its frame.
struct Callback
{
    virtual void Unused() {}
    virtual void Execute(const uint64_t* frame)
    {
        DrawDrops(gFrames[*frame % Frames]);
    }
};
static Callback gCallback;

// The extensions of the renderer at a stage of a view: the one of the HUD and of
// the menus, the renderer of Scaleform, draws in the second, into the scene that
// is finished by then, and the drops are put in the stream right before it.
SafetyHookInline shExtensions = {};
static uintptr_t Extensions(uint8_t* renderer, void* view, void* extensions, int stage, uint8_t* target, void* parameters)
{
    if (stage == MafiaDE::HudStage && renderer && target)
    {
        Diagnostics::stages++;

        const uint64_t number = gFrameCount++;
        Frame& frame = gFrames[number % Frames];
        frame.target = target;
        frame.camera = Game::Camera(frame);
        const bool covered = frame.camera && Cover::covered;
        frame.rain = covered ? 0.0f : Game::Rain();
        frame.water = frame.camera ? Water::Wetness(frame.pos) : 0.0f;
        Diagnostics::Write(frame, covered);

        auto* state = *reinterpret_cast<uint8_t**>(renderer + MafiaDE::RendererState);
        auto* context = state ? *reinterpret_cast<void**>(state) : nullptr;
        if (context)
            MafiaDE::Virtual<MafiaDE::ContextCallback, void, Callback*, const uint64_t*>(context, &gCallback, &number);
    }
    return shExtensions.unsafe_call<uintptr_t>(renderer, view, extensions, stage, target, parameters);
}

void Init()
{
    WaterDrops::ReadSettings();

    // the environment, out of the function that hands it out
    auto pattern = hook::pattern("48 8B 05 ? ? ? ? 48 8B 40 28 C3");
    if (!pattern.empty())
        MafiaDE::Environment = Relative<uint8_t*>(pattern.get_first<uint8_t>(3));

    // the manager of the cameras, where it is made
    pattern = hook::pattern("48 83 EC 28 B9 58 09 00 00 E8 ? ? ? ? 48 85 C0 74 08 48 8B C8 E8 ? ? ? ? 48 8B C8 48 89 05 ? ? ? ? E8");
    if (!pattern.empty())
        MafiaDE::CameraManager = Relative<uint8_t*>(pattern.get_first<uint8_t>(33));

    // the managers of the sound, out of the function that hands out the one of
    // the world
    pattern = hook::pattern("48 8B 05 ? ? ? ? 48 8B 40 08 C3 CC CC CC CC 48 8B 05 ? ? ? ? 48 8B 40 10 C3");
    if (!pattern.empty())
        MafiaDE::SoundManagers = Relative<uint8_t*>(pattern.get_first<uint8_t>(3));

    // The effects of the world the game updates in a frame, see Water: the ones
    // in sight with the first pass of the first of these (the others are passes
    // again over the same), and the rest with the second.
    pattern = hook::pattern("48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 20 41 56 48 83 EC 40 0F 29 74 24 30 0F 28 F3 66 C7 02 00 01");
    if (!pattern.empty())
    {
        static auto EffectHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& ctx)
        {
            if ((uint8_t)ctx.r8 == 0)
                Water::Updated(reinterpret_cast<uint8_t*>(ctx.rcx));
        });
    }

    pattern = hook::pattern("48 89 5C 24 08 44 88 44 24 18 55 56 57 41 54 41 55 41 56 41 57 48 83 EC 60 0F 29 74 24 50 45 8B E9 0F 29 7C 24 40 41 0F B6 D0 0F 28 F9 48 8B F1");
    if (!pattern.empty())
    {
        static auto EffectHook2 = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& ctx)
        {
            Water::Updated(reinterpret_cast<uint8_t*>(ctx.rcx));
        });
    }

    // the ray of the physics, and the game it asks the physics of out of it
    pattern = hook::pattern("40 53 48 83 EC 50 48 8D 4C 24 20 48 8B DA E8 ? ? ? ? 8B 43 10 F2 0F 10 43 08 8B 4B 24");
    if (!pattern.empty())
    {
        auto* ray = pattern.get_first<uint8_t>(0);
        if (ray[0x66] == 0x48 && ray[0x67] == 0x8B && ray[0x68] == 0x05)
        {
            MafiaDE::RayHitsAny = reinterpret_cast<bool (*)(void*, const MafiaDE::RayQuery*)>(ray);
            MafiaDE::Physics = Relative<uint8_t*>(ray + 0x69);
        }
    }

    // The effects of the world wait for the jobs of their frame, once a frame on
    // the thread of the game: where the cover of the lens is asked, see Cover.
    pattern = hook::pattern("44 88 44 24 18 48 89 4C 24 08 53 55 56 57 41 54 41 55 41 56 41 57 48 81 EC 88 00 00 00 0F 29 74 24 70 4C 8B E9 0F 29 7C 24 60 0F 28 F9 E8");
    if (!pattern.empty())
    {
        static auto CoverHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext&)
        {
            Cover::Tick();
        });
    }

    // the extensions of the renderer, see Extensions
    pattern = hook::pattern("48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 60 80 B9 27 68 04 00 00");
    if (!pattern.empty())
        shExtensions = safetyhook::create_inline(pattern.get_first(), Extensions);

    // The context that plays the stream presents the frame: its swap chain is the
    // one the drops are drawn with, and the next frame may draw them again.
    pattern = hook::pattern("40 53 57 41 57 48 83 EC 40 48 8B 81 F0 82 03 00");
    if (!pattern.empty())
    {
        static auto PresentHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& ctx)
        {
            gDrawnThisFrame = false;
            auto* swapChain = *reinterpret_cast<uint8_t**>(ctx.rcx + MafiaDE::ImmediateSwapChain);
            if (swapChain)
                gSwapChain = *reinterpret_cast<IDXGISwapChain**>(swapChain + MafiaDE::SwapChainDXGI);
        });
    }

    // The views of the buffers of the swap chain are given up before it is
    // resized, and what the drops hold of them goes first.
    pattern = hook::pattern("48 89 5C 24 10 57 48 83 EC 20 80 7A 11 00");
    if (!pattern.empty())
    {
        static auto ResizeHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext&)
        {
            // the swap chain may be made again, and is known again when it presents
            std::lock_guard lock(gTargetMutex);
            ReleaseTarget();
            Xrd::Shutdown();
            gSwapChain = nullptr;
        });
    }
}

extern "C" __declspec(dllexport) void InitializeASI()
{
    std::call_once(CallbackHandler::flag, []()
    {
        RegisterOldPluginCheck();
        CallbackHandler::RegisterCallbackAtGetSystemTimeAsFileTime(Init, hook::pattern("48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 60 80 B9 27 68 04 00 00"));
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
