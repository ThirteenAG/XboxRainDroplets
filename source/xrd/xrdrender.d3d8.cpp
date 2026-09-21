// ---------------------------------------------------------------------------
// Direct3D 8, for the projects that need it next to Direct3D 9.
//
// The backend itself is a header, like the backend of every other API, but this
// is the one that cannot be included by a translation unit that also sees
// Direct3D 9. Games use one of the two and say so, the wrapper and the tests
// want both in one binary and put this file in the project instead: it compiles
// the header on its own and the backend registers itself, so RENDERER_D3D8 is
// available to the whole module.
// ---------------------------------------------------------------------------

#include "xrdrender.d3d8.h"
