#include "xrd.h"
#include "snow.h"

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
            regs.eax = *(uint32_t*)(dword_8AFA60) ;
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
                    CSnow::targetSnow = 0.0f;
                }
                else
                {
                    WaterDrops::ms_rainIntensity = 0.0f;
                    if (WaterDrops::bEnableSnow)
                        CSnow::targetSnow = 1.0f;
                    else
                        CSnow::targetSnow = 0.0f;
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
                
                WaterDrops::Process(pDevice);
                WaterDrops::Render(pDevice);
                
                {
                    static RwMatrix camMatrix;
                    camMatrix.right.x = -WaterDrops::right.x;
                    camMatrix.right.y = -WaterDrops::right.y;
                    camMatrix.right.z = -WaterDrops::right.z;
                    camMatrix.up = WaterDrops::up;
                    camMatrix.at = WaterDrops::at;
                    camMatrix.pos = WaterDrops::pos;

                    static float ts = 0.0f;
                    ts = WaterDrops::GetTimeStepInMilliseconds();
                    CSnow::AddSnow(pDevice, WaterDrops::ms_fbWidth, WaterDrops::ms_fbHeight, &camMatrix, &GviewMatrix, &ts, false);
                }
            }
        }
    }; injector::MakeInline<Render>(pattern.get_first(0), pattern.get_first(7));

    pattern = hook::pattern("0F B7 53 50 8B 43 40");
    struct MatrixCheck
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.edx = *(uint16_t*)(regs.ebx + 0x50);
            regs.eax = *(uint32_t*)(regs.ebx + 0x40);

            if (regs.edx == 0x00000011)
                GviewMatrix = *(RwMatrix*)(regs.eax + *(uint32_t*)(regs.ebx));
        }
    }; injector::MakeInline<MatrixCheck>(pattern.get_first(0), pattern.get_first(7));
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