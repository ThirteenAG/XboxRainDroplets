#define XRD_ENABLE_D3D11
#define XRD_ENABLE_D3D12
#include "xrd/xrd.h"
#include <map>
#include <mutex>
#include <vector>
#include <injector/injector.hpp>
#include <safetyhook.hpp>
#include <utility/Scan.hpp>

#define FUSIONDXHOOK_INCLUDE_D3D11    1
#define FUSIONDXHOOK_INCLUDE_D3D12    1
#define FUSIONDXHOOK_USE_SAFETYHOOK   0
#include "FusionDxHook.h"

enum tParticleType
{
    PARTICLE_SPARK = 0,
    PARTICLE_SPARK_SMALL,
    PARTICLE_WATER_SPARK,
    PARTICLE_WHEEL_DIRT,
    PARTICLE_SAND,
    PARTICLE_WHEEL_WATER,
    PARTICLE_BLOOD,
    PARTICLE_BLOOD_SMALL,
    PARTICLE_BLOOD_SPURT,
    PARTICLE_DEBRIS,
    PARTICLE_DEBRIS2,
    PARTICLE_FLYERS,
    PARTICLE_WATER,
    PARTICLE_FLAME,
    PARTICLE_FIREBALL,
    PARTICLE_GUNFLASH,
    PARTICLE_GUNFLASH_NOANIM,
    PARTICLE_GUNSMOKE,
    PARTICLE_GUNSMOKE2,
    PARTICLE_CIGARETTE_SMOKE,
    PARTICLE_SMOKE,
    PARTICLE_SMOKE_SLOWMOTION,
    PARTICLE_DRY_ICE,
    PARTICLE_TEARGAS,
    PARTICLE_GARAGEPAINT_SPRAY,
    PARTICLE_SHARD,
    PARTICLE_SPLASH,
    PARTICLE_CARFLAME,
    PARTICLE_STEAM,
    PARTICLE_STEAM2,
    PARTICLE_STEAM_NY,
    PARTICLE_STEAM_NY_SLOWMOTION,
    PARTICLE_GROUND_STEAM,
    PARTICLE_ENGINE_STEAM,
    PARTICLE_RAINDROP,
    PARTICLE_RAINDROP_SMALL,
    PARTICLE_RAIN_SPLASH,
    PARTICLE_RAIN_SPLASH_BIGGROW,
    PARTICLE_RAIN_SPLASHUP,
    PARTICLE_WATERSPRAY,
    PARTICLE_EXPLOSION_MEDIUM,
    PARTICLE_EXPLOSION_LARGE,
    PARTICLE_EXPLOSION_MFAST,
    PARTICLE_EXPLOSION_LFAST,
    PARTICLE_CAR_SPLASH,
    PARTICLE_BOAT_SPLASH,
    PARTICLE_BOAT_THRUSTJET,
    PARTICLE_WATER_HYDRANT,
    PARTICLE_WATER_CANNON,
    PARTICLE_EXTINGUISH_STEAM,
    PARTICLE_PED_SPLASH,
    PARTICLE_PEDFOOT_DUST,
    PARTICLE_CAR_DUST,
    PARTICLE_HELI_DUST,
    PARTICLE_HELI_ATTACK,
    PARTICLE_ENGINE_SMOKE,
    PARTICLE_ENGINE_SMOKE2,
    PARTICLE_CARFLAME_SMOKE,
    PARTICLE_FIREBALL_SMOKE,
    PARTICLE_PAINT_SMOKE,
    PARTICLE_TREE_LEAVES,
    PARTICLE_CARCOLLISION_DUST,
    PARTICLE_CAR_DEBRIS,
    PARTICLE_BIRD_DEBRIS,
    PARTICLE_HELI_DEBRIS,
    PARTICLE_EXHAUST_FUMES,
    PARTICLE_RUBBER_SMOKE,
    PARTICLE_BURNINGRUBBER_SMOKE,
    PARTICLE_BULLETHIT_SMOKE,
    PARTICLE_GUNSHELL_FIRST,
    PARTICLE_GUNSHELL,
    PARTICLE_GUNSHELL_BUMP1,
    PARTICLE_GUNSHELL_BUMP2,
    PARTICLE_ROCKET_SMOKE,
    PARTICLE_THROWN_FLAME,
    PARTICLE_SWIM_SPLASH,
    PARTICLE_SWIM_WAKE,
    PARTICLE_SWIM_WAKE2,
    PARTICLE_HELI_WATER_DROP,
    PARTICLE_BALLOON_EXP,
    PARTICLE_AUDIENCE_FLASH,
    PARTICLE_TEST,
    PARTICLE_BIRD_FRONT,
    PARTICLE_SHIP_SIDE,
    PARTICLE_BEASTIE,
    PARTICLE_RAINDROP_2D,
    PARTICLE_FERRY_CHIM_SMOKE,
    PARTICLE_MULTIPLAYER_HIT,
    PARTICLE_HYDRANT_STEAM,
    PARTICLE_FLOOR_HIT,
    PARTICLE_BLOODDROP,
    PARTICLE_HEATHAZE,
    PARTICLE_HEATHAZE_IN_DIST,
    PARTICLE_WATERDROP,

    MAX_PARTICLES,
    PARTICLE_FIRST = PARTICLE_SPARK,
    PARTICLE_LAST = PARTICLE_HYDRANT_STEAM
};

namespace CWeather
{
    GameRef<float> Rain;
}

namespace CCutsceneMgr
{
    GameRef<bool> ms_running;
}

namespace CGame
{
    GameRef<int> currArea;
}

namespace CTimer
{
    GameRef<bool> m_CodePause;
    GameRef<bool> m_UserPause;
}

namespace CCullZones
{
    GameRef<uint32_t> CurrentFlags_Camera;
    bool CamNoRain()
    {
        return (CurrentFlags_Camera & 8) != 0;
    }

    GameRef<uint32_t> CurrentFlags_Player;
    bool PlayerNoRain()
    {
        return (CurrentFlags_Player & 8) != 0;
    }
}

struct CCamera
{
    char unk[0x860];
    RwMatrix m_mCameraMatrix;
};

GameRef<CCamera> TheCamera;
GameRef<int> gGameState;

bool NoDrops()
{
    return false;
}

bool NoRain()
{
    return CCullZones::CamNoRain() || CCullZones::PlayerNoRain() || CGame::currArea != 0 || NoDrops();
}

void SprayDroplets(RwV3d* position, float pd = 20.0f, bool isBlood = false)
{
    RwV3d dist;
    RwV3dSub(&dist, position, &WaterDrops::ms_lastPos);
    float len = RwV3dLength(&dist);
    if (len <= pd)
        WaterDrops::FillScreenMoving((1.0f / (len / 2.0f)) * 100.0f, isBlood);
}

thread_local std::string cachedPrt;
thread_local std::map<void*, std::string> prtMap;

namespace FxManager_c
{
    SafetyHookInline shCreateFxSystem{};
    void CreateFxSystem(void* a1, void* name, int a3, int id)
    {
        shCreateFxSystem.fastcall(a1, name, a3, id);
        cachedPrt = *(const char**)name; // FString
    }
}

bool bGotoAddParticle = false;
namespace UParticleSystemComponent
{
    SafetyHookInline shResetParticles{};
    void* ResetParticles(void* UParticleSystemComponent, void* a2, void* a3, void* a4)
    {
        auto res = shResetParticles.fastcall<void*>(UParticleSystemComponent, a2, a3, a4);
        prtMap[res] = std::move(cachedPrt);

        bGotoAddParticle = false;
        RwV3d* position = (RwV3d*)a3;
        if (position->x == 0.0f && position->y == 0.0f && position->z == 0.0f)
        {
            bGotoAddParticle = true;
        }
        else
        {
            std::string_view name = prtMap[res];
            if (name == "water_swim")
            {
                SprayDroplets((RwV3d*)a3, 10.0f, false);
            }
            if (name == "water_splsh_sml")
            {
                SprayDroplets((RwV3d*)a3, 2.0f, false);
            }
            else if (name == "water_splash")
            {
                SprayDroplets((RwV3d*)a3, 5.0f, false);
            }
            else if (name == "water_splash_big")
            {
                SprayDroplets((RwV3d*)a3, 20.0f, false);
            }
            else if (name == "water_hydrant")
            {
                WaterDrops::RegisterSplash((RwV3d*)a3, 10.0f, 600, 100.0f);
            }
        }

        return res;
    }
}

namespace FxSystem_c
{
    SafetyHookInline shAddParticle{};
    void AddParticle(void* a1, RwV3d* position, void* a3, float a4, int* a5)
    {
        //if (bGotoAddParticle)
        {
            std::string_view name = prtMap[a1];

            if (name == "prt_blood" || name == "prt_blood_Fountain")
            {
                SprayDroplets(position, 3.0f, true);
            }
            else if (name == "boat_prop")
            {
                SprayDroplets(position, 20.0f, false);
            }
            else if (name == "prt_watersplash")
            {
                SprayDroplets(position, 20.0f, false);
            }
            else if (name == "prt_watercannon_splash")
            {
                SprayDroplets(position, 7.0f, false);
            }
        }

        return shAddParticle.fastcall(a1, position, a3, a4, a5);
    }
}

// ---------------------------------------------------------------------------
// The drops, drawn in the middle of the frame of the game, under its UI.
// ---------------------------------------------------------------------------
// The drops are drawn once in every frame, and what was drawn since the present
// before is remembered, so that the draw at the present call knows whether there is
// anything left for it to do.
static bool gbDropsDrawnThisFrame = false;

// The drops, once per frame: what the game says about them, and the draw itself.
void DrawDroplets()
{
    WaterDrops::right = TheCamera.get().m_mCameraMatrix.right;
    WaterDrops::up = TheCamera.get().m_mCameraMatrix.up;
    WaterDrops::at = TheCamera.get().m_mCameraMatrix.at;
    WaterDrops::pos = TheCamera.get().m_mCameraMatrix.pos;

    //when you put camera underwater, droplets disappear instantly instead of fading out
    if (NoDrops())
    {
        WaterDrops::Clear();
        return;
    }

    if (NoRain())
        WaterDrops::ms_rainIntensity = 0.0f;
    else
        WaterDrops::ms_rainIntensity = CWeather::Rain;

    if (!CTimer::m_CodePause && !CTimer::m_UserPause)
    {
        WaterDrops::Process();

        if (WaterDrops::ms_numDrops > 0 && !CCutsceneMgr::ms_running)
        {
            WaterDrops::Render();
        }
    }
}

// Making an inline hook is what FusionDxHook asks of the host, since what makes one is
// a library of its own, and the library this plugin hooks the game with is what makes
// them as well, see FusionDxHook::Frame. The hooks live for as long as the game runs,
// so they are kept here.
static std::vector<SafetyHookInline> gFrameHooks;

static void* MakeFrameHook(void* target, void* destination)
{
    gFrameHooks.push_back(safetyhook::create_inline(target, destination));

    SafetyHookInline& hook = gFrameHooks.back();

    return hook.target() ? (void*)hook.trampoline().address() : nullptr;
}

// The frame of this game reaches the buffer that is presented in one draw that covers
// it, and the UI of the game is drawn over it afterwards, out of that same buffer and
// with a shader of its own: the first draw into that buffer is the frame, and the ones
// that follow it with another shader are the UI. That is a thing of the renderer of
// the game and not of the API, which is why it is here and not in the hook library,
// see FusionDxHook::Frame.
static unsigned gNumBackBufferDraws = 0;
static const void* gpSceneDrawShader = nullptr;

// The drops of a frame of Direct3D 11, drawn in the middle of the frame of the game,
// in front of a draw of the UI and after the draw of the frame itself. They are drawn
// with the context of the device and into the very target the UI is drawn into, which
// is the buffer that is presented, and the effect reads the frame back out of it,
// which is the scene they are drawn over, see the note about the target in
// xrdrender.d3d11.h.
static void DrawDropletsUnderUiD3D11(ID3D11DeviceContext* /*pContext*/, ID3D11RenderTargetView* pTarget, const void* pPixelShader)
{
    ++gNumBackBufferDraws;

    if (gNumBackBufferDraws == 1)
    {
        gpSceneDrawShader = pPixelShader;
        return;
    }

    if (gpSceneDrawShader && pPixelShader == gpSceneDrawShader)
        return;

    if (gbDropsDrawnThisFrame || gGameState < 9 || !Xrd::IsActive() || Xrd::GetRenderer() != Xrd::RENDERER_D3D11 || !pTarget)
        return;

    gbDropsDrawnThisFrame = true;

    Xrd::RenderTarget target{};
    target.resource = pTarget;

    Xrd::SetCommandContext(nullptr);
    Xrd::SetTarget(&target);
    Xrd::SetTargetState(Xrd::TARGET_STATE_RENDER_TARGET);
    DrawDroplets();
    Xrd::SetTarget(nullptr);
    Xrd::SetTargetState(Xrd::TARGET_STATE_PRESENT);
}

// The same for Direct3D 12, except that the draws come from one of the lists of the
// frame and not from a context: the frame is put into the buffer that is presented by a
// draw of a list, the UI over it is drawn out of that same list afterwards with another
// shader, and a list is submitted on its own, so the submission the frame itself went
// in is remembered by its number.
static constexpr unsigned NoDx12Submit = 0xFFFFFFFFu;

struct Dx12Detector
{
    unsigned presentedDraws = 0;
    bool bHasComposite = false;
    const void* pCompositePipelineState = nullptr;
    unsigned compositeSubmitIndex = NoDx12Submit;
};

static std::mutex gDetectorLock;
static std::map<ID3D12GraphicsCommandList*, Dx12Detector> gDetectors;
static unsigned gDx12SubmitIndex = 0;
static bool gbDx12CompositeSeen = false;
static bool gbDx12CompositeSubmitted = false;

// The drops of a frame of Direct3D 12, recorded into the list the UI of the frame is
// drawn out of, in the middle of it, see FusionDxHook::Frame. What is recorded there
// changes the state of the list, which is what the library puts back afterwards, so it
// is told that something was recorded.
static void DrawDropletsInListD3D12(ID3D12GraphicsCommandList* pList, const void* pPipelineState, bool& bRecorded)
{
    {
        std::lock_guard<std::mutex> guard(gDetectorLock);

        Dx12Detector& detector = gDetectors[pList];

        ++detector.presentedDraws;

        if (!gbDx12CompositeSeen)
        {
            gbDx12CompositeSeen = true;
            detector.bHasComposite = true;
            detector.pCompositePipelineState = pPipelineState;
            detector.compositeSubmitIndex = gDx12SubmitIndex;
            return;
        }

        if (!detector.bHasComposite || !detector.pCompositePipelineState || pPipelineState == detector.pCompositePipelineState)
            return;

        if (gbDropsDrawnThisFrame || gGameState < 9 || !Xrd::IsActive() || Xrd::GetRenderer() != Xrd::RENDERER_D3D12)
            return;

        gbDropsDrawnThisFrame = true;
        bRecorded = true;
    }

    Xrd::SetCommandContext(pList);
    Xrd::SetTarget(nullptr);
    Xrd::SetTargetState(Xrd::TARGET_STATE_RENDER_TARGET);
    DrawDroplets();
    Xrd::SetTargetState(Xrd::TARGET_STATE_PRESENT);
    Xrd::SetCommandContext(nullptr);
}

// What is left for the frames whose UI is not in the list the frame itself was put
// into, and for the ones that never reach such a draw at all, which the menus are: the
// submission that comes after the one the frame itself went in, and that carries draws
// into the buffer that is presented, is the last place they can be drawn at.
static void DrawDropletsBeforeUiSubmissionD3D12(ID3D12CommandQueue* /*pQueue*/, unsigned NumCommandLists,
    ID3D12CommandList* const* ppCommandLists)
{
    bool bUiSubmission = false;

    {
        std::lock_guard<std::mutex> guard(gDetectorLock);

        unsigned composite = 0;
        unsigned presented = 0;

        for (unsigned i = 0; i < NumCommandLists; i++)
        {
            const auto it = gDetectors.find((ID3D12GraphicsCommandList*)ppCommandLists[i]);

            if (it == gDetectors.end())
                continue;

            presented += it->second.presentedDraws;

            if (it->second.compositeSubmitIndex == gDx12SubmitIndex)
                ++composite;
        }

        if (composite)
            gbDx12CompositeSubmitted = true;

        bUiSubmission = gbDx12CompositeSubmitted && !composite && presented > 0;

        ++gDx12SubmitIndex;
    }

    if (!bUiSubmission || gbDropsDrawnThisFrame || gGameState < 9 || !Xrd::IsActive() || Xrd::GetRenderer() != Xrd::RENDERER_D3D12)
        return;

    gbDropsDrawnThisFrame = true;

    Xrd::SetTarget(nullptr);
    Xrd::SetTargetState(Xrd::TARGET_STATE_RENDER_TARGET);
    DrawDroplets();
    Xrd::SetTargetState(Xrd::TARGET_STATE_PRESENT);
}

// The present call is the only place that has the swap chain, which is what the
// renderer is built on, so what is left of the effect lives here. What is drawn is
// drawn in the middle of the frame for the APIs that can be told where the frame of
// the game is, see FusionDxHook::Frame: the present call only draws it for the APIs
// that cannot, and for the frames that never reach such a draw.
static void PresentHandler(IDXGISwapChain* pSwapChain)
{
    if (gGameState < 9 || !pSwapChain)
        return;

    // Direct3D 11 and Direct3D 12 present through the same function of dxgi, so which
    // one of them a frame was drawn with is what its swap chain says, not which
    // function of the game it came through: the one hands over a chain of Direct3D 12
    // resources, the other one of Direct3D 11 textures.
    const bool d3d12 = (FusionDxHook::GetSwapChainKind(pSwapChain) == FusionDxHook::SwapChainKind::Direct3D12);

    // The buffers of the frame, which is what it is placed in the middle of, and the
    // queue it is submitted with, which is what anything recorded into a list of it has
    // to be submitted with as well.
    FusionDxHook::Frame::OnPresent(pSwapChain);

    // The context of the device is the one the thread that presents uses, which is also
    // the thread that executes the frame of the game.
    Xrd::SetCommandContext(nullptr);

    if (d3d12)
    {
        // The queue the frames of the game are submitted with, which is the one the
        // effect has to submit on as well, see xrdrender.d3d12.h.
        Xrd::SetCommandQueue(FusionDxHook::Frame::GetD3D12CommandQueue(pSwapChain));

        // The renderer is built on the swap chain, which only this call has.
        Xrd::Init(Xrd::RENDERER_D3D12, pSwapChain);

        // The drops of this API are recorded into the list of the frame itself, in the
        // middle of it, see DrawDropletsInListD3D12. What was drawn there leaves
        // nothing for this to do.
        if (!gbDropsDrawnThisFrame)
            DrawDroplets();
    }
    else
    {
        Xrd::Init(Xrd::RENDERER_D3D11, pSwapChain);

        // The frame of this renderer is drawn under its UI, in the middle of the
        // commands of the frame, see DrawDropletsUnderUiD3D11. This is only what is left
        // for the frames that never reached such a draw, which the menus are.
        if (!gbDropsDrawnThisFrame)
            DrawDroplets();
    }

    // The frame is over: what is drawn from here on belongs to the next one, and what
    // was counted of this one is forgotten with it. The library forgets its own the
    // same way, see FusionDxHook::Frame::OnPresent.
    {
        std::lock_guard<std::mutex> guard(gDetectorLock);

        gDetectors.clear();
        gDx12SubmitIndex = 0;
        gbDx12CompositeSeen = false;
        gbDx12CompositeSubmitted = false;
    }

    gbDropsDrawnThisFrame = false;
    gNumBackBufferDraws = 0;
    gpSceneDrawShader = nullptr;
}

uintptr_t ResolveDisplacement(hook::pattern& pattern, ptrdiff_t offset = 0)
{
    return utility::resolve_displacement((uintptr_t)pattern.get_first(offset)).value_or(0);
}

void Init()
{
    // The hooks of FusionDxHook itself. The effect does not draw with them, but the game
    // does not come up in Direct3D 12 without them, so they stay.
    FusionDxHook::Init();

    // What puts a draw of the effect in the middle of a frame is held by FusionDxHook,
    // and what makes an inline hook is asked of the host, see MakeFrameHook.
    FusionDxHook::Frame::Init(MakeFrameHook);

    // The place in the frame the drops go in, which is the one the effect is drawn at,
    // see FusionDxHook::Frame.
    FusionDxHook::Frame::onPresentedDrawD3D11Event += [](ID3D11DeviceContext* pContext, ID3D11RenderTargetView* pTarget, const void* pPixelShader)
    {
        DrawDropletsUnderUiD3D11(pContext, pTarget, pPixelShader);
    };

    FusionDxHook::Frame::onPresentedDrawD3D12Event += [](ID3D12GraphicsCommandList* pList, const void* pPipelineState, bool& bRecorded)
    {
        DrawDropletsInListD3D12(pList, pPipelineState, bRecorded);
    };

    FusionDxHook::Frame::onSubmitD3D12Event += [](ID3D12CommandQueue* pQueue, unsigned NumCommandLists, ID3D12CommandList* const* ppCommandLists)
    {
        DrawDropletsBeforeUiSubmissionD3D12(pQueue, NumCommandLists, ppCommandLists);
    };

    WaterDrops::ReadIniSettings(true);
    static DXGI_FORMAT gFormat = DXGI_FORMAT_R10G10B10A2_UNORM;
    WaterDrops::SetXUVScale(0.125f, 0.875f);

    auto pattern = hook::pattern("44 38 3D ? ? ? ? 0F 85 ? ? ? ? 44 38 3D ? ? ? ? 0F 85 ? ? ? ? 44 38 3D");
    CTimer::m_CodePause.SetAddress(ResolveDisplacement(pattern));
    CTimer::m_UserPause.SetAddress(ResolveDisplacement(pattern, 13));

    pattern = hook::pattern("F3 0F 10 0D ? ? ? ? 0F 2F 0D ? ? ? ? 0F 86");
    CWeather::Rain.SetAddress(ResolveDisplacement(pattern));

    pattern = hook::pattern("44 88 2D ? ? ? ? 48 89 88");
    CCutsceneMgr::ms_running.SetAddress(ResolveDisplacement(pattern));

    pattern = hook::pattern("8B 05 ? ? ? ? 85 C0 74 ? 83 F8 0D");
    CGame::currArea.SetAddress(ResolveDisplacement(pattern));

    pattern = hook::pattern("F6 05 ? ? ? ? ? 75 ? F6 05 ? ? ? ? ? 0F 84");
    CCullZones::CurrentFlags_Camera.SetAddress(ResolveDisplacement(pattern));

    pattern = hook::pattern("F6 05 ? ? ? ? ? 0F 84 ? ? ? ? 41 B9 04 00 00 00");
    CCullZones::CurrentFlags_Player.SetAddress(ResolveDisplacement(pattern));

    pattern = hook::pattern("83 3D ? ? ? ? ? 0F 84 ? ? ? ? 48 8B 0D ? ? ? ? 83 B9");
    gGameState.SetAddress(ResolveDisplacement(pattern));

    pattern = hook::pattern("48 8D 35 ? ? ? ? F3 0F 58 C8");
    TheCamera.SetAddress(ResolveDisplacement(pattern));

    pattern = hook::pattern("48 8B 4B ? 48 8B 01 FF 50 ? 8B F0");
    static auto FD3D11ViewportPresentCheckedHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
    {
        auto pSwapChain = *(IDXGISwapChain**)(regs.rbx + 0x78);
        PresentHandler(pSwapChain);
    });

    pattern = hook::pattern("48 8B 01 44 8B C3 8B D5");
    static auto FD3D12ViewportPresentCheckedHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
    {
        auto pSwapChain = (IDXGISwapChain*)(regs.rcx);
        PresentHandler(pSwapChain);
    });

    pattern = hook::pattern("89 44 24 ? 41 FF 52 ? BA 00 00 00 80");
    static auto onBeforeResizeHookD3D11 = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
    {
        //IDXGISwapChain* pSwapChain = (IDXGISwapChain*)regs.rcx;
        //UINT Width = regs.r8;
        //UINT Height = regs.r9;
        gFormat = (DXGI_FORMAT)regs.rax;

        // The buffers of the chain are given up before it is resized, which is what DXGI
        // asks of anything that holds one of them, see FusionDxHook::Frame.
        FusionDxHook::Frame::OnBeforeResize();

        WaterDrops::Reset();
        Xrd::Shutdown();
    });

    pattern = hook::pattern("44 89 74 24 ? 89 44 24 ? 41 FF 52");
    static auto onBeforeResizeHookD3D12 = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
    {
        //IDXGISwapChain* pSwapChain = (IDXGISwapChain*)regs.rcx;
        //UINT Width = regs.r8;
        //UINT Height = regs.r9;
        gFormat = (DXGI_FORMAT)regs.rax;

        // The buffers of the chain are given up before it is resized, which is what DXGI
        // asks of anything that holds one of them, and the queue of the game is waited
        // on before anything of the renderer is freed, see FusionDxHook::Frame.
        FusionDxHook::Frame::OnBeforeResize();

        WaterDrops::Reset();
        Xrd::Shutdown();
    });

    pattern = hook::pattern("E8 ? ? ? ? 49 8B 07 49 8B CF 48 8B 95");
    FxManager_c::shCreateFxSystem = safetyhook::create_inline(ResolveDisplacement(pattern), FxManager_c::CreateFxSystem);

    pattern = hook::pattern("E8 ? ? ? ? 48 8B D8 48 85 C0 74 ? 48 8B C8 E8 ? ? ? ? 48 8B 53");
    UParticleSystemComponent::shResetParticles = safetyhook::create_inline(ResolveDisplacement(pattern), UParticleSystemComponent::ResetParticles);

    pattern = hook::pattern("E8 ? ? ? ? FF C3 3B DF 0F 8C ? ? ? ? 44 0F 28 BC 24");
    FxSystem_c::shAddParticle = safetyhook::create_inline(ResolveDisplacement(pattern), FxSystem_c::AddParticle);

    pattern = hook::pattern("8D 47 ? 83 F8 24");
    static auto CParticleAddParticleHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
    {
        auto type = (tParticleType)(regs.rcx);
        auto position = (RwV3d*)(regs.rdx);
        
        if (type == PARTICLE_SPLASH || type == PARTICLE_BOAT_THRUSTJET)
        {
            SprayDroplets(position, 7.0f, false);
        }
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