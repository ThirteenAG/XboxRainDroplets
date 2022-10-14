workspace "XboxRainDroplets"
   configurations { "Release", "Debug" }
   architecture "x86"
   location "build"
   buildoptions {"-std:c++latest"}
   kind "SharedLib"
   language "C++"
   targetdir "bin/%{cfg.buildcfg}"
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
   
   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"
      staticruntime "On"
	  
project "NFSUnderground2.XboxRainDroplets"
   setpaths("Z:/WFP/Games/Need For Speed/Need for Speed Underground 2/", "speed2.exe")
project "NFSMostWanted.XboxRainDroplets"
   setpaths("Z:/WFP/Games/Need For Speed/Need for Speed Most Wanted/", "speed.exe")
project "NFSCarbon.XboxRainDroplets"
   setpaths("Z:/WFP/Games/Need For Speed/Need for Speed Carbon/", "NFSC.exe")
project "GTAIV.XboxRainDroplets"
   setpaths("Z:/WGTA/IV/Episodes from Liberty City/", "EFLC.exe", "plugins/")
project "GTASA.XboxRainDroplets"
   setpaths("Z:/WFP/Games/Grand Theft Auto/GTA San Andreas/", "gta_sa.exe", "scripts/")
project "GTA3.XboxRainDroplets"
   setpaths("Z:/WFP/Games/Grand Theft Auto/GTAIII/", "gta3.exe", "scripts/")
project "Mafia.XboxRainDroplets"
   setpaths("Z:/WFP/Games/Mafia/", "GameV12.exe")
project "Scarface.XboxRainDroplets"
   setpaths("Z:/WFP/Games/Scarface/", "Scarface.exe")
project "Manhunt.XboxRainDroplets"
   setpaths("Z:/WFP/Games/Manhunt/", "manhunt.exe", "scripts/")
project "MaxPayne2.XboxRainDroplets"
   setpaths("Z:/WFP/Games/Max Payne/Max Payne 2 The Fall of Max Payne/", "MaxPayne2.exe", "scripts/")