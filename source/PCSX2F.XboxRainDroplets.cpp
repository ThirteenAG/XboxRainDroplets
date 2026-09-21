#define XRD_ENABLE_D3D9
#define XRD_ENABLE_D3D10
#define XRD_ENABLE_D3D10_1
#define XRD_ENABLE_D3D11
#define XRD_ENABLE_D3D12
#define XRD_ENABLE_OPENGL
#define XRD_ENABLE_VULKAN
#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
// the Vulkan declarations of the repository come first, the small set of the
// hook library then stays out of the way (it is guarded by VULKAN_H_)
#include <vulkan/vulkan.h>
#include "xrd/xrd.h"
#define FUSIONDXHOOK_INCLUDE_D3D8     0
#define FUSIONDXHOOK_INCLUDE_D3D9     0
#define FUSIONDXHOOK_INCLUDE_D3D10    0
#define FUSIONDXHOOK_INCLUDE_D3D10_1  0
#define FUSIONDXHOOK_INCLUDE_D3D11    1
#define FUSIONDXHOOK_INCLUDE_D3D12    1
#define FUSIONDXHOOK_INCLUDE_OPENGL   1
#define FUSIONDXHOOK_INCLUDE_VULKAN   1
#define FUSIONDXHOOK_USE_SAFETYHOOK   1
#define DELAYED_BIND 10000ms
#include "FusionDxHook.h"

#pragma pack(push, 1)
struct XRData {
    uint32_t p_enabled;
    uint32_t ms_enabled;

    float ms_rainIntensity;
    uint32_t p_rainIntensity;

    float ms_right_x;
    float ms_right_y;
    float ms_right_z;
    float ms_up_x;
    float ms_up_y;
    float ms_up_z;
    float ms_at_x;
    float ms_at_y;
    float ms_at_z;
    float ms_pos_x;
    float ms_pos_y;
    float ms_pos_z;

    uint32_t p_right_x;
    uint32_t p_right_y;
    uint32_t p_right_z;
    uint32_t p_up_x;
    uint32_t p_up_y;
    uint32_t p_up_z;
    uint32_t p_at_x;
    uint32_t p_at_y;
    uint32_t p_at_z;
    uint32_t p_pos_x;
    uint32_t p_pos_y;
    uint32_t p_pos_z;

    float RegisterSplash_Vec_x;
    float RegisterSplash_Vec_y;
    float RegisterSplash_Vec_z;
    float RegisterSplash_distance;
    int32_t RegisterSplash_duration;
    float RegisterSplash_removaldistance;

    int32_t FillScreen_amount;

    float FillScreenMoving_Vec_x;
    float FillScreenMoving_Vec_y;
    float FillScreenMoving_Vec_z;
    float FillScreenMoving_amount;
    int32_t FillScreenMoving_isBlood;

#ifdef __cplusplus
    bool Enabled(uint64_t ptr)
    {
        if (p_enabled)
            return *(uint32_t*)(ptr + p_enabled);
        else
            return ms_enabled != 0;
    }

    float GetRainIntensity(uint64_t ptr)
    {
        if (p_rainIntensity)
            return *(float*)(ptr + p_rainIntensity);
        else
            return ms_rainIntensity != 0;
    }

    RwV3d GetRight(uint64_t ptr)
    {
        return RwV3d((p_right_x ? *(float*)(ptr + p_right_x) : ms_right_x),
            (p_right_y ? *(float*)(ptr + p_right_y) : ms_right_y),
            (p_right_z ? *(float*)(ptr + p_right_z) : ms_right_z));
    }

    RwV3d GetUp(uint64_t ptr)
    {
        return RwV3d((p_up_x ? *(float*)(ptr + p_up_x) : ms_up_x),
            (p_up_y ? *(float*)(ptr + p_up_y) : ms_up_y),
            (p_up_z ? *(float*)(ptr + p_up_z) : ms_up_z));
    }

    RwV3d GetAt(uint64_t ptr)
    {
        return RwV3d((p_at_x ? *(float*)(ptr + p_at_x) : ms_at_x),
            (p_at_y ? *(float*)(ptr + p_at_y) : ms_at_y),
            (p_at_z ? *(float*)(ptr + p_at_z) : ms_at_z));
    }

    RwV3d GetPos(uint64_t ptr)
    {
        return RwV3d((p_pos_x ? *(float*)(ptr + p_pos_x) : ms_pos_x),
            (p_pos_y ? *(float*)(ptr + p_pos_y) : ms_pos_y),
            (p_pos_z ? *(float*)(ptr + p_pos_z) : ms_pos_z));
    }

    void RegisterSplash()
    {
        if (RegisterSplash_duration)
        {
            RwV3d prt_pos = { RegisterSplash_Vec_x, RegisterSplash_Vec_y, RegisterSplash_Vec_z };
            auto len = WaterDrops::GetDistanceBetweenEmitterAndCamera(prt_pos);
            if (len <= RegisterSplash_removaldistance)
            {
                WaterDrops::RegisterSplash(&prt_pos, RegisterSplash_distance, RegisterSplash_duration, RegisterSplash_removaldistance);
                RegisterSplash_Vec_x = 0.0f;
                RegisterSplash_Vec_y = 0.0f;
                RegisterSplash_Vec_z = 0.0f;
                RegisterSplash_distance = 0.0f;
                RegisterSplash_duration = 0;
                RegisterSplash_removaldistance = 0.0f;
            }
        }
    }

    void FillScreen()
    {
        if (FillScreen_amount)
        {
            WaterDrops::FillScreen(FillScreen_amount);
            FillScreen_amount = 0;
        }
    }

    void FillScreenMoving()
    {
        if (FillScreenMoving_amount)
        {
            if (FillScreenMoving_amount == 1)
            {
                RwV3d prt_pos = { FillScreenMoving_Vec_x, FillScreenMoving_Vec_y, FillScreenMoving_Vec_z };
                auto len = WaterDrops::GetDistanceBetweenEmitterAndCamera(prt_pos);
                WaterDrops::FillScreenMoving(WaterDrops::GetDropsAmountBasedOnEmitterDistance(len, 35.0f, 100.0f), FillScreenMoving_isBlood);
            }
            else
            {
                WaterDrops::FillScreenMoving(FillScreenMoving_amount, FillScreenMoving_isBlood);
            }
            FillScreenMoving_amount = 0;
            FillScreenMoving_isBlood = 0;
        }
    }
#endif
};
#pragma pack(pop)

// ---------------------------------------------------------------------------
// Drawing into the frame of the game, before its UI
//
// Mirrored from source/API/pcsx2f_api.h of the plugin injector. A guest plugin
// reports the point of its frame where the world is done and the UI is not drawn
// yet, the emulator stops the guest there and calls the export below with the frame
// the game is drawing into, and what is drawn into it is under the UI of the game
// instead of on top of it the way the draw at the present call is.
// ---------------------------------------------------------------------------
enum PCSX2FRenderPhase
{
    // The frame of the game is handed over: the effect takes the state of the drops from
    // the game at this point and draws nothing yet, see the handling below.
    PCSX2FRenderPhase_PrepareFrame = 1,

    // The drops are drawn into the frame that was handed over by the call before this one.
    PCSX2FRenderPhase_DrawFrame = 2,
};

enum PCSX2FRenderer
{
    PCSX2FRenderer_Unknown = 0,
    PCSX2FRenderer_D3D11,
    PCSX2FRenderer_D3D12,
    PCSX2FRenderer_OpenGL,
    PCSX2FRenderer_Vulkan,
};

struct PCSX2FRenderTargetInfo
{
    uint32_t renderer;  // PCSX2FRenderer
    void* resource;     // ID3D11Texture2D, ID3D12Resource, VkImage or the texture of OpenGL
    uint32_t format;
    uint32_t width;
    uint32_t height;
    uint32_t state;
};

// Latches on the first time the emulator asks for the phase, and from then on the
// drops are drawn there and not at the present call any more. An emulator without
// the support of it never asks and the drops keep being drawn at the present call.
static bool gGuestRenderPhaseSupported = false;

// Whether the report of the frame said there is anything to draw, see the phase handling
// below: the call that prepares the frame only reads what the game says, the one that
// draws the drops comes right after it.
static bool gGuestRenderPhaseAsked = false;

// The plugin of the guest the drops are read out of. A name is enough: the injector
// resolves it against the plugins folder of the emulator, see GetPluginSymbolAddr there,
// so nothing here has to know where the emulator or its working directory is.
constexpr const char* GUEST_PLUGIN_PATH = "PCSX2F.XboxRainDroplets.elf";

// Reads what the guest plugin reports into the effect. True when the effect has
// something to draw this frame.
bool UpdateDroplets()
{
    static uintptr_t(*GetEEMainMemoryStart)();
    static size_t(*GetEEMainMemorySize)();
    static void*(*GetWindowHandle)();
    static uintptr_t(*GetPluginSymbolAddr)(const char* path, const char* sym_name);
    static bool(*VMStateIsRunning)();

    static bool once = false;
    if (!once)
    {
        auto PCSX2PluginInjector = L"PCSX2PluginInjector.asi";
        auto h = GetModuleHandle(PCSX2PluginInjector);
        if (h != NULL)
        {
            GetEEMainMemoryStart = (uintptr_t(*)())GetProcAddress(h, "GetEEMainMemoryStart");
            GetEEMainMemorySize = (size_t(*)())GetProcAddress(h, "GetEEMainMemorySize");
            GetWindowHandle = (void* (*)())GetProcAddress(h, "GetWindowHandle");
            GetPluginSymbolAddr = (uintptr_t(*)(const char*, const char*))GetProcAddress(h, "GetPluginSymbolAddr");
            VMStateIsRunning = (bool(*)()) GetProcAddress(h, "VMStateIsRunning");
        }
        once = true;
    }

    static XRData* pXRData = nullptr;

    // The address of the memory of the guest is only given while a game is running,
    // and only with it can the data of the plugin be reached: the symbol is an
    // offset into that memory, not an address on its own.
    const uintptr_t eeStart = (VMStateIsRunning && VMStateIsRunning() && GetEEMainMemoryStart) ? GetEEMainMemoryStart() : 0;

    if (!eeStart)
    {
        pXRData = nullptr;
        return false;
    }

    if (!pXRData)
    {
        auto sym = GetPluginSymbolAddr ? GetPluginSymbolAddr(GUEST_PLUGIN_PATH, "XboxRainDropletsData") : 0;
        if (sym)
            pXRData = (XRData*)(eeStart + sym);

        return false;
    }

    // the marker the plugin is compiled with, cleared once so that it fills its
    // fields in from then on
    if (std::string_view((char*)pXRData).starts_with("X"))
    {
        memset(pXRData, 0, 255);
        return false;
    }

    if (!pXRData->Enabled(0))
        return false;

    WaterDrops::ms_rainIntensity = pXRData->GetRainIntensity(0);

    WaterDrops::bRadial = false;

    WaterDrops::up = pXRData->GetUp(0);
    WaterDrops::at = pXRData->GetAt(0);
    WaterDrops::right = pXRData->GetRight(0);
    WaterDrops::pos = pXRData->GetPos(0);

    pXRData->RegisterSplash();
    pXRData->FillScreen();
    pXRData->FillScreenMoving();

    return true;
}

void DrawDroplets()
{
    WaterDrops::Process();
    WaterDrops::Render();
}

// The present call: where the drops were always drawn, on top of everything the
// game drew. It is only what happens now when the emulator has no phase to hand
// the frame of the game over at, see PCSX2F_OnGuestRenderPhase.
void RenderDroplets()
{
    if (gGuestRenderPhaseSupported)
        return;

    if (UpdateDroplets())
        DrawDroplets();
}

// Direct3D 10 and 11 draw into a view of a resource and not into the resource
// itself, which is what the emulator hands over, so a view of it is made here and
// kept for as long as the target stays the same one.
ID3D11RenderTargetView* GetRenderTargetView(ID3D11Texture2D* pTexture)
{
    static ID3D11Texture2D* pCachedTexture = nullptr;
    static ID3D11RenderTargetView* pCachedView = nullptr;

    if (pCachedTexture == pTexture && pCachedView)
        return pCachedView;

    if (pCachedView)
    {
        pCachedView->Release();
        pCachedView = nullptr;
    }

    if (pCachedTexture)
    {
        pCachedTexture->Release();
        pCachedTexture = nullptr;
    }

    ID3D11Device* pDevice = nullptr;
    pTexture->GetDevice(&pDevice);
    if (pDevice)
    {
        // a null description is the format of the resource itself
        if (SUCCEEDED(pDevice->CreateRenderTargetView(pTexture, nullptr, &pCachedView)))
        {
            pTexture->AddRef();
            pCachedTexture = pTexture;
        }
        else
        {
            pCachedView = nullptr;
        }

        pDevice->Release();
    }

    return pCachedView;
}

// The frame of the game, between its world and its UI. The emulator stops the guest
// where its plugin reported the phase and asks for this, on the thread that owns
// the graphics API, so the draw is done with it right away and the UI the game
// draws next lands on top of it. The name is what the plugin injector looks up in
// a module it loaded, see its dllmain.cpp.
extern "C" __declspec(dllexport) void PCSX2F_OnGuestRenderPhase(uint32_t phase, const PCSX2FRenderTargetInfo* target)
{
    // The emulator calls a plugin with the frame the game is drawing into, once to let it
    // read what its game says and once to draw, and does so at the point of the frame where
    // the world is done and the UI is not drawn yet, see source/API/pcsx2f_api.h of the
    // plugin injector: what is drawn there is under the UI of the game instead of on top of
    // it, which is what a draw at the present call can only be.
    if (phase != PCSX2FRenderPhase_PrepareFrame && phase != PCSX2FRenderPhase_DrawFrame)
        return;

    if (!target || !target->resource)
        return;

    gGuestRenderPhaseSupported = true;

    // Direct3D 8 and 9 need the size of the target, Vulkan the size and the format
    // of the image, and Direct3D 12 and Vulkan also have to be told what the target
    // is for, since nothing about a resource says it: this one is the frame that is
    // being drawn, not the one that is being presented.
    Xrd::RenderTarget rt{};
    rt.resource = target->resource;
    rt.size = { (int32_t)target->width, (int32_t)target->height };
    rt.format = target->format;
    rt.state = Xrd::TARGET_STATE_RENDER_TARGET;

    if (target->renderer == PCSX2FRenderer_D3D11)
        rt.resource = GetRenderTargetView((ID3D11Texture2D*)target->resource);

    Xrd::SetTarget(&rt);
    Xrd::SetTargetState(Xrd::TARGET_STATE_RENDER_TARGET);

    if (phase == PCSX2FRenderPhase_PrepareFrame)
        gGuestRenderPhaseAsked = UpdateDroplets();
    else if (gGuestRenderPhaseAsked)
        DrawDroplets();

    Xrd::SetTarget(nullptr);
    Xrd::SetTargetState(Xrd::TARGET_STATE_PRESENT);
}

// The window a frame is presented in, which is what the drops have to keep their shape
// against: the frame of a game is drawn into a buffer of its own and stretched to that
// window, so a drop drawn round into the buffer is an oval on screen, see
// WaterDrops::ms_xScale. It is the present call that knows the window.
#if FUSIONDXHOOK_INCLUDE_D3D11 || FUSIONDXHOOK_INCLUDE_D3D12
static void SetScreenSize(IDXGISwapChain* pSwapChain)
{
    if (!pSwapChain)
        return;

    DXGI_SWAP_CHAIN_DESC desc = {};

    if (SUCCEEDED(pSwapChain->GetDesc(&desc)))
    {
        WaterDrops::ms_screenWidth = (int32_t)desc.BufferDesc.Width;
        WaterDrops::ms_screenHeight = (int32_t)desc.BufferDesc.Height;
    }
}
#endif

extern "C" __declspec(dllexport) void InitializeASI()
{
    static std::once_flag flag;
    std::call_once(flag, []()
    {
        FusionDxHook::Init();

        FusionDxHook::onInitEvent += []()
        {

        };

        #if FUSIONDXHOOK_INCLUDE_D3D11
        FusionDxHook::D3D11::onPresentEvent += [](IDXGISwapChain* pSwapChain)
        {
            Xrd::Init(Xrd::RENDERER_D3D11, pSwapChain);

            SetScreenSize(pSwapChain);

            RenderDroplets();
        };

        FusionDxHook::D3D11::onBeforeResizeEvent += [](IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
        {
            WaterDrops::Reset();
            Xrd::Shutdown();
        };

        FusionDxHook::D3D11::onAfterResizeEvent += [](IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
        {
            
        };

        FusionDxHook::D3D11::onShutdownEvent += []()
        {
            // the device is gone, nothing of the backend may be handed back to it
            Xrd::Detach();
            WaterDrops::Shutdown();
            Xrd::Shutdown();
        };
        #endif // FUSIONDXHOOK_INCLUDE_D3D11

        // D3D11on12
        #if FUSIONDXHOOK_INCLUDE_D3D12
        FusionDxHook::D3D12::onPresentEvent += [](IDXGISwapChain* pSwapChain)
        {
            Xrd::SetCommandQueue(FusionDxHook::D3D12::GetCommandQueueFromSwapChain(pSwapChain));
            Xrd::Init(Xrd::RENDERER_D3D12, pSwapChain);

            SetScreenSize(pSwapChain);

            RenderDroplets();
        };

        FusionDxHook::D3D12::onExecuteCommandListsEvent += [](ID3D12CommandQueue* pCommandQueue, UINT NumCommandLists, const ID3D12CommandList** ppCommandLists)
        {
            //FusionDxHook::D3D12::GetCommandQueueFromSwapChain(pSwapChain) is more reliable
            //if (pCommandQueue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT)
            //{
            //    Xrd::SetCommandQueue(pCommandQueue);
            //}
        };

        FusionDxHook::D3D12::onBeforeResizeEvent += [](IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
        {
            WaterDrops::Reset();
            Xrd::Shutdown();
        };

        FusionDxHook::D3D12::onAfterResizeEvent += [](IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
        {

        };

        FusionDxHook::D3D12::onShutdownEvent += []()
        {
            // the device is gone, nothing of the backend may be handed back to it
            Xrd::Detach();
            WaterDrops::Shutdown();
            Xrd::Shutdown();
        };
        #endif // FUSIONDXHOOK_INCLUDE_D3D12

        // -------------------------------------------------------------------
        // OpenGL
        //
        // The window is presented with its device context, the frame is whatever
        // is in the framebuffer at that moment and it is read back from there.
        // -------------------------------------------------------------------
        #if FUSIONDXHOOK_INCLUDE_OPENGL
        FusionDxHook::OPENGL::onSwapBuffersEvent += [](HDC hDC)
        {
            Xrd::Init(Xrd::RENDERER_OPENGL, hDC);

            RECT rect = {};
            HWND window = WindowFromDC(hDC);

            if (window && GetClientRect(window, &rect))
            {
                WaterDrops::ms_screenWidth = rect.right - rect.left;
                WaterDrops::ms_screenHeight = rect.bottom - rect.top;
            }

            RenderDroplets();
        };

        FusionDxHook::OPENGL::onShutdownEvent += []() {
            // the context is gone, nothing of the backend may be handed back to it
            Xrd::Detach();
            WaterDrops::Shutdown();
            Xrd::Shutdown();
        };
        #endif // FUSIONDXHOOK_INCLUDE_OPENGL

        // -------------------------------------------------------------------
        // Vulkan
        //
        // Vulkan has the least implicit state of them all, so the hooks collect
        // what the backend cannot find out on its own: the device and its queue
        // family from the device creation, the format and the size of the images
        // from the description of the swap chain, and the image of the moment
        // from the present call. Xrd::VulkanPresent is where the three meet, see
        // source/xrd/xrdrender.vk.h.
        // -------------------------------------------------------------------
        #if FUSIONDXHOOK_INCLUDE_VULKAN
        static Xrd::VulkanPresent gVulkanPresent;

        FusionDxHook::VULKAN::onvkCreateDeviceEvent += [](VkPhysicalDevice gpu, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks*, VkDevice* pDevice)
        {
            gVulkanPresent.OnCreateDevice(gpu, pCreateInfo, pDevice);
        };

        FusionDxHook::VULKAN::onVkCreateSwapchainKHREvent += [](VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks*, VkSwapchainKHR*)
        {
            if (pCreateInfo)
            {
                WaterDrops::ms_screenWidth = (int32_t)pCreateInfo->imageExtent.width;
                WaterDrops::ms_screenHeight = (int32_t)pCreateInfo->imageExtent.height;
            }

            gVulkanPresent.OnCreateSwapchain(device, pCreateInfo);
        };

        FusionDxHook::VULKAN::onVkQueuePresentKHREvent += [](VkQueue queue, const VkPresentInfoKHR* pPresentInfo)
        {
            if (!gVulkanPresent.Prepare(queue, pPresentInfo))
                return;

            RenderDroplets();
        };

        FusionDxHook::VULKAN::onShutdownEvent += []() {
            // The loader is unloaded after the device was destroyed, so nothing
            // of the backend may be handed back to the driver any more.
            Xrd::Detach();
            WaterDrops::Shutdown();
            Xrd::Shutdown();
            gVulkanPresent.Clear();
        };
        #endif // FUSIONDXHOOK_INCLUDE_VULKAN

        FusionDxHook::onShutdownEvent += []()
        {

        };
    });
}

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD fdwReason, LPVOID)
{
    DisableThreadLibraryCalls(hInstance);

    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        if (!IsUALPresent()) { InitializeASI(); }
        break;
    case DLL_PROCESS_DETACH:
        FusionDxHook::DeInit();
        break;
    }

    return TRUE;
}