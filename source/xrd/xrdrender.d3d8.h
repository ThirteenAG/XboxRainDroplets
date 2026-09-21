#pragma once
// ---------------------------------------------------------------------------
// Direct3D 8 backend.
//
// One backend like all the others, it only cannot share a translation unit with
// the Direct3D 9 one: the headers of the two versions define the same Direct3D
// enumerations, so a project can only ever compile one of them. A project that
// is a Direct3D 8 game therefore defines XRD_ENABLE_D3D8 before including
// xrd/xrd.h, that one define is the device type, the headers and the renderer
// at once, and it is all such a project has to say.
//
// A binary that wants Direct3D 8 next to Direct3D 9 (the wrapper) includes this
// header from a translation unit of its own instead, which is what
// source/xrd/xrdrender.d3d8.cpp is for.
//
// Like Direct3D 9 it uses the fixed function pipeline with two texture stages
// and copies the frame with CopyRects, which is what the original Direct3D 8
// code did.
// ---------------------------------------------------------------------------

#include "xrdrender.h"

#include <d3d8.h>

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
            pDevice = (IDirect3DDevice8*)pNative;
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

                ApplyState(desc);

                // the picture the drops are drawn on top of
                pDevice->CopyRects(pTarget, nullptr, 0, pSceneSurface, nullptr);

                void* pVertexData = nullptr;
                if (SUCCEEDED(pVertexBuffer->Lock(0, numVertices * (projection == PROJECTION_SCREEN ? ScreenVertexStride : sizeof(Vertex)), (BYTE**)&pVertexData, 0)))
                {
                    if (projection == PROJECTION_SCREEN)
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
                            pDst[i].u0 = pVertices[i].u0;
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

            if (bOwnTarget)
                pTarget->Release();
        }

    private:
        static constexpr UINT ScreenVertexStride = sizeof(float) * 9;

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
            static constexpr int MaxVertices = 24000;
            static constexpr int MaxIndices = MaxVertices * 6;

            if (numVertices > MaxVertices)
                return false;

            if (!pVertexBuffer)
            {
                if (FAILED(pDevice->CreateVertexBuffer(MaxVertices * ScreenVertexStride, D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED, &pVertexBuffer)))
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

            if (FAILED(pDevice->CreateTexture(desc.Width, desc.Height, 1, D3DUSAGE_RENDERTARGET, desc.Format, D3DPOOL_DEFAULT, &pSceneTexture)))
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

            for (int stage = 0; stage < 2; stage++)
            {
                pDevice->SetTextureStageState(stage, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
                pDevice->SetTextureStageState(stage, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
                pDevice->SetTextureStageState(stage, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
                pDevice->SetTextureStageState(stage, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
                pDevice->SetTextureStageState(stage, D3DTSS_MIPFILTER, D3DTEXF_NONE);
            }

            pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
            pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
            pDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
            pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
            pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
            pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
            pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
            pDevice->SetRenderState(D3DRS_COLORWRITEENABLE, 0x0000000F);

            D3DVIEWPORT8 viewport{};
            viewport.X = 0;
            viewport.Y = 0;
            viewport.Width = desc.Width;
            viewport.Height = desc.Height;
            viewport.MinZ = 0.0f;
            viewport.MaxZ = 1.0f;
            pDevice->SetViewport(&viewport);
        }

    private:
        IDirect3DDevice8* pDevice = nullptr;

        IDirect3DTexture8* pSceneTexture = nullptr;
        IDirect3DSurface8* pSceneSurface = nullptr;
        D3DFORMAT sceneFormat = D3DFMT_UNKNOWN;
        UINT sceneWidth = 0;
        UINT sceneHeight = 0;

        IDirect3DVertexBuffer8* pVertexBuffer = nullptr;
        IDirect3DIndexBuffer8* pIndexBuffer = nullptr;

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
