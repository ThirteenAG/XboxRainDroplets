-- The folder a project is deployed to, and the game it is started from when debugging,
-- is the path of one machine and does not belong in the repository. It is read from a
-- `.env` file next to this script, which is not tracked by git and holds one
-- `<KEY>=<folder>` line per game (quotes and a trailing slash are optional). A project
-- whose key is missing is not deployed at all.
local envkeys = nil
function envdir(key)
   if not envkeys then
      envkeys = {}
      local text = io.readfile(path.join(_SCRIPT_DIR, ".env")) or ""
      for line in text:gmatch("[^\r\n]+") do
         local k, v = line:match("^%s*([%w_]+)%s*=%s*(.-)%s*$")
         if k and v ~= "" then
            v = v:gsub('^"', ""):gsub('"$', ""):gsub("^'", ""):gsub("'$", "")
            envkeys[k] = v
         end
      end
   end

   local value = envkeys[key]
   if not value then return nil end

   value = value:gsub("[%s\\/]+$", "")
   if value == "" then return nil end

   return path.translate(value)
end

-- The version of the plugins is the date of the build plus the short hash of the commit
-- they were built from, like in the WidescreenFixesPack repository. It is written into
-- the version resource of every plugin, see source/resources/Versioninfo.rc.
function AddVersionDefines()
   local major = os.date("%d")
   local minor = os.date("%m")
   local build = os.date("%Y")
   local revision = os.date("%H") .. os.date("%M")

   local githash = ""
   local f = io.popen("git rev-parse --short HEAD")
   if f then
      githash = f:read("*a"):gsub("%s+", "")
      f:close()
   end

   local productVersion = major .. "." .. minor .. "." .. build .. "." .. revision
   if githash ~= "" then
      productVersion = productVersion .. "-" .. githash
   end

   defines { "rsc_FileVersion_MAJOR=" .. major }
   defines { "rsc_FileVersion_MINOR=" .. minor }
   defines { "rsc_FileVersion_BUILD=" .. build }
   defines { "rsc_FileVersion_REVISION=" .. revision }
   defines { "rsc_FileVersion=\"" .. major .. "." .. minor .. "." .. build .. "\"" }
   defines { "rsc_ProductVersion=\"" .. productVersion .. "\"" }
   defines { "rsc_GitSHA1=\"" .. githash .. "\"" }
   defines { "rsc_GitSHA1W=L\"" .. githash .. "\"" }
end

-- The refraction of Direct3D 8 is a shader of model 1, which cannot be built at
-- runtime the way the renderers of Direct3D 9 and above build theirs (a game hands
-- IDirect3DDevice8::CreatePixelShader the bytecode of one, not a source string): it
-- is built here and embedded as a resource, exactly like the menu blur of Scarface.
--
-- The June 2010 compiler only builds a ps_1_x profile with /LD, and the old compiler
-- it loads then is d3dx9_31.dll, which sits next to it in tools/x86. Both profiles
-- are built: ps_1_4 is as high as the model goes and what every device that can run
-- a game today reports, ps_1_1 is what the hardware the games themselves ran on has.
-- The fade in of the light of the field needs one instruction more than ps_1_1 has,
-- see lightPS8.hlsl and D3D8Backend::EnsureShaders.
function BuildD3D8Shaders()
   prebuildcommands {
      "for /R \"../source/resources/shaders/ps8/\" %%f in (*.hlsl) do (\"../tools/x86/fxc.exe\" /LD /T ps_1_4 /E main /nologo /Fo \"../source/resources/%%~nf_14.cso\" %%f)",
      "for /R \"../source/resources/shaders/ps8/\" %%f in (*.hlsl) do (\"../tools/x86/fxc.exe\" /LD /T ps_1_1 /D XRD_LIGHT_RAMP=0 /E main /nologo /Fo \"../source/resources/%%~nf_11.cso\" %%f)",
   }
end

-- The settings every plugin of this repository is built with. Visual Studio 2026 has no
-- mixed platform solutions, so the plugins of one architecture are one solution: this
-- is called once for the 32 bit plugins and once for the 64 bit ones, below.
function PluginsSetup(name, platform, arch)
   workspace (name)
      configurations { "Release", "Debug" }
      platforms { platform }
      architecture (arch)
      location "build"
      cppdialect "C++latest"
      kind "SharedLib"
      language "C++"
      targetextension ".asi"
      linkoptions "/SAFESEH:NO"
      buildoptions { "/Zc:__cplusplus /utf-8" }
      multiprocessorcompile "On"
      defines { "_CRT_SECURE_NO_WARNINGS" }
      characterset ("Unicode")

      defines { "rsc_CompanyName=\"ThirteenAG\"" }
      defines { "rsc_LegalCopyright=\"MIT License\""}
      defines { "rsc_InternalName=\"%{prj.name}\"", "rsc_ProductName=\"%{prj.name}\"", "rsc_OriginalFilename=\"%{cfg.buildtarget.name}\"" }
      defines { "rsc_FileDescription=\"Xbox Rain Droplets Plugin\"" }
      defines { "rsc_UpdateUrl=\"https://github.com/ThirteenAG/XboxRainDroplets\"" }
      AddVersionDefines()

      includedirs { "source" }
      includedirs { "external" }
      files { "source/%{prj.name}.cpp" }
      files { "source/resources/Versioninfo.rc" }
      files { "source/resources/Dropmask.rc" }
      BuildD3D8Shaders()
      files { "external/hooking/Hooking.Patterns.h", "external/hooking/Hooking.Patterns.cpp" }
      files { "external/injector/safetyhook/include/**.hpp", "external/injector/safetyhook/src/**.cpp" }
      files { "external/injector/zydis/**.h", "external/injector/zydis/**.c" }
      includedirs { "external/hooking" }
      includedirs { "external/injector/include" }
      includedirs { "external/injector/safetyhook/include" }
      includedirs { "external/injector/zydis" }
      includedirs { "external/FusionDxHook/includes" }

      -- Deploys the built .asi into the folder that `key` names in the .env file, and
      -- starts the game from there when debugging. Only a plugin that is already
      -- installed in the game folder is replaced, a folder without one is left alone.
      function setpaths (key, exepath, scriptspath)
         scriptspath = scriptspath or "scripts/"
         local gamepath = envdir(key)
         if gamepath then
            local target = gamepath .. "\\" .. path.translate(scriptspath)
            postbuildcommands {
               "if exist \"" .. target .. "$(TargetFileName)\" copy /y \"$(TargetPath)\" \"" .. target .. "\"",
            }
            debugdir (gamepath)
            if (exepath) then
               debugcommand (gamepath .. "\\" .. path.translate(exepath))
               local dir = exepath:match'(.*/)(.*)'
               debugdir (gamepath .. "\\" .. path.translate(dir or ""))
            end
         end
         targetdir ("bin")
      end

      function add_kananlib()
         defines { "BDDISASM_HAS_MEMSET", "BDDISASM_HAS_VSNPRINTF" }
         files { "external/injector/kananlib/include/utility/**.hpp", "external/injector/kananlib/src/**.cpp" }
         files { "external/injector/bddisasm/bddisasm/*.c" }
         files { "external/injector/bddisasm/bdshemu/*.c" }
         includedirs { "external/injector/kananlib/include" }
         includedirs { "external/injector/bddisasm/inc" }
         includedirs { "external/injector/bddisasm/bddisasm/include" }
      end

      filter "architecture:x86"
         includedirs { "source/dxsdk" }
         libdirs { "source/dxsdk/lib/x86" }
         includedirs { "source/dxsdk/dx8" }
         libdirs { "source/dxsdk/dx8" }

      filter "architecture:x64"
         includedirs { "source/dxsdk" }
         libdirs { "source/dxsdk/lib/x64" }

      filter "configurations:Debug"
         defines { "DEBUG" }
         symbols "On"

      filter "configurations:Release"
         defines { "NDEBUG" }
         optimize "On"
         staticruntime "On"

      filter {}
end

-- ====================== WIN32 SOLUTION ======================
PluginsSetup("XboxRainDroplets", "Win32", "x86")

project "Driv3r.XboxRainDroplets"
   setpaths("DRIV3R_DIR", "driv3r.exe")
project "DriverParallelLines.XboxRainDroplets"
   setpaths("DRIVER_PARALLEL_LINES_DIR", "DriverParallelLines.exe")
project "NFSUnderground2.XboxRainDroplets"
   setpaths("NEED_FOR_SPEED_UNDERGROUND_2_DIR", "speed2.exe")
project "NFSMostWanted.XboxRainDroplets"
   setpaths("NEED_FOR_SPEED_MOST_WANTED_DIR", "speed.exe")
project "NFSCarbon.XboxRainDroplets"
   setpaths("NEED_FOR_SPEED_CARBON_DIR", "NFSC.exe")
project "GTAIV.XboxRainDroplets"
   setpaths("GTAIV_DIR", "GTAIV.exe", "plugins/")
project "Mafia.XboxRainDroplets"
   setpaths("MAFIA_DIR", "GameV12.exe")
project "Scarface.XboxRainDroplets"
    prebuildcommands {
        "for /R \"../source/resources/shaders/ps/\" %%f in (*.hlsl) do (\"../source/dxsdk/lib/x86/fxc.exe\" /T ps_3_0 /nologo /E main /Fo \"../source/resources/%%~nf.cso\" %%f)",
        "for /R \"../source/resources/shaders/vs/\" %%f in (*.hlsl) do (\"../source/dxsdk/lib/x86/fxc.exe\" /T vs_3_0 /nologo /E main /Fo \"../source/resources/%%~nf.cso\" %%f)",
    }
   setpaths("SCARFACE_DIR", "Scarface.exe")
project "Manhunt.XboxRainDroplets"
   setpaths("MANHUNT_DIR", "manhunt.exe", "scripts/")
project "MaxPayne.XboxRainDroplets"
   setpaths("MAX_PAYNE_DIR", "MaxPayne.exe", "scripts/")
project "MaxPayne2.XboxRainDroplets"
   setpaths("MAX_PAYNE_2_THE_FALL_OF_MAX_PAYNE_DIR", "MaxPayne2.exe", "scripts/")
project "MaxPayne3.XboxRainDroplets"
   setpaths("MAX_PAYNE_3_DIR", "MaxPayne3.exe", "plugins/")
project "SplinterCell.XboxRainDroplets"
   setpaths("SPLINTER_CELL_DIR", "SplinterCell.exe", "scripts/")
project "SplinterCellPandoraTomorrow.XboxRainDroplets"
   debugargs { "-uplay_steam_mode" }
   setpaths("SPLINTER_CELL_PANDORA_TOMORROW_DIR", "SplinterCell2.exe", "scripts/")
project "SplinterCellChaosTheory.XboxRainDroplets"
   setpaths("SPLINTERCELL_CHAOS_THEORY_DIR", "splintercell3.exe", "scripts/")
project "SplinterCellDoubleAgent.XboxRainDroplets"
   setpaths("SPLINTER_CELL_DOUBLE_AGENT_DIR", "SplinterCell4.exe", "scripts/")
project "SplinterCellBlacklist.XboxRainDroplets"
   setpaths("SPLINTER_CELL_BLACKLIST_DIR", "Blacklist_DX11_game.exe", "scripts/")
project "TrueCrimeNewYorkCity.XboxRainDroplets"
   setpaths("TRUE_CRIME_NEW_YORK_CITY_DIR", "True Crime New York City.exe", "scripts/")
project "KingKongGamersEdition.XboxRainDroplets"
   setpaths("KING_KONG_GAMERS_EDITION_DIR", "KingKong8.exe", "scripts/")
project "SR2.XboxRainDroplets"
   setpaths("SAINTS_ROW_2_DIR", "SR2_pc.exe", "scripts/")
project "GTA3.XboxRainDroplets"
   setpaths("GTAIII_DIR", "gta3.exe")
project "GTAVC.XboxRainDroplets"
   setpaths("GRAND_THEFT_AUTO_VICE_CITY_DIR", "gta-vc.exe")
project "GTASA.XboxRainDroplets"
   setpaths("GTA_SAN_ANDREAS_DIR", "gta_sa.exe")
PluginsSetup("XboxRainDroplets64", "x64", "x64")

project "GTASADE.XboxRainDroplets"
   add_kananlib()
   setpaths("GTA_SAN_ANDREAS_DEFINITIVE_EDITION_DIR", "Gameface/Binaries/Win64/SanAndreas.exe", "Gameface/Binaries/Win64/scripts/")
project "GTAVCDE.XboxRainDroplets"
   add_kananlib()
   setpaths("GTA_VICE_CITY_DEFINITIVE_EDITION_DIR", "Gameface/Binaries/Win64/ViceCity.exe", "Gameface/Binaries/Win64/scripts/")
project "GTA3DE.XboxRainDroplets"
   add_kananlib()
   setpaths("GTA_III_DEFINITIVE_EDITION_DIR", "Gameface/Binaries/Win64/LibertyCity.exe", "Gameface/Binaries/Win64/scripts/")

-- The settings the wrapper and the emulator plugins are built with, see the note above
-- PluginsSetup: one solution per architecture there as well.
function WrapperSetup(name, platform, arch)
   workspace (name)
      configurations { "Release", "Debug" }
      platforms { platform }
      architecture (arch)
      location "build"
      objdir ("build/obj")
      buildlog ("build/log/%{prj.name}.log")
      cppdialect "C++latest"

      kind "SharedLib"
      language "C++"
      targetextension ".asi"
      characterset ("Unicode")
      staticruntime "On"

      defines { "rsc_CompanyName=\"ThirteenAG\"" }
      defines { "rsc_LegalCopyright=\"MIT License\""}
      defines { "rsc_InternalName=\"%{prj.name}\"", "rsc_ProductName=\"%{prj.name}\"", "rsc_OriginalFilename=\"%{cfg.buildtarget.name}\"" }
      defines { "rsc_FileDescription=\"https://thirteenag.github.io/wfp\"" }
      defines { "rsc_UpdateUrl=\"https://github.com/ThirteenAG/XboxRainDroplets\"" }
      AddVersionDefines()

      files { "source/%{prj.name}.cpp" }
      files { "source/*.def" }
      files { "source/resources/Versioninfo.rc" }
      files { "source/resources/Dropmask.rc" }
      files { "external/hooking/Hooking.Patterns.h", "external/hooking/Hooking.Patterns.cpp" }
      files { "external/injector/safetyhook/include/**.hpp", "external/injector/safetyhook/src/**.cpp" }
      files { "external/injector/zydis/**.h", "external/injector/zydis/**.c" }
      includedirs { "source" }
      includedirs { "external" }
      includedirs { "external/hooking" }
      includedirs { "external/injector/include" }
      includedirs { "external/injector/safetyhook/include" }
      includedirs { "external/injector/zydis" }
      includedirs { "external/FusionDxHook/includes" }
      includedirs { "source/dxsdk/dx8" }
      libdirs { "source/dxsdk/dx8" }
      -- the wrapper is the one project that hooks every API, Vulkan included, and
      -- the declarations of Vulkan ship with the repository
      includedirs { "external/vulkan/include" }
      defines { "VK_USE_PLATFORM_WIN32_KHR" }
      -- the wrapper builds the Direct3D 8 renderer as one of its translation units
      BuildD3D8Shaders()

      filter "configurations:Debug"
         defines "DEBUG"
         symbols "On"

      filter "configurations:Release"
         defines "NDEBUG"
         optimize "On"

      filter {}

      if (arch == "x86") then
         files { "source/xrd/xrdrender.d3d8.cpp" }
         includedirs { "source/dxsdk" }
         libdirs { "source/dxsdk/lib/x86" }
      else
         targetname "%{prj.name}64"
         includedirs { "source/dxsdk" }
         libdirs { "source/dxsdk/lib/x64" }
      end
end

-- ====================== WIN32 SOLUTION ======================
WrapperSetup("XboxRainDropletsWrapper", "Win32", "x86")

project "XboxRainDropletsWrapper"
   setpaths("PPSSPP_DIR", "PPSSPPWindows.exe", "")
-- the emulator plugin is loaded from the folder of the executable, it is not a game
-- script
project "PPSSPP.XboxRainDroplets"
   setpaths("PPSSPP_DIR", "PPSSPPWindows.exe", "")
   -- The plugin is loaded into the emulator and draws with the drawing of the
   -- emulator itself, see source/xrd/xrdrender.thin3d.h: the headers of that
   -- drawing are the ones the build of the emulator was made with, and are copied
   -- into this repository next to everything else it needs, see
   -- external/ppsspp/README.md.
   includedirs { "external/ppsspp" }

-- ====================== X64 SOLUTION ======================
-- The two architectures of these projects have the same names, so the 64 bit ones
-- cannot be generated next to the 32 bit ones: their project files would overwrite
-- each other. They get a folder of their own instead. The names, and with them the
-- file names of the plugins, stay the same for both architectures, only the ones
-- built for 64 bit end in 64, see targetname above. PCSX2 is 64 bit only.
WrapperSetup("XboxRainDropletsWrapper64", "x64", "x64")

project "XboxRainDropletsWrapper"
   location "build/x64"
   setpaths("PPSSPP_DIR", "PPSSPPWindows.exe", "")
project "PPSSPP.XboxRainDroplets"
   location "build/x64"
   setpaths("PPSSPP_DIR", "PPSSPPWindows.exe", "")
   includedirs { "external/ppsspp" }
project "PCSX2F.XboxRainDroplets"
   location "build/x64"
   setpaths("PCSX2F_DIR", "pcsx2-qtx64-clang.exe", "")
-- Tests: one small application per renderer, each one draws its own scene and
-- its own UI, the drops go in between so it is visible that they end up behind
-- what the application draws last.
workspace "XboxRainDropletsTests"
   configurations { "Release", "Debug" }
   platforms { "Win32" }
   architecture "x86"
   location "build"
   cppdialect "C++latest"
   kind "ConsoleApp"
   language "C++"
   characterset ("Unicode")
   buildoptions { "/Zc:__cplusplus /utf-8" }
   defines { "_CRT_SECURE_NO_WARNINGS" }
   targetdir "bin/tests"
   -- the Direct3D 8 library of the 2002 sdk has no safe exception handlers
   linkoptions "/SAFESEH:NO"

   files { "source/resources/Dropmask.rc" }
   BuildD3D8Shaders()
   files { "external/hooking/Hooking.Patterns.h", "external/hooking/Hooking.Patterns.cpp" }
   files { "external/injector/safetyhook/include/**.hpp", "external/injector/safetyhook/src/**.cpp" }
   files { "external/injector/zydis/**.h", "external/injector/zydis/**.c" }

   includedirs { "tests" }
   includedirs { "source" }
   includedirs { "external" }
   includedirs { "external/hooking" }
   includedirs { "external/injector/include" }
   includedirs { "external/injector/safetyhook/include" }
   includedirs { "external/injector/zydis" }
   includedirs { "source/dxsdk/dx8" }
   libdirs { "source/dxsdk/dx8" }
   -- the applications do not use the Direct3D 9 helpers, and the headers of the
   -- two Direct3D versions cannot be mixed
   defines { "XRD_NO_D3DX" }

   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"

   filter "action:vs*"
      disablewarnings { "4005", "4244", "4267" }

project "XrdTestD3D8"
   files { "tests/XrdTestD3D8.cpp" }
   links { "d3d8" }

project "XrdTestD3D9"
   files { "tests/XrdTestD3D9.cpp" }
   links { "d3d9" }

project "XrdTestD3D11"
   files { "tests/XrdTestD3D11.cpp" }
   links { "d3d11", "dxgi" }

project "XrdTestD3D10"
   files { "tests/XrdTestD3D10.cpp" }
   links { "d3d10", "dxgi" }

project "XrdTestD3D12"
   files { "tests/XrdTestD3D12.cpp" }
   links { "d3d12", "dxgi" }

project "XrdTestOpenGL"
   files { "tests/XrdTestOpenGL.cpp" }
   links { "opengl32" }

project "XrdTestVulkan"
   files { "tests/XrdTestVulkan.cpp" }
   defines { "XRD_ENABLE_VULKAN", "VK_USE_PLATFORM_WIN32_KHR" }
   includedirs { "external/vulkan/include" }
