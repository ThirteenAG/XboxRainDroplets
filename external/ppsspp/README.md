# The drawing of the emulator

The plugin of the drops is loaded into PPSSPP and draws with the drawing of
PPSSPP's own backend, which is the `Draw::DrawContext` of `Common/GPU/thin3d.h`.
The interface of it has to be exactly the one the build of the emulator was made
with - every call goes through its vtable - so the headers are copied here
instead of being taken from a checkout of the emulator, which would make the
build of this repository depend on one being in a particular place.

Nothing in here is modified: the files are the ones of the emulator, with the
paths they have there, so `#include "Common/GPU/thin3d.h"` finds this copy
(see the PPSSPP project in `premake5.lua`).

    ppsspp_config.h                              the platform defines the headers below include
    Common/Common.h                              the small things everything of the emulator uses
    Common/CommonTypes.h
    Common/CommonFuncs.h
    Common/Log.h                                 needed by Common/Data/Collections/FastVec.h
    Common/GPU/thin3d.h                          the drawing itself
    Common/GPU/DataFormat.h
    Common/GPU/Shader.h
    Common/GPU/MiscTypes.h
    Common/Data/Collections/Slice.h
    Common/Data/Collections/FastVec.h

## Keeping them current

Copy the files over again from a checkout of the emulator, by hand:

    git -C <ppsspp> log --oneline -1 -- Common/GPU/thin3d.h

A version of the emulator that changed any of these is a version the plugin has
to be rebuilt for: a newer `thin3d.h` can move what a virtual call does, and the
plugin would then be calling into the wrong place without anything complaining.
It is worth writing down which commit of the emulator these came from when they
are synced:

    from PPSSPP f293b10fb2 (2026-09-21)
        Common/GPU/thin3d.h   70095e458b  (2026-09-12)
        ppsspp_config.h       3ae9638885  (2026-06-03)
