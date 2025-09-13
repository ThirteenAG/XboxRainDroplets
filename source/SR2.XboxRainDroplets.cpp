#include <injector\injector.hpp>
#include <injector\hooking.hpp>
#include <injector\calling.hpp>
#include <injector\utility.hpp>
#include <injector\assembly.hpp>
#include "xrd.h"

static auto HandleDynAddress = GetModuleHandle(nullptr);
template<typename AT>
AT DynAddress(AT address)
{
    static_assert(sizeof(AT) == sizeof(uintptr_t), "AT must be pointer sized");

    uintptr_t inputAddr = std::bit_cast<uintptr_t>(address);

        uintptr_t baseAddr = std::bit_cast<uintptr_t>(HandleDynAddress);

#ifdef _WIN64
        uintptr_t result = baseAddr - 0x140000000ULL + inputAddr;
#else
        uintptr_t result = baseAddr - 0x400000UL + inputAddr;
#endif
        return std::bit_cast<AT>(result);

}

inline uintptr_t operator""_g(unsigned long long val)
{
    return DynAddress(static_cast<uintptr_t>(val));
}

static IDirect3DDevice9** ppDevice;

using vector3 = RwV3d;


struct batch_queue
{
    int command_type;
    int command_data_offset;
};


struct batch
{
    batch_queue batch_commands[3000];
    int current_batch_no;
    unsigned __int8 parms[92160];
    int parmsize;
};

struct matrix {
    vector3 x;
    vector3 y;
    vector3 z;
};

typedef char(__cdecl* gr_batch_add_command_to_bufferT)(int, ...);
gr_batch_add_command_to_bufferT gr_batch_add_command_to_buffer = (gr_batch_add_command_to_bufferT)0xCF8EA0_g;

typedef BOOL(__cdecl* gr_is_batch_contextT)();
gr_is_batch_contextT gr_is_batch_context = (gr_is_batch_contextT)0x56C310_g;

bool is_game_paused() {
    return (*(int*)0x2527C08_g > 0);
}

bool rendered = false;

static uintptr_t is_pos_in_interior_addr = 0x4AB430_g;
char __declspec(naked) is_pos_in_interiorasm(vector3* pos)
{
    _asm {
        push ebp
        mov ebp, esp
        sub esp, __LOCAL_SIZE

        mov edi, pos
        call is_pos_in_interior_addr

        mov esp, ebp
        pop ebp
        ret
    }
}

bool is_pos_in_interior(vector3* pos) {

    return is_pos_in_interiorasm(pos) != 0;

}
bool* rain_havok_allow_render = (bool*)0x02526D7A_g;
bool is_in_interior = false;

float WeatherMultiplier = 4.85f;

bool bBloodPlayer = true;
bool bWaterGuns = true;

static void rain_render_hook() {
    if (rendered) {
        rendered = false;
        //return;
    }
    auto cam_matrix = (matrix*)0x025F5B38_g;
    auto rvec = cam_matrix->x;
    auto uvec = cam_matrix->y;
    auto fvec = cam_matrix->z;
    auto pos = *(RwV3d*)(0x025F5B14_g);

    rvec.x = -rvec.x;
    rvec.y = -rvec.y;
    rvec.z = -rvec.z;

    WaterDrops::right = rvec;
    WaterDrops::up = uvec;
    WaterDrops::at = fvec;

    WaterDrops::pos = pos;
    auto pDevice = *(LPDIRECT3DDEVICE9*)ppDevice;
    if (!is_in_interior && *rain_havok_allow_render)
        WaterDrops::ms_rainIntensity = (*(float*)0x02526D74_g) * WeatherMultiplier;
    else
        WaterDrops::ms_rainIntensity = 0.f;
    if(!is_game_paused())
    WaterDrops::Process(pDevice);
    WaterDrops::Render(pDevice);

    rendered = true;
    
}

void __cdecl render_batched(int data) {
    rain_render_hook();
}

SafetyHookInline game_render_do_frameT;

void __cdecl game_render_do_frame_hook() {
    rain_render_hook();
    game_render_do_frameT.ccall();


}
struct RENDER_BUFFER
{
    void* Array;
    int Array_length;
    void* ArrayExecuteWritePosition;
    void* ArrayWritePosition;
    void* ArrayWritePositionStart;
    void* ArrayReadPosition;
};

RENDER_BUFFER* PC_RENDER_BUFFER = (RENDER_BUFFER*)0x033D62EC_g;

void* cached_read = NULL;

bool rendered_this_frame = false;

SafetyHookInline huds_renderT;

int hud_number_execute = 0;

void __cdecl huds_hook() {

    batch* pre_batch = *(batch**)(0x0230597C_g);


    rendered_this_frame = false;
    hud_number_execute = pre_batch->current_batch_no;
    huds_renderT.ccall();

}

bool bounding_box_check(vector3& pos, vector3& bmin, vector3& bmax) {
    if (pos.x >= bmin.x && pos.x <= bmax.x &&
        pos.y >= bmin.y && pos.y <= bmax.y &&
        pos.z >= bmin.z && pos.z <= bmax.z) {
        return true;
    }
    return false;
}

bool is_object_player(uintptr_t object) {
    if (object == *(uintptr_t*)0x021703D4_g)
        return true;
    return false;
}

typedef uintptr_t(__fastcall* GetPointerT)(uintptr_t VehiclePointer);
GetPointerT GetPointer = (GetPointerT)0x00AE28F0_g;

uintptr_t FindPlayer() {
    return *(uintptr_t*)(0x21703D4_g);
}

uintptr_t FindPlayersVehicle() {
    if (!FindPlayer())
        return NULL;
    auto players_vehicle_handle = FindPlayer() + 0xD74;
    if (players_vehicle_handle) {
        uintptr_t value = *(uintptr_t*)players_vehicle_handle;
        if (value) {
            return GetPointer(value);
        }
    }
    return NULL;
}

struct effect_start_data
{
    int effects_handle;
    vector3 pos;
    matrix orient;
    char padding[44];
    unsigned int name_checksum;
    unsigned int unk1;
    float unk2;
};
SafetyHookInline effect_playT;

constexpr int blood_drip_hit_multi_hash = 0x7F7190FA;
constexpr int blood_drip_hit_hash = 0xDA75A2B2;
constexpr int blood_drip_multi_hash = 0x510C241A;
constexpr int blood_drip_strm_hash = 0xD30AFDDC;
constexpr int blood_headshot_hash = 0xEA2E1192;
constexpr int blood_slice_sm_hash = 0xD65E298A;
constexpr int blood_slice_hash = 0x30DF6A09;
constexpr int blood_sputter_hash = 0xDB0A9220;
constexpr int blood_hit01_hash = 0xBFB53B3B;

int* Water1_Effect_handle = (int*)0xF9E520_g;
int* Water2_effect_handle = (int*)0xF9E538_g;

int __cdecl effects_play_hook(effect_start_data* effect) {
    int handle = effect_playT.ccall<int>(effect);
    for (int i = 0; i < 6; i++) {
        if (effect->effects_handle == Water1_Effect_handle[i]) {
            auto len = WaterDrops::GetDistanceBetweenEmitterAndCamera(effect->pos);
            WaterDrops::FillScreenMoving(WaterDrops::GetDropsAmountBasedOnEmitterDistance(len, 50.0f, 0.125f));
            break;
        }
        else if (effect->effects_handle == Water2_effect_handle[i]) {
            auto len = WaterDrops::GetDistanceBetweenEmitterAndCamera(effect->pos);
            WaterDrops::FillScreenMoving(WaterDrops::GetDropsAmountBasedOnEmitterDistance(len, 50.0f, 100.0f));
            break;
        }
    }

    return handle;
}

void Init()
{

    CIniReader iniReader;
    static auto graphics_rest = safetyhook::create_mid(0xD1FEE3_g, [](SafetyHookContext& ctx) {
        WaterDrops::Reset();
        });
    effect_playT = safetyhook::create_inline(0x50E330_g, &effects_play_hook);

    static auto execute = safetyhook::create_mid(0xCF8FEC_g, [](SafetyHookContext& ctx) {
        if (hud_number_execute == ctx.esi + 1) {
            cached_read = PC_RENDER_BUFFER->ArrayWritePosition;
        }
        });

    huds_renderT = safetyhook::create_inline(0x793BB0_g, huds_hook);
    WaterDrops::ReadIniSettings(true);

    bBloodPlayer = iniReader.ReadInteger("SR2", "BloodDropsForPlayer", 1) != 0;
    WeatherMultiplier = iniReader.ReadFloat("SR2", "WeatherMultiplier", 4.85f);
    bWaterGuns = iniReader.ReadInteger("SR2", "WaterGuns", 1) != 0;

    WaterDrops::ms_rainIntensity = 0.0f;
    ppDevice = *hook::get_pattern<IDirect3DDevice9**>("A1 ? ? ? ? 8B 08 8B 91 ? ? ? ? 83 EC", 1);

    static auto game_loop = safetyhook::create_mid(0x68CB69_g, [](SafetyHookContext& ctx) {
        is_in_interior = is_pos_in_interior((RwV3d*)(0x025F5B14_g));
        });



    auto pattern = hook::pattern("E8 ? ? ? ? 8B 4D ? 89 0D");
    static auto D3D9CreateDevice = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& ctx)
    {
        auto pDevice = *ppDevice;
#ifdef SIRE_INCLUDE_DX9
        Sire::Init(Sire::SIRE_RENDERER_DX9, pDevice);
#endif
    });

    static auto chainsaw_blood = safetyhook::create_mid(0x971550_g, [](SafetyHookContext& ctx) {
        RwV3d* hit_pos = (RwV3d*)ctx.edx;

        auto len = WaterDrops::GetDistanceBetweenEmitterAndCamera(hit_pos);
        if (WaterDrops::bBloodDrops)
        WaterDrops::FillScreenMoving(WaterDrops::GetDropsAmountBasedOnEmitterDistance(len, 50.0f, 95.0f), true);
        });

    static auto sword_blood = safetyhook::create_mid(0x97B28F, [](SafetyHookContext& ctx) {
        RwV3d* hit_pos = (RwV3d*)ctx.edx;

        auto len = WaterDrops::GetDistanceBetweenEmitterAndCamera(hit_pos);
        if (WaterDrops::bBloodDrops)
        WaterDrops::FillScreenMoving(WaterDrops::GetDropsAmountBasedOnEmitterDistance(len, 50.0f, 100.0f), true);
        });


    static bool is_player = false;

    static auto blood_hook_is_player = safetyhook::create_mid(0x978775_g, [](SafetyHookContext& ctx) {
        is_player = is_object_player(ctx.ebp);
        });

    static auto blood_hook = safetyhook::create_mid(0x4BCF23_g, [](SafetyHookContext& ctx) {
        if (!WaterDrops::bBloodDrops)
            return;
        int blood_strength = *(int*)(ctx.esp + 0x98);

        RwV3d* hit_pos = (RwV3d*)ctx.edx;

        auto len = WaterDrops::GetDistanceBetweenEmitterAndCamera(hit_pos);
        if(!is_player)
        WaterDrops::FillScreenMoving(WaterDrops::GetDropsAmountBasedOnEmitterDistance(len, 50.0f, 80.0f), true);
        else if(bBloodPlayer && is_player)
        WaterDrops::FillScreenMoving(WaterDrops::GetDropsAmountBasedOnEmitterDistance(len, 50.0f, 1.0f), true);
        is_player = false;

        });

    static auto human_water_hit = safetyhook::create_mid(0x56A7DB_g, [](SafetyHookContext& ctx) {
        if (!bWaterGuns || !is_object_player(ctx.ebp))
            return;
        auto is_sewage = *(bool*)(ctx.esp + 0x1C);

        if (!is_sewage)
            WaterDrops::FillScreenMoving(25.f, false);
        else
            WaterDrops::FillScreenMovingColor(125.f, 210, 105, 30);
        });

    static auto vehicle_water_hit = safetyhook::create_mid(0x56A6D8_g, [](SafetyHookContext& ctx) {
        if (!bWaterGuns || ctx.edi != FindPlayersVehicle())
            return;
        auto is_sewage = (ctx.ebx & 0xFF) != 0;

        if (!is_sewage)
            WaterDrops::FillScreenMoving(25.f, false);
        else
            WaterDrops::FillScreenMovingColor(125.f, 210, 105, 30);
        });


    static auto read = safetyhook::create_mid(0xD203D8_g, [](SafetyHookContext& ctx) {
        if (PC_RENDER_BUFFER->ArrayReadPosition == cached_read) {
            //printf("read!\n");
            rendered_this_frame = true;
            rain_render_hook();
        }
        });
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