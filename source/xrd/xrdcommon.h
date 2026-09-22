#pragma once
// ---------------------------------------------------------------------------
// Shared definitions for the Xbox Rain Droplets renderer.
//
// One drop effect, every graphics API the games run on:
//   Direct3D 8, Direct3D 9, Direct3D 10, Direct3D 10.1, Direct3D 11,
//   Direct3D 12, OpenGL and Vulkan.
//
// The API backends (xrdrender.*.h) all draw the same vertices, so this header
// only holds what they share: the vertex format, the matrices, the colour
// helpers and the handles the effect passes around.
// ---------------------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <vector>
#include <mutex>
#include <atomic>

// ---------------------------------------------------------------------------
// The resources of the module. They are embedded exactly like the original
// builds did and are read by the effect and by the backends, so they are here,
// where both of them can see them: source/resources/Dropmask.rc has to agree
// with these ids.
// ---------------------------------------------------------------------------
// the atlas of drop shapes
#define IDR_DROPMASK 100
#define IDR_SNOWDROPMASK 101
// the menu blur of Scarface, built from source/resources/shaders
#define IDR_BLURPS 103
#define IDR_BLURVS 104
// The refraction of Direct3D 8 is a shader of model 1, which cannot be built at
// runtime the way the renderers of Direct3D 9 and above build theirs: these are
// the shaders of source/resources/shaders/ps8, built by the tools of tools/x86,
// see xrdrender.d3d8.h. One set is for the hardware of today (ps_1_4, which is as
// high as the model goes) and one for the hardware the games themselves ran on
// (ps_1_1).
#define IDR_DROP8PS14 110
#define IDR_DROP8PS11 111
#define IDR_BLUR8PS14 112
#define IDR_BLUR8PS11 113
#define IDR_LIGHT8PS14 114
#define IDR_LIGHT8PS11 115
#define IDR_FADE8PS14 116
#define IDR_FADE8PS11 117

namespace Xrd
{
    // -----------------------------------------------------------------------
    // renderer selection
    // -----------------------------------------------------------------------
    enum RendererId
    {
        RENDERER_NONE = 0,
        RENDERER_D3D8,
        RENDERER_D3D9,
        RENDERER_D3D10,
        RENDERER_D3D10_1,
        RENDERER_D3D11,
        RENDERER_D3D12,
        RENDERER_OPENGL,
        RENDERER_VULKAN,

        // Not an API of a game but the drawing of an emulator that hands the
        // frame of a game over where it is in between its world and its UI, see
        // xrdrender.thin3d.h. It serves whichever API that emulator runs.
        RENDERER_THIN3D,

        RENDERER_COUNT
    };

    // Where the vertices that get submitted are in.
    //   SCREEN - pixels, origin in the top left corner, y grows downwards, no
    //            matrix is involved. This is what the rain drops on the camera
    //            lens use.
    //   WORLD  - world space, transformed by the matrix passed to
    //            SetWorldMatrix. This is what the snow and the rain streaks
    //            that live in the world use.
    enum Projection
    {
        PROJECTION_SCREEN = 0,
        PROJECTION_WORLD,
    };

    enum PrimitiveType
    {
        PRIMITIVE_TRIANGLES = 0,
        PRIMITIVE_TRIANGLE_STRIP,
    };

    // Where the target is in its life when Render is called. Direct3D 8, 9, 10,
    // 11 and OpenGL find that out on their own, Direct3D 12 and Vulkan have to
    // be told, because nothing about a resource says what it is being used for.
    //
    //   PRESENT       the frame is finished and about to be presented, which is
    //                 what a hook around the present call sees.
    //   RENDER_TARGET the game is in the middle of drawing its frame, which is
    //                 what a hook before the UI sees. The drops then end up
    //                 under everything the game draws after them.
    enum TargetState
    {
        TARGET_STATE_PRESENT = 0,
        TARGET_STATE_RENDER_TARGET,
    };

    // -----------------------------------------------------------------------
    // geometry
    // -----------------------------------------------------------------------
    struct Vertex
    {
        float x, y, z;
        uint32_t color;     // 0xAARRGGBB
        float u0, v0;       // atlas of the drop shape, sampled with the alpha
        float u1, v1;       // copy of what was on screen, the refracted backdrop
    };

    // A drop of clear water is a lens, and the renderer has to be told which of the
    // drops are those: the atlas coordinate of one is moved below zero, which is a
    // coordinate no drop has, and the shader of the drops moves it back up before
    // it samples the atlas with it. Every renderer that draws the drops with such a
    // shader takes the mark off itself, and one that draws them with the fixed
    // function pipeline is handed the coordinate untouched.
    constexpr float AtlasLightMarker = 2.0f;

    struct Matrix
    {
        float m[4][4];

        static Matrix Identity()
        {
            Matrix r{};
            r.m[0][0] = r.m[1][1] = r.m[2][2] = r.m[3][3] = 1.0f;
            return r;
        }

        // Left handed, matching what the games and the original code use.
        static Matrix OrthographicOffCenter(float left, float right, float bottom, float top, float zn, float zf)
        {
            Matrix r = Identity();
            r.m[0][0] = 2.0f / (right - left);
            r.m[1][1] = 2.0f / (top - bottom);
            r.m[2][2] = 1.0f / (zf - zn);
            r.m[3][0] = (left + right) / (left - right);
            r.m[3][1] = (top + bottom) / (bottom - top);
            r.m[3][2] = zn / (zn - zf);
            return r;
        }

        static Matrix Multiply(const Matrix& a, const Matrix& b)
        {
            Matrix r{};
            for (int i = 0; i < 4; i++)
                for (int j = 0; j < 4; j++)
                    r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j] + a.m[i][3] * b.m[3][j];
            return r;
        }
    };

    struct Size
    {
        int32_t width = 0;
        int32_t height = 0;
    };

    // -----------------------------------------------------------------------
    // resources the effect creates. A backend fills these in, everything else
    // only passes them back untouched.
    // -----------------------------------------------------------------------
    struct Texture
    {
        void* resource = nullptr;   // IDirect3DTexture8/9, ID3D10Texture2D, ...
        void* extra = nullptr;      // ID3D11ShaderResourceView, ID3D12Resource, ...
        void* extra2 = nullptr;     // IDirect3DSurface8/9, VkImageView, ...
        Size size{};
        bool ownsResource = true;
    };

    // Something to draw into that is not the target the API reports on its own.
    // A game hook before the UI hands over the render target of the scene here.
    // The state is only looked at by the APIs without implicit state.
    struct RenderTarget
    {
        void* resource = nullptr;
        void* extra = nullptr;
        Size size{};
        TargetState state = TARGET_STATE_RENDER_TARGET;
        uint32_t format = 0;    // only Vulkan needs to be told, the other APIs know
    };

    // -----------------------------------------------------------------------
    // blends the effect uses
    // -----------------------------------------------------------------------
    enum Blend
    {
        BLEND_ALPHA = 0,        // srcalpha, invsrcalpha, the drop darkens what is behind it
    };

    // -----------------------------------------------------------------------
    // helpers
    // -----------------------------------------------------------------------
    inline uint32_t ColorARGB(uint32_t a, uint32_t r, uint32_t g, uint32_t b)
    {
        return (a << 24) | (r << 16) | (g << 8) | b;
    }

    inline uint32_t ColorFloat(float r, float g, float b, float a)
    {
        auto toByte = [](float v) -> uint32_t
        {
            v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
            return (uint32_t)(v * 255.0f + 0.5f);
        };

        return ColorARGB(toByte(a), toByte(r), toByte(g), toByte(b));
    }

    // The mask textures are 8 bits per channel, stored as ARGB in memory, while
    // Direct3D 8 and 9 expect a D3DCOLOR (ARGB in a dword) and Direct3D 10 and
    // above expect RGBA. Reading them as bytes keeps every backend on the same
    // page, see the texture creation of each one.
    inline float Saturate(float v)
    {
        return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    }
}
