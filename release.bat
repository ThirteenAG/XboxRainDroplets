copy ".\source\resources\inis\*.ini" ".\bin\" /Y

for %%x in (
GTAIV.XboxRainDroplets
Mafia.XboxRainDroplets
NFSCarbon.XboxRainDroplets
NFSMostWanted.XboxRainDroplets
NFSUnderground2.XboxRainDroplets
Scarface.XboxRainDroplets
Manhunt.XboxRainDroplets
MaxPayne.XboxRainDroplets
MaxPayne2.XboxRainDroplets
MaxPayne3.XboxRainDroplets
Driv3r.XboxRainDroplets
DriverParallelLines.XboxRainDroplets
SplinterCell.XboxRainDroplets
SplinterCellPandoraTomorrow.XboxRainDroplets
SplinterCellChaosTheory.XboxRainDroplets
SplinterCellDoubleAgent.XboxRainDroplets
SplinterCellBlacklist.XboxRainDroplets
GTASADE.XboxRainDroplets
GTAVCDE.XboxRainDroplets
GTA3DE.XboxRainDroplets
) do (
    7za a -tzip ".\bin\%%x.zip" ".\bin\%%x.asi" ".\bin\%%x.ini"
)

for %%x in (
XboxRainDropletsWrapper
) do (
    7za a -tzip ".\bin\%%x.zip" ".\bin\%%x.asi" ".\bin\%%x.ini" ".\bin\version.dll"
)

for %%x in (
XboxRainDropletsWrapper64
) do (
    7za a -tzip ".\bin\%%x.zip" ".\bin\%%x.asi" ".\bin\%%x.ini" ".\bin\dinput8.dll"
)

for %%x in (
PPSSPP.XboxRainDroplets
) do (
    7za a -tzip ".\bin\%%x.zip" ".\bin\%%x.asi" ".\bin\%%x.ini" ".\bin\version.dll" ".\bin\memstick"
)

for %%x in (
PPSSPP.XboxRainDroplets64
) do (
    7za a -tzip ".\bin\%%x.zip" ".\bin\%%x.asi" ".\bin\%%x.ini" ".\bin\dinput8.dll" ".\bin\memstick"
)

for %%x in (
PCSX2F.XboxRainDroplets64
) do (
    7za a -tzip ".\bin\%%x.zip" ".\bin\%%x.asi" ".\bin\%%x.ini" ".\bin\PLUGINS"
)

EXIT

7-Zip Extra
~~~~~~~~~~~
License for use and distribution
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Copyright (C) 1999-2016 Igor Pavlov.

7-Zip Extra files are under the GNU LGPL license.


Notes: 
  You can use 7-Zip Extra on any computer, including a computer in a commercial 
  organization. You don't need to register or pay for 7-Zip.


GNU LGPL information
--------------------

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You can receive a copy of the GNU Lesser General Public License from 
  http://www.gnu.org/

