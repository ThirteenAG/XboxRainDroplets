#include "xrd.h"
#include <unordered_set>

namespace UObject
{
    std::unordered_set<std::wstring> cachedTypes = {
        L"EOpticCable",
        L"EPlayerCam",
        L"EGoggle",
    };

    std::unordered_map<std::wstring, std::wstring> objectStates;
    std::wstring_view GetState(const std::wstring& type)
    {
        auto it = objectStates.find(type);
        return (it != objectStates.end()) ? std::wstring_view(it->second) : std::wstring_view(L"");
    }

    wchar_t* (__fastcall* GetFullName)(void*, void*, wchar_t*) = nullptr;
    void* (__fastcall* FindState)(void*, void*, int) = nullptr;

    SafetyHookInline shGotoState = {};
    int __fastcall GotoState(void* uObject, void* edx, int StateID, int a3)
    {
        wchar_t buffer[256];
        auto objName = std::wstring_view(GetFullName(uObject, edx, buffer));

        size_t spacePos = objName.find(L' ');
        std::wstring type = (spacePos != std::wstring::npos) ? std::wstring(objName.substr(0, spacePos)) : std::wstring(objName);
        if (cachedTypes.count(type))
        {
            auto svStateName = std::wstring_view(GetFullName(FindState(uObject, edx, StateID), edx, buffer));
            size_t lastDot = svStateName.rfind(L'.');
            std::wstring stateName = (lastDot != std::wstring::npos) ? std::wstring(svStateName.substr(lastDot + 1)) : std::wstring(svStateName);
            objectStates[type] = stateName;
        }

        return shGotoState.unsafe_fastcall<int>(uObject, edx, StateID, a3);
    }
}

struct FVector
{
    float X, Y, Z;
};

struct FRotator
{
    int Pitch, Yaw, Roll;
};

struct CameraData
{
    FVector Location;
    FRotator Rotation;
};

uint8_t(__fastcall* ULevel__IsRainingOn)(void* uLevel, void* edx, FVector* a2) = nullptr;

uintptr_t gCurrentPlayerController = 0;
SafetyHookInline shULevel__Tick = {};
int __fastcall ULevel__Tick(void* uLevel, void* edx, int a2, float a3)
{
    static float TimeStep = 0.0f;
    TimeStep = a3;
    WaterDrops::fTimeStep = &TimeStep;

    if (gCurrentPlayerController)
    {
        auto curCam = (CameraData*)(gCurrentPlayerController + 0x408);

        if (ULevel__IsRainingOn(uLevel, 0, &curCam->Location))
        {
            if (UObject::GetState(L"EOpticCable") == L"s_Sneaking")
            {
                WaterDrops::Clear();
            }
            else
            {
                WaterDrops::ms_rainIntensity = 1.0f;

                constexpr float UnrealToRadians = (2.0f * 3.14159265359f) / 65536.0f;

                float SR = sinf(curCam->Rotation.Roll * UnrealToRadians);
                float CR = cosf(curCam->Rotation.Roll * UnrealToRadians);
                float SP = sinf(curCam->Rotation.Pitch * UnrealToRadians);
                float CP = cosf(curCam->Rotation.Pitch * UnrealToRadians);
                float SY = sinf(curCam->Rotation.Yaw * UnrealToRadians);
                float CY = cosf(curCam->Rotation.Yaw * UnrealToRadians);

                // Build RwMatrix from Unreal rotation matrix
                RwMatrix matrix;

                // Right vector (from M[1][0-2])
                matrix.right.x = SR * SP * CY - CR * SY;
                matrix.right.y = SR * SP * SY + CR * CY;
                matrix.right.z = -SR * CP;
                matrix.flags = 0;

                // Up vector (from M[2][0-2])
                matrix.up.x = -(CR * SP * CY + SR * SY);
                matrix.up.y = CY * SR - CR * SP * SY;
                matrix.up.z = CR * CP;
                matrix.pad1 = 0;

                // At vector (from M[0][0-2]) - Forward
                matrix.at.x = CP * CY;
                matrix.at.y = CP * SY;
                matrix.at.z = SP;
                matrix.pad2 = 0;

                // Position
                matrix.pos.x = curCam->Location.X;
                matrix.pos.y = curCam->Location.Y;
                matrix.pos.z = curCam->Location.Z;
                matrix.pad3 = 0;

                // Apply to WaterDrops
                WaterDrops::right.x = -matrix.right.x;
                WaterDrops::right.y = -matrix.right.y;
                WaterDrops::right.z = -matrix.right.z;
                WaterDrops::up = matrix.up;
                WaterDrops::at.x = matrix.at.x * 2.0f;
                WaterDrops::at.y = matrix.at.y * 2.0f;
                WaterDrops::at.z = matrix.at.z * 2.0f;
                WaterDrops::pos = matrix.pos;
            }
        }
    }
    gCurrentPlayerController = 0;
    return shULevel__Tick.unsafe_fastcall<int>(uLevel, edx, a2, a3);
}

SafetyHookInline shAPlayerController__Tick = {};
int __fastcall APlayerController__Tick(void* PlayerController, int a2, float a3, int a4)
{
    static void* prevPlayerController = nullptr;
    gCurrentPlayerController = (uintptr_t)PlayerController;

    if (PlayerController != prevPlayerController)
    {
        prevPlayerController = PlayerController;
        WaterDrops::Clear();
    }

    return shAPlayerController__Tick.unsafe_fastcall<int>(PlayerController, a2, a3, a4);
}

void InitEngine()
{
    ULevel__IsRainingOn = (decltype(ULevel__IsRainingOn))GetProcAddress(GetModuleHandle(L"Engine"), "?IsRainingOn@ULevel@@UAEEAAVFVector@@@Z");
    shULevel__Tick = safetyhook::create_inline(GetProcAddress(GetModuleHandle(L"Engine"), "?Tick@ULevel@@UAEXW4ELevelTick@@M@Z"), ULevel__Tick);
    shAPlayerController__Tick = safetyhook::create_inline(GetProcAddress(GetModuleHandle(L"Engine"), "?Tick@APlayerController@@UAEHMW4ELevelTick@@@Z"), APlayerController__Tick);
}

void InitD3DDrv()
{
    WaterDrops::ReadIniSettings();

    auto pattern = hook::module_pattern(GetModuleHandle(L"D3DDrv"), "53 56 FF 90 ? ? ? ? 6A 02 8B CD E8 ? ? ? ? 68 00 00 00 3F");
    static auto RenderHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
    {
        IDirect3DDevice9* pDevice = (IDirect3DDevice9*)(regs.esi);
        WaterDrops::Process(pDevice);
        WaterDrops::Render(pDevice);
        WaterDrops::ms_rainIntensity = 0.0f;
    });
    
    static auto DeviceResetHook = safetyhook::create_mid(GetProcAddress(GetModuleHandle(L"D3DDrv"), "?resetDevice@UD3DRenderDevice@@QAEXAAU_D3DPRESENT_PARAMETERS_@@@Z"), [](SafetyHookContext& regs)
    {
        WaterDrops::Reset();
    });
}

void InitCore()
{
    UObject::GetFullName = (decltype(UObject::GetFullName))GetProcAddress(GetModuleHandle(L"Core"), "?GetFullName@UObject@@QBEPBGPAG@Z");
    UObject::FindState = (decltype(UObject::FindState))GetProcAddress(GetModuleHandle(L"Core"), "?FindState@UObject@@QAEPAVUState@@VFName@@@Z");
    UObject::shGotoState = safetyhook::create_inline(GetProcAddress(GetModuleHandle(L"Core"), "?GotoState@UObject@@UAE?AW4EGotoState@@VFName@@H@Z"), UObject::GotoState);
}

extern "C" __declspec(dllexport) void InitializeASI()
{
    std::call_once(CallbackHandler::flag, []()
    {
        CallbackHandler::RegisterCallback(L"Engine.dll", InitEngine);
        CallbackHandler::RegisterCallback(L"D3DDrv.dll", InitD3DDrv);
        CallbackHandler::RegisterCallback(L"Core.dll", InitCore);
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