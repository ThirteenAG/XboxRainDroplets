#ifndef __KIERO_H__
#define __KIERO_H__

#include <stdint.h>
#include "vtbl.h"

#define KIERO_INCLUDE_D3D8   1 // 1 if you need D3D8 hook
#define KIERO_INCLUDE_D3D9   1 // 1 if you need D3D9 hook
#define KIERO_INCLUDE_D3D10  1 // 1 if you need D3D10 hook
#define KIERO_INCLUDE_D3D11  1 // 1 if you need D3D11 hook
#define KIERO_INCLUDE_D3D12  1 // 1 if you need D3D12 hook
#define KIERO_INCLUDE_OPENGL 1 // 1 if you need OpenGL hook
#define KIERO_INCLUDE_VULKAN 0 // 1 if you need Vulkan hook
#define KIERO_USE_MINHOOK    1 // 1 if you will use kiero::bind function

#if KIERO_INCLUDE_D3D8
//to avoid conflicts with dx9 sdk
#include "minidx8.h"
#endif

#if KIERO_INCLUDE_D3D9
#include <d3d9.h>
#endif

#if KIERO_INCLUDE_D3D10
#include <dxgi.h>
#include <d3d10_1.h>
#include <d3d10.h>
#endif

#if KIERO_INCLUDE_D3D11
#include <dxgi.h>
#include <d3d11.h>
#endif

#if KIERO_INCLUDE_D3D12
#include <dxgi.h>
#include <d3d12.h>
#endif

#if KIERO_INCLUDE_OPENGL
#include <gl/GL.h>
#endif

#if KIERO_INCLUDE_VULKAN
#include <vulkan/vulkan.h>
#endif

#if KIERO_USE_MINHOOK
#include "minhook/include/MinHook.h"
#endif

#ifdef _UNICODE
# define KIERO_TEXT(text) L##text
#else
# define KIERO_TEXT(text) text
#endif

namespace kiero
{
    struct Status
    {
        enum Enum
        {
            UnknownError = -1,
            NotSupportedError = -2,
            ModuleNotFoundError = -3,

            AlreadyInitializedError = -4,
            NotInitializedError = -5,

            Success = 0,
        };
    };

    struct RenderType
    {
        enum Enum
        {
            None,

            D3D8,
            D3D9,
            D3D10,
            D3D11,
            D3D12,

            OpenGL,
            Vulkan,

            Auto,

            Size
        };
    };

    Status::Enum init(RenderType::Enum renderType);
    void shutdown();

    Status::Enum bind(uint16_t index, void** original, void* function);
    void unbind(uint16_t index);

    RenderType::Enum getRenderType();
}

#endif // __KIERO_H__