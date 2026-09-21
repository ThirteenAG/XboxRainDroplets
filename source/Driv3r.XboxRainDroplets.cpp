// Direct3D 9, the API this game uses
#define XRD_ENABLE_D3D9
#include "xrd/xrd.h"

void RegisterFountains()
{
    WaterDrops::RegisterGlobalEmitter({ 1448.16699f, 1737.32788f, 14.3802023f });
    WaterDrops::RegisterGlobalEmitter({ 1458.16797f, 1724.57727f, 14.0609770f });
    WaterDrops::RegisterGlobalEmitter({ 1459.05823f, 1741.43860f, 14.3181515f });
    WaterDrops::RegisterGlobalEmitter({ 1444.47217f, 1743.65039f, 14.3181515f });
    WaterDrops::RegisterGlobalEmitter({ 1442.56702f, 1730.56396f, 14.3181429f });
}

void Init()
{
    WaterDrops::ReadIniSettings();

    RegisterFountains();

    WaterDrops::ms_rainIntensity = 0.0f;

    static auto ppDevice = *hook::get_pattern<uint32_t>("B8 ? ? ? ? 89 9F", 1);
    static auto pCamMatrix = *hook::get_pattern<uint32_t>("BF ? ? ? ? F3 A5 89 1D ? ? ? ? 8B 4D B8", 1);
    static uint32_t* bRainCheck = nullptr;

    auto pattern = hook::pattern("A1 ? ? ? ? 8B 10 50");
    static auto dword_8AFA60 = *pattern.get_first<uint32_t*>(1);
    struct EndSceneHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.eax = *(uint32_t*)(dword_8AFA60);
            //WaterDrops::ms_rainIntensity = 0.0f;            
        }
    }; injector::MakeInline<EndSceneHook>(pattern.get_first(0));

    pattern = hook::pattern("8B 83 ? ? ? ? 83 EC 38");
    struct ResetHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.eax = *(uint32_t*)(regs.ebx + 0x158);
            WaterDrops::Reset();
            CSnow::Reset();
        }
    }; injector::MakeInline<ResetHook>(pattern.get_first(0), pattern.get_first(6));

    pattern = hook::pattern("8D 95 ? ? ? ? 52 68 ? ? ? ? 56 E8 ? ? ? ? 83 C4 0C E9 ? ? ? ? 6A 0A");
    struct RainHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.edx = regs.ebp + 0x158;
            bRainCheck = (uint32_t*)regs.edx;
        }
    }; injector::MakeInline<RainHook>(pattern.get_first(0), pattern.get_first(6));

    pattern = hook::pattern("C7 44 24 ? ? ? ? ? 7E 51");
    struct CamNoRainHook
    {
        void operator()(injector::reg_pack& regs)
        {
            *(uint32_t*)(regs.esp + 0x14) = 0;

            if (*(uint8_t*)(regs.eax + 0x2C) == 0)
            {
                auto ptr = *(uint8_t**)(regs.eax + 0x24);
                if (ptr)
                {
                    if (!*ptr)
                        WaterDrops::ms_rainIntensity = 0.0f;
                    else
                        WaterDrops::ms_rainIntensity = 1.0f;
                }
                else
                {
                    WaterDrops::ms_rainIntensity = 0.0f;
                }
            }
        }
    }; injector::MakeInline<CamNoRainHook>(pattern.get_first(0), pattern.get_first(8));

    static RwMatrix GviewMatrix;
    static auto loc_5D1CAF = (uintptr_t)hook::get_pattern<uintptr_t>("83 EF 04 83 ED 01", 0);
    static auto dw8AFA60 = *hook::get_pattern<uintptr_t*>("A1 ? ? ? ? 8B 08 6A 00 6A 00 50 FF 91", 1);
    pattern = hook::pattern("C6 83 ? ? ? ? ? 83 EF 04 83 ED 01");
    struct Render
    {
        void operator()(injector::reg_pack& regs)
        {
            *(uint8_t*)(regs.ebx + 0x1D8) = 0;

            if (regs.ebp == 5)
            {
                (*(void(__stdcall**)(int, int, int))(*(int*)*(int*)dw8AFA60 + 260))(*(int*)dw8AFA60, 0, 0);
                (*(void(__stdcall**)(int, int, int))(*(int*)*(int*)dw8AFA60 + 260))(*(int*)dw8AFA60, 1, 0);
                (*(void(__stdcall**)(int, int, int))(*(int*)*(int*)dw8AFA60 + 260))(*(int*)dw8AFA60, 2, 0);
                (*(void(__stdcall**)(int, int, int))(*(int*)*(int*)dw8AFA60 + 260))(*(int*)dw8AFA60, 3, 0);

                auto pDevice = *(LPDIRECT3DDEVICE9*)(ppDevice + 0x158);

                auto right = *(RwV3d*)(pCamMatrix + 0x00);
                auto up = *(RwV3d*)(pCamMatrix + 0x10);
                auto at = *(RwV3d*)(pCamMatrix + 0x20);
                auto pos = *(RwV3d*)(pCamMatrix + 0x30);

                WaterDrops::right = { -right.x, -right.z, -right.y };
                WaterDrops::up = { up.x, up.z, up.y };
                WaterDrops::at = { at.x, at.z, at.y };
                WaterDrops::pos = { pos.x, pos.z, pos.y };

                Xrd::Init(XRD_DEVICE_RENDERER, pDevice);
                WaterDrops::Process();

                // The snow of this effect is a bonus and has nothing to do with the
                // weather of the game: with it switched on it falls always, and the
                // droplets of the rain step aside for it. This used to force
                // targetSnow to zero whenever the game owned its rain object (the
                // ordinary case), so snow could never be seen in this game no matter
                // what the ini said, while the droplets kept raining. Same rule as in
                // the Parallel Lines plugin.
                if (WaterDrops::bEnableSnow)
                {
                    WaterDrops::ms_rainIntensity = 0.0f;
                    CSnow::targetSnow = 1.0f;
                }
                else
                {
                    CSnow::targetSnow = 0.0f;
                }

                WaterDrops::Render();

                {
                    static RwMatrix camMatrix;
                    camMatrix.right.x = -WaterDrops::right.x;
                    camMatrix.right.y = -WaterDrops::right.y;
                    camMatrix.right.z = -WaterDrops::right.z;
                    camMatrix.up = WaterDrops::up;
                    camMatrix.at = WaterDrops::at;
                    camMatrix.pos = WaterDrops::pos;

                    // The view matrix of this build of the game never reaches the hook
                    // that used to capture it, and an empty one puts every flake on the
                    // origin of the frame: that alone is why the snow of this game was
                    // invisible while its droplets were fine. What the module wants is
                    // the inverse of the camera matrix it builds the world matrix from -
                    // the very matrix the game itself would hand over. Row vector maths:
                    // the rotation is the transpose and the translation is turned around
                    // with it.
                    //
                    // The game of the Parallel Lines plugin hands over such a matrix,
                    // and measuring the two against each other says what it is exactly:
                    // world * view comes out as the camera axes with a scale on x and y
                    // (the view window of that camera) and a minus on z. The minus is the
                    // whole reason the flakes of this game flew towards the eye instead
                    // of away from it, so the z axis of the effect is turned around here
                    // as the game's own matrix turns it (the third component of every
                    // row, which is the column of that axis).
                    GviewMatrix.right = { camMatrix.right.x, camMatrix.up.x, -camMatrix.at.x };
                    GviewMatrix.up = { camMatrix.right.z, camMatrix.up.z, -camMatrix.at.z };
                    GviewMatrix.at = { camMatrix.right.y, camMatrix.up.y, -camMatrix.at.y };
                    GviewMatrix.pos = {
                        -(camMatrix.pos.x * camMatrix.right.x + camMatrix.pos.z * camMatrix.right.z + camMatrix.pos.y * camMatrix.right.y),
                        -(camMatrix.pos.x * camMatrix.up.x + camMatrix.pos.z * camMatrix.up.z + camMatrix.pos.y * camMatrix.up.y),
                        (camMatrix.pos.x * camMatrix.at.x + camMatrix.pos.z * camMatrix.at.z + camMatrix.pos.y * camMatrix.at.y),
                    };

                    static float ts = 0.0f;
                    ts = WaterDrops::GetFrameTimeSeconds() * 1000.0f;
                    CSnow::AddSnow(WaterDrops::ms_fbWidth, WaterDrops::ms_fbHeight, &camMatrix, &GviewMatrix, &ts, false);
                }
            }
        }
    }; injector::MakeInline<Render>(pattern.get_first(0), pattern.get_first(7));
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