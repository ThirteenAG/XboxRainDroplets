#include "xrd11.h"
#define FUSIONDXHOOK_INCLUDE_D3D8     0
#define FUSIONDXHOOK_INCLUDE_D3D9     1
#define FUSIONDXHOOK_INCLUDE_D3D10    0
#define FUSIONDXHOOK_INCLUDE_D3D10_1  0
#define FUSIONDXHOOK_INCLUDE_D3D11    1
#define FUSIONDXHOOK_INCLUDE_D3D12    0
#define FUSIONDXHOOK_INCLUDE_OPENGL   1
#define FUSIONDXHOOK_INCLUDE_VULKAN   1
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
        __try
        {
            if (p_enabled)
                return *(uint32_t*)(ptr + p_enabled);
            else
                return ms_enabled != 0;
        }
        __except ((GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
        {
        }
        return false;
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
            if (FillScreenMoving_amount == 1.0f)
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
    static HWND hWndPPSSPP = NULL;

    if (!hWndPPSSPP)
    {
        auto GetAllWindowsFromProcessID = [](DWORD dwProcessID, std::vector<HWND>& vhWnds)
        {
            HWND hCurWnd = nullptr;
            do
            {
                hCurWnd = FindWindowEx(nullptr, hCurWnd, nullptr, nullptr);
                DWORD checkProcessID = 0;
                GetWindowThreadProcessId(hCurWnd, &checkProcessID);
                if (checkProcessID == dwProcessID)
                {
                    vhWnds.push_back(hCurWnd);
                }
            } while (hCurWnd != nullptr);
        };

        std::vector<HWND> vhWnds;
        GetAllWindowsFromProcessID(GetCurrentProcessId(), vhWnds);
        auto it = std::find_if(vhWnds.begin(), vhWnds.end(), [](auto hwnd) {
            std::wstring PPSSPPWnd(L"PPSSPPWnd");
            std::wstring title(GetWindowTextLength(hwnd) + 1, L'\0');
            GetClassNameW(hwnd, &title[0], title.size());
            title.resize(PPSSPPWnd.size());
            return title == PPSSPPWnd;
        });

        if (it != std::end(vhWnds))
            hWndPPSSPP = *it;
    }
    else
    {
        const UINT WM_USER_GET_BASE_POINTER = WM_APP + 0x3118;  // 0xB118
        const UINT WM_USER_GET_EMULATION_STATE = WM_APP + 0x3119;  // 0xB119

        static XRData* pXRData = nullptr;
        static bool bRefreshXRData = false;

        DWORD_PTR dwResult = 0;
        SendMessageTimeout(hWndPPSSPP, WM_USER_GET_EMULATION_STATE, 0, 0, SMTO_NORMAL, 10L, &dwResult);
        if (dwResult == 1)
        {
            enum
            {
                lo,   // Lower 32 bits of pointer to the base of emulated memory
                hi,   // Upper 32 bits of pointer to the base of emulated memory
                p_lo, // Lower 32 bits of pointer to the pointer to the base of emulated memory
                p_hi, // Upper 32 bits of pointer to the pointer to the base of emulated memory
            };

            DWORD_PTR high, low = 0;
            auto f1 = SendMessageTimeout(hWndPPSSPP, WM_USER_GET_BASE_POINTER, 0, hi, SMTO_NORMAL, 10L, &high);
            auto f2 = SendMessageTimeout(hWndPPSSPP, WM_USER_GET_BASE_POINTER, 0, lo, SMTO_NORMAL, 10L, &low);
            uint64_t ptr = (uint64_t(high) << 32 | low); // +0x08804000;

            if (SUCCEEDED(f1) && SUCCEEDED(f2) && ptr)
            {
                if (pXRData && !bRefreshXRData)
                {
                    if (pXRData->Enabled(ptr))
                    {
                        WaterDrops::ms_rainIntensity = pXRData->GetRainIntensity(ptr);

                        WaterDrops::bRadial = false;

                        WaterDrops::up = pXRData->GetUp(ptr);
                        WaterDrops::at = pXRData->GetAt(ptr);
                        WaterDrops::right = pXRData->GetRight(ptr);
                        WaterDrops::pos = pXRData->GetPos(ptr);

                        pXRData->RegisterSplash();
                        pXRData->FillScreen();
                        pXRData->FillScreenMoving();

                        WaterDrops::Process();
                        WaterDrops::Render();
                    }
                }
                else
                {
                    MEMORY_BASIC_INFORMATION MemoryInf;
                    if (VirtualQuery((LPCVOID)(ptr + 0x08804000), &MemoryInf, sizeof(MemoryInf)) != 0)
                    {
                        auto mem = hook::range_pattern((uintptr_t)MemoryInf.BaseAddress, (uintptr_t)MemoryInf.BaseAddress + MemoryInf.RegionSize, "58 42 4F 58 52 41 49 4E 44 52 4F 50 4C 45 54 53 44 41 54 41");
                        if (!mem.empty())
                        {
                            pXRData = (XRData*)mem.get_first();
                            memset(pXRData, 0, 255);
                        }
                    }
                    bRefreshXRData = false;
                }
            }
            else
                bRefreshXRData = true;
        }
        else
            bRefreshXRData = true;
    }
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

        #if FUSIONDXHOOK_INCLUDE_D3D9
        FusionDxHook::D3D9::onEndSceneEvent += [](LPDIRECT3DDEVICE9 pDevice)
        {
            Sire::Init(Sire::SIRE_RENDERER_DX9, pDevice);

            RenderDroplets();
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

        #if FUSIONDXHOOK_INCLUDE_D3D11
        FusionDxHook::D3D11::onPresentEvent += [](IDXGISwapChain* pSwapChain)
        {
            Sire::Init(Sire::SIRE_RENDERER_DX11, pSwapChain);

            RenderDroplets();
        };

        FusionDxHook::D3D11::onShutdownEvent += []()
        {
            WaterDrops::Shutdown();
            Sire::Shutdown();
        };
        #endif // FUSIONDXHOOK_INCLUDE_D3D11

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