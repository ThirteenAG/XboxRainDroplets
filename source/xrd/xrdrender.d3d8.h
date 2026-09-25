#pragma once
// ---------------------------------------------------------------------------
// Direct3D 8 backend.
//
// One backend like all the others, it only cannot share a translation unit with
// the Direct3D 9 one: the headers of the two versions define the same Direct3D
// enumerations, so a translation unit can only include one of them. A project that
// is a Direct3D 8 game therefore defines XRD_ENABLE_D3D8 before including
// xrd/xrd.h, that one define is the device type, the headers and the renderer
// at once, and it is all such a project has to say.
//
// A binary that wants Direct3D 8 next to Direct3D 9 (the wrapper) includes this
// header from a translation unit of its own instead, which is what
// source/xrd/xrdrender.d3d8.cpp is for.
//
// If QueryInterface exposes a D3D9 device (d3d8to9), drawing is delegated to the
// registered D3D9 backend. D3D8 plugins compile xrdrender.d3d9.cpp for this;
// the wrapper already registers D3D9. Missing devices or resource interfaces
// leave the original D3D8 path below in use, with no new runtime dependency.
//
// Like Direct3D 9 it uses the fixed function pipeline with two texture stages
// and copies the frame with CopyRects, which is what the original Direct3D 8
// code did.
//
// The one thing Direct3D 8 can do that Direct3D 9 cannot is the light of the
// refraction: Direct3D 9 needs shader model 3 for the gather, Direct3D 8 has
// pixel shaders of model 1, which are enough for the same light when the gather
// is done in passes of its own into a small render target first. See
// EnsureShaders and RenderLightField, and source/resources/shaders/ps8, which
// are those shaders. A device that cannot run one of them keeps the fixed
// function pipeline, which is what every Direct3D 8 game drew its drops with
// until now.
// ---------------------------------------------------------------------------

#include "xrdrender.h"

#include <d3d8.h>

// What the light of the frame is left to out of the depth of it asks the device
// for the container of the depth surface, see GetContainer, and the identifier of
// a texture of Direct3D 8 is in dxguid rather than in the header. A project of a
// game that is not a Direct3D 8 one compiles this backend and never calls it, and
// would not fail to link over a symbol it never uses.
#pragma comment(lib, "dxguid.lib")

// Only COM identifiers are needed here; including d3d9.h would conflict with
// d3d8.h. The optional D3D9 backend is compiled in a separate translation unit.
EXTERN_C const IID IID_IDirect3DDevice9;
EXTERN_C const IID IID_IDirect3DTexture9;
EXTERN_C const IID IID_IDirect3DSurface9;

namespace Xrd
{
    namespace D3D8Lists
    {
        struct StageState
        {
            DWORD stage;
            D3DTEXTURESTAGESTATETYPE state;
        };

        inline const StageState stageStates[] =
        {
            { 0, D3DTSS_TEXCOORDINDEX },
            { 1, D3DTSS_TEXCOORDINDEX },
            { 0, D3DTSS_COLOROP },
            { 0, D3DTSS_COLORARG1 },
            { 0, D3DTSS_COLORARG2 },
            { 0, D3DTSS_ALPHAOP },
            { 0, D3DTSS_ALPHAARG1 },
            { 0, D3DTSS_ALPHAARG2 },
            { 1, D3DTSS_COLOROP },
            { 1, D3DTSS_COLORARG1 },
            { 1, D3DTSS_COLORARG2 },
            { 1, D3DTSS_ALPHAOP },
            { 1, D3DTSS_ALPHAARG1 },
        };

        constexpr int NumStageStates = sizeof(stageStates) / sizeof(stageStates[0]);
    }

    class D3D8Backend : public Backend
    {
    public:
        ~D3D8Backend() override
        {
            Shutdown();
        }

        bool Init(void* pNative) override
        {
            Shutdown();
            pDevice = (IDirect3DDevice8*)pNative;
            if (pDevice && SUCCEEDED(pDevice->QueryInterface(IID_IDirect3DDevice9, (void**)&pDevice9)) && pDevice9)
            {
                pBackend9 = Detail::Create(RENDERER_D3D9);
                if (!pBackend9 || !pBackend9->Init(pDevice9))
                    ReleaseD3D9();
            }
            return pDevice != nullptr;
        }

        void Shutdown() override
        {
            ReleaseD3D9();
            ReleaseResources();
            pMaskTexture = nullptr;
            pTargetOverride = nullptr;
            pDevice = nullptr;
        }

        void Reset() override
        {
            if (pBackend9)
                pBackend9->Reset();
            ReleaseResources();
        }

        bool UpdateNative(void* pNative) override
        {
            return pNative == pDevice;
        }

        bool UsesD3D9() const { return usingD3D9; }

        bool Prepare(int maxVertices) override
        {
            if (!pDevice) return false;
            usingD3D9 = RenderD3D9(nullptr, maxVertices, PRIMITIVE_TRIANGLES);
            if (usingD3D9) return true;
            IDirect3DSurface8* target = nullptr;
            if (pTargetOverride && pTargetOverride->resource)
            {
                target = (IDirect3DSurface8*)pTargetOverride->resource;
                target->AddRef();
            }
            else
                pDevice->GetRenderTarget(&target);
            if (!target) return false;
            D3DSURFACE_DESC desc{};
            const bool ready = SUCCEEDED(target->GetDesc(&desc)) && EnsureResources(desc, maxVertices) &&
                (!bShaderDrops || EnsureField(desc));
            target->Release();
            return ready;
        }

        bool IsActive() const override
        {
            return pDevice != nullptr;
        }

        // Whether the drops are drawn with the shaders of the refraction, see
        // EnsureShaders. The effect asks this to know whether a drop of clear water
        // is a lens, see WaterDrops::GatheredLight, and it can change between two
        // frames: the shaders are built the first time the drops are drawn.
        bool GathersLight() const override
        {
            return usingD3D9 ? pBackend9->GathersLight() : bShaderDrops;
        }

        Size GetSize() const override
        {
            Size size = targetSize;

            if (pTargetOverride && pTargetOverride->size.width > 0)
            {
                size = pTargetOverride->size;
            }
            else if (pDevice)
            {
                IDirect3DSurface8* pTarget = nullptr;

                if (pTargetOverride && pTargetOverride->resource)
                    pTarget = (IDirect3DSurface8*)pTargetOverride->resource;
                else
                    pDevice->GetRenderTarget(&pTarget);

                if (pTarget)
                {
                    D3DSURFACE_DESC desc{};
                    if (SUCCEEDED(pTarget->GetDesc(&desc)))
                        size = { (int32_t)desc.Width, (int32_t)desc.Height };

                    if (!pTargetOverride || !pTargetOverride->resource)
                        pTarget->Release();
                }
            }

            const_cast<D3D8Backend*>(this)->targetSize = size;
            return size;
        }

        void SetMaskTexture(Texture* pMask) override
        {
            if (pMaskTexture != pMask)
            {
                ReleaseD3D9Mask();
                if (pBackend9 && pMask && pMask->resource)
                {
                    ((IDirect3DTexture8*)pMask->resource)->QueryInterface(IID_IDirect3DTexture9, &mask9.resource);
                    mask9.size = pMask->size;
                }
            }
            pMaskTexture = pMask;
        }

        Texture* CreateTexture(int textureWidth, int textureHeight, const uint8_t* pixels) override
        {
            if (!pDevice || textureWidth <= 0 || textureHeight <= 0)
                return nullptr;

            IDirect3DTexture8* pTexture = nullptr;
            if (FAILED(pDevice->CreateTexture(textureWidth, textureHeight, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &pTexture)))
                return nullptr;

            if (pixels)
            {
                D3DLOCKED_RECT locked{};
                if (SUCCEEDED(pTexture->LockRect(0, &locked, nullptr, 0)))
                {
                    for (int y = 0; y < textureHeight; y++)
                    {
                        const uint8_t* src = pixels + (size_t)y * textureWidth * 4;
                        uint8_t* dst = (uint8_t*)locked.pBits + (size_t)y * locked.Pitch;

                        for (int x = 0; x < textureWidth; x++)
                        {
                            // the masks are stored as RGBA, A8R8G8B8 wants BGRA
                            dst[x * 4 + 0] = src[x * 4 + 2];
                            dst[x * 4 + 1] = src[x * 4 + 1];
                            dst[x * 4 + 2] = src[x * 4 + 0];
                            dst[x * 4 + 3] = src[x * 4 + 3];
                        }
                    }

                    pTexture->UnlockRect(0);
                }
            }

            auto* pResult = new Texture{};
            pResult->resource = pTexture;
            pResult->size = { textureWidth, textureHeight };
            return pResult;
        }

        void DestroyTexture(Texture* pTexture) override
        {
            if (!pTexture)
                return;

            if (pMaskTexture == pTexture)
                SetMaskTexture(nullptr);

            if (pTexture->resource)
                ((IDirect3DTexture8*)pTexture->resource)->Release();

            delete pTexture;
        }

        void SetTarget(RenderTarget* pTarget) override
        {
            pTargetOverride = pTarget;
        }

        void SetTargetState(TargetState state) override
        {
            // Direct3D 8 has no resource states
            (void)state;
        }

        void SetProjection(Projection proj, const Matrix* pWorld, float viewWidth, float viewHeight) override
        {
            projection = proj;
            width = viewWidth;
            height = viewHeight;

            if (pWorld)
                worldMatrix = *pWorld;
        }

        void SetSceneUVScale(float offsetX, float scaleX, float offsetY, float scaleY) override
        {
            uvOffsetX = offsetX;
            uvScaleX = scaleX;
            uvOffsetY = offsetY;
            uvScaleY = scaleY;
        }

        void SetSceneComplement(bool enabled) override
        {
            sceneComplement = enabled;
        }

        void SetSceneSampling(bool enabled) override
        {
            sceneSampling = enabled;
        }

        void Render(const Vertex* pVertices, int numVertices, PrimitiveType primitive) override
        {
            if (!pDevice || !pVertices || numVertices <= 0)
                return;

            usingD3D9 = RenderD3D9(pVertices, numVertices, primitive);
            if (usingD3D9)
                return;

            const int numIndices = (primitive == PRIMITIVE_TRIANGLES) ? (numVertices / 4) * 6 : 0;

            if (primitive == PRIMITIVE_TRIANGLES && numIndices <= 0)
                return;

            IDirect3DSurface8* pTarget = nullptr;
            bool bOwnTarget = false;

            if (pTargetOverride && pTargetOverride->resource)
            {
                pTarget = (IDirect3DSurface8*)pTargetOverride->resource;
            }
            else if (SUCCEEDED(pDevice->GetRenderTarget(&pTarget)) && pTarget)
            {
                bOwnTarget = true;
            }
            else
            {
                return;
            }

            D3DSURFACE_DESC desc{};
            if (FAILED(pTarget->GetDesc(&desc)) || !EnsureResources(desc, numVertices))
            {
                if (bOwnTarget)
                    pTarget->Release();

                return;
            }

            targetSize = { (int32_t)desc.Width, (int32_t)desc.Height };

            // What the drops and the light of the frame are drawn into, and the
            // viewport they are drawn through. A state block holds the drawing state
            // of the game and puts it back, but not these: the light of the frame is
            // found in a field of an eighth of the target with no depth buffer of its
            // own, so a game that is left with either of them changed draws no more
            // of its own world than the depth buffer it no longer has lets it, which
            // is the buildings of a city gone and the sky still there. The renderers
            // of Direct3D 9 and above keep both of them by hand the same way.
            IDirect3DSurface8* pOriginalTarget = nullptr;
            IDirect3DSurface8* pOriginalDepth = nullptr;
            D3DVIEWPORT8 originalViewport{};
            const bool bHaveOriginalViewport = SUCCEEDED(pDevice->GetViewport(&originalViewport));
            pDevice->GetRenderTarget(&pOriginalTarget);
            pDevice->GetDepthStencilSurface(&pOriginalDepth);

            // The depth of the frame, when the game hands out one that can be read.
            // A game that replaces its depth buffer with a texture of its own - see
            // DepthStencil.ixx of the widescreen fix of True Crime: New York City -
            // has the container of that surface be the texture, and the light of a
            // drop can then be left to what is close to the camera rather than to the
            // whole of the frame, see fadePS8.hlsl. Every other game hands out a depth
            // buffer of a kind no texture can be made of, the container of it is
            // nothing, and the light of the frame is what it always was.
            IDirect3DTexture8* pDepthTexture = nullptr;

            if (pOriginalDepth)
                pOriginalDepth->GetContainer(IID_IDirect3DTexture8, (void**)&pDepthTexture);

            // The original Direct3D 8 code did the same thing: capture a state
            // block, change everything, draw and put the state back. It is what
            // keeps the game's own drawing intact afterwards.
            DWORD stateBlock = 0;
            if (SUCCEEDED(pDevice->CreateStateBlock(D3DSBT_ALL, &stateBlock)))
            {
                pDevice->CaptureStateBlock(stateBlock);

                D3DMATRIX world{}, view{}, projectionMatrix{};
                if (projection == PROJECTION_WORLD)
                {
                    pDevice->GetTransform(D3DTS_WORLD, &world);
                    pDevice->GetTransform(D3DTS_VIEW, &view);
                    pDevice->GetTransform(D3DTS_PROJECTION, &projectionMatrix);
                }

                // The picture the drops are drawn on top of. The frame is copied
                // into the surface of the system memory pool first and handed to
                // the texture the drops sample from there, see EnsureResources for
                // why it cannot be copied into that one directly.
                const HRESULT copyResult = pDevice->CopyRects(pTarget, nullptr, 0, pSceneImage, nullptr);
                const HRESULT uploadResult = copyResult;

                if (SUCCEEDED(copyResult))
                    UploadScene(desc);

                // Whether this batch of drops is drawn with the shader of the
                // refraction: the light of the frame is a light of the screen and
                // the copy of the frame is what the shader is handed, so a frame
                // that is drawn with the complement of itself (snow), one that is
                // not sampled at all, and the snow and the streaks that live in
                // the world are drawn the way they always were.
                const bool bRefract = bShaderDrops && projection == PROJECTION_SCREEN && sceneSampling &&
                    !sceneComplement;

                // The light of the frame is found before a single drop is drawn, into
                // a field of its own, and every pass of that is a pass of its own:
                // the passes bind textures of their own and change the target and
                // the viewport, so the states of the drops are set after them, which
                // is what ApplyState below is.
                if (bRefract)
                {
                    RenderLightField(desc, pDepthTexture);
                    pDevice->SetRenderTarget(pTarget, nullptr);
                }

                ApplyState(desc);

                if (bRefract)
                {
                    // the field the light was gathered into, on the third stage
                    pDevice->SetTexture(2, pLightField);
                    SetStageSampler(2);
                }
                else
                {
                    // The drops of the fixed function pipeline are drawn by the
                    // combiners of the stages, so a pixel shader of the game must
                    // not be left set over them. It is put back with the state of
                    // the game when the batch is drawn, see ApplyStateBlock below.
                    pDevice->SetPixelShader(0);
                }

                void* pVertexData = nullptr;
                if (SUCCEEDED(pVertexBuffer->Lock(0, numVertices * (projection == PROJECTION_SCREEN ? vertexStride : sizeof(Vertex)), (BYTE**)&pVertexData, 0)))
                {
                    if (bRefract)
                    {
                        // A vertex of the refraction: the shape of the drop, the
                        // frame behind it, and where on the screen the drop is,
                        // which is where its light is. Where on the screen is not
                        // the same as where in the frame: the crop a game asks the
                        // refraction to sample moves the frame and not the light.
                        ShaderVertex* pDst = (ShaderVertex*)pVertexData;

                        for (int i = 0; i < numVertices; i++)
                        {
                            // the mark of a lens, which the effect put into the
                            // atlas coordinate because there is nowhere else a
                            // shader of model 1 could be told about it
                            const bool lens = pVertices[i].u0 < 0.0f;

                            pDst[i].x = pVertices[i].x;
                            pDst[i].y = pVertices[i].y;
                            pDst[i].z = 0.0f;
                            pDst[i].rhw = 1.0f;
                            pDst[i].color = pVertices[i].color;
                            pDst[i].lens = lens ? 0xFFFFFFFF : 0x00000000;
                            pDst[i].u0 = lens ? pVertices[i].u0 + AtlasLightMarker : pVertices[i].u0;
                            pDst[i].v0 = pVertices[i].v0;
                            pDst[i].u1 = pVertices[i].u1 * uvScaleX + uvOffsetX;
                            pDst[i].v1 = pVertices[i].v1 * uvScaleY + uvOffsetY;

                            // Where on the screen the drop is, which is where its light
                            // is. The coordinate of the frame a drop samples is not it:
                            // that one is the rectangle of the frame the refraction
                            // reads, which is a neighbourhood of the drop scaled by the
                            // drop and by the game, so a drop would gather the light of
                            // a place half a screen away from itself. The crop a game
                            // asks the refraction to sample moves the frame and not the
                            // light, and neither does the drop.
                            //
                            // The texel of the field the drop is in and not the corner
                            // of it, which is what the position of the drop over the
                            // size of the target is: half a texel of the field, which
                            // is four pixels of the target, is what puts the tap in
                            // the middle of that texel, see HalfTexelX.
                            pDst[i].u2 = targetSize.width > 0
                                ? pVertices[i].x * 0.125f / (float)fieldWidth + HalfTexelX() : 0.0f;
                            pDst[i].v2 = targetSize.height > 0
                                ? pVertices[i].y * 0.125f / (float)fieldHeight + HalfTexelY() : 0.0f;
                        }

                        pDevice->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX3);
                        pDevice->SetStreamSource(0, pVertexBuffer, ShaderVertexStride);

                        // how much of the light of the frame a drop of clear water
                        // takes: one it takes, and the part over one is added by
                        // the shader, see dropPS8.hlsl and LightStrength
                        const float strength[4] = { LightStrength - 1.0f, 1.0f, 1.0f, 1.0f };
                        pDevice->SetPixelShaderConstant(0, strength, 1);
                        pDevice->SetPixelShader(pDropShader);
                    }
                    else if (projection == PROJECTION_SCREEN)
                    {
                        struct ScreenVertex
                        {
                            float x, y, z, rhw;
                            uint32_t color;
                            float u0, v0, u1, v1;
                        };

                        ScreenVertex* pDst = (ScreenVertex*)pVertexData;
                        for (int i = 0; i < numVertices; i++)
                        {
                            pDst[i].x = pVertices[i].x;
                            pDst[i].y = pVertices[i].y;
                            pDst[i].z = 0.0f;
                            pDst[i].rhw = 1.0f;
                            pDst[i].color = pVertices[i].color;
                            // A drop the effect marked as a lens is drawn without
                            // the shader here, because the device could not run it:
                            // the mark is not a coordinate of the atlas and has to
                            // be taken off, or the wrong tile of it is sampled.
                            pDst[i].u0 = pVertices[i].u0 < 0.0f ? pVertices[i].u0 + AtlasLightMarker : pVertices[i].u0;
                            pDst[i].v0 = pVertices[i].v0;
                            pDst[i].u1 = pVertices[i].u1 * uvScaleX + uvOffsetX;
                            pDst[i].v1 = pVertices[i].v1 * uvScaleY + uvOffsetY;
                        }

                        pDevice->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX2);
                        pDevice->SetStreamSource(0, pVertexBuffer, ScreenVertexStride);
                    }
                    else
                    {
                        Vertex* pDst = (Vertex*)pVertexData;
                        for (int i = 0; i < numVertices; i++)
                        {
                            pDst[i] = pVertices[i];
                            pDst[i].u1 = pVertices[i].u1 * uvScaleX + uvOffsetX;
                            pDst[i].v1 = pVertices[i].v1 * uvScaleY + uvOffsetY;
                        }

                        pDevice->SetVertexShader(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX2);
                        pDevice->SetStreamSource(0, pVertexBuffer, sizeof(Vertex));
                    }

                    pVertexBuffer->Unlock();

                    if (primitive == PRIMITIVE_TRIANGLES)
                    {
                        pDevice->SetIndices(pIndexBuffer, 0);
                        pDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, numVertices, 0, numIndices / 3);
                    }
                    else
                    {
                        pDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, numVertices - 2);
                    }
                }

                if (projection == PROJECTION_WORLD)
                {
                    pDevice->SetTransform(D3DTS_WORLD, &world);
                    pDevice->SetTransform(D3DTS_VIEW, &view);
                    pDevice->SetTransform(D3DTS_PROJECTION, &projectionMatrix);
                }

                pDevice->ApplyStateBlock(stateBlock);
                pDevice->DeleteStateBlock(stateBlock);
            }

            // and what they were drawn into, see above
            if (pOriginalTarget || pOriginalDepth)
                pDevice->SetRenderTarget(pOriginalTarget, pOriginalDepth);

            if (bHaveOriginalViewport)
                pDevice->SetViewport(&originalViewport);

            if (pDepthTexture)
                pDepthTexture->Release();

            if (pOriginalTarget)
                pOriginalTarget->Release();

            if (pOriginalDepth)
                pOriginalDepth->Release();

            if (bOwnTarget)
                pTarget->Release();
        }

    private:
        static constexpr UINT ScreenVertexStride = sizeof(float) * 9;

        // A vertex of a drop that is drawn with the shader of the refraction
        // carries one thing more than a vertex of the fixed function pipeline: the
        // colour that says whether the drop is a lens. A shader of model 1 cannot
        // be told about it in a texture coordinate (arithmetic on one is not part
        // of the model), and a vertex has a colour of its own to spare.
        static constexpr UINT ShaderVertexStride = sizeof(float) * 12;

        struct ShaderVertex
        {
            float x, y, z, rhw;
            uint32_t color;
            uint32_t lens;
            float u0, v0;       // the shape of the drop in the atlas
            float u1, v1;       // the copy of the frame behind it
            float u2, v2;       // where on the screen it is, which is where its light is
        };

        // What the drops do with the light: the numbers the shaders of Direct3D 9
        // and above use for it, see xrdshaders.h and source/resources/shaders/ps8,
        // with two of them different here.
        //
        // The floor is one and not the 1.25 of those renderers, because the two
        // averages the light is found as the difference of here are of the very same
        // quantity, the square of the frame: a frame that is the same all over leaves
        // exactly the same in both of them, so the whole of what is left over is a
        // light. Rendering the light of a drop out of an average of the frame that is
        // weighted by how bright it is, which is what those renderers do, needs the
        // slack of the 1.25 to keep a frame that is brighter on one side of a drop
        // than on the other from counting as a light.
        //
        // The colour of a light is pushed out of its own grey by arithmetic here and
        // not by the 2.5 of those renderers, because a constant of a shader of model
        // 1 holds a value between minus one and one and a device clamps what is
        // written into it: see lightPS8.hlsl, where the push and the fade in of the
        // light are the two instructions that are worth spending on it.
        static constexpr float LightStrength = 2.0f;    // how much of it a drop takes
        // How much brighter than the frame around it an area has to be to count at
        // all. The renderers above weigh what a light is by how bright and how much
        // of a colour it is and divide that by the weight of the whole of the gather,
        // which is what tells a light from the scenery around it: a small rear light
        // of a car is what stands out of that division and a building of a skyline,
        // which is as bright as the frame around it is, is not, however bright it is.
        //
        // A shader of model 1 cannot divide, so what tells a light from the scenery
        // here is the square of the frame: the small area of it is weighed by its own
        // brightness in the shader, see lightPS8.hlsl, and what is taken off it is the
        // frame around it times this. One of the renderers above is what a frame that
        // is the same all over leaves of that - the square of it less the whole of it,
        // which is nothing for every level below one - and two is what leaves room
        // over that for a lamp of half the frame to be a light of its own, see
        // lightPS8.hlsl: the frame around a light is bright and the light is brighter.
        //
        // The floor is a weight of a pass of the field and not a constant of the shader
        // (see RenderLightField), so it is not the shader that is limited to one here.
        static constexpr float LightFloor = 2.0f;
        // What of a light is left once it is not bright enough to be one, out of the grey
        // of it, see lightPS8.hlsl. The renderers above ramp on the brightness of the
        // light from a low of 0.03 to a high of 0.20; four of the grey of the light is
        // what a shader of model 1 can afford, so what is taken off it is four times the
        // low: a light of a sky, which stands out of the frame around it by a little
        // wherever the sky is brightest, is left out and a light of a lamp, which stands
        // out of it by several times that, is not.
        static constexpr float LightRampLow = 0.005f;
        // How close to the camera the frame behind a drop has to be for the light of it
        // to fall on that drop, out of the depth of the frame, see fadePS8.hlsl. The
        // depth a game hands out to be read runs from one at the camera to zero at the
        // far plane, so this is what of it still counts as the light of the street a
        // drop is on rather than the light of the skyline behind it, and what is left
        // of the light of a frame that is further away than that.
        static constexpr float LightDepthScale = 1.0f;
        static constexpr float LightDepthOffset = 0.35f;
        // How much of the colour of a light a drop keeps, the same as LightChroma,
        // which the renderers above use. Two and a half times the light of a small
        // area less one and a half times the grey of the frame it was found in, and
        // a constant of a shader of model 1 holds no more than one, so what is
        // passed in is the light, then what of it is beyond its own grey, and then
        // half of that again, see lightPS8.hlsl.
        static constexpr float LightChroma = 2.5f;

        void ReleaseResources()
        {
            if (pDropShader) { pDevice->DeletePixelShader(pDropShader); pDropShader = 0; }
            if (pBlurShader) { pDevice->DeletePixelShader(pBlurShader); pBlurShader = 0; }
            if (pLightShader) { pDevice->DeletePixelShader(pLightShader); pLightShader = 0; }
            if (pFadeShader) { pDevice->DeletePixelShader(pFadeShader); pFadeShader = 0; }
            pLightField = nullptr;

            shaderAttempted = false;
            bShaderDrops = false;

            ReleaseField();

            if (pSceneImage)
            {
                pSceneImage->Release();
                pSceneImage = nullptr;
            }

            if (pSceneTexture)
            {
                pSceneTexture->Release();
                pSceneTexture = nullptr;
            }

            if (pVertexBuffer)
            {
                pVertexBuffer->Release();
                pVertexBuffer = nullptr;
            }

            if (pIndexBuffer)
            {
                pIndexBuffer->Release();
                pIndexBuffer = nullptr;
            }

            sceneFormat = D3DFMT_UNKNOWN;
            sceneWidth = 0;
            sceneHeight = 0;
        }

        bool EnsureResources(const D3DSURFACE_DESC& desc, int numVertices)
        {
            static constexpr int MaxVertices = 64000;
            static constexpr int MaxIndices = (MaxVertices / 4) * 6;

            if (numVertices > MaxVertices)
                return false;

            // The shaders of the refraction are built the first time the drops are
            // drawn, because that is when the device is known to be one that can
            // run them, and a drop that is drawn with them carries more than one
            // that is not: the vertex buffer holds the larger of the two strides.
            EnsureShaders();
            vertexStride = bShaderDrops ? ShaderVertexStride : ScreenVertexStride;

            if (!pVertexBuffer)
            {
                if (FAILED(pDevice->CreateVertexBuffer(MaxVertices * vertexStride, D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED, &pVertexBuffer)))
                    return false;

                if (FAILED(pDevice->CreateIndexBuffer(MaxIndices * sizeof(uint16_t), D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_MANAGED, &pIndexBuffer)))
                {
                    pVertexBuffer->Release();
                    pVertexBuffer = nullptr;
                    return false;
                }

                void* pData = nullptr;
                if (SUCCEEDED(pIndexBuffer->Lock(0, 0, (BYTE**)&pData, 0)))
                {
                    uint16_t* pIndicesData = (uint16_t*)pData;
                    for (int i = 0; i < MaxVertices / 4; i++)
                    {
                        pIndicesData[i * 6 + 0] = (uint16_t)(i * 4 + 0);
                        pIndicesData[i * 6 + 1] = (uint16_t)(i * 4 + 1);
                        pIndicesData[i * 6 + 2] = (uint16_t)(i * 4 + 2);
                        pIndicesData[i * 6 + 3] = (uint16_t)(i * 4 + 0);
                        pIndicesData[i * 6 + 4] = (uint16_t)(i * 4 + 2);
                        pIndicesData[i * 6 + 5] = (uint16_t)(i * 4 + 3);
                    }

                    pIndexBuffer->Unlock();
                }
            }

            if (pSceneTexture && sceneFormat == desc.Format && sceneWidth == desc.Width && sceneHeight == desc.Height)
                return true;

            if (pSceneImage)
            {
                pSceneImage->Release();
                pSceneImage = nullptr;
            }

            if (pSceneTexture)
            {
                pSceneTexture->Release();
                pSceneTexture = nullptr;
            }

            // What the drops show of the frame behind them, and the reason there are
            // two of them: the frame is copied into the surface and the drops sample
            // the texture, and Direct3D 8 can only do one of the two usefully. A
            // copy into a texture, whatever pool it was created in and whether it is
            // a render target or not, is accepted by the driver (it returns S_OK)
            // and then does nothing at all, so the drops sample the empty texture it
            // was and are drawn black - which is what this backend did until now.
            // A copy into a surface of the system memory pool really copies, so the
            // frame goes there first and UploadScene hands it to the texture.
            if (FAILED(pDevice->CreateImageSurface(desc.Width, desc.Height, desc.Format, &pSceneImage)))
                return false;

            // The texture the drops sample is one the driver keeps where it draws
            // from, and one it expects to be written into from here: a managed
            // texture takes the rows of the copy and hands them to a device that
            // samples nothing of them.
            if (FAILED(pDevice->CreateTexture(desc.Width, desc.Height, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &pSceneTexture)))
            {
                pSceneImage->Release();
                pSceneImage = nullptr;
                return false;
            }

            sceneFormat = desc.Format;
            sceneWidth = desc.Width;
            sceneHeight = desc.Height;
            return true;
        }


        void ApplyState(const D3DSURFACE_DESC& desc)
        {
            if (projection == PROJECTION_WORLD)
            {
                // the matrix of the caller already combines view and projection
                D3DMATRIX identity{};
                identity._11 = identity._22 = identity._33 = identity._44 = 1.0f;

                pDevice->SetTransform(D3DTS_WORLD, &identity);
                pDevice->SetTransform(D3DTS_VIEW, &identity);
                pDevice->SetTransform(D3DTS_PROJECTION, (const D3DMATRIX*)&worldMatrix);
            }

            pDevice->SetTexture(0, (pMaskTexture && pMaskTexture->resource) ? (IDirect3DTexture8*)pMaskTexture->resource : nullptr);
            pDevice->SetTexture(1, sceneSampling ? pSceneTexture : nullptr);
            pDevice->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
            pDevice->SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, 1);
            pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
            pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
            pDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TEXTURE);
            pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
            pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
            pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TEXTURE);
            if (sceneSampling)
            {
                pDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_MODULATE);
                pDevice->SetTextureStageState(1, D3DTSS_COLORARG1, sceneComplement ? (D3DTA_TEXTURE | D3DTA_COMPLEMENT) : D3DTA_TEXTURE);
                pDevice->SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_CURRENT);
            }
            else
            {
                pDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
                pDevice->SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_CURRENT);
            }
            pDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
            pDevice->SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_CURRENT);

            // Three stages, because the shader of the drops is handed the field the
            // light of the frame was gathered into on the third one. The states of a
            // stage whose texture the fixed function pipeline does not use are read
            // by nothing.
            for (int stage = 0; stage < 3; stage++)
                SetStageSampler(stage);

            pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
            pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
            pDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
            pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
            pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
            pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
            pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
            pDevice->SetRenderState(D3DRS_COLORWRITEENABLE, 0x0000000F);

            SetViewport(desc.Width, desc.Height);
        }

        // What every stage of every pass of the refraction is sampled with: the
        // picture is read at the coordinate of the vertex and never wrapped around,
        // and the field the light is in has no mip levels to filter. The coordinate
        // is the one the vertex carries, so a transform of the coordinates the game
        // may have left on a stage is taken off: a game that transforms its own
        // textures (Unreal does it for its menus and its lightmaps) would otherwise
        // skew every pass of the refraction, the field of the light with it, and
        // nothing of the effect would be where the picture is.
        void SetStageSampler(DWORD stage) const
        {
            pDevice->SetTextureStageState(stage, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
            pDevice->SetTextureStageState(stage, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
            pDevice->SetTextureStageState(stage, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
            pDevice->SetTextureStageState(stage, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
            pDevice->SetTextureStageState(stage, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
            pDevice->SetTextureStageState(stage, D3DTSS_MIPFILTER, D3DTEXF_NONE);
        }

        void SetViewport(UINT width, UINT height) const
        {
            D3DVIEWPORT8 viewport{};
            viewport.X = 0;
            viewport.Y = 0;
            viewport.Width = width;
            viewport.Height = height;
            viewport.MinZ = 0.0f;
            viewport.MaxZ = 1.0f;
            pDevice->SetViewport(&viewport);
        }

        // The frame the device copied into the surface of the system memory pool,
        // handed to the texture the drops sample. There is no way on the device to
        // do this (see EnsureResources for what a copy into a texture does), so the
        // rows are copied here, and one row at a time: the pitch of the two is not
        // the same.
        void UploadScene(const D3DSURFACE_DESC& desc)
        {
            D3DLOCKED_RECT source{};

            if (FAILED(pSceneImage->LockRect(&source, nullptr, D3DLOCK_READONLY)))
                return;

            D3DLOCKED_RECT target{};
            // The whole of it is written every frame, which is what the discard tells
            // the driver: without it the rows may land in a buffer of the driver's
            // that the device never draws from.
            const HRESULT lockTarget = pSceneTexture->LockRect(0, &target, nullptr, D3DLOCK_DISCARD);

            if (SUCCEEDED(lockTarget))
            {
                const size_t rowBytes = (size_t)desc.Width * 4;

                for (UINT y = 0; y < desc.Height; y++)
                    memcpy((unsigned char*)target.pBits + (size_t)y * target.Pitch,
                        (const unsigned char*)source.pBits + (size_t)y * source.Pitch, rowBytes);

                pSceneTexture->UnlockRect(0);
            }

            pSceneImage->UnlockRect();
        }

        // -------------------------------------------------------------------
        // the refraction: the frame behind the drops, the light it holds, and
        // the shader that puts both into a drop of water
        // -------------------------------------------------------------------

        // A shader of model 1 cannot be built at runtime the way the renderers of
        // Direct3D 9 and above build theirs, so the bytecode of one is a resource of
        // the module, built by the tools of tools/x86, see
        // source/resources/shaders/ps8. It is built once per device here, and a
        // device that cannot run one - or that has fewer than the four textures at
        // once the blur of the field needs - keeps the fixed function pipeline.
        void EnsureShaders()
        {
            if (shaderAttempted || !pDevice)
                return;

            shaderAttempted = true;

            D3DCAPS8 caps{};

            if (FAILED(pDevice->GetDeviceCaps(&caps)) || caps.MaxSimultaneousTextures < 4)
                return;

            // ps_1_4 is the highest a shader of model 1 goes and what every device
            // that can run a game today reports, and the hardware the games
            // themselves ran on has ps_1_1. The two sets of shaders draw the same
            // thing, only the fade in of the light of the field is left out of the
            // ps_1_1 one, which has an instruction less to spend (see lightPS8.hlsl).
            const bool bPs14 = caps.PixelShaderVersion >= D3DPS_VERSION(1, 4);
            const bool bPs11 = caps.PixelShaderVersion >= D3DPS_VERSION(1, 1);

            if (!bPs14 && !bPs11)
                return;

            pDropShader = CreateShader(bPs14 ? IDR_DROP8PS14 : IDR_DROP8PS11);
            pBlurShader = CreateShader(bPs14 ? IDR_BLUR8PS14 : IDR_BLUR8PS11);
            pLightShader = CreateShader(bPs14 ? IDR_LIGHT8PS14 : IDR_LIGHT8PS11);
            // The fade of the light out of the depth of the frame is a pass of its own
            // and not part of the light, and a game that hands out no depth of its own
            // to read does without it, so it is not part of bShaderDrops.
            pFadeShader = CreateShader(bPs14 ? IDR_FADE8PS14 : IDR_FADE8PS11);

            bShaderDrops = pDropShader != 0 && pBlurShader != 0 && pLightShader != 0;
        }

        // The module the shaders are a resource of, which is the module of the game
        // the plugin was loaded into.
        static HMODULE ShaderModule()
        {
            HMODULE hModule = nullptr;
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                (LPCWSTR)&D3D8Backend::ShaderModule, &hModule);
            return hModule;
        }

        DWORD CreateShader(int resourceId) const
        {
            HMODULE hModule = ShaderModule();
            HRSRC hResource = hModule ? FindResource(hModule, MAKEINTRESOURCE(resourceId), RT_RCDATA) : nullptr;

            if (!hResource)
                return 0;

            HGLOBAL hLoaded = LoadResource(hModule, hResource);

            if (!hLoaded)
                return 0;

            // The bytecode of a shader of model 1 is handed to the device as it is.
            const DWORD* pCode = (const DWORD*)LockResource(hLoaded);

            if (!pCode)
                return 0;

            DWORD shader = 0;
            return SUCCEEDED(pDevice->CreatePixelShader(pCode, &shader)) ? shader : 0;
        }

        // The field the light of the frame lives in, an eighth of the target in
        // each direction, and the five others that the passes of it chain through,
        // see RenderLightField.
        bool EnsureField(const D3DSURFACE_DESC& desc)
        {
            const UINT wantedWidth = desc.Width > 8 ? desc.Width / 8 : 1;
            const UINT wantedHeight = desc.Height > 8 ? desc.Height / 8 : 1;

            if (pFields[FIELD_FRAME] && fieldWidth == wantedWidth && fieldHeight == wantedHeight)
                return true;

            ReleaseField();

            for (int i = 0; i < FIELD_COUNT; i++)
            {
                if (FAILED(pDevice->CreateTexture(wantedWidth, wantedHeight, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &pFields[i])))
                {
                    ReleaseField();
                    return false;
                }

                if (FAILED(pFields[i]->GetSurfaceLevel(0, &pFieldSurfaces[i])))
                {
                    ReleaseField();
                    return false;
                }
            }

            fieldWidth = wantedWidth;
            fieldHeight = wantedHeight;
            return true;
        }

        void ReleaseField()
        {
            for (int i = 0; i < FIELD_COUNT; i++)
            {
                if (pFieldSurfaces[i]) { pFieldSurfaces[i]->Release(); pFieldSurfaces[i] = nullptr; }
                if (pFields[i]) { pFields[i]->Release(); pFields[i] = nullptr; }
            }

            fieldWidth = 0;
            fieldHeight = 0;
        }

        void FieldState(bool additive) const
        {
            pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
            pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
            pDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
            pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
            pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, additive ? TRUE : FALSE);
            pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
            pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
            pDevice->SetRenderState(D3DRS_COLORWRITEENABLE, 0x0000000F);
        }

        // Half a texel of the field, which is what every pass that reads a field of this
        // backend has to be handed: a vertex of a screen quad sits on the edge of a pixel
        // and the coordinate of the texture it carries is read at the middle of that
        // pixel, which is the corner between four texels of the field and not the middle
        // of the one the pass is for. Left out, a pass of four taps reads each of them as
        // an average of four texels and puts what it read in a texel a half of one away
        // from the one it was for, and a chain of a dozen passes of them leaves a light
        // that is nowhere near its lamp and no brighter than the frame around it.
        float HalfTexelX() const { return fieldWidth ? 0.5f / (float)fieldWidth : 0.0f; }
        float HalfTexelY() const { return fieldHeight ? 0.5f / (float)fieldHeight : 0.0f; }

        // The axis the four taps of a pass of the field are taken on. A pass of a
        // shader of model 1 holds four taps and no more, so a pass can be a blur
        // along one axis of the field but never a blur of a disc: two passes at one
        // radius, one on each axis of a pair, are what a box of sixteen taps is made
        // of, and two boxes of the two pairs put together are an octagon instead of
        // the square or the diamond either pair makes of it on its own, which is what
        // a light is spread with (see RenderLightField).
        enum FieldAxis
        {
            FIELD_TEXEL,        // the four taps on the texel itself: what the pass hands on
            FIELD_ROW,          // the four of them along a row of the field
            FIELD_COLUMN,       // the four of them along a column of it
            FIELD_DIAGONAL_A,   // and the four of them along a diagonal of it,
            FIELD_DIAGONAL_B,   // each of the two diagonals being an axis of its own
        };

        // One pass over the field: four taps of pSource, on every texel of it or
        // along one axis of it, each of them weighted and, if the pass is not the
        // first one of the field, added to what the pass before it left in the
        // target. That is the whole of what a blur of four taps of a shader of model
        // 1 is, and what the passes chain into a gather that reaches as wide as the
        // one of the renderers of Direct3D 9 and above out of.
        void FieldPass(IDirect3DTexture8* pSource, IDirect3DSurface8* pTargetSurface, float radius, FieldAxis axis, float weight, bool additive)
        {
            struct FieldVertex
            {
                float x, y, z, rhw;
                float u0, v0, u1, v1, u2, v2, u3, v3;
            };

            const float offsetX = radius / (float)fieldWidth;
            const float offsetY = radius / (float)fieldHeight;

            // A vertex of a screen quad sits on the edge of a pixel, and the coordinate
            // of the texture it carries is read at the middle of the texel that edge is
            // on and not at the middle of the pixel: a tap that is handed no offset at
            // all reads the corner between four texels of the field, and every pass of
            // the light of the frame comes out of that a half of a texel away from where
            // it should be, which a chain of passes turns into a light that is nowhere
            // near the lamp it came from, see HalfTexelX.
            const float halfTexelX = HalfTexelX();
            const float halfTexelY = HalfTexelY();

            // The four corners of the field, in the order of a triangle strip and not a
            // walk around the quad: a strip of a top left, a top right, a bottom right and
            // a bottom left is two triangles that are BOTH of them on the right of the
            // field, and the region they leave out is the one between the two diagonals of
            // it, which is half of the frame of drops not written at all - and a light of
            // its own along those two diagonals, because the frame around a light there is
            // nothing. That is the X across a frame of drops. A strip of a top left, a top
            // right, a bottom left and a bottom right is the two halves of the quad.
            const float xs[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
            const float ys[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
            FieldVertex quad[4]{};

            // A third of the radius apart, which is what fills the box of two passes
            // of a radius instead of leaving the sides of it in the target and the
            // corners of it empty, and no further apart than the texels of the field
            // are from each other, which is what the filtering of the field between
            // them makes an average of anyway.
            const float taps[4] = { -1.0f, -0.33333333f, 0.33333333f, 1.0f };

            // A tap of a diagonal is taken at the same distance from the texel as a tap
            // of a row or a column of the same radius, which is the radius over the
            // square root of two on each of the two axes at once.
            const float corner = 0.70710678f;

            float du[4] = {}, dv[4] = {};

            for (int i = 0; i < 4; i++)
            {
                const bool diagonal = axis == FIELD_DIAGONAL_A || axis == FIELD_DIAGONAL_B;
                const float step = taps[i] * (diagonal ? corner : 1.0f);

                du[i] = (axis == FIELD_ROW || diagonal) ? offsetX * step : 0.0f;
                dv[i] = (axis == FIELD_COLUMN || diagonal) ? offsetY * step *
                    (axis == FIELD_DIAGONAL_B ? -1.0f : 1.0f) : 0.0f;
            }

            for (int i = 0; i < 4; i++)
            {
                quad[i].x = xs[i] * (float)fieldWidth;
                quad[i].y = ys[i] * (float)fieldHeight;
                quad[i].rhw = 1.0f;
                quad[i].u0 = xs[i] + halfTexelX + du[0]; quad[i].v0 = ys[i] + halfTexelY + dv[0];
                quad[i].u1 = xs[i] + halfTexelX + du[1]; quad[i].v1 = ys[i] + halfTexelY + dv[1];
                quad[i].u2 = xs[i] + halfTexelX + du[2]; quad[i].v2 = ys[i] + halfTexelY + dv[2];
                quad[i].u3 = xs[i] + halfTexelX + du[3]; quad[i].v3 = ys[i] + halfTexelY + dv[3];
            }

            pDevice->SetRenderTarget(pTargetSurface, nullptr);
            SetViewport(fieldWidth, fieldHeight);
            FieldState(additive);

            for (DWORD stage = 0; stage < 4; stage++)
            {
                pDevice->SetTexture(stage, pSource);
                pDevice->SetTextureStageState(stage, D3DTSS_TEXCOORDINDEX, stage);
                SetStageSampler(stage);
            }

            pDevice->SetPixelShaderConstant(0, &weight, 1);
            pDevice->SetPixelShader(pBlurShader);
            pDevice->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_TEX4);
            pDevice->SetStreamSource(0, nullptr, 0);
            pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(FieldVertex));
        }

        // The light itself: what a small area of the frame holds and what the frame
        // around it holds, out of the two blurred copies of it (see lightPS8.hlsl).
        void LightPass(IDirect3DTexture8* pGathered, IDirect3DTexture8* pAround, IDirect3DSurface8* pTargetSurface)
        {
            struct FieldVertex
            {
                float x, y, z, rhw;
                float u0, v0, u1, v1;
            };

            // a triangle strip of a top left, a top right, a bottom left and a bottom
            // right, which is the two halves of the quad, see FieldPass
            const float xs[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
            const float ys[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
            FieldVertex quad[4]{};

            for (int i = 0; i < 4; i++)
            {
                quad[i].x = xs[i] * (float)fieldWidth;
                quad[i].y = ys[i] * (float)fieldHeight;
                quad[i].rhw = 1.0f;
                quad[i].u0 = xs[i] + HalfTexelX(); quad[i].v0 = ys[i] + HalfTexelY();
                quad[i].u1 = xs[i] + HalfTexelX(); quad[i].v1 = ys[i] + HalfTexelY();
            }

            pDevice->SetRenderTarget(pTargetSurface, nullptr);
            SetViewport(fieldWidth, fieldHeight);
            FieldState(false);

            pDevice->SetTexture(0, pGathered);
            pDevice->SetTexture(1, pAround);

            for (DWORD stage = 0; stage < 2; stage++)
            {
                pDevice->SetTextureStageState(stage, D3DTSS_TEXCOORDINDEX, stage);
                SetStageSampler(stage);
            }

            // what is taken off the light for being the frame around it and not the
            // light itself, the whole of it, because the floor is in the field, and
            // what of its own colour the light keeps
            const float params[4] = { 0.0f, 0.0f, -1.0f, 0.0f };
            const float luma[4] = { 0.2126f, 0.7152f, 0.0722f, 0.0f };
            const float chroma[4] = { 1.0f, LightChroma - 2.0f, 0.0f, -LightRampLow * 4.0f };
            const float peak[4] = { 0.5f, 0.5f, 0.5f, 0.0f };
            pDevice->SetPixelShaderConstant(0, params, 1);
            pDevice->SetPixelShaderConstant(1, luma, 1);
            pDevice->SetPixelShaderConstant(2, chroma, 1);
            pDevice->SetPixelShaderConstant(3, peak, 1);
            pDevice->SetPixelShader(pLightShader);
            pDevice->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_TEX2);
            pDevice->SetStreamSource(0, nullptr, 0);
            pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(FieldVertex));
        }

        // The light of the frame left to what is close to the camera, out of the depth
        // of the frame the game handed out, see fadePS8.hlsl. One pass, and the only
        // pass of this backend that reads a texture of the game rather than its own.
        void FadePass(IDirect3DTexture8* pLight, IDirect3DSurface8* pTargetSurface, IDirect3DTexture8* pDepth)
        {
            struct FieldVertex
            {
                float x, y, z, rhw;
                float u0, v0, u1, v1;
            };

            // a triangle strip of a top left, a top right, a bottom left and a bottom
            // right, which is the two halves of the quad, see FieldPass
            const float xs[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
            const float ys[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
            FieldVertex quad[4]{};

            for (int i = 0; i < 4; i++)
            {
                quad[i].x = xs[i] * (float)fieldWidth;
                quad[i].y = ys[i] * (float)fieldHeight;
                quad[i].rhw = 1.0f;
                quad[i].u0 = xs[i] + HalfTexelX(); quad[i].v0 = ys[i] + HalfTexelY();
                quad[i].u1 = xs[i] + HalfTexelX(); quad[i].v1 = ys[i] + HalfTexelY();
            }

            pDevice->SetRenderTarget(pTargetSurface, nullptr);
            SetViewport(fieldWidth, fieldHeight);
            FieldState(false);

            pDevice->SetTexture(0, pLight);
            pDevice->SetTexture(1, pDepth);

            for (DWORD stage = 0; stage < 2; stage++)
            {
                pDevice->SetTextureStageState(stage, D3DTSS_TEXCOORDINDEX, stage);
                SetStageSampler(stage);
            }

            const float params[4] = { LightDepthScale, LightDepthOffset, 0.0f, 0.0f };
            pDevice->SetPixelShaderConstant(0, params, 1);
            pDevice->SetPixelShader(pFadeShader);
            pDevice->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_TEX2);
            pDevice->SetStreamSource(0, nullptr, 0);
            pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(FieldVertex));
        }

        // The whole of the light of the frame, out of passes of four taps each. What
        // the renderers of Direct3D 9 and above gather out of in one pass of a gather
        // of thirteen by thirteen cells of four taps each, of which a light of the size
        // of a rear light of a car is one cell, is built out of what those passes chain
        // into here:
        //
        //   1-4.  the frame as a field, one texel of it to a block of eight pixels of
        //         the frame, as half the texel and half the box of a texel around it
        //   5-14. the frame around the drop: the field averaged over a wide square of
        //         sixty-four texels of it, which is 0.7 of the height of the frame,
        //         and is what the light of a drop is measured against
        //   15.   the light itself: the texel of the field the drop is in against the
        //         frame around it (see lightPS8.hlsl)
        //   16-22. and the light spread over the frame around it, because a drop reads
        //         one texel of the field and the gather of the other renderers is of
        //         a square of 0.14 of the height of the frame around every drop
        //
        // The passes of the two averages are two at a level, one on each axis of the
        // field, which is what makes the box of sixteen taps of them: the four taps of
        // a pass alone reach up and down and left and right of a texel and not the
        // corners between them, and a light spread out of a chain of those reaches a
        // diamond with its corners up and down and left and right of it, see FieldAxis.
        //
        // What is left in the field of the light when this is done is the light,
        // which is what the drops are drawn with.
        void RenderLightField(const D3DSURFACE_DESC& desc, IDirect3DTexture8* pDepthTexture)
        {
            if (!EnsureField(desc))
            {
                bShaderDrops = false;
                return;
            }

            IDirect3DTexture8* pFrame = pFields[FIELD_FRAME];
            IDirect3DTexture8* pScratch = pFields[FIELD_SCRATCH];
            IDirect3DTexture8* pAround = pFields[FIELD_AROUND];

            // the frame as a field, one texel of it to a block of eight pixels of the
            // frame, which is the size of a rear light of a car: a light of that size is
            // a texel or two of the field, and the texel is what carries it. Half of the
            // field is the texel itself and half of it the box of a texel around it,
            // because a light of the size of a texel has to stay a light of the field
            // and not be averaged into the frame around it, and because a light that the
            // texel itself falls between is what the box around it finds. The two of
            // them together leave the level of a frame that is the same all over where
            // it was, which is what the floor of the light is measured against.
            FieldPass(pSceneTexture, pFieldSurfaces[FIELD_FRAME], 0.0f, FIELD_TEXEL, 0.125f, false);        // half of it, at the texel
            FieldPass(pSceneTexture, pFieldSurfaces[FIELD_SCRATCH], 1.0f, FIELD_ROW, 0.25f, false);
            FieldPass(pScratch, pFieldSurfaces[FIELD_SCRATCH2], 1.0f, FIELD_COLUMN, 0.25f, false);
            FieldPass(pFields[FIELD_SCRATCH2], pFieldSurfaces[FIELD_FRAME], 0.0f, FIELD_TEXEL, 0.125f, true); // and half, around it
            // The frame the light of a drop is measured against: the frame itself
            // averaged over a wide square, six levels of two passes of four taps each,
            // the radii of them 2, 2, 4, 8, 16 and 32 texels of the field, which is
            // sixty-four texels in all, 0.7 of the height of the frame and a light the
            // drop is nowhere near. The light of a drop is what stands out of the frame
            // around it, and the frame around it is the level of the whole of it and not
            // the level of the area of it the drop is in: a frame that is bright in one
            // corner of it and dark in the other is what a skyline is, and a drop in the
            // dark corner of one is not lit by the lamps of the bright one.
            //
            // Every level of it is two passes, one on each axis of a pair of them, at a
            // weight of a quarter each, and each of them is an average of four taps of
            // what the pass before it left, so the level of a frame that is the same all
            // over is what comes out of every one of them and the difference of it and
            // of the area of the frame around the drop is zero over a frame that is the
            // same all over, which is what keeps a drop in a bright frame the colour of
            // water. A level of a row and a column is a square of the frame and a level
            // of the two diagonals is that same square turned by an eighth of a turn, so
            // the two of them alternate and what the six levels reach is an octagon and
            // not a square.
            //
            // Two passes at one radius and not the four taps of a pass put around the
            // texel: four taps of that kind reach up and down and left and right of the
            // texel and not the corners between them, so a chain of them reaches a
            // diamond and not a square, and what a light of the frame spreads out of a
            // diamond into is a light with its corners up and down and left and right of
            // it and nothing in between, which is what a drop of the rain looks like
            // when it is coloured by an X. Two passes, one on each axis, make the box of
            // sixteen taps of them instead (see FieldPass and FieldAxis).
            //
            // The last pass of it is LightFloor of the frame rather than the whole of
            // it, which is where the floor of the light is, see LightFloor.
            FieldPass(pFrame, pFieldSurfaces[FIELD_SCRATCH], 2.0f, FIELD_ROW, 0.25f, false);
            FieldPass(pScratch, pFieldSurfaces[FIELD_SCRATCH2], 2.0f, FIELD_COLUMN, 0.25f, false);
            FieldPass(pFields[FIELD_SCRATCH2], pFieldSurfaces[FIELD_SCRATCH], 2.0f, FIELD_DIAGONAL_A, 0.25f, false);
            FieldPass(pScratch, pFieldSurfaces[FIELD_SCRATCH2], 2.0f, FIELD_DIAGONAL_B, 0.25f, false);
            FieldPass(pFields[FIELD_SCRATCH2], pFieldSurfaces[FIELD_SCRATCH], 4.0f, FIELD_ROW, 0.25f, false);
            FieldPass(pScratch, pFieldSurfaces[FIELD_SCRATCH2], 4.0f, FIELD_COLUMN, 0.25f, false);
            FieldPass(pFields[FIELD_SCRATCH2], pFieldSurfaces[FIELD_SCRATCH], 8.0f, FIELD_DIAGONAL_A, 0.25f, false);
            FieldPass(pScratch, pFieldSurfaces[FIELD_SCRATCH2], 8.0f, FIELD_DIAGONAL_B, 0.25f, false);
            FieldPass(pFields[FIELD_SCRATCH2], pFieldSurfaces[FIELD_SCRATCH], 16.0f, FIELD_ROW, 0.25f, false);
            FieldPass(pScratch, pFieldSurfaces[FIELD_SCRATCH2], 16.0f, FIELD_COLUMN, 0.25f, false);
            FieldPass(pFields[FIELD_SCRATCH2], pFieldSurfaces[FIELD_SCRATCH], 32.0f, FIELD_DIAGONAL_A, 0.25f, false);
            FieldPass(pScratch, pFieldSurfaces[FIELD_AROUND], 32.0f, FIELD_DIAGONAL_B, 0.25f * LightFloor, false);

            // The light itself: the texel of the field the drop is in against the frame
            // around it, which is what stands out of it and what the drop is coloured
            // by (see lightPS8.hlsl). The texel and not a blur of a square of them: a
            // light of the size of a rear light of a car is a texel or two of the field,
            // and anything wider than that averages the light away into the frame around
            // it. Both sides of the comparison are the frame itself and not the square
            // of it, because one of the two is taken off the other and a square of the
            // frame grows twice as fast as the frame does: an area of the frame a third
            // of the way to white would stand out of the frame around it only for being
            // squared, so every building of a skyline would be a light of its own.
            //
            // The floor is what decides how far out of the frame around a drop a light
            // has to stand, see LightFloor.
            LightPass(pFrame, pAround, pFieldSurfaces[FIELD_LIGHT]);

            // What the light of a drop is, spread over the frame around the light the
            // way the gather of the other renderers reads it: the gather of a drop is
            // not of the place the drop is but of a square of 0.14 of the height of the
            // frame around it, so a rear light of a car lights the drops within a car
            // of it and not only the one that is on top of it. Every drop reads one
            // texel of this field and no more, so the spread has to be in the field.
            //
            // The light itself is kept where it is and two levels of the box of it are
            // added to it, one of a texel of the field and one of three, which is a light
            // that keeps the whole of what it is where it is and reaches thirty-four
            // pixels of the frame around it. A box and not the four taps of a pass put
            // around the texel: four taps of that kind reach up and down and left and
            // right of the texel and not the corners between them, so a chain of them
            // spreads a light into a diamond with the corners of a box out of it, which
            // is what a drop of the rain looks like when it is coloured by an X.
            //
            // A level is two boxes and not one, a square of a row and a column of the
            // field and the diamond of its two diagonals at the same radius, because a
            // square reaches half as far again at its corners as at its sides and a
            // diamond reaches half as far again at its sides as at its corners: the two
            // of them together reach as far in the four directions of the field as in
            // the four between them, and what a light spreads out into is round.
            //
            // A box of a light and not a blur of it put in the place of the light: the
            // light of a rear light of a car is a texel or two of the field and a blur
            // of it of the width of the box leaves a sixteenth of what it was, which is
            // what has to be made up for by the gain of the light pass and by nothing
            // else. What is added here is a quarter of the box of it on top of the whole
            // of what it is, so a light is as bright where it is as it was and reaches
            // the drops around it.
            // Fade at the source before spreading. Testing the receiving pixel
            // lets a distant lamp colour drops over nearby geometry, and suppresses
            // nearby lamps when their glow reaches a distant background.
            // FIELD_AROUND is no longer needed after LightPass, so reuse it without
            // adding a texture or pass. The existing depth convention is preserved.
            int lightField = FIELD_LIGHT;
            if (pDepthTexture && pFadeShader)
            {
                FadePass(pFields[FIELD_LIGHT], pFieldSurfaces[FIELD_AROUND], pDepthTexture);
                lightField = FIELD_AROUND;
            }
            SpreadLight(lightField);
            pLightField = pFields[lightField];

        }

        // What the light of the frame is spread out into, which is what the drops around a
        // light are coloured by: the light itself, where it is, and six levels of the box
        // of it added on top of it, every level reaching further than the one before it and
        // every level being a blur of the light as it is by then. See RenderLightField for
        // what a box of it is and why a level is a square or a diamond at one radius.
        //
        // The radii of the levels are close together and there are six of them, which is
        // what makes of the six of them a falloff and not a handful of rings: a box of a
        // light of one texel is SIXTEEN points of a sixteenth of it around that texel and
        // not a disc, so a level on its own puts the light of a lamp in a ring of dots
        // and it is the levels after it that fill the gaps in it. The field is read by the
        // drops as a picture of the light around them, and a level that leaves one texel
        // of the field in sixteen lit is a drop of the rain lit at random, which is what
        // colouring a frame of drops looks like when it is a confetti of colour and not a
        // light.
        //
        // A level reads the light WITH the levels before it already in it and not the
        // light on its own: a level that read the light on its own would put a sixteenth
        // of it thirty pixels away and nothing in between, and a level that reads the
        // light the level before it left spends its reach on what that level spread out,
        // which is what fills the gaps of it.
        void SpreadLight(int lightField)
        {
            IDirect3DTexture8* pLight = pFields[lightField];
            IDirect3DTexture8* pScratch = pFields[FIELD_SCRATCH];
            IDirect3DTexture8* pScratch2 = pFields[FIELD_SCRATCH2];

            // a texel of the field is eight pixels of the target, so the six levels of it
            // reach four, twelve, twenty-eight, sixty and a hundred and twenty pixels of
            // the frame around a light, which is where the gather of the other renderers
            // reaches of it, see GatherTap in source/xrd/xrdshaders.h
            static constexpr float radii[6] = { 0.5f, 1.0f, 2.0f, 3.0f, 5.0f, 8.0f };
            static constexpr float weights[6] = { 0.125f, 0.125f, 0.125f, 0.125f, 0.125f, 0.125f };

            for (int level = 0; level < 6; level++)
            {
                // A level is the square of a row and a column of the field or the diamond
                // of its two diagonals, one after the other: a square reaches half as far
                // again at its corners as at its sides and a diamond reaches half as far
                // again at its sides as at its corners, so the two of them one after the
                // other reach as far in the four directions of the field as in the four
                // between them and what a light spreads out into is round.
                const bool square = (level % 2) == 0;
                const float radius = radii[level];

                FieldPass(pLight, pFieldSurfaces[FIELD_SCRATCH],
                    radius, square ? FIELD_ROW : FIELD_DIAGONAL_A, 0.25f, false);
                FieldPass(pScratch, pFieldSurfaces[FIELD_SCRATCH2],
                    radius, square ? FIELD_COLUMN : FIELD_DIAGONAL_B, 0.25f, false);
                FieldPass(pFields[FIELD_SCRATCH2], pFieldSurfaces[lightField],
                    0.0f, FIELD_TEXEL, weights[level], true);
            }
        }

    private:
        void ReleaseD3D9Mask()
        {
            if (pBackend9)
                pBackend9->SetMaskTexture(nullptr);
            if (mask9.resource)
                ((IUnknown*)mask9.resource)->Release();
            mask9 = {};
        }

        void ReleaseD3D9()
        {
            ReleaseD3D9Mask();
            delete pBackend9;
            pBackend9 = nullptr;
            if (pDevice9)
                pDevice9->Release();
            pDevice9 = nullptr;
            usingD3D9 = false;
        }

        bool RenderD3D9(const Vertex* vertices, int count, PrimitiveType primitive)
        {
            if (!pBackend9 || (pMaskTexture && pMaskTexture->resource && !mask9.resource))
                return false;

            // Keep D3D8 handles at the public boundary. A wrapper may expose the
            // device but not its resources; such a batch uses the original path.
            RenderTarget target9{};
            if (pTargetOverride)
            {
                target9 = *pTargetOverride;
                target9.resource = nullptr;
                if (pTargetOverride->resource &&
                    (FAILED(((IDirect3DSurface8*)pTargetOverride->resource)->QueryInterface(
                        IID_IDirect3DSurface9, &target9.resource)) || !target9.resource))
                    return false;
            }

            pBackend9->SetTarget(pTargetOverride ? &target9 : nullptr);
            pBackend9->SetMaskTexture(mask9.resource ? &mask9 : nullptr);
            pBackend9->SetProjection(projection, &worldMatrix, width, height);
            pBackend9->SetSceneUVScale(uvOffsetX, uvScaleX, uvOffsetY, uvScaleY);
            pBackend9->SetSceneComplement(sceneComplement);
            pBackend9->SetSceneSampling(sceneSampling);
            const bool ready = vertices ? true : pBackend9->Prepare(count);
            if (vertices)
                pBackend9->Render(vertices, count, primitive);
            pBackend9->SetTarget(nullptr);
            if (target9.resource)
                ((IUnknown*)target9.resource)->Release();
            return ready;
        }

        IDirect3DDevice8* pDevice = nullptr;
        // QueryInterface owns this reference for the lifetime of the delegate.
        IUnknown* pDevice9 = nullptr;
        Backend* pBackend9 = nullptr;
        Texture mask9{};
        bool usingD3D9 = false;

        // The frame the copy of it is taken into, a surface of the system memory
        // pool, and the texture the drops sample, which the same frame is handed
        // to, see UploadScene.
        IDirect3DSurface8* pSceneImage = nullptr;
        IDirect3DTexture8* pSceneTexture = nullptr;
        D3DFORMAT sceneFormat = D3DFMT_UNKNOWN;
        UINT sceneWidth = 0;
        UINT sceneHeight = 0;

        IDirect3DVertexBuffer8* pVertexBuffer = nullptr;
        IDirect3DIndexBuffer8* pIndexBuffer = nullptr;
        UINT vertexStride = ScreenVertexStride;

        // The shaders of the refraction, see EnsureShaders.
        DWORD pDropShader = 0;
        DWORD pBlurShader = 0;
        DWORD pLightShader = 0;
        // What of the light is close enough to the camera to fall on a drop of the
        // rain on the glass in front of it, see fadePS8.hlsl and FadePass.
        DWORD pFadeShader = 0;
        bool shaderAttempted = false;
        bool bShaderDrops = false;

        // The field of the light the drops sample: the light of the frame as it was
        // found, or what is left of it once the depth of the frame is taken into
        // account, see RenderLightField.
        IDirect3DTexture8* pLightField = nullptr;

        // The field of the light of the frame, an eighth of the target in each
        // direction, and what the passes of it chain through, see RenderLightField.
        enum Field
        {
            FIELD_FRAME = 0,   // the frame itself, as a field
            FIELD_AROUND,      // the frame the light of a drop is measured against
            FIELD_SCRATCH,     // what the passes chain through
            FIELD_SCRATCH2,    //
            FIELD_LIGHT,       // the light itself, what the drops sample
            FIELD_COUNT
        };

        IDirect3DTexture8* pFields[FIELD_COUNT] = {};
        IDirect3DSurface8* pFieldSurfaces[FIELD_COUNT] = {};
        UINT fieldWidth = 0;
        UINT fieldHeight = 0;

        Texture* pMaskTexture = nullptr;
        RenderTarget* pTargetOverride = nullptr;

        Projection projection = PROJECTION_SCREEN;
        Matrix worldMatrix = Matrix::Identity();
        float width = 0.0f;
        float height = 0.0f;
        float uvOffsetX = 0.0f, uvScaleX = 1.0f;
        float uvOffsetY = 0.0f, uvScaleY = 1.0f;
        bool sceneComplement = false;
        bool sceneSampling = true;

        Size targetSize{};
    };

    namespace D3D8Factory
    {
        // The backend registers itself, exactly like the others, so including
        // this header is all it takes to have RENDERER_D3D8 available.
        inline Detail::Register registrar(RENDERER_D3D8, []() -> Backend* { return new D3D8Backend(); });
    }
}




