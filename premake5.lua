workspace "XboxRainDroplets"
   configurations { "Release", "Debug" }
   architecture "x86"
   location "build"
   buildoptions {"-std:c++latest"}
   kind "SharedLib"
   language "C++"
   targetextension ".asi"
   linkoptions "/SAFESEH:NO"
   defines { "_CRT_SECURE_NO_WARNINGS" }
   
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
   includedirs { "external/hooking" }
   includedirs { "external/injector/include" }
   local dxsdk = os.getenv "DXSDK_DIR"
   if dxsdk then
      includedirs { dxsdk .. "/include" }
      libdirs { dxsdk .. "/lib/x86" }
   else
      includedirs { "source/dxsdk" }
      libdirs { "source/dxsdk/lib/x86" }
   end
   includedirs { "source/dxsdk/dx8" }
   libdirs { "source/dxsdk/dx8" }
   
   characterset ("Unicode")
   
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

	prebuildcommands {
		"for /R \"../source/resources/shaders/ps/\" %%f in (*.hlsl) do (\"../source/dxsdk/lib/x86/fxc.exe\" /T ps_3_0 /nologo /E main /Fo \"../source/resources/%%~nf.cso\" %%f)",
		"for /R \"../source/resources/shaders/vs/\" %%f in (*.hlsl) do (\"../source/dxsdk/lib/x86/fxc.exe\" /T vs_3_0 /nologo /E main /Fo \"../source/resources/%%~nf.cso\" %%f)",
	}
   
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
   setpaths("Z:/WFP/Games/Scarface/", "Scarface.exe")
project "Manhunt.XboxRainDroplets"
   setpaths("Z:/WFP/Games/Manhunt/", "manhunt.exe", "scripts/")
project "MaxPayne.XboxRainDroplets"
   setpaths("Z:/WFP/Games/Max Payne/Max Payne/", "MaxPayne.exe", "scripts/")
project "MaxPayne2.XboxRainDroplets"
   setpaths("Z:/WFP/Games/Max Payne/Max Payne 2 The Fall of Max Payne/", "MaxPayne2.exe", "scripts/")
project "MaxPayne3.XboxRainDroplets"
   setpaths("E:/Games/Steam/steamapps/common/Max Payne 3/Max Payne 3/", "MaxPayne3.exe", "plugins/")
--tests
--project "GTA3.XboxRainDroplets"
--   setpaths("Z:/WFP/Games/Grand Theft Auto/GTAIII/", "gta3.exe", "scripts/")
--project "GTASA.XboxRainDroplets"
--   setpaths("Z:/WFP/Games/Grand Theft Auto/GTA San Andreas/", "gta_sa.exe", "scripts/")

workspace "XboxRainDropletsWrapper"
   configurations { "Release", "Debug" }
   platforms { "Win32", "Win64" }
   location "build"
   objdir ("build/obj")
   buildlog ("build/log/%{prj.name}.log")
   buildoptions {"-std:c++latest"}
   
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
   includedirs { "source" }
   includedirs { "external" }
   includedirs { "external/hooking" }
   includedirs { "external/injector/include" }
   includedirs { "external/FusionDxHook/includes" }
   includedirs { "external/FusionDxHook/minhook" }
   includedirs { "external/FusionDxHook/minhook/include" }
   includedirs { "external/FusionDxHook/minhook/src" }
   includedirs { "external/sire" }
   includedirs { "source/dxsdk/dx8" }
   libdirs { "source/dxsdk/dx8" }

   files { "external/FusionDxHook/includes/minhook/include/*.*" }
   files { "external/FusionDxHook/includes/minhook/src/*.*" }
   files { "external/FusionDxHook/includes/minhook/src/hde/*.*" }
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
      
   filter "platforms:Win64"
      architecture "x64"
      targetname "%{prj.name}64"
	  includedirs { "source/dxsdk" }
	  libdirs { "source/dxsdk/lib/x64" }

project "XboxRainDropletsWrapper"
   setpaths("Z:/WFP/Games/PPSSPP/", "PPSSPPWindows.exe")
project "PPSSPP.XboxRainDroplets"
   setpaths("Z:/WFP/Games/PPSSPP/", "PPSSPPWindows.exe")
project "PCSX2F.XboxRainDroplets"
   architecture "x64"
   setpaths("Z:/GitHub/PCSX2-Fork-With-Plugins/bin/", "pcsx2-qtx64-clang.exe")