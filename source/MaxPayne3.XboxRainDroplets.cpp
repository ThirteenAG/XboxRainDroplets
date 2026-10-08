#include <injector\injector.hpp>
#include <injector\hooking.hpp>
#include <injector\calling.hpp>
#include <injector\utility.hpp>
#include <injector\assembly.hpp>
#define XRD_ENABLE_D3D9
#define XRD_ENABLE_D3D10
#define XRD_ENABLE_D3D10_1
#define XRD_ENABLE_D3D11
#define XRD_ENABLE_D3D12
#include "xrd/xrd.h"
#include <atomic>
#include <mutex>
#include <cmath>

namespace
{
    // Optional Fusion Fix menu API v1. No import-library dependency.
    struct FFMenuChoice { float value; const char* text; };
    using MenuSetOption = int (__cdecl*)(const char*, unsigned int, unsigned int,
        const char*, const char*, const FFMenuChoice*, unsigned int, float, float, float,
        float (__cdecl*)(void*), void (__cdecl*)(void*, float), void*);
    using MenuSetPage = int (__cdecl*)(const char*, const char*, const char*, void (__cdecl*)(void*), void*);
    std::atomic<bool> menuRegistered = false;
    std::atomic<bool> menuPending = false;
    std::filesystem::path menuSettingsPath;
    std::mutex menuSettingsMutex;
    bool settingsDirty = false;
    struct Setting
    {
        const char* key;
        const char* label;
        float initial, minimum, maximum, step;
        bool toggle;
        float value = 0;
        const char* section = "MAIN";
    };
    // Defaults match the shipped MP3 INI; the additional engine settings use
    // ReadIniSettings defaults. Values imported from an INI remain off-step.
    Setting menuSettings[] = {
        {"Enabled", "Rain droplets", 1, 0, 1, 1, true},
        {"MinSize", "Minimum drop size", 4, 1, 100, 1, false},
        {"MaxSize", "Maximum drop size", 15, 1, 100, 1, false},
        {"MaxDrops", "Maximum drops", 3000, 100, 16000, 100, false},
        {"MaxMovingDrops", "Maximum moving drops", 6000, 100, 16000, 100, false},
        {"EnableGravity", "Gravity", 1, 0, 1, 1, true},
        {"Refractions", "Refractions", 1, 0, 1, 1, true},
        {"SpeedAdjuster", "Speed multiplier", 1, 0, 10, 0.1f, false},
        {"MoveStep", "Movement step", 1, 0.1f, 10, 0.1f, false},
        {"BloodDrops", "Blood droplets", 1, 0, 1, 1, true},
        {"EnableSnow", "Snow", 0, 0, 1, 1, true, 0, "BONUS"},
        {"ForceRain", "Force rain", 0, 0, 1, 1, true},
    };
    void SaveMenuSettings()
    {
        CIniReader settings(menuSettingsPath);
        for (const auto& entry : menuSettings)
            if (entry.step >= 1) settings.WriteInteger(entry.section, entry.key, static_cast<int>(entry.value));
            else settings.WriteFloat(entry.section, entry.key, entry.value);
    }
    float __cdecl GetDroplets(void* context)
    {
        std::lock_guard lock(menuSettingsMutex);
        return static_cast<Setting*>(context)->value;
    }
    void __cdecl SetDroplets(void* context, float value)
    {
        std::lock_guard lock(menuSettingsMutex);
        auto& entry = *static_cast<Setting*>(context);
        if (!std::isfinite(value)) return;
        entry.value = std::clamp(value, entry.minimum, entry.maximum);
        // Keep the size interval valid when either endpoint is changed.
        if (&entry == &menuSettings[1]) menuSettings[2].value = (std::max)(menuSettings[2].value, entry.value);
        if (&entry == &menuSettings[2]) menuSettings[1].value = (std::min)(menuSettings[1].value, entry.value);
        settingsDirty = true;
        SaveMenuSettings();
    }
    void __cdecl ResetDroplets(void*)
    {
        std::lock_guard lock(menuSettingsMutex);
        for (auto& entry : menuSettings) entry.value = entry.initial;
        settingsDirty = true;
        SaveMenuSettings();
    }
    // Only the render callback changes WaterDrops or resizes its pointer pools.
    bool ApplyMenuSettings()
    {
        if (!menuRegistered) return true;
        std::lock_guard lock(menuSettingsMutex);
        if (settingsDirty)
        {
            WaterDrops::MinSize = static_cast<int>(menuSettings[1].value);
            WaterDrops::MaxSize = static_cast<int>(menuSettings[2].value);
            WaterDrops::MaxDrops = static_cast<int>(menuSettings[3].value);
            WaterDrops::MaxDropsMoving = static_cast<int>(menuSettings[4].value);
            WaterDrops::bGravity = menuSettings[5].value != 0;
            WaterDrops::bRefractions = menuSettings[6].value != 0;
            WaterDrops::fSpeedAdjuster = menuSettings[7].value;
            WaterDrops::fMoveStep = menuSettings[8].value;
            WaterDrops::bBloodDrops = menuSettings[9].value != 0;
            // through SetSnow, which loads the shapes of the flakes or the drops
            WaterDrops::SetSnow(menuSettings[10].value != 0);
            WaterDrops::bForceRain = menuSettings[11].value != 0;
            // Init already loaded the INI. Do not overwrite the CFG on first render.
            WaterDrops::ms_iniRead = true;
            WaterDrops::ResizePools();
            WaterDrops::Clear();
            settingsDirty = false;
        }
        return menuSettings[0].value != 0;
    }
    bool RegisterMenuText(HMODULE fusionFix)
    {
        using SetText = int (__cdecl*)(const char*, unsigned int, const char*);
        auto setText = reinterpret_cast<SetText>(GetProcAddress(fusionFix, "FusionFix_MenuSetText"));
        if (!setText) return false;
        // Register English fallbacks even if a language resource is missing.
        for (const auto& entry : menuSettings)
            if (setText((std::string("XboxRainDroplets.") + entry.label).c_str(), 0, entry.label) != 1) return false;
        if (setText("XboxRainDroplets.XBOX RAIN DROPLETS", 0, "XBOX RAIN DROPLETS") != 1 ||
            setText("XboxRainDroplets.Customize rain droplets.", 0, "Customize rain droplets.") != 1) return false;
        // English, French, German, Italian, Spanish, Japanese, Russian,
        // Brazilian Portuguese, Polish, Korean. UTF-8 literals are independent of the build code page.
        static const struct { const char* key; const char8_t* text[10]; } translations[] = {
            {"XboxRainDroplets.XBOX RAIN DROPLETS", {
                u8"XBOX RAIN DROPLETS",
                u8"XBOX RAIN DROPLETS",
                u8"XBOX RAIN DROPLETS",
                u8"XBOX RAIN DROPLETS",
                u8"XBOX RAIN DROPLETS",
                u8"XBOX RAIN DROPLETS",
                u8"XBOX RAIN DROPLETS",
                u8"XBOX RAIN DROPLETS",
                u8"XBOX RAIN DROPLETS",
                u8"XBOX RAIN DROPLETS",
            }},
            {"XboxRainDroplets.Customize rain droplets.", {
                u8"Customize rain droplets.",
                u8"Personnaliser les gouttes de pluie.",
                u8"Regentropfen anpassen.",
                u8"Personalizza le gocce di pioggia.",
                u8"Personaliza las gotas de lluvia.",
                u8"雨滴エフェクトを設定します。",
                u8"Настройка капель дождя.",
                u8"Personalize as gotas de chuva.",
                u8"Dostosuj krople deszczu.",
                u8"빗방울 효과를 설정합니다.",
            }},
            {"XboxRainDroplets.Rain droplets", {
                u8"Rain droplets",
                u8"Gouttes de pluie",
                u8"Regentropfen",
                u8"Gocce di pioggia",
                u8"Gotas de lluvia",
                u8"雨滴",
                u8"Капли дождя",
                u8"Gotas de chuva",
                u8"Krople deszczu",
                u8"빗방울",
            }},
            {"XboxRainDroplets.Minimum drop size", {
                u8"Minimum drop size",
                u8"Taille minimale des gouttes",
                u8"Minimale Tropfengröße",
                u8"Dimensione minima gocce",
                u8"Tamaño mínimo de gotas",
                u8"雨滴の最小サイズ",
                u8"Минимальный размер капель",
                u8"Tamanho mínimo das gotas",
                u8"Minimalny rozmiar kropli",
                u8"최소 물방울 크기",
            }},
            {"XboxRainDroplets.Maximum drop size", {
                u8"Maximum drop size",
                u8"Taille maximale des gouttes",
                u8"Maximale Tropfengröße",
                u8"Dimensione massima gocce",
                u8"Tamaño máximo de gotas",
                u8"雨滴の最大サイズ",
                u8"Максимальный размер капель",
                u8"Tamanho máximo das gotas",
                u8"Maksymalny rozmiar kropli",
                u8"최대 물방울 크기",
            }},
            {"XboxRainDroplets.Maximum drops", {
                u8"Maximum drops",
                u8"Nombre maximal de gouttes",
                u8"Maximale Tropfenanzahl",
                u8"Numero massimo di gocce",
                u8"Cantidad máxima de gotas",
                u8"雨滴の最大数",
                u8"Максимальное число капель",
                u8"Quantidade máxima de gotas",
                u8"Maksymalna liczba kropli",
                u8"최대 물방울 수",
            }},
            {"XboxRainDroplets.Maximum moving drops", {
                u8"Maximum moving drops",
                u8"Gouttes mobiles maximales",
                u8"Max. bewegliche Tropfen",
                u8"Massimo gocce in movimento",
                u8"Máximo de gotas en movimiento",
                u8"動く雨滴の最大数",
                u8"Максимум движущихся капель",
                u8"Máximo de gotas em movimento",
                u8"Maks. liczba ruchomych kropli",
                u8"움직이는 물방울 최대 수",
            }},
            {"XboxRainDroplets.Gravity", {
                u8"Gravity",
                u8"Gravité",
                u8"Schwerkraft",
                u8"Gravità",
                u8"Gravedad",
                u8"重力",
                u8"Гравитация",
                u8"Gravidade",
                u8"Grawitacja",
                u8"중력",
            }},
            {"XboxRainDroplets.Refractions", {
                u8"Refractions",
                u8"Réfraction",
                u8"Lichtbrechung",
                u8"Rifrazione",
                u8"Refracción",
                u8"屈折",
                u8"Преломление",
                u8"Refração",
                u8"Załamanie światła",
                u8"굴절",
            }},
            {"XboxRainDroplets.Speed multiplier", {
                u8"Speed multiplier",
                u8"Multiplicateur de vitesse",
                u8"Geschwindigkeitsfaktor",
                u8"Moltiplicatore velocità",
                u8"Multiplicador de velocidad",
                u8"速度倍率",
                u8"Множитель скорости",
                u8"Multiplicador de velocidade",
                u8"Mnożnik prędkości",
                u8"속도 배율",
            }},
            {"XboxRainDroplets.Movement step", {
                u8"Movement step",
                u8"Pas de déplacement",
                u8"Bewegungsschritt",
                u8"Passo di movimento",
                u8"Paso de movimiento",
                u8"移動ステップ",
                u8"Шаг движения",
                u8"Passo de movimento",
                u8"Krok ruchu",
                u8"이동 간격",
            }},
            {"XboxRainDroplets.Blood droplets", {
                u8"Blood droplets",
                u8"Gouttes de sang",
                u8"Bluttropfen",
                u8"Gocce di sangue",
                u8"Gotas de sangre",
                u8"血の滴",
                u8"Капли крови",
                u8"Gotas de sangue",
                u8"Krople krwi",
                u8"핏방울",
            }},
            {"XboxRainDroplets.Snow", {
                u8"Snow",
                u8"Neige",
                u8"Schnee",
                u8"Neve",
                u8"Nieve",
                u8"雪",
                u8"Снег",
                u8"Neve",
                u8"Śnieg",
                u8"눈",
            }},
            {"XboxRainDroplets.Force rain", {
                u8"Force rain",
                u8"Forcer la pluie",
                u8"Regen erzwingen",
                u8"Forza pioggia",
                u8"Forzar lluvia",
                u8"雨滴を常に表示",
                u8"Принудительный дождь",
                u8"Forçar chuva",
                u8"Wymuś deszcz",
                u8"비 강제 적용",
            }},
        };
        for (const auto& entry : translations)
            for (unsigned int language = 0; language < 10; ++language)
                if (setText(entry.key, language, reinterpret_cast<const char*>(entry.text[language])) != 1)
                    OutputDebugStringA("Xbox Rain Droplets: translation rejected; using English.\n");
        return true;
    }
    void __cdecl RegisterMenuOnFrontend(void* context)
    {
        auto setOption = reinterpret_cast<MenuSetOption>(context);
        try
        {
            auto module = GetModuleHandleW(L"MaxPayne3.FusionFix.asi");
            auto setPage = reinterpret_cast<MenuSetPage>(GetProcAddress(module, "FusionFix_MenuSetPage"));
            if (setPage)
            {
                const bool translated = RegisterMenuText(module);
                const int page = setPage("XboxRainDroplets",
                    translated ? "XboxRainDroplets.XBOX RAIN DROPLETS" : "XBOX RAIN DROPLETS",
                    translated ? "XboxRainDroplets.Customize rain droplets." : "Customize rain droplets.", ResetDroplets, nullptr);
                if (page >= 5)
                {
                    std::lock_guard lock(menuSettingsMutex);
                    if (menuSettingsPath.empty())
                    {
                        CIniReader legacy("");
                        menuSettingsPath = legacy.GetIniPath();
                        menuSettingsPath.replace_extension(L".cfg");
                        CIniReader settings(menuSettingsPath);
                        for (auto& entry : menuSettings)
                        {
                            float value = settings.ReadFloat(entry.section, entry.key,
                                legacy.ReadFloat(entry.section, entry.key, entry.initial));
                            entry.value = std::isfinite(value) ? std::clamp(value, entry.minimum, entry.maximum) : entry.initial;
                        }
                        menuSettings[2].value = (std::max)(menuSettings[1].value, menuSettings[2].value);
                    }
                    bool complete = true;
                    for (auto& entry : menuSettings)
                    {
                        auto id = std::string("XboxRainDroplets.") + entry.key;
                        auto label = translated ? std::string("XboxRainDroplets.") + entry.label : entry.label;
                        complete &= setOption(id.c_str(), page, entry.toggle ? 1 : 2, label.c_str(),
                            label.c_str(), nullptr, 0, entry.minimum, entry.maximum, entry.step,
                            GetDroplets, SetDroplets, &entry) == 1;
                    }
                    settingsDirty = complete;
                    menuRegistered = complete;
                    OutputDebugStringA(complete ? "Xbox Rain Droplets: menu page registered.\n" :
                        "Xbox Rain Droplets: menu registration rejected.\n");
                }
            }
        } catch (...) { OutputDebugStringA("Xbox Rain Droplets: menu registration failed.\n"); }
        menuPending = false;
    }

    void RegisterFusionFixMenu()
    {
        if (menuRegistered || menuPending) return;
        static ULONGLONG nextAttempt = 0;
        const auto now = GetTickCount64();
        if (now < nextAttempt) return;
        nextAttempt = now + 1000;
        auto fusionFix = GetModuleHandleW(L"MaxPayne3.FusionFix.asi");
        if (!fusionFix) return;
        auto version = reinterpret_cast<unsigned int (__cdecl*)()>(GetProcAddress(fusionFix, "FusionFix_MenuVersion"));
        auto setOption = GetProcAddress(fusionFix, "FusionFix_MenuSetOption");
        using QueueCallback = int (__cdecl*)(void (__cdecl*)(void*), void*);
        auto queue = reinterpret_cast<QueueCallback>(GetProcAddress(fusionFix, "FusionFix_MenuQueueCallback"));
        if (!version || !setOption || !queue || version() != 1 ||
            !GetProcAddress(fusionFix, "FusionFix_MenuSetPage")) return;
        menuPending = true;
        if (queue(RegisterMenuOnFrontend, reinterpret_cast<void*>(setOption)) != 1) menuPending = false;
    }

}

void Init()
{
    WaterDrops::ReadIniSettings(true);
    RegisterFusionFixMenu();

    WaterDrops::ms_rainIntensity = 0.0f;

    static auto ppDevice = *hook::get_pattern<IDirect3DDevice9**>("68 ? ? ? ? 68 ? ? ? ? 8B D7 83 CA 10", 1);
    static auto ppSwapChain = *hook::get_pattern<IDXGISwapChain**>("A1 ? ? ? ? 8B 08 53 8D 54 24 14 52 50", 1);

    //this enables something for next hook to work
    auto pattern = hook::pattern("38 1D ? ? ? ? 75 21 E8");
    injector::WriteMemory<uint8_t>(pattern.get_first(6), 0xEB, true);

    static bool bAmbientCheck = false;

    pattern = hook::pattern("8D 9B ? ? ? ? 8A 08 3A 0A");
    struct AmbientCheckHook
    {
        void operator()(injector::reg_pack& regs)
        {
            std::string_view str((char*)regs.eax);

            if (str == "Ambient_RoofCorner_Drip_S" || str == "Ambient_RoofLine_Drip_Long_S" || str == "Ambient_RoofLine_Splash_Long_S")
                bAmbientCheck = true;
        }
    }; injector::MakeInline<AmbientCheckHook>(pattern.get_first(0), pattern.get_first(6));

    pattern = hook::pattern("A3 ? ? ? ? 8B 16 8B 82 ? ? ? ? 6A 03");
    static auto D3D11CreateDeviceAndSwapChain = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& ctx)
    {
        auto pSwapChain = *ppSwapChain;
        Xrd::Init(Xrd::RENDERER_D3D11, pSwapChain);
    });

    pattern = hook::pattern("A3 ? ? ? ? 8B 06 8B 90 ? ? ? ? 6A 06");
    static auto D3D9CreateDevice = safetyhook::create_mid(pattern.get_first(0), [](SafetyHookContext& ctx)
    {
        auto pDevice = *ppDevice;
        Xrd::Init(Xrd::RENDERER_D3D9, pDevice);
    });

    pattern = hook::pattern("68 ? ? ? ? E8 ? ? ? ? 8B 15 ? ? ? ? 8B 0D ? ? ? ? 8B 04 95 ? ? ? ? 83 C4 08 03 C1");
    static auto shsub_12D4470 = safetyhook::create_mid(*pattern.get_first<void*>(1), [](SafetyHookContext& ctx)
    {
        RegisterFusionFixMenu();
        if (!ApplyMenuSettings())
        {
            WaterDrops::Clear();
            WaterDrops::ms_rainIntensity = 0.0f;
            return;
        }
        #ifdef DEBUG
        WaterDrops::ms_rainIntensity = 1.0f;
        #endif // DEBUG
        WaterDrops::Process();
        WaterDrops::Render();
        WaterDrops::ms_rainIntensity = 0.0f;
    });

    pattern = hook::pattern("F3 0F 11 6E ? F3 0F 11 46 ? 5E 5B");
    struct CameraHook
    {
        void operator()(injector::reg_pack& regs)
        {
            auto right = *(RwV3d*)(regs.esi + 0x20);
            auto up = *(RwV3d*)(regs.esi + 0x30);
            auto at = *(RwV3d*)(regs.esi + 0x40);
            auto pos = *(RwV3d*)(regs.esi + 0x50);

            // The first row of the matrix points to the right of the screen and
            // the effect takes right as pointing to the left of it, the way
            // RenderWare has it, or the drops drift the wrong way when the
            // camera turns.
            WaterDrops::right = { -right.x, -right.y, -right.z };
            WaterDrops::up = up;
            WaterDrops::at = at;
            WaterDrops::pos = pos;
        }
    }; injector::MakeInline<CameraHook>(pattern.get_first(5));

    pattern = hook::pattern("FF D2 8B 06 8B 90 ? ? ? ? 8B CE FF D2 A1 ? ? ? ? 8B 08");
    static auto D3D9onReset = safetyhook::create_mid(pattern.get_first(), [](SafetyHookContext& ctx)
    {
        WaterDrops::Reset();
    });

    pattern = hook::pattern("C6 86 ? ? ? ? ? EB 1C 8B 0D ? ? ? ? 57 E8");
    struct InteriornessHook1
    {
        void operator()(injector::reg_pack& regs)
        {
            *(uint8_t*)(regs.esi + 0x21BD8) = 1;
            if (bAmbientCheck)
            {
                WaterDrops::ms_rainIntensity = 1.0f;
                bAmbientCheck = false;
            }
        }
    }; injector::MakeInline<InteriornessHook1>(pattern.get_first(0), pattern.get_first(7));

    pattern = hook::pattern("C6 86 ? ? ? ? ? 85 FF 74 2E 80 BF ? ? ? ? ? 74 1B");
    struct InteriornessHook2
    {
        void operator()(injector::reg_pack& regs)
        {
            *(uint8_t*)(regs.esi + 0x21BD8) = 0;
            WaterDrops::ms_rainIntensity = 0.0f;
        }
    }; injector::MakeInline<InteriornessHook2>(pattern.get_first(0), pattern.get_first(7));

    pattern = hook::pattern("D9 05 ? ? ? ? 8B 10 51");
    WaterDrops::fTimeStep = *pattern.get_first<float*>(2);
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
