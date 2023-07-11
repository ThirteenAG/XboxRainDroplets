#include "xrd11.h"
#define FUSIONDXHOOK_INCLUDE_D3D8     1
#define FUSIONDXHOOK_INCLUDE_D3D9     1
#define FUSIONDXHOOK_INCLUDE_D3D10    1
#define FUSIONDXHOOK_INCLUDE_D3D10_1  1
#define FUSIONDXHOOK_INCLUDE_D3D11    1
#define FUSIONDXHOOK_INCLUDE_D3D12    1
#define FUSIONDXHOOK_INCLUDE_OPENGL   1
#define FUSIONDXHOOK_INCLUDE_VULKAN   1
#define FUSIONDXHOOK_USE_MINHOOK      1
#define DELAYED_BIND 2000ms
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

        #if FUSIONDXHOOK_INCLUDE_D3D8
        FusionDxHook::D3D8::onPresentEvent += [](D3D8_LPDIRECT3DDEVICE8 pDevice)
        {
            // reinterpret_cast<LPDIRECT3DDEVICE8>(pDevice);
        };

        FusionDxHook::D3D8::onResetEvent += [](D3D8_LPDIRECT3DDEVICE8 pDevice)
        {
            // reinterpret_cast<LPDIRECT3DDEVICE8>(pDevice);
        };

        FusionDxHook::D3D8::onShutdownEvent += []()
        {

        };
        #endif // FUSIONDXHOOK_INCLUDE_D3D8

        #if FUSIONDXHOOK_INCLUDE_D3D9
        FusionDxHook::D3D9::onEndSceneEvent += [](LPDIRECT3DDEVICE9 pDevice)
        {
            Sire::Init(Sire::SIRE_RENDERER_DX9, pDevice);

            WaterDrops::Process();
            WaterDrops::Render();
        };

        FusionDxHook::D3D9::onResetEvent += [](LPDIRECT3DDEVICE9 pDevice)
        {
            WaterDrops::Reset();
        };

        FusionDxHook::D3D9::onShutdownEvent += []()
        {
            WaterDrops::Shutdown();
            Sire::Shutdown();
        };
        #endif // FUSIONDXHOOK_INCLUDE_D3D9

        #if FUSIONDXHOOK_INCLUDE_D3D10
        FusionDxHook::D3D10::onPresentEvent += [](IDXGISwapChain* pSwapChain)
        {
            Sire::Init(Sire::SIRE_RENDERER_DX10, pSwapChain);

            WaterDrops::Process();
            WaterDrops::Render();
        };

        FusionDxHook::D3D10::onAfterResizeEvent += [](IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
        {

        };

        FusionDxHook::D3D10::onShutdownEvent += []()
        {
            WaterDrops::Shutdown();
            Sire::Shutdown();
        };
        #endif // FUSIONDXHOOK_INCLUDE_D3D10

        #if FUSIONDXHOOK_INCLUDE_D3D10_1
        FusionDxHook::D3D10_1::onPresentEvent += [](IDXGISwapChain* pSwapChain)
        {
            Sire::Init(Sire::SIRE_RENDERER_DX10, pSwapChain);

            WaterDrops::Process();
            WaterDrops::Render();
        };

        FusionDxHook::D3D10_1::onAfterResizeEvent += [](IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
        {

        };
        #endif // FUSIONDXHOOK_INCLUDE_D3D10_1

        #if FUSIONDXHOOK_INCLUDE_D3D11
        FusionDxHook::D3D11::onPresentEvent += [](IDXGISwapChain* pSwapChain)
        {
            Sire::Init(Sire::SIRE_RENDERER_DX11, pSwapChain);

            WaterDrops::Process();
            WaterDrops::Render();
        };

        FusionDxHook::D3D11::onAfterResizeEvent += [](IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
        {

        };

        FusionDxHook::D3D11::onShutdownEvent += []()
        {
            WaterDrops::Shutdown();
            Sire::Shutdown();
        };
        #endif // FUSIONDXHOOK_INCLUDE_D3D11

        #if FUSIONDXHOOK_INCLUDE_D3D12
        FusionDxHook::D3D12::onPresentEvent += [](IDXGISwapChain*)
        {

        };

        FusionDxHook::D3D12::onBeforeResizeEvent += [](IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
        {

        };

        FusionDxHook::D3D12::onAfterResizeEvent += [](IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
        {

        };
        #endif // FUSIONDXHOOK_INCLUDE_D3D12

        #if FUSIONDXHOOK_INCLUDE_OPENGL
        FusionDxHook::OPENGL::onSwapBuffersEvent += [](HDC hDC)
        {

        };

        FusionDxHook::OPENGL::onShutdownEvent += []() {
            WaterDrops::Shutdown();
            Sire::Shutdown();
        };
        #endif // FUSIONDXHOOK_INCLUDE_OPENGL

        #if FUSIONDXHOOK_INCLUDE_VULKAN
        FusionDxHook::VULKAN::onvkCreateDeviceEvent += [](VkPhysicalDevice gpu, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice)
        {

        };

        FusionDxHook::VULKAN::onVkQueuePresentKHREvent += [](VkQueue queue, const VkPresentInfoKHR* pPresentInfo)
        {

        };

        FusionDxHook::VULKAN::onShutdownEvent += []() {
            WaterDrops::Shutdown();
            Sire::Shutdown();
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