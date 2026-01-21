#define DIRECT3D_VERSION 0x0800
#include "xrd.h"

bool* bPause = nullptr;
bool* bCutscene = nullptr;
uint32_t* nLoading = nullptr;

// Camera data pointers
__m128* xmmword_778BF0 = nullptr;  // Quaternion
__m128* xmmword_778BD0 = nullptr;  // Position
__m128* rain_grid = nullptr;
uintptr_t dword_7B1980 = 0;

void ComputeCameraVectors(float& upX, float& upY, float& upZ,
                          float& atX, float& atY, float& atZ,
                          float& rightX, float& rightY, float& rightZ,
                          float& posX, float& posY, float& posZ)
{
    if (!xmmword_778BF0 || !xmmword_778BD0) return;

    float qx = -xmmword_778BF0->m128_f32[0];
    float qy = -xmmword_778BF0->m128_f32[1];
    float qz = -xmmword_778BF0->m128_f32[2];
    float qw = xmmword_778BF0->m128_f32[3];

    atX = 2.0f * (qx * qz + qw * qy);
    atY = 2.0f * (qy * qz - qw * qx);
    atZ = 1.0f - 2.0f * (qx * qx + qy * qy);

    rightX = 1.0f - 2.0f * (qy * qy + qz * qz);
    rightY = 2.0f * (qx * qy + qw * qz);
    rightZ = 2.0f * (qx * qz - qw * qy);

    upX = 2.0f * (qx * qy - qw * qz);
    upY = 1.0f - 2.0f * (qx * qx + qz * qz);
    upZ = 2.0f * (qy * qz + qw * qx);

    atX = -atX;
    atY = -atY;
    atZ = -atZ;

    float len = sqrtf(atX * atX + atY * atY + atZ * atZ);
    if (len > 0.001f)
    {
        atX /= len;
        atY /= len;
        atZ /= len;
    }
    if (atZ < 0.1f)
    {
        atZ = 0.1f;
    }

    upX = -upX;
    upY = -upY;
    upZ = -upZ;

    rightX = -rightX;
    rightY = -rightY;
    rightZ = -rightZ;

    posX = xmmword_778BD0->m128_f32[0];
    posY = xmmword_778BD0->m128_f32[1];
    posZ = xmmword_778BD0->m128_f32[2];
}

bool IsCameraInsideActiveRainVolume(float camX, float camY, float camZ)
{
    constexpr float MAX_DIST_SQ = 4.0f;

    for (int idx = 0; idx < 256; idx++)
    {
        // Check if this grid cell is active
        int byteOffset = idx >> 3;
        int bit = idx & 7;
        if ((*(uint8_t*)(dword_7B1980 + byteOffset) & (1u << bit)) == 0)
            continue;

        __m128 center = rain_grid[idx];

        float dx = camX - center.m128_f32[0];
        float dy = camY - center.m128_f32[1];
        float dz = camZ - center.m128_f32[2];

        auto dist = dx * dx + dy * dy + dz * dz;
        if (dist <= MAX_DIST_SQ)
            return true;
    }

    return false;
}

void Init()
{
    WaterDrops::ReadIniSettings();

    auto pattern = hook::pattern("88 15 ? ? ? ? 8D 45");
    bPause = *pattern.get_first<bool*>(2);

    pattern = hook::pattern("32 C0 88 81 ? ? ? ? A2 ? ? ? ? E8 ? ? ? ? 33 C0 C3");
    bCutscene = *pattern.get_first<bool*>(9);

    pattern = hook::pattern("83 3D ? ? ? ? ? 74 ? 84 DB");
    nLoading = *pattern.get_first<uint32_t*>(2);

    pattern = hook::pattern("0F 29 05 ? ? ? ? 0F 28 46 ? 6A 00");
    xmmword_778BF0 = *pattern.get_first<__m128*>(3);

    pattern = hook::pattern("0F 29 05 ? ? ? ? 0F 28 46 ? 68 8F C2 F5 3C");
    xmmword_778BD0 = *pattern.get_first<__m128*>(3);

    pattern = hook::pattern("BE ? ? ? ? BF ? ? ? ? 0F 29 05 ? ? ? ? F3 A5 5F C7 05");
    rain_grid = *pattern.get_first<__m128*>(1);

    pattern = hook::pattern("8D 84 7A ? ? ? ? 8B CE");
    dword_7B1980 = *pattern.get_first<uintptr_t>(3);

    pattern = hook::pattern("F3 0F 11 15 ? ? ? ? 77");
    static float* fRainIntensity = *pattern.get_first<float*>(4);

    pattern = hook::pattern("A1 ? ? ? ? 8B 10 8D 4C 24 ? 51 50");
    static LPDIRECT3DDEVICE8* pDevice = *pattern.get_first<LPDIRECT3DDEVICE8*>(1);

    pattern = find_pattern("8B 74 24 ? 56 B9 ? ? ? ? E8 ? ? ? ? 56 B9 ? ? ? ? E8 ? ? ? ? 5E", "83 3D ? ? ? ? ? 74 ? 56 8B 74 24");
    static auto RenderHook = safetyhook::create_mid(pattern.get_first(10), [](SafetyHookContext& regs)
    {
        if (*nLoading != 0 || *bCutscene || *bPause)
        {
            WaterDrops::Clear();
            return;
        }

        WaterDrops::ms_rainIntensity = *fRainIntensity;

        if (WaterDrops::ms_rainIntensity >= 0.0133333206f && WaterDrops::ms_rainIntensity < 0.279999971f)
            WaterDrops::ms_rainIntensity = 0.279999971f;

        float upX, upY, upZ, atX, atY, atZ, rightX, rightY, rightZ, posX, posY, posZ;
        ComputeCameraVectors(upX, upY, upZ, atX, atY, atZ, rightX, rightY, rightZ, posX, posY, posZ);

        if (!IsCameraInsideActiveRainVolume(posX, posY, posZ))
            WaterDrops::ms_rainIntensity = 0.0f;

        WaterDrops::up.x = upX;
        WaterDrops::up.y = upY;
        WaterDrops::up.z = upZ;

        WaterDrops::at.x = atX;
        WaterDrops::at.y = atY;
        WaterDrops::at.z = atZ;

        WaterDrops::right.x = rightX;
        WaterDrops::right.y = rightY;
        WaterDrops::right.z = rightZ;

        WaterDrops::pos.x = posX;
        WaterDrops::pos.y = posY;
        WaterDrops::pos.z = posZ;

        WaterDrops::Process(*pDevice);
        WaterDrops::Render(*pDevice);
    });

    // The game does not support device reset, it just dies if that happens.

    // Hydrants
    static std::string currentSoundArchive;
    static uintptr_t pFireHydrant = 0;

    pattern = hook::pattern("50 E8 ? ? ? ? 8B D8 85 DB 74 ? 53");
    static auto GetCurrentAudioArchiveHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
    {
        currentSoundArchive = (const char*)regs.eax;
        std::transform(currentSoundArchive.begin(), currentSoundArchive.end(), currentSoundArchive.begin(), ::tolower);
    });

    pattern = hook::pattern("89 7E ? 74 ? 8B 4E ? 03 C9");
    static auto GetFireHydrantPtrHook = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
    {
        if (currentSoundArchive.ends_with("weather.fsb"))
        {
            pFireHydrant = regs.edi + 4; //.wav
        }
    });

    pattern = hook::pattern("89 14 81 E8 ? ? ? ? 83 4E");
    static auto OnFireHydrantSoundStart = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
    {
        if (pFireHydrant == (regs.ecx + regs.eax * 4))
        {
            __m128* sound_struct = (__m128*)regs.esi;
            __m128 pos = sound_struct[1];
            float x = pos.m128_f32[0];
            float y = pos.m128_f32[1];
            float z = pos.m128_f32[2];

            RwV3d prt_pos = { x, y, z };
            auto len = WaterDrops::GetDistanceBetweenEmitterAndCamera(prt_pos);
            if (len <= 100.0f)
                WaterDrops::RegisterSplash(&prt_pos, 2.0f, 3000, 100.0f);
        }
    });

    pattern = hook::pattern("C7 04 88 00 00 00 00");
    static auto OnFireHydrantSoundEnd = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& regs)
    {
        if (pFireHydrant == (regs.eax + regs.ecx * 4))
        {
            WaterDrops::ms_splashDuration = 0;
        }
    });
}

extern "C" __declspec(dllexport) void InitializeASI()
{
    std::call_once(CallbackHandler::flag, []()
    {
        CallbackHandler::RegisterCallbackAtGetSystemTimeAsFileTime(Init, hook::pattern("BF 94 00 00 00 8B C7"));
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