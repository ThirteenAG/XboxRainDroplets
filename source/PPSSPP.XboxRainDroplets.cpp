// ---------------------------------------------------------------------------
// Xbox Rain Droplets for PPSSPP.
//
// There are two ways in, and the plugin takes whichever the emulator it was
// loaded into offers:
//
//   the frame point   A PPSSPP of the version this plugin was written for hands an
//                     application of its own process the drawing of its backend
//                     where the frame of a game is in between its world and its
//                     UI, see PPSSPP_RegisterBeforeUIDrawDraw in GPUCommon.h of
//                     the emulator. The drops are drawn right there, with that
//                     drawing (the Draw::DrawContext of Common/GPU/thin3d.h, see
//                     source/xrd/xrdrender.thin3d.h), so they land under the UI of
//                     the game on every backend the emulator has, and no hook of
//                     any API is involved.
//
//   hooks             An older PPSSPP has no such drawing to hand over, so the
//                     APIs are hooked instead and the drops are drawn at the
//                     present call of the backend, see the second half of this
//                     file, which puts them over everything the game drew. An
//                     emulator that says where it is between the world of a frame
//                     and its UI is the only thing that can put them under it:
//                     a report of a guest names a moment of the game, and the
//                     emulator runs the commands of a game on its own schedule,
//                     so a plugin that watches the game from the outside cannot
//                     tell where the emulator is in the frame it is drawing.
//
// Whichever way it goes, the two halves that meet here are the same:
//
//   the guest plugin (WidescreenFixesPack, source/PPSSPP.XboxRainDroplets/main.c)
//   runs inside the game, reports where its frame is between its world and its UI
//   and publishes the state of the rain in its own memory,
//
//   this plugin reads that state, fills the frame in, and everything else is the
//   effect that also runs on a PC game (source/xrd).
//
// Nothing of this talks to the window of the emulator from the frame of a game.
// That would be the easy way to the memory of the guest, and it is what this
// plugin used to do - but the window of the emulator answers on the thread of its
// UI, and that thread is waiting for the very frame that is being drawn at the
// moment the frame point is reported. Asking it from there gets no answer, and
// the two of them wait for each other. The memory of the guest and the address
// the guest reported from are handed over with the frame instead, see
// PPSSPPBeforeUIDrawTarget. The hooked way asks the window at the present call,
// which is between frames and where it always has been asked from.
// ---------------------------------------------------------------------------

// The drawing of the emulator, and every API the hooked way can run into.
#define XRD_ENABLE_D3D11
#define XRD_ENABLE_OPENGL
#define XRD_ENABLE_VULKAN
#define XRD_ENABLE_THIN3D

#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif

// the Vulkan declarations come first, the hook library then stays out of the way
#include <vulkan/vulkan.h>

#include "xrd/xrd.h"

#define FUSIONDXHOOK_INCLUDE_D3D8     0
#define FUSIONDXHOOK_INCLUDE_D3D9     0
#define FUSIONDXHOOK_INCLUDE_D3D10    0
#define FUSIONDXHOOK_INCLUDE_D3D10_1  0
#define FUSIONDXHOOK_INCLUDE_D3D11    1
#define FUSIONDXHOOK_INCLUDE_D3D12    0
#define FUSIONDXHOOK_INCLUDE_OPENGL   1
// The Vulkan entry points are hooked through the loader's own lookup functions here, which is
// what reaches an emulator that never calls the exported ones, see InstallVulkanHooks. The hook
// library's own Vulkan section would be in the way of that.
#define FUSIONDXHOOK_INCLUDE_VULKAN   0
#define FUSIONDXHOOK_USE_SAFETYHOOK   1
#include "FusionDxHook.h"

#include <atomic>
#include <chrono>
#include <thread>

#pragma pack(push, 1)
struct XRData
{
    char signature[21]; // "XBOXRAINDROPLETSDATA"
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
        } __except ((GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
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

// The blocks of the guest plugin this reads, see its source/PPSSPP.XboxRainDroplets/main.c: the
// block of the report of the point of the frame between the world and the UI, and the data of the
// drops the block publishes. The block of the report is found by the address the guest reported
// from, which is the counter inside it, or by a search of the module of the guest plugin, and the
// data of the drops by the address the block publishes, so nothing of the data has to be alive to
// be found. The markers of both are the signature of the other one.
static constexpr char XR_DATA_MARKER[] = "XBOXRAINDROPLETSDATA";
static constexpr char BEFORE_UI_MARKER[] = "PPSSPPBEFOREUIDATA";

// The block of the report of the guest plugin, see its source/PPSSPP.XboxRainDroplets/main.c:
// the signature is the marker above, the counter right behind it counts the reports of the point
// of the frame of the game (the emulator counts the reports it takes into it, see
// PPSSPP_DEVCTL__BEFORE_UI_DRAW of the emulator), and the address right behind that one is where
// the data of the drops is in the memory of the guest. The block is found by the address the
// guest reported from, which is the counter inside it, or by a search of the module of the guest.
struct BeforeUIBlock
{
    char signature[20];
    volatile uint32_t tick;
    uint32_t p_dropsData;
};

// ---------------------------------------------------------------------------
// the state of the guest, which both ways read the same
// ---------------------------------------------------------------------------

// Reads the state of the guest out of the block it publishes and hands it to the effect. True
// when the effect has something to draw.
static bool ReadGuestData(uint64_t memory, uint32_t blockAddress)
{
    if (!memory || !blockAddress)
        return false;

    const uint8_t* pMemory = (const uint8_t*)memory;
    const BeforeUIBlock* pBlock = (const BeforeUIBlock*)(pMemory + blockAddress);

    if (memcmp(pBlock->signature, BEFORE_UI_MARKER, sizeof(BEFORE_UI_MARKER) - 1) != 0)
        return false;

    const uint32_t dropsDataAddress = pBlock->p_dropsData;

    if (!dropsDataAddress)
        return false;

    XRData* pXRData = (XRData*)(pMemory + dropsDataAddress);

    if (memcmp(pXRData->signature, XR_DATA_MARKER, sizeof(XR_DATA_MARKER) - 1) != 0)
        return false;

    // The first time the data of this game is seen, the holds of the drops of this plugin are
    // wiped: they are the ones of the guest, and the guest fills its own in from the moment its
    // game is recognised.
    static uint32_t initialisedAddress = 0;

    if (dropsDataAddress != initialisedAddress)
    {
        initialisedAddress = dropsDataAddress;
        memset((char*)pXRData + offsetof(XRData, p_enabled), 0, sizeof(XRData) - offsetof(XRData, p_enabled));
    }

    if (!pXRData->Enabled(memory))
        return false;

    WaterDrops::ms_rainIntensity = pXRData->GetRainIntensity(memory);

    WaterDrops::bRadial = false;

    WaterDrops::up = pXRData->GetUp(memory);
    WaterDrops::at = pXRData->GetAt(memory);
    WaterDrops::right = pXRData->GetRight(memory);
    WaterDrops::pos = pXRData->GetPos(memory);

    pXRData->RegisterSplash();
    pXRData->FillScreen();
    pXRData->FillScreenMoving();

    return true;
}

// Moves the drops along and draws them, which is one step of the frame of the effect.
static void DrawDroplets()
{
    WaterDrops::Process();
    WaterDrops::Render();
}

// ---------------------------------------------------------------------------
// the frame point, for an emulator that hands one over
// ---------------------------------------------------------------------------

// The frame of a game as that emulator hands it over at the point between its world and its UI,
// see PPSSPPBeforeUIDrawTarget in GPUCommon.h of the emulator. The pointers are handed over as
// void* so that the emulator does not have to know what they are, and this is what they are.
typedef struct PPSSPPBeforeUIDrawTarget
{
    Draw::DrawContext* draw;
    Draw::Framebuffer* frame;
    int width;
    int height;
    int shownWidth;
    int shownHeight;
    void* memory;
    uint32_t reportAddress;
    void (*release)(void* object);
} PPSSPPBeforeUIDrawTarget;

typedef void (*RegisterBeforeUIDrawDrawFn)(void (*fn)(const PPSSPPBeforeUIDrawTarget* target));

// Every frame of the game, once, at the point the guest reported. The drops are drawn right here,
// so they are part of the frame of the game and everything the game draws after this point (its
// UI, most of all) ends up on top of them.
static void BeforeUIDrawDraw(const PPSSPPBeforeUIDrawTarget* pTarget)
{
    if (!pTarget || !pTarget->draw || !pTarget->frame)
        return;

    Xrd::Thin3DTarget target{};
    target.pDrawing = pTarget->draw;
    target.pFrame = pTarget->frame;
    target.width = pTarget->width;
    target.height = pTarget->height;
    target.release = pTarget->release;

    // The backend is built once and handed the frame of every frame after that, see Xrd::Init
    // and Thin3DBackend::UpdateNative.
    if (!Xrd::Init(Xrd::RENDERER_THIN3D, &target))
        return;

    // A drop has to be round as it is seen, not as it is drawn, and the frame of the game is
    // stretched from the buffer it is drawn into to the window it is shown in unless the two have
    // the same proportion: the effect asks for the size of that window, see
    // WaterDrops::ComputeXScale. Zero means nothing is stretched.
    WaterDrops::ms_screenWidth = pTarget->shownWidth;
    WaterDrops::ms_screenHeight = pTarget->shownHeight;

    // The effect has to be up before the state of the guest is read into it: a fill of the screen
    // the guest asks for is dropped while the effect has no size of its own yet, see
    // WaterDrops::FillScreen.
    WaterDrops::Process();

    const uint32_t blockAddress = pTarget->reportAddress
        ? pTarget->reportAddress - (uint32_t)offsetof(BeforeUIBlock, tick)
        : 0;

    const bool dataRead = ReadGuestData((uint64_t)(uintptr_t)pTarget->memory, blockAddress);

    if (dataRead)
        WaterDrops::Render();
}

// ---------------------------------------------------------------------------
// the hooks, for an emulator that hands nothing over
// ---------------------------------------------------------------------------

// The window of the emulator answers where the memory of the guest is and where the plugin of the
// guest is loaded in it, and this is asked at the present call: that is between two frames, which
// is the only moment it can be answered, see the note at the top of this file.
static HWND FindPPSSPPWindow()
{
    // The handle is kept: this is asked on every frame of every hook.
    static HWND hCached = nullptr;

    if (hCached && IsWindow(hCached))
        return hCached;

    HWND hWnd = nullptr;

    do
    {
        hWnd = FindWindowEx(nullptr, hWnd, nullptr, nullptr);
        DWORD processId = 0;
        GetWindowThreadProcessId(hWnd, &processId);

        if (processId != GetCurrentProcessId())
            continue;

        wchar_t name[64] = {};
        GetClassNameW(hWnd, name, 64);

        if (wcscmp(name, L"PPSSPPWnd") == 0)
        {
            hCached = hWnd;
            return hWnd;
        }
    } while (hWnd != nullptr);

    return nullptr;
}

// What the window was asked for. The interval between frames is where this changes, so it is kept
// between calls and only read by the drawing of a frame.
struct GuestAddresses
{
    uint64_t memory = 0;        // the base of the memory of the guest
    uint32_t moduleAddress = 0; // the plugin of the guest, in that memory
    uint32_t moduleSize = 0;
    uint32_t blockAddress = 0;  // the block of the report of that plugin
};

static GuestAddresses gGuest;

// Asks the window of the emulator for the addresses of the guest, and searches the plugin of the
// guest for the block of its report once. False while the game is not there yet.
static bool LookUpGuest()
{
    const UINT WM_USER_GET_BASE_POINTER = WM_APP + 0x3118;  // 0xB118
    const UINT WM_USER_GET_EMULATION_STATE = WM_APP + 0x3119;  // 0xB119
    const UINT WM_USER_GET_MODULE_INFO = WM_APP + 0x311B;  // 0xB11B

    static HWND hWnd = nullptr;
    static int searches = 0;
    static uint32_t framesSinceLookUp = 0;

    // Everything the window knows is asked once and then only every so many frames: a game can be
    // started again, and that is a new plugin of the guest in the memory of the emulator, but
    // asking the window four times on every frame is nothing a frame of a game should do.
    if (gGuest.blockAddress && (framesSinceLookUp++ % 60) != 0)
        return true;

    if (!hWnd || !IsWindow(hWnd))
        hWnd = FindPPSSPPWindow();

    if (!hWnd)
        return false;

    DWORD_PTR state = 0;
    SendMessageTimeout(hWnd, WM_USER_GET_EMULATION_STATE, 0, 0, SMTO_NORMAL, 10L, &state);

    if (state != 1)
        return false;

    enum
    {
        GUEST_POINTER_LOW = 0,   // Lower 32 bits of pointer to the base of emulated memory
        GUEST_POINTER_HIGH = 1,  // Upper 32 bits of pointer to the base of emulated memory
    };

    DWORD_PTR high = 0;
    DWORD_PTR low = 0;
    const auto gotHigh = SendMessageTimeout(hWnd, WM_USER_GET_BASE_POINTER, 0, GUEST_POINTER_HIGH, SMTO_NORMAL, 10L, &high);
    const auto gotLow = SendMessageTimeout(hWnd, WM_USER_GET_BASE_POINTER, 0, GUEST_POINTER_LOW, SMTO_NORMAL, 10L, &low);

    if (!gotHigh || !gotLow || !high && !low)
        return false;

    gGuest.memory = ((uint64_t)high << 32 | (uint64_t)low);

    // Get module info for "PPSSPP.XboxRainDroplets" (all info packed (lParam=3))
    DWORD_PTR moduleResult = 0;
    SendMessageTimeout(hWnd, WM_USER_GET_MODULE_INFO, (WPARAM)"PPSSPP.XboxRainDroplets", 3, SMTO_NORMAL, 10L, &moduleResult);

    const uint64_t moduleInfo = (uint64_t)moduleResult;
    gGuest.moduleAddress = (uint32_t)(moduleInfo & 0xFFFFFFFF);
    gGuest.moduleSize = (uint32_t)((moduleInfo >> 32) & 0x7FFFFFFF);

    if ((moduleInfo >> 63) == 0 || !gGuest.moduleAddress || !gGuest.moduleSize)
        return false;

    // The block of the report of the guest plugin, which is in it from the moment it is loaded.
    // The search is only worth making once, and only every so many frames while it is empty: the
    // memory of the guest is large and this walks all of it.
    if (!gGuest.blockAddress && (searches++ % 30) == 0)
    {
        const uintptr_t start = (uintptr_t)(gGuest.memory + gGuest.moduleAddress);
        auto pattern = hook::range_pattern(start, start + gGuest.moduleSize, pattern_str(to_bytes(BEFORE_UI_MARKER)));

        if (!pattern.empty())
            gGuest.blockAddress = (uint32_t)((uintptr_t)pattern.get_first() - gGuest.memory);
    }

    return gGuest.blockAddress != 0;
}

// One step of the frame of the effect with the state of the guest read into it, which is the
// order the frame point uses as well: the effect has to be up before the state is read, because
// a fill of the screen the guest asks for is dropped while the effect has no size of its own yet,
// see WaterDrops::FillScreen.
static bool ProcessWithGuestState()
{
    WaterDrops::Process();

    if (!LookUpGuest())
        return false;

    return ReadGuestData(gGuest.memory, gGuest.blockAddress);
}

// ---------------------------------------------------------------------------
// Vulkan: the entry points the emulator takes are the loader's to hand out
// ---------------------------------------------------------------------------
//
// An emulator asks the loader of Vulkan for the functions of the device with
// vkGetInstanceProcAddr and vkGetDeviceProcAddr instead of calling the ones the loader exports,
// so a hook on the exported entry points is never called and nothing of the device or of the
// present call is ever seen. These two are therefore hooked instead, and the entry points that
// are asked for are handed over as the ones of this plugin.

static Xrd::VulkanPresent gVulkanPresent;

static SafetyHookInline* gpGetInstanceProcAddrHook = nullptr;
static SafetyHookInline* gpGetDeviceProcAddrHook = nullptr;

static PFN_vkCreateDevice gpVulkanCreateDevice = nullptr;
static PFN_vkCreateSwapchainKHR gpVulkanCreateSwapchainKHR = nullptr;
static PFN_vkQueuePresentKHR gpVulkanQueuePresentKHR = nullptr;

static VkResult VKAPI_CALL HookedVkCreateDevice(VkPhysicalDevice gpu, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice)
{
    if (!gpVulkanCreateDevice)
        return VK_ERROR_INITIALIZATION_FAILED;

    const VkResult result = gpVulkanCreateDevice(gpu, pCreateInfo, pAllocator, pDevice);

    if (result == VK_SUCCESS && pDevice && *pDevice)
    {
        gVulkanPresent.OnCreateDevice(gpu, pCreateInfo, pDevice);
    }

    return result;
}

static VkResult VKAPI_CALL HookedVkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain)
{
    gVulkanPresent.OnCreateSwapchain(device, pCreateInfo);

    if (!gpVulkanCreateSwapchainKHR)
        return VK_ERROR_DEVICE_LOST;

    return gpVulkanCreateSwapchainKHR(device, pCreateInfo, pAllocator, pSwapchain);
}

static VkResult VKAPI_CALL HookedVkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo)
{
    if (gpVulkanQueuePresentKHR)
    {
        // The drops are drawn into the image that is presented, and the backend of the effect
        // does that with its own submission, which runs before the present call below: it is over
        // everything the game drew, which is the best an API without a frame point of its own can
        // do.
        if (gVulkanPresent.Prepare(queue, pPresentInfo))
        {
            Xrd::SetTargetState(Xrd::TARGET_STATE_PRESENT);

            if (ProcessWithGuestState())
                WaterDrops::Render();
        }

        return gpVulkanQueuePresentKHR(queue, pPresentInfo);
    }

    return VK_ERROR_DEVICE_LOST;
}

static PFN_vkVoidFunction VKAPI_CALL HookedGetInstanceProcAddr(VkInstance instance, const char* pName)
{
    PFN_vkVoidFunction pFunction = gpGetInstanceProcAddrHook->unsafe_stdcall<PFN_vkVoidFunction>(instance, pName);

    if (!pName || !pFunction)
        return pFunction;

    if (strcmp(pName, "vkCreateDevice") == 0)
    {
        gpVulkanCreateDevice = (PFN_vkCreateDevice)pFunction;
        return (PFN_vkVoidFunction)HookedVkCreateDevice;
    }

    if (strcmp(pName, "vkCreateSwapchainKHR") == 0)
    {
        gpVulkanCreateSwapchainKHR = (PFN_vkCreateSwapchainKHR)pFunction;
        return (PFN_vkVoidFunction)HookedVkCreateSwapchainKHR;
    }

    if (strcmp(pName, "vkQueuePresentKHR") == 0)
    {
        gpVulkanQueuePresentKHR = (PFN_vkQueuePresentKHR)pFunction;
        return (PFN_vkVoidFunction)HookedVkQueuePresentKHR;
    }

    return pFunction;
}

static PFN_vkVoidFunction VKAPI_CALL HookedGetDeviceProcAddr(VkDevice device, const char* pName)
{
    PFN_vkVoidFunction pFunction = gpGetDeviceProcAddrHook->unsafe_stdcall<PFN_vkVoidFunction>(device, pName);

    if (!pName || !pFunction)
        return pFunction;

    if (strcmp(pName, "vkCreateSwapchainKHR") == 0)
    {
        if (!gpVulkanCreateSwapchainKHR)
            gpVulkanCreateSwapchainKHR = (PFN_vkCreateSwapchainKHR)pFunction;

        return (PFN_vkVoidFunction)HookedVkCreateSwapchainKHR;
    }

    if (strcmp(pName, "vkQueuePresentKHR") == 0)
    {
        if (!gpVulkanQueuePresentKHR)
            gpVulkanQueuePresentKHR = (PFN_vkQueuePresentKHR)pFunction;

        return (PFN_vkVoidFunction)HookedVkQueuePresentKHR;
    }

    return pFunction;
}

// The two lookup functions of the loader, which the emulator asks for the functions above. They
// have to be hooked before the emulator asks, which is right after it loaded the loader: the
// watcher tries from the moment this plugin is loaded, and keeps trying, because the loader is
// loaded and unloaded again and again (an emulator asks it whether Vulkan is there at all, and
// frees it again when it is not going to use it).
static std::atomic<bool> gVulkanWatcherStop{ false };

static void InstallVulkanHooks()
{
    HMODULE hVulkan = GetModuleHandleW(L"vulkan-1.dll");

    if (!hVulkan)
        return;

    void* pGetInstanceProcAddr = (void*)GetProcAddress(hVulkan, "vkGetInstanceProcAddr");
    void* pGetDeviceProcAddr = (void*)GetProcAddress(hVulkan, "vkGetDeviceProcAddr");

    if (!pGetInstanceProcAddr || !pGetDeviceProcAddr)
        return;

    // The library hooks an image that has been loaded again as well, so this may be called as
    // often as the watcher wants, see CreateHook of the hook library.
    gpGetInstanceProcAddrHook = FusionDxHook::InstallAddressHook(pGetInstanceProcAddr, (void*)HookedGetInstanceProcAddr, gpGetInstanceProcAddrHook);
    gpGetDeviceProcAddrHook = FusionDxHook::InstallAddressHook(pGetDeviceProcAddr, (void*)HookedGetDeviceProcAddr, gpGetDeviceProcAddrHook);
}

static void WatchForVulkan()
{
    // The thread is started from the load of this plugin, which is before the emulator has done
    // anything with Vulkan, and the first look waits a moment so that this plugin is loaded
    // completely. The loader of Vulkan is looked for until the process ends, because an emulator
    // frees it again and a new load is a new image.
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    while (!gVulkanWatcherStop)
    {
        InstallVulkanHooks();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

// ---------------------------------------------------------------------------
// the way in
// ---------------------------------------------------------------------------

// How far the frame of the game is, for the log, whoever drew the drops.
static void SetScreenSizeFromWindow()
{
    HWND hWnd = FindPPSSPPWindow();

    if (!hWnd)
        return;

    RECT rect = {};
    GetClientRect(hWnd, &rect);

    // A drop has to be round as it is seen, and the frame of the game is stretched to the window
    // it is shown in, see WaterDrops::ComputeXScale.
    WaterDrops::ms_screenWidth = rect.right - rect.left;
    WaterDrops::ms_screenHeight = rect.bottom - rect.top;
}

static void InitializeHookedWay()
{
    FusionDxHook::Init();

    #if FUSIONDXHOOK_INCLUDE_D3D11
    FusionDxHook::D3D11::onPresentEvent += [](IDXGISwapChain* pSwapChain)
    {
        Xrd::Init(Xrd::RENDERER_D3D11, pSwapChain);

        // The frame of the game, and the UI of it, are over by the time it is presented: this way
        // draws over both of them. Filling the frame in behind the UI instead needs the emulator
        // to say where it is between the two, which is what the frame point is for, see the note
        // at the top of this file.
        Xrd::SetTargetState(Xrd::TARGET_STATE_PRESENT);

        SetScreenSizeFromWindow();

        if (ProcessWithGuestState())
            WaterDrops::Render();
    };

    FusionDxHook::D3D11::onShutdownEvent += []()
    {
        WaterDrops::Shutdown();
        Xrd::Shutdown();
    };
    #endif // FUSIONDXHOOK_INCLUDE_D3D11

    #if FUSIONDXHOOK_INCLUDE_OPENGL
    FusionDxHook::OPENGL::onSwapBuffersEvent += [](HDC hDC)
    {
        Xrd::Init(Xrd::RENDERER_OPENGL, hDC);

        // The window is presented with its device context, the frame is whatever is in the
        // framebuffer at that moment and it is read back from there. OpenGL keeps the frame of a
        // game in a recording that another thread runs, so a drawing of a plugin cannot be put
        // between the world and the UI of it: the drops of this way are over everything the game
        // drew.
        Xrd::SetTargetState(Xrd::TARGET_STATE_PRESENT);

        // The frame is read out of the window here, and a window of OpenGL is the other way up
        // than the frame a game drew: without this a drop would move up the screen instead of
        // down it, see SetPresentSceneFlipY of the backend.
        Xrd::SetPresentSceneFlipY(true);

        SetScreenSizeFromWindow();

        if (ProcessWithGuestState())
            WaterDrops::Render();
    };

    FusionDxHook::OPENGL::onShutdownEvent += []()
    {
        WaterDrops::Shutdown();
        Xrd::Shutdown();
    };
    #endif // FUSIONDXHOOK_INCLUDE_OPENGL

    // The loader of Vulkan is loaded and unloaded in the middle of a session, and the hooks of
    // its lookup functions have to be installed on every load, see WatchForVulkan.
    try
    {
        std::thread(WatchForVulkan).detach();
    }
    catch (...)
    {
        // The watcher was not started: the drops are drawn at the present call unless the loader
        // of Vulkan is loaded before the emulator takes its entry points.
    }
}

extern "C" __declspec(dllexport) void InitializeASI()
{
    static std::once_flag flag;
    std::call_once(flag, []()
    {
        // is this an emulator that hands the drawing of a frame over?
        HMODULE hEmulator = GetModuleHandleW(nullptr);
        auto registerBeforeUIDrawDraw = hEmulator
            ? (RegisterBeforeUIDrawDrawFn)GetProcAddress(hEmulator, "PPSSPP_RegisterBeforeUIDrawDraw")
            : nullptr;

        if (registerBeforeUIDrawDraw)
        {
            registerBeforeUIDrawDraw(BeforeUIDrawDraw);
            return;
        }

        InitializeHookedWay();
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
            // The emulator can be on its way out already, in which case the drawing this plugin
            // was built on is gone: everything of it is only forgotten then, see Xrd::Detach.
            Xrd::Detach();
            Xrd::Shutdown();
            break;
    }

    return TRUE;
}
