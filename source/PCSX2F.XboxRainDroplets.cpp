#include "xrd11.h"
#define FUSIONDXHOOK_INCLUDE_D3D8     0
#define FUSIONDXHOOK_INCLUDE_D3D9     0
#define FUSIONDXHOOK_INCLUDE_D3D10    0
#define FUSIONDXHOOK_INCLUDE_D3D10_1  0
#define FUSIONDXHOOK_INCLUDE_D3D11    1
#define FUSIONDXHOOK_INCLUDE_D3D12    1
#define FUSIONDXHOOK_INCLUDE_OPENGL   0
#define FUSIONDXHOOK_INCLUDE_VULKAN   0
#define FUSIONDXHOOK_USE_SAFETYHOOK   1
#define DELAYED_BIND 2000ms
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

void RenderDroplets()
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

    if (VMStateIsRunning && VMStateIsRunning() && GetEEMainMemoryStart())
    {
        if (pXRData)
        {
            std::string_view buf((char*)pXRData);
            if (!buf.starts_with("X"))
            {
                if (pXRData->Enabled(0))
                {
                    WaterDrops::ms_rainIntensity = pXRData->GetRainIntensity(0);

                    WaterDrops::bRadial = false;

                    WaterDrops::up = pXRData->GetUp(0);
                    WaterDrops::at = pXRData->GetAt(0);
                    WaterDrops::right = pXRData->GetRight(0);
                    WaterDrops::pos = pXRData->GetPos(0);

                    pXRData->RegisterSplash();
                    pXRData->FillScreen();
                    pXRData->FillScreenMoving();

                    WaterDrops::Process();
                    WaterDrops::Render();
                }
            }
            else
                memset(pXRData, 0, 255);
        }
        else
        {
            auto sym = GetPluginSymbolAddr("PLUGINS/PCSX2F.XboxRainDroplets.elf", "XboxRainDropletsData");
            if (sym)
                pXRData = (XRData*)(GetEEMainMemoryStart() + sym);
        }
    }
    else
        pXRData = nullptr;
}

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
            #ifdef SIRE_INCLUDE_DX11ON12
            if (!d3d11on12::isD3D11on12)
            #endif
            {
                Sire::Init(Sire::SIRE_RENDERER_DX11, pSwapChain);
                RenderDroplets();
            }
        };

        FusionDxHook::D3D11::onAfterResizeEvent += [](IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
        {
            
        };

        FusionDxHook::D3D11::onShutdownEvent += []()
        {
            #ifdef SIRE_INCLUDE_DX11ON12
            if (!d3d11on12::isD3D11on12)
            #endif
            {
                WaterDrops::Shutdown();
                Sire::Shutdown();
            }
        };
        #endif // FUSIONDXHOOK_INCLUDE_D3D11

        // D3D11on12
        #if FUSIONDXHOOK_INCLUDE_D3D12
        FusionDxHook::D3D12::onPresentEvent += [](IDXGISwapChain* pSwapChain)
        {
            Sire::Init(Sire::SIRE_RENDERER_DX11, pSwapChain);
            RenderDroplets();
        };

        #ifdef SIRE_INCLUDE_DX11ON12
        FusionDxHook::D3D12::onExecuteCommandListsEvent += [](ID3D12CommandQueue* pCommandQueue, UINT NumCommandLists, const ID3D12CommandList** ppCommandLists)
        {
            if (pCommandQueue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT)
            {
                Sire::SetCommandQueue(pCommandQueue);
            }
        };
        #endif

        FusionDxHook::D3D12::onBeforeResizeEvent += [](IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
        {
            WaterDrops::Reset();
            Sire::Shutdown();
        };

        FusionDxHook::D3D12::onAfterResizeEvent += [](IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
        {

        };

        FusionDxHook::D3D12::onShutdownEvent += []()
        {
            WaterDrops::Shutdown();
            Sire::Shutdown();
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