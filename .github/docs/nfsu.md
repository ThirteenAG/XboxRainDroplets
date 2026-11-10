<div align="center">

[Download](https://github.com/ThirteenAG/FusionRainDroplets/releases/download/nfsu/NFSUnderground.FusionRainDroplets.zip) · [Source Code](https://github.com/ThirteenAG/FusionRainDroplets/blob/master/source/NFSUnderground.XboxRainDroplets.cpp)

</div>

---

## Installation

1. Download and install the [NFS Underground Fusion Fix](https://fusionfix.io/wfp#nfsu) - it includes the required [ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader).
2. Download [Fusion Rain Droplets for NFSU](https://github.com/ThirteenAG/FusionRainDroplets/releases/download/nfsu/NFSUnderground.FusionRainDroplets.zip) and put the `.asi` file into the `scripts` folder.
3. Launch the game and start a race.

Used Xbox Rain Droplets before? Delete the old `NFSUnderground.XboxRainDroplets.asi`: while it is there, the game shows a message and closes.

## Notes

- The game has no rain, so the plugin makes it up: when a race starts, it decides at random whether it rains in it (`Rain chance`, 30% by default) and how hard. It rains until the race is over; the front end stays dry.
- The settings are in the `.cfg` the plugin writes next to itself on the first start. They can be changed there, in FusionFixHub or in the Fusion Fix's Advanced Options, and apply while the game runs; a new rain chance applies from the next race.
