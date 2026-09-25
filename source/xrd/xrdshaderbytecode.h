#pragma once
#include "xrdcommon.h"

namespace Xrd
{
    // Embedded DXBC is compiled by tools/BuildShaders.ps1. Loading a
    // shader must never invoke the HLSL compiler on the game's render thread.
    struct ShaderBytecode
    {
        const void* data = nullptr;
        size_t size = 0;
        explicit operator bool() const { return data && size; }
        const void* GetBufferPointer() const { return data; }
        size_t GetBufferSize() const { return size; }
    };

    inline ShaderBytecode LoadShaderBytecode(int resourceId)
    {
        HMODULE module = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCWSTR)&LoadShaderBytecode, &module);
        const auto resource = FindResourceW(module, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
        if (!resource) return {};
        const auto loaded = LoadResource(module, resource);
        return { loaded ? LockResource(loaded) : nullptr, SizeofResource(module, resource) };
    }
}
