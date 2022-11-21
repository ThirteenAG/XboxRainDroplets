#include "xrd.h"
#include "snow.h"

static uint32_t crc32_tab[] = {
  0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
  0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
  0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
  0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
  0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
  0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
  0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
  0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
  0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
  0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
  0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
  0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
  0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
  0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
  0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
  0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
  0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
  0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
  0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
  0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
  0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
  0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
  0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
  0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
  0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
  0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
  0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
  0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
  0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
  0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
  0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
  0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
  0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
  0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
  0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
  0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
  0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
  0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
  0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
  0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
  0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
  0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
  0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

uint32_t crc32(uint32_t crc, const void* buf, size_t size)
{
    const uint8_t* p;

    p = (uint8_t*)buf;
    crc = crc ^ ~0U;

    while (size--)
        crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);

    return crc ^ ~0U;
}

class CParticleEffectData
{
public:
    static int GetID(void* _this)
    {
        return *(int*)(*(int*)_this + 4);
    }
    static int IsContinuous(void* _this)
    {
        return *(int*)(*(int*)_this + 8) & 1;
    }
};

uintptr_t dw4DE72F = 0x4DE72F;
class CParticleEffectManager 
{
public:
    static int* __fastcall AddParticleEffect(CParticleEffectManager* _this, void* edx, int* a2, int a3, int a4)
    {
        return ((int* (__thiscall*)(CParticleEffectManager*, int* a2, int a3, int a4))dw4DE72F)(_this, a2, a3, a4);
    }
    static void* __fastcall GetEffect(CParticleEffectManager* _this, void* edx, int a2)
    {
        unsigned int v2; // esi
        int v3; // ebx
        int* i; // edi

        v2 = (int)_this + 0x6738; //v2 = this[6606];
        v3 = 0;
        if (!v2)
            return 0;
        for (i = (int*)_this + 6; a2 != CParticleEffectData::GetID(i); i += 0x84)
        {
            if (++v3 >= v2)
                return 0;
        }
        return i;
    }
    static void* __fastcall AddOneShotParticleEffect(CParticleEffectManager* _this, void* edx, int* a2, int a3, int a4)
    {
        auto Effect = GetEffect(_this, edx, a3);
        if (Effect && !(uint8_t)CParticleEffectData::IsContinuous(Effect))
        {
            AddParticleEffect(_this, edx, a2, (int)Effect, a4);
            
            auto crc = crc32(0, *(void**)Effect, 0x84);
            if (crc == 0x08d8c95b) //hydrant
            {
                RwV3d vec = *(RwV3d*)(a4 + 0x30);
                RwV3d prt_pos = { vec.x, vec.z, vec.y };
                WaterDrops::RegisterSplash(&prt_pos, 10.0f, 2500, 100.0f);
            } 
            else if (crc == 0x4faf489b)
            {
                WaterDrops::RegisterSplash(&WaterDrops::pos, 50.0f, 14);
            }
            else if (crc == 0x443a6fb0)
            {
                if (WaterDrops::bBloodDrops)
                {
                    RwV3d vec = *(RwV3d*)(a4 + 0x30);
                    RwV3d prt_pos = { vec.x, vec.z, vec.y };
                    auto len = WaterDrops::GetDistanceBetweenEmitterAndCamera(prt_pos);
                    WaterDrops::FillScreenMoving(WaterDrops::GetDropsAmountBasedOnEmitterDistance(len, 50.0f, 100.0f), true);
                }
            }
        }
        else
        {
            a2[0] = 0;
            a2[1] = 0;
        }
        return a2;
    }
};

bool bMenu = false;
bool bCamNoRain = true;
bool bCamNoRain2 = true;
void __fastcall CActiveNodeCollectionManager__Apply(void* _this, void* edx)
{
    unsigned int v2; // ebx
    DWORD* v3; // edi
    int v4; // eax

    v2 = 0;
    if (*((DWORD*)_this + 136))
    {
        v3 = (DWORD*)((char*)_this + 4);
        do
        {
            v4 = *(v3 - 1);
            if (v4)
            {
                if (v4 == 1)
                {
                    bCamNoRain = false;
                    (*(void(__thiscall**)(DWORD, DWORD))(**((DWORD**)_this + 137) + 8))(*((DWORD*)_this + 137), *v3);
                }
            }
            else
            {
                bCamNoRain = true;
                (*(void(__thiscall**)(DWORD, DWORD))(**((DWORD**)_this + 137) + 16))(*((DWORD*)_this + 137), *v3);
            }
            ++v2;
            v3 += 2;
        } while (v2 < *((DWORD*)_this + 136));
    }
    *((DWORD*)_this + 136) = 0;
}

void Init()
{
    WaterDrops::ReadIniSettings();
    
    WaterDrops::ms_rainIntensity = 0.0f;

    static LPDIRECT3DDEVICE9 pDevice = nullptr;
    static auto pCamMatrix = *hook::get_pattern<uint32_t>("BF ? ? ? ? F3 A5 BE", 1);
    dw4DE72F = (uint32_t)hook::get_pattern<uint32_t>("55 8B EC 51 53 56 8B F1 8B 0D ? ? ? ? 57 E8 ? ? ? ? 8B 0D ? ? ? ? 8B F8 E8", 0);
    static auto dw70C5B0 = *hook::get_pattern<uint32_t*>("A1 ? ? ? ? 8B 40 04 53 8B D9", 1);
    static auto dw6E8E18 = *hook::get_pattern<uint32_t*>("A3 ? ? ? ? 8B 4E 30", 1);
    static auto dw9804F0 = *hook::get_pattern<uint32_t>("BB ? ? ? ? 8B CD", 1);

    auto pattern = hook::pattern("8B 81 ? ? ? ? 8B 08 50 FF 91 ? ? ? ? C3");
    struct EndSceneHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.eax = *(uint32_t*)(regs.ecx + 0x164);
            pDevice = (LPDIRECT3DDEVICE9)(regs.eax);
        }
    }; injector::MakeInline<EndSceneHook>(pattern.get_first(0), pattern.get_first(6));

    pattern = hook::pattern("E8 ? ? ? ? FF 37 8B 4D F0");
    struct RenderHook
    {
        void operator()(injector::reg_pack& regs)
        {
            if (!pDevice || *(uint32_t*)(regs.ebp - 4) != 0)
                return;

            auto pGameTime = *dw70C5B0;
            if (pGameTime)
            {
                auto timer = *(uint32_t*)(pGameTime + 4);
                //auto weeks = (timer / 10000) / 168;
                //auto days = ((timer / 10000) % 168) / 24;
                auto hrs = (timer / 10000) % 24;

                static auto randomRainIntensity = 0.0f;
                static std::vector<int> chanceofRain = { 1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }; // twice a day
                static bool once = false;
                if (timer > 1000 && hrs == 0 && !once)
                {
                    randomRainIntensity = (1.0f - 0.65f) + WaterDrops::GetRandomFloat(0.65f);
                    
                    std::random_device rd;
                    std::mt19937 g(rd());
                    std::shuffle(chanceofRain.begin(), chanceofRain.end(), g);
                    once = true;
                }
                else if (hrs == 1)
                    once = false;

                if (chanceofRain.at(hrs) && (!bCamNoRain && !bCamNoRain2))
                {
                    WaterDrops::ms_rainIntensity = randomRainIntensity;
                    CSnow::targetSnow = randomRainIntensity;
                }
                else
                {
                    WaterDrops::ms_rainIntensity = 0.0f;
                    CSnow::targetSnow = 0.0f;
                }
                
                if (WaterDrops::bEnableSnow)
                    CSnow::targetSnow = 1.0f;
            }

            auto right = *(RwV3d*)(pCamMatrix + 0x00);
            auto up = *(RwV3d*)(pCamMatrix + 0x10);
            auto at = *(RwV3d*)(pCamMatrix + 0x20);
            auto pos = *(RwV3d*)(pCamMatrix + 0x30);

            WaterDrops::right = { -right.x, -right.z, -right.y };
            WaterDrops::up = { up.x, up.z, up.y };
            WaterDrops::at = { -at.x, -at.z, -at.y };
            WaterDrops::pos = { pos.x, pos.z, pos.y };

            WaterDrops::Process(pDevice);
            if (!bMenu && *dw6E8E18 != 256)
            {
                if (WaterDrops::bEnableSnow)
                    WaterDrops::ms_rainIntensity = 0.0f;
                WaterDrops::Render(pDevice);
            }
            
            if (!bMenu && *dw6E8E18 != 256)
            {
                static float ts = 0.0f;

                static RwMatrix camMatrix;
                camMatrix.right = { right.x, right.z, right.y };
                camMatrix.up = { up.x, up.z, up.y };
                camMatrix.at = { at.x, at.z, at.y };
                camMatrix.pos = { pos.x, pos.z, pos.y };

                static RwMatrix viewMatrix;
                viewMatrix.right = *(RwV3d*)(dw9804F0 + 0x100 + 0x00);
                viewMatrix.up = *(RwV3d*)(dw9804F0 + 0x100 + 0x10);
                viewMatrix.at = *(RwV3d*)(dw9804F0 + 0x100 + 0x20);
                viewMatrix.pos = *(RwV3d*)(dw9804F0 + 0x100 + 0x30);

                ts = WaterDrops::GetTimeStepInMilliseconds();

                CSnow::zn = 0.0f;
                CSnow::zf = 1.0f;

                CSnow::AddSnow(pDevice, WaterDrops::ms_fbWidth, WaterDrops::ms_fbHeight, &camMatrix, &viewMatrix, &ts, WaterDrops::bEnableSnow ? false : true);
            }
        }
    }; injector::MakeInline<RenderHook>(pattern.get_first(0));
    
    pattern = hook::pattern("8B 83 ? ? ? ? 6A 0E 59");
    struct ResetHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.eax = *(uint32_t*)(regs.ebx + 0x164);
            WaterDrops::Reset();
        }
    }; injector::MakeInline<ResetHook>(pattern.get_first(0), pattern.get_first(6));

    pattern = hook::pattern("55 8B EC 56 57 FF 75 0C 8B F9 E8 ? ? ? ? 8B F0 85 F6 74 0B 8B CE E8 ? ? ? ? 84 C0 74 0C 8B 45 08");
    injector::MakeJMP(pattern.get_first(0), &CParticleEffectManager::AddOneShotParticleEffect, true);
    
    pattern = hook::pattern("89 30 8B 52 04 89 50 04 5E FF 81 ? ? ? ? C2 04 00");
    struct CamNoRainHook
    {
        void operator()(injector::reg_pack& regs)
        {
            *(uint32_t*)(regs.eax) = regs.esi;
            regs.edx = *(uint32_t*)(regs.edx + 4);
    
            if (!regs.esi && !regs.ebx)
                bCamNoRain2 = true;
            else if (!(regs.edi == 1 && regs.esi == 1))
                bCamNoRain2 = false;
        }
    }; injector::MakeInline<CamNoRainHook>(pattern.count_hint(10).get(4).get<void>(0));

    pattern = hook::pattern("E8 ? ? ? ? 8B 0D ? ? ? ? E8 ? ? ? ? 8B CF E8 ? ? ? ? 8B CF E8");
    injector::MakeCALL(pattern.get_first(0), CActiveNodeCollectionManager__Apply, true);
    pattern = hook::pattern("E8 ? ? ? ? 80 7E 11 00");
    injector::MakeCALL(pattern.get_first(0), CActiveNodeCollectionManager__Apply, true);

    pattern = hook::pattern("8B C4 89 18 8B 01 68 ? ? ? ? FF 50 10 8B 46 10 48 74 4E 83 E8 03 74 1C");
    struct DeactivateMenuHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.eax = regs.esp;
            *(uint32_t*)(regs.eax) = regs.ebx;
            regs.eax = *(uint32_t*)(regs.ecx);
            bMenu = 0;
        }
    }; injector::MakeInline<DeactivateMenuHook>(pattern.get_first(0), pattern.get_first(6));

    pattern = hook::pattern("8B C4 89 18 8B 01 68 ? ? ? ? FF 50 10 5F 5E 5B C2 08 00");
    struct ActivateMenuHook
    {
        void operator()(injector::reg_pack& regs)
        {
            regs.eax = regs.esp;
            *(uint32_t*)(regs.eax) = regs.ebx;
            regs.eax = *(uint32_t*)(regs.ecx);
            bMenu = 1;
        }
    }; injector::MakeInline<ActivateMenuHook>(pattern.get_first(0), pattern.get_first(6));
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