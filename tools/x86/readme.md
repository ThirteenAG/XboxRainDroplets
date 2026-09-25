# tools/x86

Bundled tools used by [`BuildShaders.ps1`](../BuildShaders.ps1) for all native
Direct3D shader profiles. The June 2010 `fxc.exe` builds shader models 3–5 directly
and uses the November 2006 D3DX DLL beside it for shader model 1 with `/LD`.

| file | what it is |
| --- | --- |
| `fxc.exe` | the June 2010 shader compiler, and with `/LD` the one that still builds `ps_1_1` .. `ps_1_4` and `vs_1_1` |
| `d3dx9_31.dll` | the old compiler `/LD` loads, **required** for the ps_1_x profiles (an application directory is searched before the system one, so this copy is the one that gets used) |
| `D3DX9_43.dll` | what `fxc.exe` and `asm_shader.exe` import |
| `D3DCompiler_43.dll` | what `fxc.exe` imports |
| `asm_shader.exe` | `D3DXAssembleShader` in a small wrapper: assembles ps_1_x/vs_1_1 **assembly** into the bytecode `IDirect3DDevice8::CreatePixelShader` takes |

## Building a Direct3D 8 shader

Native Direct3D renderers load precompiled bytecode. Shader sources and generated
outputs live under [`source/shaders`](../../source/shaders/README.md); use the
shared build script rather than invoking a separate compiler for a game.

    rem HLSL, shader model 1.x (the /LD is what makes the ps_1_x profiles work)
    tools\x86\fxc.exe /LD /T ps_1_4 /E main /nologo /Fo dropPS.pso xrdDrop.ps
    tools\x86\fxc.exe /LD /T ps_1_1 /E main /nologo /Fo dropPS.pso xrdDrop.ps

    rem or the assembly of the same shader model
    tools\x86\asm_shader.exe xrdDrop.psh dropPS.pso

The blob is then embedded as `RCDATA` (see `source/resources/Dropmask.rc`) and handed
to `CreatePixelShader`, which is what the menu blur of Scarface does on Direct3D 9 and
what `source/xrd/xrdrender.d3d8.h` would do on Direct3D 8.

Keep in mind what shader model 1.x has to work with: four texture coordinate registers
(so four taps and no more), no branching and no `max`, and the texture coordinate
registers can only be read by texture instructions, never by arithmetic. A check of
what a device actually supports is `D3DCAPS8::PixelShaderVersion`, which
`D3DPS_VERSION(1, 4)` has to be reached for the profiles above.
