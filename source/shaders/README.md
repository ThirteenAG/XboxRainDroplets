# Shaders

Edit sources here. `generated/` contains tracked build outputs, never hand-edited shader implementations. `manifest.json` is the list of sources, entry points, profiles, compiler flags, and outputs used by every project.

| Folder | Purpose |
| --- | --- |
| `d3d8/` | Shader model 1.1/1.4 droplets and light-field passes |
| `d3d9/` | Shader model 3 droplets, light gather, and Scarface menu blur |
| `d3d10/` | Shared HLSL for D3D10/11 (model 4) and D3D12 (model 5) |
| `vulkan/` | Native Vulkan HLSL, compiled to SPIR-V |
| `opengl/` | Compatibility and core GLSL for native OpenGL |
| `thin3d/` | PPSSPP shader sources and its GLSL language adapter |
| `tests/` | Test-scene shaders; not included by production renderers |
| `generated/` | Bytecode, embedded source headers, and `build.json` validation hashes |

## Building

From the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/BuildShaders.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/BuildShaders.ps1 -Verify
python -m unittest discover -s tests -p test_shader_build.py
```

Premake's `BuildShaders()` attaches this same build step to every plugin, wrapper, and renderer test. There are no game-specific shader compiler loops. Unchanged outputs retain their timestamps, including across a clean checkout. Source or recipe changes, missing outputs, and altered outputs invalidate their cached build. Concurrent projects serialize generation. Compilation failures stop the build before any changed outputs are published. `-Verify` only checks freshness and never runs a compiler or writes files.

Direct3D uses the bundled `tools/x86/fxc.exe`. Shader model 1 requires `/LD` and the bundled `d3dx9_31.dll`. Vulkan source changes require DXC with SPIR-V support, found through `VULKAN_SDK`, `PATH`, or `-DxcPath`. Unchanged Vulkan bytecode is verified without requiring that SDK. Use `-Force` to regenerate all outputs after changing a compiler; this requires DXC too. Commit changed sources, their generated outputs, and `generated/build.json` together.

An entry's `sources` array also lists any include dependencies; the first source is the compiler input. Add future includes there so edits invalidate the output. Source hashes normalize CRLF/LF, so Git checkout line endings do not trigger unnecessary rebuilds. Generated headers are also verified with normalized line endings; binary bytecode is checked byte-for-byte.

## Runtime work

Native Direct3D loads resource bytecode from `source/resources/Dropmask.rc`; native Vulkan loads the generated SPIR-V arrays. Production renderer headers no longer contain HLSL or a runtime HLSL compiler. The runtime HLSL helper in `tests/ShaderCompiler.h` belongs only to the test scene.

OpenGL compiles and links embedded GLSL with the active driver. Thin3D accepts shader source through the emulator's API and performs its own compilation. `Prepare()` runs this work, pipeline creation, and buffer/texture allocation before visible drops when a valid device and target are available. Reset recovery uses the same path. No shader files are read or built from disk by a game, and no recurring compilation is added to rendering. Driver-specific first-use costs can still occur; this is not a promise of zero stalls on every driver.
