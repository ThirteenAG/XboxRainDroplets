#define XRD_ENABLE_D3D9
#include "xrd/xrd.h"

IDirect3DDevice9** pDevice = nullptr;
uintptr_t* pCurrentScene = nullptr;
int* pTimeOfDay = nullptr;
uint8_t* pIndoors = nullptr;
float* (__cdecl* pGetTransform)(int) = nullptr;
bool bWorldRendered = false;
bool bCameraValid = false;

void CaptureCamera()
{
    auto scene = *pCurrentScene;
    if (!scene)
        return;
    auto target = *reinterpret_cast<uintptr_t*>(scene + 0x334);
    if (!target || !(*reinterpret_cast<uint32_t*>(target + 0x34) & 4))
        return; // Ignore radar, shadow maps and other offscreen views.

    // geometry_manager::XFORM_VIEW_TO_WORLD. USM is Y-up; droplets are Z-up.
    auto matrix = pGetTransform(1);
    for (size_t i = 0; i < 16; ++i)
        if (!std::isfinite(matrix[i]))
            return;
    // The droplets' lateral motion convention is opposite USM's view-right.
    WaterDrops::right = { -matrix[0], -matrix[2], -matrix[1] };
    WaterDrops::up = { matrix[4], matrix[6], matrix[5] };
    WaterDrops::at = { matrix[8], matrix[10], matrix[9] };
    WaterDrops::pos = { matrix[12], matrix[14], matrix[13] };
    if (!bCameraValid)
    {
        WaterDrops::ms_lastPos = WaterDrops::pos;
        WaterDrops::ms_lastAt = WaterDrops::at;
    }
    bCameraValid = true;
    bWorldRendered = true;
    // Native environment preset and camera region classification. A separate
    // rain-volume/overhang test has not been identified; do not invent one.
    WaterDrops::ms_rainIntensity = *pTimeOfDay == 2 && !*pIndoors ? 1.0f : 0.0f;
}

void (__cdecl* pBeginScene)(int) = nullptr;
void (__cdecl* pEndScene)() = nullptr;
void (__cdecl* pSetClearFlags)(int) = nullptr;
void (__cdecl* pAddCustomNode)(void(__cdecl*)(void*), void*, const uint32_t*) = nullptr;

void __cdecl RenderDrops(void*)
{
    if (!bWorldRendered || !*pDevice)
    {
        WaterDrops::Clear();
        WaterDrops::ms_rainIntensity = 0.0f;
        bCameraValid = false;
        return;
    }
    bWorldRendered = false;
    // Executed by NGL after world rendering, before the UI is composited.
    // The bound scene target is correct here; the final swap-chain copy is later.
    Xrd::Init(XRD_DEVICE_RENDERER, *pDevice);
    WaterDrops::Process();
    WaterDrops::Render();
}

void Init()
{
    WaterDrops::ReadIniSettings();
    WaterDrops::ms_rainIntensity = 0.0f;

    auto pattern = hook::pattern("8B 0D ? ? ? ? 8A 81 ? ? ? ? 84 C0 74 ? E8 ? ? ? ? 84 C0 75 ? 6A"); //0x406516 + 2
    pCurrentScene = *pattern.get_first<uintptr_t*>(2);
    pattern = hook::pattern("8B 0D ? ? ? ? 8B 14 8D ? ? ? ? 52 B9"); //0x54E469 + 2
    pTimeOfDay = *pattern.get_first<int*>(2);
    pattern = hook::pattern("A0 ? ? ? ? 8B 35 ? ? ? ? 83 C4 04 84 C0 74 20"); //0x54E4EF + 1
    pIndoors = *pattern.get_first<uint8_t*>(1);
    pattern = hook::pattern("8B 44 24 04 C1 E0 06 05 ? ? ? ? C3"); //0x515740
    pGetTransform = reinterpret_cast<decltype(pGetTransform)>(pattern.get_first());

    pattern = hook::pattern("8B 4F 50 6A 00 55 81 C1 A0 00 00 00 E8 ? ? ? ? 85 DB"); //0x54E521 + 12
    static auto CameraHook = safetyhook::create_mid(pattern.get_first(12), [](SafetyHookContext&)
    {
        CaptureCamera();
    });

    pattern = hook::pattern("A1 ? ? ? ? 8B 08 50 FF 91 A4 00 00 00"); //0x76E98B + 1
    pDevice = *pattern.get_first<IDirect3DDevice9**>(1);
    pattern = hook::pattern("6A 01 E8 ? ? ? ? 8A 15 ? ? ? ? 33 C0"); //0x52B284 + 2
    pBeginScene = reinterpret_cast<decltype(pBeginScene)>(injector::GetBranchDestination(pattern.get_first(2)).as_int());
    pattern = hook::pattern("E8 ? ? ? ? A0 ? ? ? ? 84 C0 75 2A"); //0x52B2A5
    pEndScene = reinterpret_cast<decltype(pEndScene)>(injector::GetBranchDestination(pattern.get_first()).as_int());
    pattern = hook::pattern("50 E8 ? ? ? ? 83 C4 08 E8 ? ? ? ? A0"); //0x52B29C + 1
    pSetClearFlags = reinterpret_cast<decltype(pSetClearFlags)>(injector::GetBranchDestination(pattern.get_first(1)).as_int());
    pattern = hook::pattern("6A 00 E8 ? ? ? ? A1 ? ? ? ? 83 C0 0F 83 C4 04 83 E0 F0 8D 48 20"); //0x76C3A0
    pAddCustomNode = reinterpret_cast<decltype(pAddCustomNode)>(pattern.get_first());
    // After the first UI scene begins, before any UI geometry is submitted.
    // WFP queues its own pass at 0x52B265;
    pattern = hook::pattern("83 C4 04 8B CE E8 ? ? ? ? E8 ? ? ? ? 6A 01"); //0x52B275
    static auto BeforeUIHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext&)
    {
        pBeginScene(1);
        pSetClearFlags(0);
        const uint32_t sortInfo[2] = { 0, 0 };
        pAddCustomNode(RenderDrops, nullptr, sortInfo);
        pEndScene();
    });

    // Same reset routine as WFP, beyond its entry hook and signature.
    pattern = hook::pattern("E8 ? ? ? ? 39 1D ? ? ? ? 74 08 89 1D"); //0x76E821
    static auto ResetHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext&)
    {
        WaterDrops::Reset();
        bWorldRendered = false;
        bCameraValid = false;
    });
}

extern "C" __declspec(dllexport) void InitializeASI()
{
    std::call_once(CallbackHandler::flag, []()
    {
        CallbackHandler::RegisterCallback(Init);
    });
}

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH && !IsUALPresent())
        InitializeASI();
    return TRUE;
}
