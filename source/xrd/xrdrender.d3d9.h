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
            ApplyState(desc);

            void* pVertexData = nullptr;
            if (SUCCEEDED(pVertexBuffer->Lock(0, numVertices * sizeof(Vertex), (void**)&pVertexData, D3DLOCK_DISCARD)))
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
            IDirect3DVertexShader9* pVertexShader = nullptr;
            IDirect3DPixelShader9* pPixelShader = nullptr;
            IDirect3DVertexBuffer9* pVertexBuffer = nullptr;
            UINT vertexOffset = 0;
            UINT vertexStride = 0;
            IDirect3DIndexBuffer9* pIndexBuffer = nullptr;
            IDirect3DBaseTexture9* pTextures[2] = {};
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
            pDevice->GetVertexShader(&state.pVertexShader);
            pDevice->GetPixelShader(&state.pPixelShader);
            pDevice->GetStreamSource(0, &state.pVertexBuffer, &state.vertexOffset, &state.vertexStride);
            pDevice->GetIndices(&state.pIndexBuffer);
            pDevice->GetTexture(0, &state.pTextures[0]);
            pDevice->GetTexture(1, &state.pTextures[1]);

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
            pDevice->SetVertexShader(state.pVertexShader);
            pDevice->SetPixelShader(state.pPixelShader);

            pDevice->SetTexture(0, state.pTextures[0]);
            pDevice->SetTexture(1, state.pTextures[1]);

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

            if (state.pVertexBuffer)
                state.pVertexBuffer->Release();

            if (state.pIndexBuffer)
                state.pIndexBuffer->Release();

            if (state.pTextures[0])
                state.pTextures[0]->Release();

            if (state.pTextures[1])
                state.pTextures[1]->Release();
        }

    private:
        static inline constexpr UINT ScreenVertexStride = sizeof(float) * 9; // x, y, z, rhw, colour, u0, v0, u1, v1

        IDirect3DDevice9* pDevice = nullptr;

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
