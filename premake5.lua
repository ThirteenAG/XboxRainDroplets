workspace "XboxRainDroplets"
   configurations { "Release", "Debug" }
   architecture "x86"
   location "build"
   cppdialect "C++latest"
   kind "SharedLib"
   language "C++"
   targetextension ".asi"
   linkoptions "/SAFESEH:NO"
   buildoptions { "/Zc:__cplusplus /utf-8" }
   flags { "MultiProcessorCompile" }
   defines { "_CRT_SECURE_NO_WARNINGS" }
   characterset ("Unicode")
   
   defines { "rsc_CompanyName=\"ThirteenAG\"" }
   defines { "rsc_LegalCopyright=\"MIT License\""} 
   defines { "rsc_FileVersion=\"1.0.0.0\"", "rsc_ProductVersion=\"1.0.0.0\"" }
   defines { "rsc_InternalName=\"%{prj.name}\"", "rsc_ProductName=\"%{prj.name}\"", "rsc_OriginalFilename=\"%{prj.name}.dll\"" }
   defines { "rsc_FileDescription=\"Xbox Rain Droplets Plugin\"" }
   defines { "rsc_UpdateUrl=\"https://github.com/ThirteenAG/XboxRainDroplets\"" }

   includedirs { "source" }
   includedirs { "external" }
   files { "source/%{prj.name}.cpp" }
   files { "source/resources/Versioninfo.rc" }
   files { "source/resources/Dropmask.rc" }
   files { "external/hooking/Hooking.Patterns.h", "external/hooking/Hooking.Patterns.cpp" }
   files { "external/injector/safetyhook/include/**.hpp", "external/injector/safetyhook/src/**.cpp" }
   files { "external/injector/zydis/**.h", "external/injector/zydis/**.c" }
   includedirs { "external/hooking" }
   includedirs { "external/injector/include" }
   includedirs { "external/injector/safetyhook/include" }
   includedirs { "external/injector/zydis" }
   includedirs { "external/FusionDxHook/includes" }
   includedirs { "external/sire" }
   files { "external/sire/sire.h" }

   pbcommands = { 
      "setlocal EnableDelayedExpansion",
      --"set \"path=" .. (gamepath) .. "\"",
      "set file=$(TargetPath)",
      "FOR %%i IN (\"%file%\") DO (",
      "set filename=%%~ni",
      "set fileextension=%%~xi",
      "set target=!path!!filename!!fileextension!",
      "if exist \"!target!\" copy /y \"!file!\" \"!target!\"",
      ")" }

   function setpaths (gamepath, exepath, scriptspath)
      scriptspath = scriptspath or "scripts/"
      if (gamepath) then
         cmdcopy = { "set \"path=" .. gamepath .. scriptspath .. "\"" }
         table.insert(cmdcopy, pbcommands)
         postbuildcommands (cmdcopy)
         debugdir (gamepath)
         if (exepath) then
            debugcommand (gamepath .. exepath)
            dir, file = exepath:match'(.*/)(.*)'
            debugdir (gamepath .. (dir or ""))
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

   filter "architecture:x32"
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

project "Driv3r.XboxRainDroplets"
   setpaths("Z:/WFP/Games/Driv3r/", "driv3r.exe")
project "DriverParallelLines.XboxRainDroplets"
   setpaths("Z:/WFP/Games/Driver Parallel Lines/", "DriverParallelLines.exe")
project "NFSUnderground2.XboxRainDroplets"
   setpaths("Z:/WFP/Games/Need For Speed/Need for Speed Underground 2/", "speed2.exe")
project "NFSMostWanted.XboxRainDroplets"
   setpaths("Z:/WFP/Games/Need For Speed/Need for Speed Most Wanted/", "speed.exe")
project "NFSCarbon.XboxRainDroplets"
   setpaths("Z:/WFP/Games/Need For Speed/Need for Speed Carbon/", "NFSC.exe")
project "GTAIV.XboxRainDroplets"
   setpaths("Z:/WGTA/IV/Episodes from Liberty City/", "EFLC.exe", "plugins/")
project "Mafia.XboxRainDroplets"
   setpaths("Z:/WFP/Games/Mafia/", "GameV12.exe")
project "Scarface.XboxRainDroplets"
    prebuildcommands {
        "for /R \"../source/resources/shaders/ps/\" %%f in (*.hlsl) do (\"../source/dxsdk/lib/x86/fxc.exe\" /T ps_3_0 /nologo /E main /Fo \"../source/resources/%%~nf.cso\" %%f)",
        "for /R \"../source/resources/shaders/vs/\" %%f in (*.hlsl) do (\"../source/dxsdk/lib/x86/fxc.exe\" /T vs_3_0 /nologo /E main /Fo \"../source/resources/%%~nf.cso\" %%f)",
    }
   setpaths("Z:/WFP/Games/Scarface/", "Scarface.exe")
project "Manhunt.XboxRainDroplets"
   setpaths("Z:/WFP/Games/Manhunt/", "manhunt.exe", "scripts/")
project "MaxPayne.XboxRainDroplets"
   setpaths("Z:/WFP/Games/Max Payne/Max Payne/", "MaxPayne.exe", "scripts/")
project "MaxPayne2.XboxRainDroplets"
   setpaths("Z:/WFP/Games/Max Payne/Max Payne 2 The Fall of Max Payne/", "MaxPayne2.exe", "scripts/")
project "MaxPayne3.XboxRainDroplets"
   setpaths("E:/Games/Steam/steamapps/common/Max Payne 3/Max Payne 3/", "MaxPayne3.exe", "plugins/")
project "SplinterCell.XboxRainDroplets"
   setpaths("Z:/WFP/Games/Splinter Cell/Splinter Cell/system/", "SplinterCell.exe", "scripts/")
project "SplinterCellPandoraTomorrow.XboxRainDroplets"
   debugargs { "-uplay_steam_mode" }
   setpaths("Z:/WFP/Games/Splinter Cell/Splinter Cell Pandora Tomorrow/system/", "SplinterCell2.exe", "scripts/")
project "SplinterCellChaosTheory.XboxRainDroplets"
   setpaths("Z:/WFP/Games/Splinter Cell/SplinterCell Chaos Theory/System/", "splintercell3.exe", "scripts/")
project "SplinterCellDoubleAgent.XboxRainDroplets"
   setpaths("Z:/WFP/Games/Splinter Cell/Splinter Cell - Double Agent/SCDA-Offline/System/", "SplinterCell4.exe", "scripts/")
project "SplinterCellBlacklist.XboxRainDroplets"
   setpaths("Z:/WFP/Games/Splinter Cell/Splinter Cell Blacklist/src/SYSTEM/", "Blacklist_DX11_game.exe", "scripts/")
project "GTASADE.XboxRainDroplets"
   architecture "x64"
   add_kananlib()
   setpaths("Z:/WFP/Games/Grand Theft Auto The Definitive Edition/GTA San Andreas - Definitive Edition/", "Gameface/Binaries/Win64/SanAndreas.exe", "Gameface/Binaries/Win64/scripts/")
project "GTAVCDE.XboxRainDroplets"
   architecture "x64"
   add_kananlib()
   setpaths("Z:/WFP/Games/Grand Theft Auto The Definitive Edition/GTA Vice City - Definitive Edition/", "Gameface/Binaries/Win64/ViceCity.exe", "Gameface/Binaries/Win64/scripts/")
project "GTA3DE.XboxRainDroplets"
   architecture "x64"
   add_kananlib()
   setpaths("Z:/WFP/Games/Grand Theft Auto The Definitive Edition/GTA III - Definitive Edition/", "Gameface/Binaries/Win64/LibertyCity.exe", "Gameface/Binaries/Win64/scripts/")
project "TrueCrimeNewYorkCity.XboxRainDroplets"
   setpaths("Z:/WFP/Games/True Crime New York City/", "True Crime New York City.exe", "scripts/")
--tests
--project "GTA3.XboxRainDroplets"
--   setpaths("Z:/WFP/Games/Grand Theft Auto/GTAIII/", "gta3.exe", "scripts/")
--project "GTASA.XboxRainDroplets"
--   setpaths("Z:/WFP/Games/Grand Theft Auto/GTA San Andreas/", "gta_sa.exe", "scripts/")

workspace "XboxRainDropletsWrapper"
   configurations { "Release", "Debug" }
   platforms { "Win32", "x64" }
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
   defines { "rsc_FileVersion=\"1.0.0.0\"", "rsc_ProductVersion=\"1.0.0.0\"" }
   defines { "rsc_InternalName=\"%{prj.name}\"", "rsc_ProductName=\"%{prj.name}\"", "rsc_OriginalFilename=\"XboxRainDroplets.asi\"" }
   defines { "rsc_FileDescription=\"https://thirteenag.github.io/wfp\"" }
   defines { "rsc_UpdateUrl=\"https://github.com/ThirteenAG/%{prj.name}\"" }
   
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
   includedirs { "external/sire" }
   includedirs { "source/dxsdk/dx8" }
   libdirs { "source/dxsdk/dx8" }

   files { "external/sire/sire.h" }
   
   
   filter "configurations:Debug"
      defines "DEBUG"
      symbols "On"

   filter "configurations:Release"
      defines "NDEBUG"
      optimize "On"
      
   filter "platforms:Win32"
      architecture "x32"
      includedirs { "source/dxsdk" }
      libdirs { "source/dxsdk/lib/x86" }
      
   filter "platforms:x64"
      architecture "x64"
      targetname "%{prj.name}64"
      includedirs { "source/dxsdk" }
      libdirs { "source/dxsdk/lib/x64" }

project "XboxRainDropletsWrapper"
   setpaths("Z:/WFP/Games/PPSSPP/", "PPSSPPWindows.exe")
project "PPSSPP.XboxRainDroplets"
   setpaths("Z:/WFP/Games/PPSSPP/", "PPSSPPWindows.exe")
project "PCSX2F.XboxRainDroplets"
   setpaths("Z:/GitHub/PCSX2-Fork-With-Plugins/bin/", "pcsx2-qtx64-clang.exe")