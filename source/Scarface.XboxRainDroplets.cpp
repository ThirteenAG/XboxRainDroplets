#include "xrd.h"
#define ID_BLURPS                            103
#define IDR_POSTFXVS                         104

class MenuBlur: WaterDrops
{
private:
    static inline int32_t ms_initialised;
    static inline IDirect3DVertexDeclaration9* ms_quadVertexDecl = nullptr;
    static inline IDirect3DPixelShader9* ms_blurps = nullptr;
    static inline IDirect3DVertexShader9* ms_blurvs = nullptr;
public:
    static inline void InitialiseRender(LPDIRECT3DDEVICE9 pDevice)
    {
        HMODULE hm = NULL;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&Render, &hm);
        auto resource = FindResource(hm, MAKEINTRESOURCE(ID_BLURPS), RT_RCDATA);
        auto shader = (DWORD*)LoadResource(hm, resource);
        pDevice->CreatePixelShader(shader, &ms_blurps);
        FreeResource(shader);

        resource = FindResource(hm, MAKEINTRESOURCE(IDR_POSTFXVS), RT_RCDATA);
        shader = (DWORD*)LoadResource(hm, resource);
        pDevice->CreateVertexShader(shader, &ms_blurvs);
        FreeResource(shader);

        static const D3DVERTEXELEMENT9 vertexElements[] =
        {
            { 0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
            { 0, 16, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
            { 0, 20, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
            { 0, 28, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },
            D3DDECL_END()
        };
        pDevice->CreateVertexDeclaration(vertexElements, &ms_quadVertexDecl);
        ms_initialised = 1;
    }

    static inline void Process(LPDIRECT3DDEVICE9 pDevice)
    {
        if (!ms_initialised)
            InitialiseRender(pDevice);
    }

    static inline void Render(LPDIRECT3DDEVICE9 pDevice)
    {
        LPDIRECT3DSTATEBLOCK9 pStateBlock = NULL;
        pDevice->CreateStateBlock(D3DSBT_ALL, &pStateBlock);
        pStateBlock->Capture();

        pDevice->StretchRect(ms_bbuf, NULL, ms_surf, NULL, D3DTEXF_LINEAR);

        float rasterWidth = ms_fbWidth;
        float rasterHeight = ms_fbHeight;
        float halfU = 0.5f / rasterWidth;
        float halfV = 0.5f / rasterHeight;
        float uMax = 1.0f;
        float vMax = 1.0f;
        int i = 0;

        float leftOff, rightOff, topOff, bottomOff;
        float scale = rasterWidth / 640.0f;
        leftOff = 8.0f * scale / 16.0f / rasterWidth;
        rightOff = 8.0f * scale / 16.0f / rasterWidth;
        topOff = 8.0f * scale / 16.0f / rasterHeight;
        bottomOff = 8.0f * scale / 16.0f / rasterHeight;

        struct QuadVertex
        {
            float      x, y, z;
            float      rhw;
            uint32_t   emissiveColor;
            float      u, v;
            float      u1, v1;
        };

        static constexpr int16_t quadIndices[12] = { 0, 1, 2, 0, 2, 3, 0, 1, 2, 0, 2, 3 };
        static QuadVertex quadVertices[4];
        quadVertices[i].x = 1.0f;
        quadVertices[i].y = -1.0f;
        quadVertices[i].z = 0.0f;
        quadVertices[i].rhw = 1.0f;
        quadVertices[i].u = uMax + halfU;
        quadVertices[i].v = vMax + halfV;
        quadVertices[i].u1 = quadVertices[i].u + rightOff;
        quadVertices[i].v1 = quadVertices[i].v + bottomOff;
        i++;

        quadVertices[i].x = -1.0f;
        quadVertices[i].y = -1.0f;
        quadVertices[i].z = 0.0f;
        quadVertices[i].rhw = 1.0f;
        quadVertices[i].u = 0.0f + halfU;
        quadVertices[i].v = vMax + halfV;
        quadVertices[i].u1 = quadVertices[i].u + leftOff;
        quadVertices[i].v1 = quadVertices[i].v + bottomOff;
        i++;

        quadVertices[i].x = -1.0f;
        quadVertices[i].y = 1.0f;
        quadVertices[i].z = 0.0f;
        quadVertices[i].rhw = 1.0f;
        quadVertices[i].u = 0.0f + halfU;
        quadVertices[i].v = 0.0f + halfV;
        quadVertices[i].u1 = quadVertices[i].u + leftOff;
        quadVertices[i].v1 = quadVertices[i].v + topOff;
        i++;

        quadVertices[i].x = 1.0f;
        quadVertices[i].y = 1.0f;
        quadVertices[i].z = 0.0f;
        quadVertices[i].rhw = 1.0f;
        quadVertices[i].u = uMax + halfU;
        quadVertices[i].v = 0.0f + halfV;
        quadVertices[i].u1 = quadVertices[i].u + rightOff;
        quadVertices[i].v1 = quadVertices[i].v + topOff;

        pDevice->SetTexture(0, ms_tex);
        pDevice->SetVertexDeclaration(ms_quadVertexDecl);
        pDevice->SetVertexShader(ms_blurvs);
        pDevice->SetPixelShader(ms_blurps);

        pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, 0);
        pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        pDevice->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, 6, 2, quadIndices, D3DFMT_INDEX16, quadVertices, sizeof(QuadVertex));
        pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, 1);

        pDevice->SetVertexShader(NULL);
        pDevice->SetPixelShader(NULL);

        pStateBlock->Apply();
        pStateBlock->Release();
    }

    static inline void Reset()
    {
        auto SafeRelease = [](auto ppT) {
            if (*ppT)
            {
                (*ppT)->Release();
                *ppT = NULL;
            }
        };
        SafeRelease(&ms_blurps);
        SafeRelease(&ms_blurvs);
        SafeRelease(&ms_quadVertexDecl);
        ms_initialised = 0;
    }
};

class Vector
{
public:
    float X, Y, Z;
};

class Matrix {
public:
    float M[4][4];

    Vector GetForward()
    {
        return { M[2][0], M[2][1],M[2][2] };
    };
    Vector GetUp()
    {
        return { M[1][0], M[1][1],M[1][2] };
    };
    Vector GetRight()
    {
        return { M[0][0], M[0][1],M[0][2] };
    };
    Vector GetPos()
    {
        return { M[3][0], M[3][1],M[3][2] };
    };
};

class BaseObject {
public:
    Vector GetLocation()
    {
        return GetMatrix().GetPos();
    }
    Matrix GetMatrix()
    {
        return *(Matrix*)((uintptr_t)this + 52);
    }
};

uintptr_t dw573200 = 0;
class CharacterObject : public BaseObject {
public:
    char IsCharacterInInterior()
    {
        return ((int(__thiscall*)(CharacterObject*))dw573200)(this);
    }
};

uintptr_t dw825A78 = 0;
CharacterObject* GetMainCharacter()
{
    return *(CharacterObject**)(dw825A78);
}

class Camera : public BaseObject {
};

injector::hook_back<int(__cdecl*)(int a1)> hb_PlaySharkNIS;
int __cdecl PlaySharkNIS(int a1)
{
    WaterDrops::FillScreenMoving(100.0f, true);
    return hb_PlaySharkNIS.fun(a1);
}

injector::hook_back<void(*)()> hb_sub_654B20;
void sub_654B20()
{
    WaterDrops::Reset();
    MenuBlur::Reset();
    return hb_sub_654B20.fun();
}

injector::hook_back<void(__cdecl*)(int mSoundPlayerHandle, int sndName, void* vehicleTransform, int Vector, int vehicleVelocity, int a6)> hb_CarSoundPlayerRequestImpactLayer;
void __cdecl CarSoundPlayerRequestImpactLayer(int mSoundPlayerHandle, int sndName, void* vehicleTransform, int Vector, int vehicleVelocity, int a6)
{
    //RwV3d vec = *(RwV3d*)vehicleTransform;
    //RwV3d prt_pos = { vec.z, vec.x, vec.y };
    //RwV3d dist;
    //RwV3dSub(&dist, &prt_pos, &WaterDrops::pos);
    //auto x = RwV3dDotProduct(&dist, &dist);
    //if (RwV3dDotProduct(&dist, &dist) <= 60.0f)
    WaterDrops::FillScreenMoving(50.0f);
    return hb_CarSoundPlayerRequestImpactLayer.fun(mSoundPlayerHandle, sndName, vehicleTransform, Vector, vehicleVelocity, a6);
}

std::string sLastUsedSound;
injector::hook_back<void(__cdecl*)(int mSoundPlayerHandle, int sndName, void* vehicleTransform, int Vector, int vehicleVelocity, int a6)> hb_CarSoundPlayerRequestImpactLayer2;
void __cdecl CarSoundPlayerRequestImpactLayer2(int mSoundPlayerHandle, int sndName, void* vehicleTransform, int Vector, int vehicleVelocity, int a6)
{
    if (sLastUsedSound == "valve_explode")
    {
        RwV3d vec = *(RwV3d*)vehicleTransform;
        RwV3d prt_pos = { vec.z, vec.x, vec.y };
        RwV3d dist;
        RwV3dSub(&dist, &prt_pos, &WaterDrops::pos);
        auto x = RwV3dDotProduct(&dist, &dist);
        if (RwV3dDotProduct(&dist, &dist) <= 100.0f)
            WaterDrops::RegisterSplash(&prt_pos, 10.0f, 3000, 100.0f);
    }
    sLastUsedSound.clear();
    return hb_CarSoundPlayerRequestImpactLayer2.fun(mSoundPlayerHandle, sndName, vehicleTransform, Vector, vehicleVelocity, a6);
}

void RegisterFountains()
{
    WaterDrops::RegisterGlobalEmitter({ -743.908f, -1796.660f, 7.921f });
    WaterDrops::RegisterGlobalEmitter({ -455.362f, -1509.833f, 6.988f });
    WaterDrops::RegisterGlobalEmitter({ -38.574f, -1837.737f, 6.561f });
    WaterDrops::RegisterGlobalEmitter({ 357.853f, -1417.937f, 8.292f });
    WaterDrops::RegisterGlobalEmitter({ 238.592f, -1381.341f, 3.553f });
    WaterDrops::RegisterGlobalEmitter({ -776.338f, 1167.000f, 8.662f });
    WaterDrops::RegisterGlobalEmitter({ -83.510f, 1269.453f, 11.266f });
    WaterDrops::RegisterGlobalEmitter({ -74.844f, 1269.331f, 10.962f });
    WaterDrops::RegisterGlobalEmitter({ -66.922f, 1269.524f, 11.738f });
    WaterDrops::RegisterGlobalEmitter({ -66.753f, 1281.050f, 11.120f });
    WaterDrops::RegisterGlobalEmitter({ -74.705f, 1281.091f, 11.040f });
    WaterDrops::RegisterGlobalEmitter({ -83.039f, 1281.037f, 11.057f });
    WaterDrops::RegisterGlobalEmitter({ -1729.224f, -2176.411f, 13.328f });
    WaterDrops::RegisterGlobalEmitter({ -1740.977f, -2110.513f, 9.240f });
    WaterDrops::RegisterGlobalEmitter({ -1065.503f, -1740.417f, 5.278f });
    WaterDrops::RegisterGlobalEmitter({ -1062.239f, -1746.649f, 5.278f });
    WaterDrops::RegisterGlobalEmitter({ -1073.977f, -1752.641f, 5.278f });
    WaterDrops::RegisterGlobalEmitter({ -1077.071f, -1746.327f, 5.278f });
    WaterDrops::RegisterGlobalEmitter({ -1815.826f, 429.235f, 2.976f });
    WaterDrops::RegisterGlobalEmitter({ -1818.582f, 415.908f, 2.976f });
    WaterDrops::RegisterGlobalEmitter({ -1821.392f, 423.498f, 2.976f });
    WaterDrops::RegisterGlobalEmitter({ -703.54278f, 1088.885376f, 3.213393f });
    WaterDrops::RegisterGlobalEmitter({ -697.76715f, 1084.932617f, 3.213393f });
    WaterDrops::RegisterGlobalEmitter({ -697.29345f, 1079.573975f, 3.213393f });
    WaterDrops::RegisterGlobalEmitter({ -703.06695f, 1083.592651f, 3.213393f });
    WaterDrops::RegisterGlobalEmitter({ -710.14062f, 1079.313721f, 3.213392f });
    WaterDrops::RegisterGlobalEmitter({ -709.60955f, 1073.850464f, 3.213392f });
    WaterDrops::RegisterGlobalEmitter({ -703.86084f, 1070.046387f, 3.213392f });
    WaterDrops::RegisterGlobalEmitter({ -704.28790f, 1075.350830f, 3.213392f });
    WaterDrops::RegisterGlobalEmitter({ 18.097f, -11997.636f, 35.251f });
    WaterDrops::RegisterGlobalEmitter({ 13.897f, -11997.556f, 34.302f });
    WaterDrops::RegisterGlobalEmitter({ 9.574f, -11997.233f, 33.858f  });
    WaterDrops::RegisterGlobalEmitter({ 5.831f, -11997.465f, 33.234f  });
    WaterDrops::RegisterGlobalEmitter({ 2.057f, -11997.495f, 32.484f  });
    WaterDrops::RegisterGlobalEmitter({ 1.284f, -12002.519f, 32.642f  });
    WaterDrops::RegisterGlobalEmitter({ 9.236f, -12002.665f, 33.688f  });
    WaterDrops::RegisterGlobalEmitter({ 13.370f, -12002.439f, 34.186f });
    WaterDrops::RegisterGlobalEmitter({ 17.981f, -12002.531f, 34.875f  });
}

injector::hook_back<int(__cdecl*)(char* a1)> hb_sub_49C480;
int __cdecl sub_49C480(char* a1)
{
    sLastUsedSound = a1;
    return hb_sub_49C480.fun(a1);
}

void Init()
{
    CIniReader iniReader("");
    WaterDrops::bRadial = iniReader.ReadInteger("MAIN", "RadialMovement", 0) != 0;
    WaterDrops::bGravity = iniReader.ReadInteger("MAIN", "EnableGravity", 1) != 0;

    RegisterFountains();
    
    dw825A78 = *hook::get_pattern<uintptr_t>("A1 ? ? ? ? 85 C0 74 50", 1);
    dw573200 = (uintptr_t)hook::get_pattern("8B 81 ? ? ? ? 85 C0 74 14 8B 80 ? ? ? ? 85 C0 74 0A 50 E8 ? ? ? ? 83 C4 04 C3 32 C0", 0);
    
    auto pattern = hook::pattern("C6 87 ? ? ? ? ? 8B 08 50 FF 51 44 85 C0");
    static LPDIRECT3DDEVICE9* pDev = nullptr;
    struct PresentHook
    {
        void operator()(injector::reg_pack& regs)
        {
            pDev = (LPDIRECT3DDEVICE9*)(regs.edi + 0x10);
            *(uint8_t*)(regs.edi + 0x1CD) = 0;
        }
    }; injector::MakeInline<PresentHook>(pattern.get_first(0), pattern.get_first(7));

    static auto nMenuCheck = (bool(*)())injector::GetBranchDestination(hook::get_pattern("E8 ? ? ? ? 84 C0 75 A4")).as_int();
    pattern = hook::pattern("A1 ? ? ? ? 53 55 56 57 8B E9 8B 48 1C 6A 01");
    static auto dword_8111D0 = pattern.get_first(1);
    pattern = hook::pattern("E8 ? ? ? ? 8B 87 ? ? ? ? 83 C4 04");
    struct DrawHook
    {
        void operator()(injector::reg_pack& regs)
        {
            if (pDev)
            {
                if (!GetMainCharacter()->IsCharacterInInterior())
                    WaterDrops::ms_rainIntensity = (float)*(uint8_t*)((*(uintptr_t*)(**(uintptr_t**)dword_8111D0 + 0x1C)) + 0x44);
                else
                    WaterDrops::ms_rainIntensity = 0.0f;
                
                if (!WaterDrops::ms_initialised || !nMenuCheck())
                    WaterDrops::Process(*pDev);
                WaterDrops::Render(*pDev);

                if (nMenuCheck())
                {
                    MenuBlur::Process(*pDev);
                    MenuBlur::Render(*pDev);
                }
            }
        }
    }; injector::MakeInline<DrawHook>(pattern.get_first(0)); //0x651383

    pattern = hook::pattern("8B 46 10 8B 08 8D 56 14 52 50 FF 51 40");
    struct ResetHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.eax = *(uint32_t*)(regs.esi + 0x10);
            regs.ecx = *(uint32_t*)(regs.eax);
            WaterDrops::Reset();
            MenuBlur::Reset();
        }
    }; injector::MakeInline<ResetHook>(pattern.get_first(0));

    //Lost Device
    pattern = hook::pattern("75 26 E8 ? ? ? ? 8B 86 ? ? ? ?");
    hb_sub_654B20.fun = injector::MakeCALL(pattern.get_first(2), sub_654B20, true).get();
    pattern = hook::pattern("E8 ? ? ? ? 8B 06 8B 08 53");
    hb_sub_654B20.fun = injector::MakeCALL(pattern.get_first(0), sub_654B20, true).get();

    pattern = hook::pattern("8B 88 ? ? ? ? 8B 11 83 C0 34");
    struct SetPositionHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.ecx = *(uint32_t*)(regs.eax + 0x84);

            auto TheCamera = *(Camera**)&regs.eax;
            Matrix viewMatrix = TheCamera->GetMatrix();

            auto up = RwV3d{ viewMatrix.GetUp().X ,viewMatrix.GetUp().Y, viewMatrix.GetUp().Z };
            auto right = RwV3d{ viewMatrix.GetRight().X ,viewMatrix.GetRight().Y, viewMatrix.GetRight().Z };
            auto at = RwV3d{ viewMatrix.GetForward().X ,viewMatrix.GetForward().Y, viewMatrix.GetForward().Z };
            auto pos = RwV3d{ viewMatrix.GetPos().X ,viewMatrix.GetPos().Y, viewMatrix.GetPos().Z };

            RwMatrix dst;
            dst.right = { at.z, at.x, at.y };
            dst.up = { -right.z, -right.x, -right.y };
            dst.at = { up.z, up.x, up.y };
            dst.pos = { pos.z, pos.x, pos.y };

            WaterDrops::right = dst.up;
            WaterDrops::up = dst.right;
            WaterDrops::at = dst.at;
            WaterDrops::pos = dst.pos;
        }
    }; injector::MakeInline<SetPositionHook>(pattern.get_first(0), pattern.get_first(6));

    pattern = hook::pattern("E8 ? ? ? ? 83 C4 04 50 E8 ? ? ? ? 83 C4 10 E8 ? ? ? ? C6 46 08 00 89 7E 04");
    hb_PlaySharkNIS.fun = injector::MakeCALL(pattern.get_first(0), PlaySharkNIS, true).get();
    
    pattern = hook::pattern("E8 ? ? ? ? 83 C4 18 5F 5E 83 C4 7C C3");
    hb_CarSoundPlayerRequestImpactLayer.fun = injector::MakeCALL(pattern.get_first(0), CarSoundPlayerRequestImpactLayer, true).get();
    pattern = hook::pattern("52 E8 ? ? ? ? 83 C4 18 5F 5B");
    hb_CarSoundPlayerRequestImpactLayer2.fun = injector::MakeCALL(pattern.get_first(1), CarSoundPlayerRequestImpactLayer2, true).get();

    pattern = hook::pattern("8B 91 ? ? ? ? 88 42 64");
    struct SwimUpdateHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.edx = *(uint32_t*)(regs.ecx + 0xF8);
            auto run_state = *(uint8_t*)((uint8_t*)GetMainCharacter() + 0x260);
            static float amount = 0.0f;
            if (run_state != 0)
            {
                if (amount < 1.0f)
                    amount += 0.001f;
                WaterDrops::FillScreenMoving(amount);
            }
            else
                amount = 0.0f;
        }
    }; injector::MakeInline<SwimUpdateHook>(pattern.get_first(0), pattern.get_first(6));

    pattern = hook::pattern("50 E8 ? ? ? ? 8B 6C 24 30");
    hb_sub_49C480.fun = injector::MakeCALL(pattern.get_first(1), sub_49C480, true).get();

    pattern = hook::pattern("83 C0 24 56 57");
    struct ActiveBoatObjectUpdatePreSimHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.eax += 0x24;
            regs.ecx = regs.eax;
            
            auto movement = *(uint64_t*)(regs.esi - 0x48 + 0x67A);
            static float amount = 0.0f;
            if (movement)
            {
                if (amount < 1.0f)
                    amount += 0.001f;
                WaterDrops::FillScreenMoving(amount);
            }
            else
                amount = 0.0f;
        }
    }; injector::MakeInline<ActiveBoatObjectUpdatePreSimHook>(pattern.get_first(0), pattern.get_first(7));
    injector::WriteMemory<uint8_t>(pattern.get_first(5), 0x56, true); //push esi
    injector::WriteMemory<uint8_t>(pattern.get_first(6), 0x57, true); //push edi
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
