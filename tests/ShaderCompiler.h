#pragma once
// Runtime compiler for the test scene only. Production Direct3D shaders use embedded bytecode.
#include <windows.h>
#include <d3dcommon.h>
#include <cstring>

namespace Xrd
{
    namespace CompileFlags
    {
        constexpr unsigned int Debug = 1;
        constexpr unsigned int SkipValidation = 2;
        constexpr unsigned int SkipOptimization = 4;
        constexpr unsigned int EnableStrictness = 2048;
        constexpr unsigned int OptimizationLevel3 = 32768;
    }

    using D3DCompileFn = long(WINAPI*)(const void* pSrcData, size_t SrcDataSize, const char* pSourceName,
        const void* pDefines, ID3DInclude* pInclude, const char* pEntrypoint, const char* pTarget,
        unsigned int Flags1, unsigned int Flags2, ID3DBlob** ppCode, ID3DBlob** ppErrorMsgs);

    inline D3DCompileFn GetD3DCompile()
    {
        static D3DCompileFn fnCompile = []() -> D3DCompileFn
        {
            static const wchar_t* versions[] =
            {
                L"d3dcompiler_47.dll", L"d3dcompiler_46.dll", L"d3dcompiler_45.dll",
                L"d3dcompiler_44.dll", L"d3dcompiler_43.dll",
            };

            for (const wchar_t* name : versions)
            {
                HMODULE module = LoadLibraryW(name);
                if (!module)
                    continue;

                if (auto fn = (D3DCompileFn)GetProcAddress(module, "D3DCompile"))
                    return fn;

                FreeLibrary(module);
            }

            return nullptr;
        }();

        return fnCompile;
    }

    // Compiles one entry point of a source string. Returns nullptr when no
    // compiler could be loaded or the shader did not build, the caller then
    // simply leaves that effect turned off.
    inline ID3DBlob* CompileShader(const char* source, const char* entryPoint, const char* target)
    {
        D3DCompileFn fnCompile = GetD3DCompile();
        if (!fnCompile)
            return nullptr;

        ID3DBlob* pBlob = nullptr;
        ID3DBlob* pErrors = nullptr;

        fnCompile(source, strlen(source), "xrd", nullptr, nullptr, entryPoint, target,
            CompileFlags::OptimizationLevel3, 0, &pBlob, &pErrors);

        if (pErrors)
            pErrors->Release();

        return pBlob;
    }
}
