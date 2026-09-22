#pragma once
// ---------------------------------------------------------------------------
// Direct3D 9 backend.
//
// It follows the original implementation: the drops are drawn with the fixed
// function pipeline and two texture stages, first the atlas of drop shapes
// modulated with the vertex colour, then the copy of the frame modulated on
// top of it. The frame is copied with StretchRect into a texture the stages
// then sample, which is what makes the drops refract the picture behind them
// instead of just tinting it.
//
// Everything the draw touches is saved and put back by hand. A state block
// cannot be trusted for this: when one fails to be created nothing is restored
// and the next thing the game draws, usually the UI, comes out wrong.
// ---------------------------------------------------------------------------

#include "xrdrender.h"
#include "xrdd3dcompile.h"
#include "xrdshaders.h"

#include <d3d9.h>

namespace Xrd
{
    namespace D3D9Lists
    {
        struct StageState
        {
            DWORD stage;
            D3DTEXTURESTAGESTATETYPE state;
        };

        struct SamplerState
        {
            DWORD stage;
            D3DSAMPLERSTATETYPE state;
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

        inline const SamplerState samplerStates[] =
        {
            { 0, D3DSAMP_ADDRESSU }, { 0, D3DSAMP_ADDRESSV },
            { 0, D3DSAMP_MINFILTER }, { 0, D3DSAMP_MAGFILTER }, { 0, D3DSAMP_MIPFILTER },
            { 1, D3DSAMP_ADDRESSU }, { 1, D3DSAMP_ADDRESSV },
            { 1, D3DSAMP_MINFILTER }, { 1, D3DSAMP_MAGFILTER }, { 1, D3DSAMP_MIPFILTER },
            { 2, D3DSAMP_ADDRESSU }, { 2, D3DSAMP_ADDRESSV },
            { 2, D3DSAMP_MINFILTER }, { 2, D3DSAMP_MAGFILTER }, { 2, D3DSAMP_MIPFILTER },
        };

        inline const D3DRENDERSTATETYPE renderStates[] =
        {
            D3DRS_CULLMODE, D3DRS_LIGHTING, D3DRS_ZENABLE, D3DRS_ZWRITEENABLE,
            D3DRS_ALPHABLENDENABLE, D3DRS_ALPHATESTENABLE, D3DRS_BLENDOP,
            D3DRS_SRCBLEND, D3DRS_DESTBLEND, D3DRS_SCISSORTESTENABLE,
            D3DRS_COLORWRITEENABLE, D3DRS_SRGBWRITEENABLE,
        };

        constexpr int NumStageStates = sizeof(stageStates) / sizeof(stageStates[0]);
        constexpr int NumSamplerStates = sizeof(samplerStates) / sizeof(samplerStates[0]);
        constexpr int NumRenderStates = sizeof(renderStates) / sizeof(renderStates[0]);
    }

    namespace D3D9Light
    {
        // The light field the drops read: an eighth of the target in each
        // direction, and the number of cells the reach of a texel of it is
        // divided into, which has to be the LightCellCount of D3D9LightSource.
        constexpr UINT Divisor = 8;
        constexpr float CellCount = 6.0f;
        constexpr float Radius = 0.14f;
    }

    class D3D9Backend : public Backend
    {
    public:
        ~D3D9Backend() override
        {
            Shutdown();
        }

        bool Init(void* pNative) override
        {
            pDevice = (IDirect3DDevice9*)pNative;
            return pDevice != nullptr;
        }

        void Shutdown() override
        {
            ReleaseResources();
            pDevice = nullptr;
        }

        void Reset() override
        {
            ReleaseResources();
        }

        bool IsActive() const override
        {
            return pDevice != nullptr;
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
                IDirect3DSurface9* pTarget = nullptr;
                if (SUCCEEDED(pDevice->GetRenderTarget(0, &pTarget)) && pTarget)
                {
                    D3DSURFACE_DESC desc{};
                    if (SUCCEEDED(pTarget->GetDesc(&desc)))
                        size = { (int32_t)desc.Width, (int32_t)desc.Height };

                    pTarget->Release();
                }
            }

            const_cast<D3D9Backend*>(this)->targetSize = size;
            return size;
        }

        void SetMaskTexture(Texture* pMask) override
        {
            pMaskTexture = pMask;
        }

        Texture* CreateTexture(int textureWidth, int textureHeight, const uint8_t* pixels) override
        {
            if (!pDevice || textureWidth <= 0 || textureHeight <= 0)
                return nullptr;

            IDirect3DTexture9* pTexture = nullptr;
            if (FAILED(pDevice->CreateTexture(textureWidth, textureHeight, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &pTexture, nullptr)))
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

            if (pTexture->resource)
                ((IDirect3DTexture9*)pTexture->resource)->Release();

            delete pTexture;
        }

        void SetTarget(RenderTarget* pTarget) override
        {
            pTargetOverride = pTarget;
        }

        void SetTargetState(TargetState targetState) override
        {
            // Direct3D 9 has no resource states
            (void)targetState;
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

            const int numIndices = (primitive == PRIMITIVE_TRIANGLES) ? (numVertices / 4) * 6 : 0;

            if (primitive == PRIMITIVE_TRIANGLES && numIndices <= 0)
                return;

            IDirect3DSurface9* pTarget = nullptr;
            bool bOwnTarget = false;

            if (pTargetOverride && pTargetOverride->resource)
            {
                pTarget = (IDirect3DSurface9*)pTargetOverride->resource;
            }
            else if (SUCCEEDED(pDevice->GetRenderTarget(0, &pTarget)) && pTarget)
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

            // a screen vertex is one float larger than a world vertex, the
            // buffer is sized for the larger of the two and the stride follows
            vertexStride = (projection == PROJECTION_SCREEN) ? sizeof(float) * 9 : sizeof(Vertex);

            targetSize = { (int32_t)desc.Width, (int32_t)desc.Height };

            // The picture the drops are drawn on top of has to be taken before
            // anything is changed. A multisampled target is resolved by the same
            // call, which is what a resolve is.
            const bool bMultisampled = desc.MultiSampleType != D3DMULTISAMPLE_NONE;
            pDevice->StretchRect(pTarget, nullptr, pSceneSurface, nullptr, bMultisampled ? D3DTEXF_NONE : D3DTEXF_LINEAR);

            SavedState state{};
            CaptureState(state);

            // The light of the frame is gathered into the field once, before any
            // drop is drawn, and the drops read the field with one tap each: see
            // D3D9LightSource. It is a pass of its own, so it puts the target and
            // the viewport back the way it found them.
            const bool bLightField = projection == PROJECTION_SCREEN && sceneSampling && !sceneComplement &&
                pLightShader && pDropShader && pLightSurface;

            if (bLightField)
                RenderLightField(state.pRenderTarget);

            ApplyState(desc);

            void* pVertexData = nullptr;
            if (SUCCEEDED(pVertexBuffer->Lock(0, numVertices * vertexStride, (void**)&pVertexData, D3DLOCK_DISCARD)))
            {
                if (projection == PROJECTION_SCREEN)
                {
                    // Screen vertices are pre transformed, which is what the
                    // original code did: no matrix, straight to pixels. They
                    // carry one float more than a world vertex, the reciprocal
                    // of w, so the stride is different as well.
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
                        pDst[i].u0 = pVertices[i].u0;
                        // The marker of a drop that gathers light is a flag for the
                        // shader of the drops, which takes it off again: only a
                        // frame that is drawn without that shader has to have it
                        // taken off here, or the shape of the drop is looked up in
                        // the wrong tile of the atlas.
                        if (!bLightField && pDst[i].u0 < 0.0f) pDst[i].u0 += AtlasLightMarker;
                        pDst[i].v0 = pVertices[i].v0;
                        pDst[i].u1 = pVertices[i].u1 * uvScaleX + uvOffsetX;
                        pDst[i].v1 = pVertices[i].v1 * uvScaleY + uvOffsetY;
                    }

                    pDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX2);
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

                    pDevice->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX2);
                }

                pVertexBuffer->Unlock();
            }

            pDevice->SetStreamSource(0, pVertexBuffer, 0, vertexStride);
            if (bLightField)
            {
                const float constants[4] = { 1.0f / desc.Width, 1.0f / desc.Height,
                    sceneSampling ? 1.0f : 0.0f, sceneComplement ? 1.0f : 0.0f };
                pDevice->SetPixelShaderConstantF(0, constants, 1);
                pDevice->SetTexture(2, pLightTexture);
                pDevice->SetSamplerState(2, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
                pDevice->SetSamplerState(2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                pDevice->SetSamplerState(2, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
                pDevice->SetSamplerState(2, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
                pDevice->SetSamplerState(2, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
                pDevice->SetPixelShader(pDropShader);
            }

            if (primitive == PRIMITIVE_TRIANGLES)
            {
                pDevice->SetIndices(pIndexBuffer);
                pDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, numVertices, 0, numIndices / 3);
            }
            else
            {
                pDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, numVertices - 2);
            }

            RestoreState(state);

            if (bOwnTarget)
                pTarget->Release();
        }

    private:
        struct SavedState
        {
            IDirect3DSurface9* pRenderTarget = nullptr;
            IDirect3DSurface9* pDepthStencil = nullptr;
            D3DVIEWPORT9 viewport{};
            DWORD fvf = 0;
            IDirect3DVertexDeclaration9* pDeclaration = nullptr;
            float pixelConstant[4]{};
            IDirect3DVertexShader9* pVertexShader = nullptr;
            IDirect3DPixelShader9* pPixelShader = nullptr;
            IDirect3DVertexBuffer9* pVertexBuffer = nullptr;
            UINT vertexOffset = 0;
            UINT vertexStride = 0;
            IDirect3DIndexBuffer9* pIndexBuffer = nullptr;
            IDirect3DBaseTexture9* pTextures[3] = {};
            bool bTransformsSaved = false;
            D3DMATRIX transformWorld{};
            D3DMATRIX transformView{};
            D3DMATRIX transformProjection{};
            DWORD stageValues[D3D9Lists::NumStageStates] = {};
            DWORD samplerValues[D3D9Lists::NumSamplerStates] = {};
            DWORD renderValues[D3D9Lists::NumRenderStates] = {};
        };

        void ReleaseResources()
        {
            if (pLightShader) { pLightShader->Release(); pLightShader = nullptr; }
            if (pDropShader) { pDropShader->Release(); pDropShader = nullptr; }
            shaderAttempted = false;
            if (pLightSurface)
            {
                pLightSurface->Release();
                pLightSurface = nullptr;
            }

            if (pLightTexture)
            {
                pLightTexture->Release();
                pLightTexture = nullptr;
            }

            lightWidth = 0;
            lightHeight = 0;
            if (pSceneSurface)
            {
                pSceneSurface->Release();
                pSceneSurface = nullptr;
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
            if (!shaderAttempted)
            {
                shaderAttempted = true;
                D3DCAPS9 caps{};
                if (SUCCEEDED(pDevice->GetDeviceCaps(&caps)) && caps.PixelShaderVersion >= D3DPS_VERSION(3, 0))
                {
                    // VPOS and the loops of the gather are what this needs, and
                    // those are shader model 3: a device that has only 2 can not
                    // find the light of a drop at all, and draws it as before.
                    if (auto* blob = CompileShader(Shaders::D3D9Source, "PSMain", "ps_3_0"))
                    {
                        pDevice->CreatePixelShader((const DWORD*)blob->GetBufferPointer(), &pDropShader);
                        blob->Release();
                    }

                    if (auto* blob = CompileShader(Shaders::D3D9LightSource, "PSMain", "ps_3_0"))
                    {
                        pDevice->CreatePixelShader((const DWORD*)blob->GetBufferPointer(), &pLightShader);
                        blob->Release();
                    }
                }
            }
            static constexpr int MaxVertices = 64000;
            static constexpr int MaxIndices = MaxVertices * 6;

            if (numVertices > MaxVertices)
                return false;

            if (!pVertexBuffer)
            {
                if (FAILED(pDevice->CreateVertexBuffer(MaxVertices * ScreenVertexStride, D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC, 0, D3DPOOL_DEFAULT, &pVertexBuffer, nullptr)))
                    return false;

                if (FAILED(pDevice->CreateIndexBuffer(MaxIndices * sizeof(uint16_t), D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &pIndexBuffer, nullptr)))
                {
                    pVertexBuffer->Release();
                    pVertexBuffer = nullptr;
                    return false;
                }

                void* pData = nullptr;
                if (SUCCEEDED(pIndexBuffer->Lock(0, 0, (void**)&pData, 0)))
                {
                    uint16_t* pIndicesData = (uint16_t*)pData;
                    for (int i = 0; i < MaxVertices; i++)
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

            // The light field the drops read, an eighth of the target in each
            // direction. A device that will not make one of these is a device whose
            // drops are drawn without a light of their own, which is what every
            // backend does that can not gather one: the effect is still there.
            const UINT wantedWidth = desc.Width / D3D9Light::Divisor;
            const UINT wantedHeight = desc.Height / D3D9Light::Divisor;

            if (pLightTexture && (lightWidth != wantedWidth || lightHeight != wantedHeight))
            {
                if (pLightSurface)
                {
                    pLightSurface->Release();
                    pLightSurface = nullptr;
                }

                pLightTexture->Release();
                pLightTexture = nullptr;
            }

            if (!pLightTexture && wantedWidth > 0 && wantedHeight > 0)
            {
                if (SUCCEEDED(pDevice->CreateTexture(wantedWidth, wantedHeight, 1, D3DUSAGE_RENDERTARGET,
                    D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &pLightTexture, nullptr)))
                {
                    if (FAILED(pLightTexture->GetSurfaceLevel(0, &pLightSurface)))
                    {
                        pLightTexture->Release();
                        pLightTexture = nullptr;
                    }
                    else
                    {
                        lightWidth = wantedWidth;
                        lightHeight = wantedHeight;
                    }
                }
            }

            if (pSceneTexture && sceneFormat == desc.Format && sceneWidth == desc.Width && sceneHeight == desc.Height)
                return true;

            if (pSceneSurface)
            {
                pSceneSurface->Release();
                pSceneSurface = nullptr;
            }

            if (pSceneTexture)
            {
                pSceneTexture->Release();
                pSceneTexture = nullptr;
            }

            if (FAILED(pDevice->CreateTexture(desc.Width, desc.Height, 1, D3DUSAGE_RENDERTARGET, desc.Format, D3DPOOL_DEFAULT, &pSceneTexture, nullptr)))
                return false;

            if (FAILED(pSceneTexture->GetSurfaceLevel(0, &pSceneSurface)))
            {
                pSceneTexture->Release();
                pSceneTexture = nullptr;
                return false;
            }

            sceneFormat = desc.Format;
            sceneWidth = desc.Width;
            sceneHeight = desc.Height;
            return true;
        }

        void CaptureState(SavedState& state)
        {
            pDevice->GetRenderTarget(0, &state.pRenderTarget);
            pDevice->GetDepthStencilSurface(&state.pDepthStencil);
            pDevice->GetViewport(&state.viewport);
            pDevice->GetFVF(&state.fvf);
            pDevice->GetVertexDeclaration(&state.pDeclaration);
            pDevice->GetPixelShaderConstantF(0, state.pixelConstant, 1);
            pDevice->GetVertexShader(&state.pVertexShader);
            pDevice->GetPixelShader(&state.pPixelShader);
            pDevice->GetStreamSource(0, &state.pVertexBuffer, &state.vertexOffset, &state.vertexStride);
            pDevice->GetIndices(&state.pIndexBuffer);
            pDevice->GetTexture(0, &state.pTextures[0]);
            pDevice->GetTexture(1, &state.pTextures[1]);
            pDevice->GetTexture(2, &state.pTextures[2]);

            for (int i = 0; i < D3D9Lists::NumStageStates; i++)
                pDevice->GetTextureStageState(D3D9Lists::stageStates[i].stage, D3D9Lists::stageStates[i].state, &state.stageValues[i]);

            for (int i = 0; i < D3D9Lists::NumSamplerStates; i++)
                pDevice->GetSamplerState(D3D9Lists::samplerStates[i].stage, D3D9Lists::samplerStates[i].state, &state.samplerValues[i]);

            for (int i = 0; i < D3D9Lists::NumRenderStates; i++)
                pDevice->GetRenderState(D3D9Lists::renderStates[i], &state.renderValues[i]);

            if (projection == PROJECTION_WORLD)
            {
                state.bTransformsSaved = true;
                pDevice->GetTransform(D3DTS_WORLD, &state.transformWorld);
                pDevice->GetTransform(D3DTS_VIEW, &state.transformView);
                pDevice->GetTransform(D3DTS_PROJECTION, &state.transformProjection);
            }
        }

        void ApplyState(const D3DSURFACE_DESC& desc)
        {
            if (projection == PROJECTION_WORLD)
            {
                // The matrix of the caller already combines view and projection,
                // the other two are set to what does nothing.
                D3DMATRIX identity{};
                identity._11 = identity._22 = identity._33 = identity._44 = 1.0f;

                pDevice->SetTransform(D3DTS_WORLD, &identity);
                pDevice->SetTransform(D3DTS_VIEW, &identity);
                pDevice->SetTransform(D3DTS_PROJECTION, (const D3DMATRIX*)&worldMatrix);
            }

            pDevice->SetVertexShader(nullptr);
            pDevice->SetPixelShader(nullptr);

            pDevice->SetTexture(0, (pMaskTexture && pMaskTexture->resource) ? (IDirect3DTexture9*)pMaskTexture->resource : nullptr);
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

            for (int stage = 0; stage < 2; stage++)
            {
                pDevice->SetSamplerState(stage, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
                pDevice->SetSamplerState(stage, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
                pDevice->SetSamplerState(stage, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
                pDevice->SetSamplerState(stage, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                pDevice->SetSamplerState(stage, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
            }

            pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
            pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
            pDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
            pDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
            pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
            pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
            pDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
            pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
            pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
            pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
            pDevice->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
            pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);

            D3DVIEWPORT9 viewport{};
            viewport.X = 0;
            viewport.Y = 0;
            viewport.Width = desc.Width;
            viewport.Height = desc.Height;
            viewport.MinZ = 0.0f;
            viewport.MaxZ = 1.0f;
            pDevice->SetViewport(&viewport);
        }

        void RestoreState(SavedState& state)
        {
            pDevice->SetStreamSource(0, state.pVertexBuffer, state.vertexOffset, state.vertexStride);
            pDevice->SetIndices(state.pIndexBuffer);
            pDevice->SetFVF(state.fvf);
            pDevice->SetVertexDeclaration(state.pDeclaration);
            pDevice->SetPixelShaderConstantF(0, state.pixelConstant, 1);
            pDevice->SetVertexShader(state.pVertexShader);
            pDevice->SetPixelShader(state.pPixelShader);

            pDevice->SetTexture(0, state.pTextures[0]);
            pDevice->SetTexture(1, state.pTextures[1]);
            pDevice->SetTexture(2, state.pTextures[2]);

            for (int i = 0; i < D3D9Lists::NumStageStates; i++)
                pDevice->SetTextureStageState(D3D9Lists::stageStates[i].stage, D3D9Lists::stageStates[i].state, state.stageValues[i]);

            for (int i = 0; i < D3D9Lists::NumSamplerStates; i++)
                pDevice->SetSamplerState(D3D9Lists::samplerStates[i].stage, D3D9Lists::samplerStates[i].state, state.samplerValues[i]);

            for (int i = 0; i < D3D9Lists::NumRenderStates; i++)
                pDevice->SetRenderState(D3D9Lists::renderStates[i], state.renderValues[i]);

            pDevice->SetRenderTarget(0, state.pRenderTarget);
            pDevice->SetDepthStencilSurface(state.pDepthStencil);
            pDevice->SetViewport(&state.viewport);

            if (state.bTransformsSaved)
            {
                pDevice->SetTransform(D3DTS_WORLD, &state.transformWorld);
                pDevice->SetTransform(D3DTS_VIEW, &state.transformView);
                pDevice->SetTransform(D3DTS_PROJECTION, &state.transformProjection);
            }

            if (state.pRenderTarget)
                state.pRenderTarget->Release();

            if (state.pDepthStencil)
                state.pDepthStencil->Release();

            if (state.pVertexShader)
                state.pVertexShader->Release();

            if (state.pPixelShader)
                state.pPixelShader->Release();
            if (state.pDeclaration) state.pDeclaration->Release();

            if (state.pVertexBuffer)
                state.pVertexBuffer->Release();

            if (state.pIndexBuffer)
                state.pIndexBuffer->Release();

            if (state.pTextures[0])
                state.pTextures[0]->Release();

            if (state.pTextures[1])
                state.pTextures[1]->Release();

            if (state.pTextures[2])
                state.pTextures[2]->Release();
        }

        // The light of the frame, gathered into the field once for the whole of it.
        // One quad of four screen vertices covers the field and every texel of it
        // gathers its own light out of the copy of the frame: hundreds of texture
        // reads for a texel that is a hundredth of the pixels of a drop, where the
        // drops themselves would have done it for every single pixel they cover.
        // The target and the viewport this changes are put back straight away, and
        // the state of the device that is not is captured around the whole frame.
        void RenderLightField(IDirect3DSurface9* pTargetBack)
        {
            pDevice->SetRenderTarget(0, pLightSurface);

            D3DVIEWPORT9 viewport{};
            viewport.X = 0;
            viewport.Y = 0;
            viewport.Width = lightWidth;
            viewport.Height = lightHeight;
            viewport.MinZ = 0.0f;
            viewport.MaxZ = 1.0f;
            pDevice->SetViewport(&viewport);

            // the field is written to, not blended into: the four corners of the
            // quad cover every texel of it and nothing of what was there is to be
            // kept, and neither the depth nor a facing of it may take the quad out
            pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
            pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
            pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
            pDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
            pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

            const float constants[4] = { 1.0f / (float)lightWidth, 1.0f / (float)lightHeight,
                D3D9Light::CellCount, D3D9Light::Radius };
            pDevice->SetPixelShaderConstantF(0, constants, 1);
            pDevice->SetVertexShader(nullptr);
            pDevice->SetPixelShader(pLightShader);
            pDevice->SetTexture(0, pSceneTexture);
            pDevice->SetTexture(1, nullptr);
            pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
            pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
            pDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
            pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
            pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

            struct ScreenVertex
            {
                float x, y, z, rhw;
                uint32_t color;
                float u0, v0, u1, v1;
            };

            void* pVertexData = nullptr;
            if (SUCCEEDED(pVertexBuffer->Lock(0, 4 * sizeof(ScreenVertex), (void**)&pVertexData, D3DLOCK_DISCARD)))
            {
                // the corners of the field, on the pixels of it and not on their
                // edge, which is the half pixel a screen vertex sits on
                const float x0 = -0.5f;
                const float y0 = -0.5f;
                const float x1 = (float)lightWidth - 0.5f;
                const float y1 = (float)lightHeight - 0.5f;
                ScreenVertex* pDst = (ScreenVertex*)pVertexData;
                const float corners[4][2] = { { x0, y0 }, { x1, y0 }, { x1, y1 }, { x0, y1 } };

                for (int i = 0; i < 4; i++)
                {
                    pDst[i].x = corners[i][0];
                    pDst[i].y = corners[i][1];
                    pDst[i].z = 0.0f;
                    pDst[i].rhw = 1.0f;
                    pDst[i].color = 0xFFFFFFFF;
                    pDst[i].u0 = 0.0f;
                    pDst[i].v0 = 0.0f;
                    pDst[i].u1 = 0.0f;
                    pDst[i].v1 = 0.0f;
                }

                pVertexBuffer->Unlock();
            }

            pDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX2);
            pDevice->SetStreamSource(0, pVertexBuffer, 0, sizeof(ScreenVertex));
            pDevice->SetIndices(pIndexBuffer);
            pDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2);

            // the target the drops are drawn into is not the field: the field is a
            // pass of its own and the viewport of the target is set by ApplyState
            pDevice->SetRenderTarget(0, pTargetBack);
        }

    private:
        static inline constexpr UINT ScreenVertexStride = sizeof(float) * 9; // x, y, z, rhw, colour, u0, v0, u1, v1

        IDirect3DDevice9* pDevice = nullptr;
        // the shader of the drops and the shader that gathers the light into the
        // field, plus the field the two of them are of: see D3D9LightSource
        IDirect3DPixelShader9* pLightShader = nullptr;
        IDirect3DPixelShader9* pDropShader = nullptr;
        IDirect3DTexture9* pLightTexture = nullptr;
        IDirect3DSurface9* pLightSurface = nullptr;
        UINT lightWidth = 0;
        UINT lightHeight = 0;
        bool shaderAttempted = false;

        IDirect3DTexture9* pSceneTexture = nullptr;
        IDirect3DSurface9* pSceneSurface = nullptr;
        D3DFORMAT sceneFormat = D3DFMT_UNKNOWN;
        UINT sceneWidth = 0;
        UINT sceneHeight = 0;

        IDirect3DVertexBuffer9* pVertexBuffer = nullptr;
        IDirect3DIndexBuffer9* pIndexBuffer = nullptr;

        Texture* pMaskTexture = nullptr;
        RenderTarget* pTargetOverride = nullptr;

        UINT vertexStride = sizeof(Vertex);
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

    namespace D3D9Factory
    {
        inline Detail::Register registrar(RENDERER_D3D9, []() -> Backend* { return new D3D9Backend(); });
    }
}
