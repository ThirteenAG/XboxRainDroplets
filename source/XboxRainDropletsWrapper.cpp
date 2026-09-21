// ---------------------------------------------------------------------------
// The wrapper, one file that hooks every graphics API a game may be using and
// draws the droplets on top of the frame.
//
// It is the only project that needs every backend at once, which is why the
// Direct3D 8 one lives in its own translation unit and is pulled in by the
// project (see source/xrd/xrdrender.d3d8.cpp in premake5.lua). Direct3D 8 and
// Direct3D 9 cannot be compiled together.
//
// Every hook does the same three things:
//   Xrd::Init(renderer, the device or swap chain that is being presented)
//   WaterDrops::Process()
//   WaterDrops::Render()
//
// The renderer picks the target up on its own, so the same two calls work at
// the end of the frame and in the middle of it. The position of the hook is
// what decides whether the drops end up under the UI or over it.
// ---------------------------------------------------------------------------

#define XRD_ENABLE_D3D9 1
#define XRD_ENABLE_D3D10 1
#define XRD_ENABLE_D3D10_1 1
#define XRD_ENABLE_D3D11 1
#define XRD_ENABLE_D3D12 1
#define XRD_ENABLE_OPENGL 1
#define XRD_ENABLE_VULKAN 1
#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
// the real Vulkan declarations come first, the hook library then keeps its own
// small set out of the way (it is guarded by VULKAN_H_)
#include <vulkan/vulkan.h>
#include "xrd/xrd.h"

#define FUSIONDXHOOK_INCLUDE_D3D8     1
#define FUSIONDXHOOK_INCLUDE_D3D9     1
#define FUSIONDXHOOK_INCLUDE_D3D10    1
#define FUSIONDXHOOK_INCLUDE_D3D10_1  1
#define FUSIONDXHOOK_INCLUDE_D3D11    1
#define FUSIONDXHOOK_INCLUDE_D3D12    1
#define FUSIONDXHOOK_INCLUDE_OPENGL   1
#define FUSIONDXHOOK_INCLUDE_VULKAN   1
#define FUSIONDXHOOK_USE_SAFETYHOOK   1
#define DELAYED_BIND 10000ms
#include "FusionDxHook.h"

extern "C" __declspec(dllexport) void InitializeASI()
{
    static std::once_flag flag;
    std::call_once(flag, []()
    {
        FusionDxHook::Init();

        FusionDxHook::onInitEvent += []()
        {

        };

        // -------------------------------------------------------------------
        // Direct3D 8
        // -------------------------------------------------------------------
        #if FUSIONDXHOOK_INCLUDE_D3D8
        FusionDxHook::D3D8::onPresentEvent += [](D3D8_LPDIRECT3DDEVICE8 pDevice)
        {
            Xrd::Init(Xrd::RENDERER_D3D8, pDevice);

            WaterDrops::Process();
            WaterDrops::Render();
        };

        FusionDxHook::D3D8::onResetEvent += [](D3D8_LPDIRECT3DDEVICE8 pDevice)
        {
            WaterDrops::Reset();
            Xrd::Reset();
        };

        FusionDxHook::D3D8::onShutdownEvent += []()
        {
            WaterDrops::Shutdown();
            Xrd::Shutdown();
        };
        #endif // FUSIONDXHOOK_INCLUDE_D3D8

        // -------------------------------------------------------------------
        // Direct3D 9
        // -------------------------------------------------------------------
        #if FUSIONDXHOOK_INCLUDE_D3D9
        FusionDxHook::D3D9::onEndSceneEvent += [](LPDIRECT3DDEVICE9 pDevice)
        {
            Xrd::Init(Xrd::RENDERER_D3D9, pDevice);

            WaterDrops::Process();
            WaterDrops::Render();
        };

        FusionDxHook::D3D9::onResetEvent += [](LPDIRECT3DDEVICE9 pDevice)
        {
            WaterDrops::Reset();
            Xrd::Reset();
        };

        FusionDxHook::D3D9::onShutdownEvent += []()
        {
            WaterDrops::Shutdown();
            Xrd::Shutdown();
        };
        #endif // FUSIONDXHOOK_INCLUDE_D3D9

        // -------------------------------------------------------------------
        // Direct3D 10
        // -------------------------------------------------------------------
        #if FUSIONDXHOOK_INCLUDE_D3D10
        FusionDxHook::D3D10::onPresentEvent += [](IDXGISwapChain* pSwapChain)
        {
            Xrd::Init(Xrd::RENDERER_D3D10, pSwapChain);

            WaterDrops::Process();
            WaterDrops::Render();
        };

        FusionDxHook::D3D10::onAfterResizeEvent += [](IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
        {
            WaterDrops::Reset();
            Xrd::Reset();
        };

        FusionDxHook::D3D10::onShutdownEvent += []()
        {
            WaterDrops::Shutdown();
            Xrd::Shutdown();
        };
        #endif // FUSIONDXHOOK_INCLUDE_D3D10

        // -------------------------------------------------------------------
        // Direct3D 10.1
        // -------------------------------------------------------------------
        #if FUSIONDXHOOK_INCLUDE_D3D10_1
        FusionDxHook::D3D10_1::onPresentEvent += [](IDXGISwapChain* pSwapChain)
        {
            Xrd::Init(Xrd::RENDERER_D3D10_1, pSwapChain);

            WaterDrops::Process();
            WaterDrops::Render();
        };

        FusionDxHook::D3D10_1::onAfterResizeEvent += [](IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
        {
            WaterDrops::Reset();
            Xrd::Reset();
        };

        FusionDxHook::D3D10_1::onShutdownEvent += []()
        {
            WaterDrops::Shutdown();
            Xrd::Shutdown();
        };
        #endif // FUSIONDXHOOK_INCLUDE_D3D10_1

        // -------------------------------------------------------------------
        // Direct3D 11
        // -------------------------------------------------------------------
        #if FUSIONDXHOOK_INCLUDE_D3D11
        FusionDxHook::D3D11::onPresentEvent += [](IDXGISwapChain* pSwapChain)
        {
            Xrd::Init(Xrd::RENDERER_D3D11, pSwapChain);

            WaterDrops::Process();
            WaterDrops::Render();
        };

        FusionDxHook::D3D11::onBeforeResizeEvent += [](IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
        {
            WaterDrops::Reset();
            Xrd::Reset();
        };

        FusionDxHook::D3D11::onShutdownEvent += []()
        {
            WaterDrops::Shutdown();
            Xrd::Shutdown();
        };
        #endif // FUSIONDXHOOK_INCLUDE_D3D11

        // -------------------------------------------------------------------
        // Direct3D 12
        //
        // Nothing about a Direct3D 12 resource says what it is used for, so the
        // queue of the game is handed over and the state of the target is
        // declared. The frame is finished here, the drops go over everything.
        // -------------------------------------------------------------------
        #if FUSIONDXHOOK_INCLUDE_D3D12
        FusionDxHook::D3D12::onPresentEvent += [](IDXGISwapChain* pSwapChain)
        {
            Xrd::SetCommandQueue(FusionDxHook::D3D12::GetCommandQueueFromSwapChain(pSwapChain));
            Xrd::Init(Xrd::RENDERER_D3D12, pSwapChain);
            Xrd::SetTargetState(Xrd::TARGET_STATE_PRESENT);

            WaterDrops::Process();
            WaterDrops::Render();
        };

        FusionDxHook::D3D12::onBeforeResizeEvent += [](IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
        {
            WaterDrops::Reset();
            Xrd::Reset();
        };

        FusionDxHook::D3D12::onShutdownEvent += []()
        {
            WaterDrops::Shutdown();
            Xrd::Shutdown();
        };
        #endif // FUSIONDXHOOK_INCLUDE_D3D12

        // -------------------------------------------------------------------
        // OpenGL
        //
        // The window is presented with its device context, which is all the
        // backend needs: the frame is whatever is in the framebuffer that is
        // bound at that moment and it is read back from there.
        // -------------------------------------------------------------------
        #if FUSIONDXHOOK_INCLUDE_OPENGL
        FusionDxHook::OPENGL::onSwapBuffersEvent += [](HDC hDC)
        {
            Xrd::Init(Xrd::RENDERER_OPENGL, hDC);

            WaterDrops::Process();
            WaterDrops::Render();
        };

        FusionDxHook::OPENGL::onShutdownEvent += []()
        {
            WaterDrops::Shutdown();
            Xrd::Shutdown();
        };
        #endif // FUSIONDXHOOK_INCLUDE_OPENGL

        // -------------------------------------------------------------------
        // Vulkan
        //
        // Vulkan has the least implicit state of them all, so the hook collects
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
            gVulkanPresent.OnCreateSwapchain(device, pCreateInfo);
        };

        FusionDxHook::VULKAN::onVkQueuePresentKHREvent += [](VkQueue queue, const VkPresentInfoKHR* pPresentInfo)
        {
            if (!gVulkanPresent.Prepare(queue, pPresentInfo))
                return;

            WaterDrops::Process();
            WaterDrops::Render();
        };

        FusionDxHook::VULKAN::onShutdownEvent += []() {
            WaterDrops::Shutdown();

            // The loader is unloaded after the device was destroyed, so nothing
            // of the backend may be handed back to the driver any more.
            Xrd::Detach();
            Xrd::Shutdown();
            gVulkanPresent.Clear();
        };
        #endif // FUSIONDXHOOK_INCLUDE_VULKAN

        FusionDxHook::onShutdownEvent += []()
        {
            WaterDrops::Shutdown();
            Xrd::Shutdown();
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
