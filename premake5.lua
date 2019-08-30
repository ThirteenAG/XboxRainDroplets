workspace "XboxRainDroplets"
   configurations { "Release", "Debug" }
   architecture "x86"
   location "build"
   buildoptions {"-std:c++latest"}
   kind "SharedLib"
   language "C++"
   targetdir "bin/%{cfg.buildcfg}"
   targetextension ".asi"
   
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
   end
   
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
   setpaths("G:/Games/Grand Theft Auto IV v1.0.8.0 - DLC/", "GTAIV.exe", "plugins/")
project "Mafia.XboxRainDroplets"
   setpaths("Z:/WFP/Games/Mafia/", "GameV12.exe")