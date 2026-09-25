#pragma once
// ---------------------------------------------------------------------------
// Direct3D 11 backend.
//
// The drops are drawn wherever the game is rendering at the moment the effect
// runs: the backend reads the render target that is bound right now, copies it
// into a texture, and draws into that same target. Nothing has to be told about
// the back buffer, so the very same code works at the end of the frame and in a
// game hook that runs before the UI is drawn.
//
// A multisampled target is resolved for the copy, the drops are still drawn
// into the multisampled target the game set up.
// ---------------------------------------------------------------------------

#include "xrdrender.h"
#include "xrdshaderbytecode.h"

#include <d3d11.h>

namespace Xrd
{
    class D3D11Backend : public Backend
    {
    public:
        ~D3D11Backend() override
        {
            Shutdown();
        }

        bool Init(void* pNative) override
        {
            if (!pNative)
                return false;

            IUnknown* pUnknown = (IUnknown*)pNative;

            IDXGISwapChain* pSwapChain = nullptr;
            if (SUCCEEDED(pUnknown->QueryInterface(__uuidof(IDXGISwapChain), (void**)&pSwapChain)) && pSwapChain)
            {
                pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&pDevice);

                // The frame that is presented is the back buffer of the swap
                // chain, so it is kept: an application that unbinds its render
                // target before it presents leaves nothing else behind.
                if (pDevice)
                {
                    this->pSwapChain = pSwapChain;
                }
                else
                {
                    pSwapChain->Release();
                }
            }
            else if (SUCCEEDED(pUnknown->QueryInterface(__uuidof(ID3D11Device), (void**)&pDevice)))
            {
                // the device was handed over directly
            }

            if (!pDevice)
                return false;

            pDevice->GetImmediateContext(&pImmediateContext);
            pContext = pImmediateContext;
            return pContext != nullptr;
        }

        // The context the commands go to, which is the one of the device unless the
        // game handed over the one it is recording its frame into, see
        // Backend::SetCommandContext. It is borrowed, the game owns it.
        void SetCommandContext(void* pNative) override
        {
            pContext = pNative ? (ID3D11DeviceContext*)pNative : pImmediateContext;
        }

        void Shutdown() override
        {
            ReleaseResources();

            pContext = nullptr;

            if (pImmediateContext)
            {
                pImmediateContext->Release();
                pImmediateContext = nullptr;
            }

            if (pDevice)
            {
                pDevice->Release();
                pDevice = nullptr;
            }
        }

        // Direct3D 11 rebuilds the swap chain of the emulator when the window is
        // replaced, a fullscreen switch does it, and nothing reports that as a
        // resize. What is presented after that is the back buffer of the new chain,
        // so the backend has to follow it.
        bool UpdateNative(void* pNative) override
        {
            if (!pNative)
                return true;

            // the swap chain of the game is what an application hands over, and it
            // is the one that was kept
            if (reinterpret_cast<void*>(pSwapChain) == pNative)
                return true;

            if (!pDevice || !pImmediateContext)
                return false;

            IDXGISwapChain* pNew = nullptr;
            if (FAILED(reinterpret_cast<IUnknown*>(pNative)->QueryInterface(__uuidof(IDXGISwapChain), (void**)&pNew)) || !pNew)
                return true;

            // Everything of the backend belongs to the device it was built with, a
            // new one means it has to be built again.
            ID3D11Device* pNewDevice = nullptr;
            pNew->GetDevice(__uuidof(ID3D11Device), (void**)&pNewDevice);

            if (pNewDevice && (pNewDevice != pDevice))
            {
                pNewDevice->Release();
                pNew->Release();
                return false;
            }

            if (pNewDevice)
                pNewDevice->Release();

            if (pNew != pSwapChain)
            {
                // the cached back buffer belongs to the chain that is gone
                ReleaseBackBuffer();

                if (pSwapChain)
                    pSwapChain->Release();

                pSwapChain = pNew;
            }
            else
            {
                pNew->Release();
            }

            return true;
        }

        void Reset() override
        {
            ReleaseSceneTexture();
        }

        bool IsActive() const override
        {
            return pDevice != nullptr && pContext != nullptr;
        }

        Size GetSize() const override
        {
            Size size = targetSize;

            ID3D11RenderTargetView* pTarget = nullptr;
            ID3D11Resource* pResource = nullptr;

            if (GetTarget(&pTarget, &pResource) && pResource)
            {
                D3D11_TEXTURE2D_DESC desc{};
                ((ID3D11Texture2D*)pResource)->GetDesc(&desc);
                size = { (int32_t)desc.Width, (int32_t)desc.Height };
            }

            if (pTarget)
                pTarget->Release();

            if (pResource)
                pResource->Release();

            const_cast<D3D11Backend*>(this)->targetSize = size;
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

            // The masks are stored as RGBA in memory, which is what this format
            // expects, so they can be uploaded as they are.
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = textureWidth;
            desc.Height = textureHeight;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

            D3D11_SUBRESOURCE_DATA data{};
            data.pSysMem = pixels;
            data.SysMemPitch = (UINT)textureWidth * 4;

            ID3D11Texture2D* pTexture = nullptr;
            if (FAILED(pDevice->CreateTexture2D(&desc, pixels ? &data : nullptr, &pTexture)))
                return nullptr;

            ID3D11ShaderResourceView* pView = nullptr;
            if (FAILED(pDevice->CreateShaderResourceView(pTexture, nullptr, &pView)))
            {
                pTexture->Release();
                return nullptr;
            }

            auto* pResult = new Texture{};
            pResult->resource = pTexture;
            pResult->extra = pView;
            pResult->size = { textureWidth, textureHeight };
            return pResult;
        }

        void DestroyTexture(Texture* pTexture) override
        {
            if (!pTexture)
                return;

            if (pTexture->resource)
                ((ID3D11Texture2D*)pTexture->resource)->Release();

            if (pTexture->extra)
                ((ID3D11ShaderResourceView*)pTexture->extra)->Release();

            delete pTexture;
        }

        void SetTarget(RenderTarget* pTarget) override
        {
            pTargetOverride = pTarget;
        }

        void SetTargetState(TargetState targetState) override
        {
            // The present hooks know that the finished frame is the one of the
            // swap chain, whatever the application has bound at that moment.
            // Everywhere else the target is whatever is bound.
            presentedTarget = (targetState == TARGET_STATE_PRESENT);
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

        bool Prepare(int maxVertices) override
        {
            (void)maxVertices;
            if (!pDevice || !EnsureDeviceObjects()) return false;
            ID3D11RenderTargetView* target = nullptr;
            ID3D11Resource* resource = nullptr;
            bool ready = GetTarget(&target, &resource) && resource;
            if (ready)
            {
                D3D11_TEXTURE2D_DESC desc{};
                ((ID3D11Texture2D*)resource)->GetDesc(&desc);
                ready = EnsureSceneTexture(desc);
            }
            if (resource) resource->Release();
            if (target) target->Release();
            return ready;
        }

        void Render(const Vertex* pVertices, int numVertices, PrimitiveType primitive) override
        {
            if (!pDevice || !pContext || !pVertices || numVertices <= 0)
                return;

            // the vertex buffer of this backend is a fixed size, and what does not
            // fit in it is not drawn rather than written past its end
            if (numVertices > MaxVertices)
                numVertices = MaxVertices;

            const int numIndices = (primitive == PRIMITIVE_TRIANGLES) ? (numVertices / 4) * 6 : 0;

            if (primitive == PRIMITIVE_TRIANGLES && numIndices <= 0)
                return;

            if (!EnsureDeviceObjects())
                return;

            ID3D11RenderTargetView* pTarget = nullptr;
            ID3D11Resource* pResource = nullptr;

            if (!GetTarget(&pTarget, &pResource) || !pResource)
            {
                if (pTarget)
                    pTarget->Release();

                return;
            }

            D3D11_TEXTURE2D_DESC desc{};
            ((ID3D11Texture2D*)pResource)->GetDesc(&desc);
            targetSize = { (int32_t)desc.Width, (int32_t)desc.Height };

            if (!EnsureSceneTexture(desc))
            {
                pResource->Release();
                pTarget->Release();
                return;
            }

            // The state of the application has to be taken before anything of it is changed.
            // The shader resource slots were the ones that were changed too early here: the
            // state that is put back afterwards had the slots empty in it, so every draw of the
            // application that came after the drops found nothing bound to its pixel shader,
            // which is what a frame that flashes black is. The slots are freed for the copy of
            // the scene, which is the only thing that needs them empty, and that happens after
            // the state has been taken.
            SavedState state{};
            CaptureState(state);

            // The copy has to happen before the states are changed, it is the
            // picture the drops are drawn on top of. Our own scene texture has
            // to be out of the shader resource slots while it is written to,
            // and the vertex shader reads it as well, see VSMain.
            ID3D11ShaderResourceView* pNullViews[2] = {};
            pContext->PSSetShaderResources(0, 2, pNullViews);
            pContext->VSSetShaderResources(0, 1, pNullViews);

            if (desc.SampleDesc.Count > 1)
                pContext->ResolveSubresource(pSceneTexture, 0, pResource, 0, desc.Format);
            else
                pContext->CopyResource(pSceneTexture, pResource);

            pResource->Release();

            ApplyState(desc, pTarget);

            // The vertices go into the buffer with a map of it: UpdateSubresource with
            // no box takes the whole buffer from the pointer it is given, and the
            // vertices of one frame are a small part of what that buffer holds, so the
            // driver would read far past the end of them. That is a read of the memory
            // behind the array the vertices are in, which is what the access violations
            // inside the driver were, and a map of a dynamic buffer copies only what is
            // there. A context that records the frame of a game cannot have its buffers
            // mapped, but this is not one: the draw is issued on the context of the
            // thread that executes the frame, see SetCommandContext.
            const UINT numVertexBytes = (UINT)numVertices * (UINT)sizeof(Vertex);
            D3D11_MAPPED_SUBRESOURCE mapped{};

            if (numVertexBytes > vertexBufferSize || FAILED(pContext->Map(pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
            {
                RestoreState(state);
                pTarget->Release();
                return;
            }

            memcpy(mapped.pData, pVertices, numVertexBytes);
            pContext->Unmap(pVertexBuffer, 0);

            {
                Constants constants{};
                if (projection == PROJECTION_SCREEN)
                {
                    Matrix ortho = Matrix::OrthographicOffCenter(0.0f, (float)desc.Width, (float)desc.Height, 0.0f, 0.0f, 1.0f);
                    memcpy(constants.projection, ortho.m, sizeof(constants.projection));
                }
                else
                {
                    memcpy(constants.projection, worldMatrix.m, sizeof(constants.projection));
                }

                constants.uvOffset[0] = uvOffsetX;
                constants.uvOffset[1] = uvOffsetY;
                constants.uvScale[0] = uvScaleX;
                constants.uvScale[1] = uvScaleY;
                constants.sceneComplement[0] = sceneComplement ? 1.0f : 0.0f;
            constants.sceneComplement[1] = sceneSampling ? 1.0f : 0.0f;

                pContext->UpdateSubresource(pConstantBuffer, 0, nullptr, &constants, 0, 0);

                UINT stride = sizeof(Vertex);
                UINT offset = 0;
                pContext->IASetVertexBuffers(0, 1, &pVertexBuffer, &stride, &offset);
                pContext->IASetIndexBuffer(pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
                pContext->IASetPrimitiveTopology(primitive == PRIMITIVE_TRIANGLES ? D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST : D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
                pContext->IASetInputLayout(pInputLayout);

                pContext->VSSetShader(pVertexShader, nullptr, 0);
                pContext->PSSetShader(pPixelShader, nullptr, 0);

                ID3D11Buffer* pBuffers[1] = { pConstantBuffer };
                pContext->VSSetConstantBuffers(0, 1, pBuffers);
                pContext->PSSetConstantBuffers(0, 1, pBuffers);

                ID3D11ShaderResourceView* pViews[2] = { pSceneTextureView, nullptr };
                if (pMaskTexture && pMaskTexture->extra)
                    pViews[1] = (ID3D11ShaderResourceView*)pMaskTexture->extra;

                pContext->PSSetShaderResources(0, 2, pViews);
                pContext->PSSetSamplers(0, 1, &pSampler);

                // The vertex shader gathers the light around a drop out of the
                // scene texture, which is slot 0 of its resources and the same
                // sampler the pixel shader uses.
                pContext->VSSetShaderResources(0, 1, pViews);
                pContext->VSSetSamplers(0, 1, &pSampler);

                if (primitive == PRIMITIVE_TRIANGLES)
                    pContext->DrawIndexed(numIndices, 0, 0);
                else
                    pContext->Draw(numVertices, 0);

                RestoreState(state);
            }

            pTarget->Release();
        }

    private:
        struct Constants
        {
            float projection[16];
            float uvOffset[4];
            float uvScale[4];
            float sceneComplement[4];
        };

        struct SavedState
        {
            // A frame of an engine that draws into several targets at once, which the
            // one of a modern game does for a deferred renderer, has all of them set,
            // and the engine keeps its own record of that: a draw that leaves only the
            // first one behind, and none past it, makes every draw that follows wrong in
            // a way that is hard to see here, the engine does not set them again because
            // its record says they are set. All of them are taken and given back, see
            // CaptureState.
            ID3D11RenderTargetView* pRenderTargets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
            UINT renderTargetCount = 0;
            ID3D11DepthStencilView* pDepthStencil = nullptr;
            ID3D11BlendState* pBlendState = nullptr;
            float blendFactor[4] = {};
            UINT sampleMask = 0;
            ID3D11DepthStencilState* pDepthStencilState = nullptr;
            UINT stencilRef = 0;
            ID3D11RasterizerState* pRasterizerState = nullptr;
            D3D11_RECT scissors[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
            UINT scissorCount = 0;
            D3D11_VIEWPORT viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
            UINT viewportCount = 0;
            ID3D11VertexShader* pVertexShader = nullptr;
            ID3D11PixelShader* pPixelShader = nullptr;
            ID3D11GeometryShader* pGeometryShader = nullptr;
            ID3D11HullShader* pHullShader = nullptr;
            ID3D11DomainShader* pDomainShader = nullptr;
            ID3D11InputLayout* pInputLayout = nullptr;
            ID3D11Buffer* pVertexBuffer = nullptr;
            UINT vertexStride = 0;
            UINT vertexOffset = 0;
            ID3D11Buffer* pIndexBuffer = nullptr;
            DXGI_FORMAT indexFormat = DXGI_FORMAT_UNKNOWN;
            UINT indexOffset = 0;
            D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            ID3D11Buffer* pVSConstants = nullptr;
            ID3D11Buffer* pPSConstants = nullptr;
            ID3D11ShaderResourceView* pViews[2] = {};
            ID3D11SamplerState* pSampler = nullptr;
            ID3D11ShaderResourceView* pVSView = nullptr;
            ID3D11SamplerState* pVSSampler = nullptr;
        };

        bool GetTarget(ID3D11RenderTargetView** ppTarget, ID3D11Resource** ppResource) const
        {
            *ppTarget = nullptr;
            *ppResource = nullptr;

            if (pTargetOverride && pTargetOverride->resource)
            {
                *ppTarget = (ID3D11RenderTargetView*)pTargetOverride->resource;
                (*ppTarget)->AddRef();
            }
            else if (!presentedTarget && pContext)
            {
                pContext->OMGetRenderTargets(1, ppTarget, nullptr);
            }

            if (!*ppTarget)
                AcquireBackBuffer(ppTarget, ppResource);

            if (!*ppTarget)
                return false;

            if (!*ppResource)
                (*ppTarget)->GetResource(ppResource);

            return *ppResource != nullptr;
        }

        // The back buffer of the swap chain, which is where the frame that is
        // about to be presented lives. It is kept between the frames, a flip
        // model swap chain hands out a different resource after every present.
        void AcquireBackBuffer(ID3D11RenderTargetView** ppTarget, ID3D11Resource** ppResource) const
        {
            if (!pSwapChain || !pDevice)
                return;

            ID3D11Texture2D* pCurrent = nullptr;
            if (FAILED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pCurrent)) || !pCurrent)
                return;

            D3D11Backend* pThis = const_cast<D3D11Backend*>(this);

            if (pCurrent != pBackBuffer)
            {
                if (pThis->pBackBufferView)
                {
                    pThis->pBackBufferView->Release();
                    pThis->pBackBufferView = nullptr;
                }

                if (pThis->pBackBuffer)
                    pThis->pBackBuffer->Release();

                pThis->pBackBuffer = pCurrent;
                pCurrent->AddRef();

                pDevice->CreateRenderTargetView(pCurrent, nullptr, &pThis->pBackBufferView);
            }

            pCurrent->Release();

            if (pThis->pBackBufferView)
            {
                pThis->pBackBufferView->AddRef();
                *ppTarget = pThis->pBackBufferView;

                pThis->pBackBuffer->AddRef();
                *ppResource = pThis->pBackBuffer;
            }
        }

        bool EnsureDeviceObjects()
        {
            if (pVertexShader)
                return true;

            if (!pVertexBuffer)
            {

                auto pVertexBlob = LoadShaderBytecode(IDR_DROP10VS);
                auto pPixelBlob = LoadShaderBytecode(IDR_DROP10PS);

                if (!pVertexBlob || !pPixelBlob)
                    return false;

                const bool bVertexOk = SUCCEEDED(pDevice->CreateVertexShader(pVertexBlob.GetBufferPointer(), pVertexBlob.GetBufferSize(), nullptr, &pVertexShader));

                D3D11_INPUT_ELEMENT_DESC layout[] =
                {
                    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, offsetof(Vertex, x),     D3D11_INPUT_PER_VERTEX_DATA, 0 },
                    { "COLOR",    0, DXGI_FORMAT_B8G8R8A8_UNORM,     0, offsetof(Vertex, color), D3D11_INPUT_PER_VERTEX_DATA, 0 },
                    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, offsetof(Vertex, u0),    D3D11_INPUT_PER_VERTEX_DATA, 0 },
                    { "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT,       0, offsetof(Vertex, u1),    D3D11_INPUT_PER_VERTEX_DATA, 0 },
                };

                const bool bLayoutOk = SUCCEEDED(pDevice->CreateInputLayout(layout, ARRAYSIZE(layout), pVertexBlob.GetBufferPointer(), pVertexBlob.GetBufferSize(), &pInputLayout));
                const bool bPixelOk = SUCCEEDED(pDevice->CreatePixelShader(pPixelBlob.GetBufferPointer(), pPixelBlob.GetBufferSize(), nullptr, &pPixelShader));


                if (!bVertexOk || !bLayoutOk || !bPixelOk)
                    return false;

                D3D11_BUFFER_DESC bufferDesc{};
                bufferDesc.ByteWidth = MaxVertices * sizeof(Vertex);
                // Dynamic, so that the vertices of a frame can be written into it with a
                // map: UpdateSubresource with no box takes the whole buffer from the
                // pointer it is given, which is far more than the vertices of one frame,
                // and the driver then reads past the end of them, see Render.
                bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
                bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
                bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

                if (FAILED(pDevice->CreateBuffer(&bufferDesc, nullptr, &pVertexBuffer)))
                    return false;

                vertexBufferSize = bufferDesc.ByteWidth;

                constexpr int MaxIndices = (MaxVertices / 4) * 6;

                std::vector<uint16_t> indices(MaxIndices);
                for (int i = 0; i < MaxVertices / 4; i++)
                {
                    indices[i * 6 + 0] = (uint16_t)(i * 4 + 0);
                    indices[i * 6 + 1] = (uint16_t)(i * 4 + 1);
                    indices[i * 6 + 2] = (uint16_t)(i * 4 + 2);
                    indices[i * 6 + 3] = (uint16_t)(i * 4 + 0);
                    indices[i * 6 + 4] = (uint16_t)(i * 4 + 2);
                    indices[i * 6 + 5] = (uint16_t)(i * 4 + 3);
                }

                D3D11_BUFFER_DESC indexDesc{};
                indexDesc.ByteWidth = (UINT)(indices.size() * sizeof(uint16_t));
                indexDesc.Usage = D3D11_USAGE_IMMUTABLE;
                indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

                D3D11_SUBRESOURCE_DATA indexData{};
                indexData.pSysMem = indices.data();

                if (FAILED(pDevice->CreateBuffer(&indexDesc, &indexData, &pIndexBuffer)))
                    return false;

                D3D11_BUFFER_DESC constantDesc{};
                constantDesc.ByteWidth = sizeof(Constants);
                constantDesc.Usage = D3D11_USAGE_DEFAULT;
                constantDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

                if (FAILED(pDevice->CreateBuffer(&constantDesc, nullptr, &pConstantBuffer)))
                    return false;

                D3D11_SAMPLER_DESC samplerDesc{};
                samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
                samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
                samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
                samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
                samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
                samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

                if (FAILED(pDevice->CreateSamplerState(&samplerDesc, &pSampler)))
                    return false;

                // The drop colour is modulated with the atlas of shapes and the
                // copy of the frame, and the result is blended over the picture
                // exactly like the alpha blending of the original code did.
                D3D11_BLEND_DESC blendDesc{};
                blendDesc.RenderTarget[0].BlendEnable = TRUE;
                blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
                blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
                blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
                blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
                blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
                blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
                blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

                if (FAILED(pDevice->CreateBlendState(&blendDesc, &pBlendState)))
                    return false;

                D3D11_DEPTH_STENCIL_DESC depthDesc{};
                depthDesc.DepthEnable = FALSE;
                depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
                depthDesc.StencilEnable = FALSE;

                if (FAILED(pDevice->CreateDepthStencilState(&depthDesc, &pDepthStencilState)))
                    return false;

                D3D11_RASTERIZER_DESC rasterizerDesc{};
                rasterizerDesc.FillMode = D3D11_FILL_SOLID;
                rasterizerDesc.CullMode = D3D11_CULL_NONE;
                rasterizerDesc.DepthClipEnable = FALSE;
                rasterizerDesc.ScissorEnable = FALSE;

                if (FAILED(pDevice->CreateRasterizerState(&rasterizerDesc, &pRasterizerState)))
                    return false;
            }

            return pVertexShader != nullptr;
        }

        // The concrete format a shader resource view of a texture of this format is
        // made with. A typeless texture has no format of its own to be read as, which
        // is what makes the view that is made of it with the format of the resource
        // itself fail in the driver: the format it is handed is the one that says
        // "no format", and a driver has nothing to put into its pipeline for it, see
        // EnsureSceneTexture.
        static DXGI_FORMAT ConcreteFormat(DXGI_FORMAT format)
        {
            switch (format)
            {
            case DXGI_FORMAT_R32G32B32A32_TYPELESS: return DXGI_FORMAT_R32G32B32A32_FLOAT;
            case DXGI_FORMAT_R32G32B32_TYPELESS:    return DXGI_FORMAT_R32G32B32_FLOAT;
            case DXGI_FORMAT_R16G16B16A16_TYPELESS: return DXGI_FORMAT_R16G16B16A16_FLOAT;
            case DXGI_FORMAT_R32G32_TYPELESS:       return DXGI_FORMAT_R32G32_FLOAT;
            case DXGI_FORMAT_R10G10B10A2_TYPELESS:  return DXGI_FORMAT_R10G10B10A2_UNORM;
            case DXGI_FORMAT_R8G8B8A8_TYPELESS:     return DXGI_FORMAT_R8G8B8A8_UNORM;
            case DXGI_FORMAT_R16G16_TYPELESS:       return DXGI_FORMAT_R16G16_FLOAT;
            case DXGI_FORMAT_R32_TYPELESS:          return DXGI_FORMAT_R32_FLOAT;
            case DXGI_FORMAT_R8G8_TYPELESS:         return DXGI_FORMAT_R8G8_UNORM;
            case DXGI_FORMAT_R16_TYPELESS:          return DXGI_FORMAT_R16_FLOAT;
            case DXGI_FORMAT_R8_TYPELESS:           return DXGI_FORMAT_R8_UNORM;
            case DXGI_FORMAT_B8G8R8A8_TYPELESS:     return DXGI_FORMAT_B8G8R8A8_UNORM;
            case DXGI_FORMAT_B8G8R8X8_TYPELESS:     return DXGI_FORMAT_B8G8R8X8_UNORM;
            case DXGI_FORMAT_BC1_TYPELESS:          return DXGI_FORMAT_BC1_UNORM;
            case DXGI_FORMAT_BC2_TYPELESS:          return DXGI_FORMAT_BC2_UNORM;
            case DXGI_FORMAT_BC3_TYPELESS:          return DXGI_FORMAT_BC3_UNORM;
            case DXGI_FORMAT_BC4_TYPELESS:          return DXGI_FORMAT_BC4_UNORM;
            case DXGI_FORMAT_BC5_TYPELESS:          return DXGI_FORMAT_BC5_UNORM;
            
            // Nothing that stands for no format at all, and nothing a depth buffer
            // is read as, which a target of a game may be as well.
            case DXGI_FORMAT_UNKNOWN:
            case DXGI_FORMAT_R32G8X24_TYPELESS:
            case DXGI_FORMAT_R24G8_TYPELESS:
            case DXGI_FORMAT_D32_FLOAT:
            case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
            case DXGI_FORMAT_D24_UNORM_S8_UINT:
            case DXGI_FORMAT_D16_UNORM:
                return DXGI_FORMAT_UNKNOWN;

            default:
                break;
            }

            return format;
        }

        bool EnsureSceneTexture(const D3D11_TEXTURE2D_DESC& target)
        {
            if (pSceneTexture && sceneFormat == target.Format && sceneWidth == target.Width && sceneHeight == target.Height)
                return true;

            ReleaseSceneTexture();

            // The copy of the frame is made with the format the resource of the game
            // was created with, a copy is only made between resources of one format,
            // and it is read with the concrete format of that one, see ConcreteFormat:
            // the two are not the same thing when the game draws into a typeless buffer.
            const DXGI_FORMAT viewFormat = ConcreteFormat(target.Format);

            if (viewFormat == DXGI_FORMAT_UNKNOWN)
                return false;

            D3D11_TEXTURE2D_DESC desc = target;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            desc.CPUAccessFlags = 0;
            desc.MiscFlags = 0;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.SampleDesc.Count = 1;
            desc.SampleDesc.Quality = 0;

            if (FAILED(pDevice->CreateTexture2D(&desc, nullptr, &pSceneTexture)))
            {
                pSceneTexture = nullptr;
                return false;
            }

            D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
            viewDesc.Format = viewFormat;
            viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            viewDesc.Texture2D.MostDetailedMip = 0;
            viewDesc.Texture2D.MipLevels = 1;

            if (FAILED(pDevice->CreateShaderResourceView(pSceneTexture, &viewDesc, &pSceneTextureView)))
            {
                ReleaseSceneTexture();
                return false;
            }

            sceneFormat = target.Format;
            sceneWidth = target.Width;
            sceneHeight = target.Height;
            return true;
        }

        void ReleaseSceneTexture()
        {
            if (pSceneTextureView)
            {
                pSceneTextureView->Release();
                pSceneTextureView = nullptr;
            }

            if (pSceneTexture)
            {
                pSceneTexture->Release();
                pSceneTexture = nullptr;
            }

            sceneFormat = DXGI_FORMAT_UNKNOWN;
            sceneWidth = 0;
            sceneHeight = 0;
        }

        void ReleaseBackBuffer()
        {
            if (pBackBufferView) { pBackBufferView->Release(); pBackBufferView = nullptr; }
            if (pBackBuffer) { pBackBuffer->Release(); pBackBuffer = nullptr; }
        }

        void ReleaseResources()
        {
            ReleaseSceneTexture();

            ReleaseBackBuffer();
            if (pSwapChain) { pSwapChain->Release(); pSwapChain = nullptr; }

            if (pVertexShader) { pVertexShader->Release(); pVertexShader = nullptr; }
            if (pPixelShader) { pPixelShader->Release(); pPixelShader = nullptr; }
            if (pInputLayout) { pInputLayout->Release(); pInputLayout = nullptr; }
            if (pVertexBuffer) { pVertexBuffer->Release(); pVertexBuffer = nullptr; vertexBufferSize = 0; }
            if (pIndexBuffer) { pIndexBuffer->Release(); pIndexBuffer = nullptr; }
            if (pConstantBuffer) { pConstantBuffer->Release(); pConstantBuffer = nullptr; }
            if (pSampler) { pSampler->Release(); pSampler = nullptr; }
            if (pBlendState) { pBlendState->Release(); pBlendState = nullptr; }
            if (pDepthStencilState) { pDepthStencilState->Release(); pDepthStencilState = nullptr; }
            if (pRasterizerState) { pRasterizerState->Release(); pRasterizerState = nullptr; }
        }

        void CaptureState(SavedState& state)
        {
            pContext->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, state.pRenderTargets, &state.pDepthStencil);

            // The slots past the last one that is set are null, so the count the draw
            // started with is the index past the last target that was set.
            state.renderTargetCount = 0;

            for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
                if (state.pRenderTargets[i])
                    state.renderTargetCount = i + 1;

            pContext->OMGetBlendState(&state.pBlendState, state.blendFactor, &state.sampleMask);
            pContext->OMGetDepthStencilState(&state.pDepthStencilState, &state.stencilRef);
            pContext->RSGetState(&state.pRasterizerState);
            pContext->RSGetScissorRects(&state.scissorCount, nullptr);
            if (state.scissorCount > D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)
                state.scissorCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
            pContext->RSGetScissorRects(&state.scissorCount, state.scissors);
            pContext->RSGetViewports(&state.viewportCount, nullptr);
            if (state.viewportCount > D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)
                state.viewportCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
            pContext->RSGetViewports(&state.viewportCount, state.viewports);

            pContext->VSGetShader(&state.pVertexShader, nullptr, nullptr);
            pContext->PSGetShader(&state.pPixelShader, nullptr, nullptr);
            pContext->GSGetShader(&state.pGeometryShader, nullptr, nullptr);
            pContext->HSGetShader(&state.pHullShader, nullptr, nullptr);
            pContext->DSGetShader(&state.pDomainShader, nullptr, nullptr);
            pContext->IAGetInputLayout(&state.pInputLayout);
            pContext->IAGetVertexBuffers(0, 1, &state.pVertexBuffer, &state.vertexStride, &state.vertexOffset);
            pContext->IAGetIndexBuffer(&state.pIndexBuffer, &state.indexFormat, &state.indexOffset);
            pContext->IAGetPrimitiveTopology(&state.topology);
            pContext->VSGetConstantBuffers(0, 1, &state.pVSConstants);
            pContext->PSGetConstantBuffers(0, 1, &state.pPSConstants);
            pContext->PSGetShaderResources(0, 2, state.pViews);
            pContext->PSGetSamplers(0, 1, &state.pSampler);
            pContext->VSGetShaderResources(0, 1, &state.pVSView);
            pContext->VSGetSamplers(0, 1, &state.pVSSampler);
        }

        void ApplyState(const D3D11_TEXTURE2D_DESC& desc, ID3D11RenderTargetView* pTarget)
        {
            // A draw runs through every stage that is set, and the stages this effect
            // has no shader for are the ones the application had: a geometry, hull or
            // domain shader of the game left set would be run on the vertices of the
            // drops, with an input signature that does not match what the shaders here
            // put out, which is a pipeline no driver can be handed. They are taken and
            // left empty for the draw, see CaptureState and RestoreState.
            pContext->GSSetShader(nullptr, nullptr, 0);
            pContext->HSSetShader(nullptr, nullptr, 0);
            pContext->DSSetShader(nullptr, nullptr, 0);

            viewport.TopLeftX = 0.0f;
            viewport.TopLeftY = 0.0f;
            viewport.Width = (float)desc.Width;
            viewport.Height = (float)desc.Height;
            viewport.MinDepth = 0.0f;
            viewport.MaxDepth = 1.0f;

            pContext->OMSetRenderTargets(1, &pTarget, nullptr);
            pContext->OMSetBlendState(pBlendState, nullptr, 0xFFFFFFFF);
            pContext->OMSetDepthStencilState(pDepthStencilState, 0);
            pContext->RSSetState(pRasterizerState);
            pContext->RSSetViewports(1, &viewport);
        }

        void RestoreState(SavedState& state)
        {
            pContext->OMSetRenderTargets(state.renderTargetCount, state.pRenderTargets, state.pDepthStencil);
            pContext->OMSetBlendState(state.pBlendState, state.blendFactor, state.sampleMask);
            pContext->OMSetDepthStencilState(state.pDepthStencilState, state.stencilRef);

            // The state of the application is given back even when there was none: what
            // is left behind here is drawn with by an engine that keeps its own record
            // of the state it set, because it does not set again what it believes is
            // still set.
            pContext->RSSetState(state.pRasterizerState);

            if (state.viewportCount)
                pContext->RSSetViewports(state.viewportCount, state.viewports);

            if (state.scissorCount)
                pContext->RSSetScissorRects(state.scissorCount, state.scissors);

            pContext->VSSetShader(state.pVertexShader, nullptr, 0);
            pContext->PSSetShader(state.pPixelShader, nullptr, 0);
            pContext->GSSetShader(state.pGeometryShader, nullptr, 0);
            pContext->HSSetShader(state.pHullShader, nullptr, 0);
            pContext->DSSetShader(state.pDomainShader, nullptr, 0);
            pContext->IASetInputLayout(state.pInputLayout);
            pContext->IASetVertexBuffers(0, 1, &state.pVertexBuffer, &state.vertexStride, &state.vertexOffset);
            pContext->IASetIndexBuffer(state.pIndexBuffer, state.indexFormat, state.indexOffset);
            pContext->IASetPrimitiveTopology(state.topology);

            ID3D11Buffer* pVSConstants[1] = { state.pVSConstants };
            pContext->VSSetConstantBuffers(0, 1, pVSConstants);

            ID3D11Buffer* pPSConstants[1] = { state.pPSConstants };
            pContext->PSSetConstantBuffers(0, 1, pPSConstants);

            pContext->PSSetShaderResources(0, 2, state.pViews);
            pContext->PSSetSamplers(0, 1, &state.pSampler);
            pContext->VSSetShaderResources(0, 1, &state.pVSView);
            pContext->VSSetSamplers(0, 1, &state.pVSSampler);

            for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
                if (state.pRenderTargets[i])
                    state.pRenderTargets[i]->Release();

            if (state.pDepthStencil) state.pDepthStencil->Release();
            if (state.pBlendState) state.pBlendState->Release();
            if (state.pDepthStencilState) state.pDepthStencilState->Release();
            if (state.pRasterizerState) state.pRasterizerState->Release();
            if (state.pVertexShader) state.pVertexShader->Release();
            if (state.pPixelShader) state.pPixelShader->Release();
            if (state.pGeometryShader) state.pGeometryShader->Release();
            if (state.pHullShader) state.pHullShader->Release();
            if (state.pDomainShader) state.pDomainShader->Release();
            if (state.pInputLayout) state.pInputLayout->Release();
            if (state.pVertexBuffer) state.pVertexBuffer->Release();
            if (state.pIndexBuffer) state.pIndexBuffer->Release();
            if (state.pVSConstants) state.pVSConstants->Release();
            if (state.pPSConstants) state.pPSConstants->Release();
            if (state.pViews[0]) state.pViews[0]->Release();
            if (state.pViews[1]) state.pViews[1]->Release();
            if (state.pSampler) state.pSampler->Release();
            if (state.pVSView) state.pVSView->Release();
            if (state.pVSSampler) state.pVSSampler->Release();
        }

    private:
        // the size of the vertex buffer the geometry of this backend is drawn
        // from: the vertex buffer below is not grown, so nothing can be drawn from
        // it that does not fit in it
        static constexpr int MaxVertices = 64000;

        ID3D11Device* pDevice = nullptr;
        ID3D11DeviceContext* pImmediateContext = nullptr;
        ID3D11DeviceContext* pContext = nullptr;

        IDXGISwapChain* pSwapChain = nullptr;
        ID3D11Texture2D* pBackBuffer = nullptr;
        ID3D11RenderTargetView* pBackBufferView = nullptr;
        bool presentedTarget = false;

        ID3D11VertexShader* pVertexShader = nullptr;
        ID3D11PixelShader* pPixelShader = nullptr;
        ID3D11InputLayout* pInputLayout = nullptr;
        ID3D11Buffer* pVertexBuffer = nullptr;
        UINT vertexBufferSize = 0;
        ID3D11Buffer* pIndexBuffer = nullptr;
        ID3D11Buffer* pConstantBuffer = nullptr;
        ID3D11SamplerState* pSampler = nullptr;
        ID3D11BlendState* pBlendState = nullptr;
        ID3D11DepthStencilState* pDepthStencilState = nullptr;
        ID3D11RasterizerState* pRasterizerState = nullptr;

        ID3D11Texture2D* pSceneTexture = nullptr;
        ID3D11ShaderResourceView* pSceneTextureView = nullptr;
        DXGI_FORMAT sceneFormat = DXGI_FORMAT_UNKNOWN;
        UINT sceneWidth = 0;
        UINT sceneHeight = 0;

        D3D11_VIEWPORT viewport{};

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

    namespace D3D11Factory
    {
        inline Detail::Register registrar(RENDERER_D3D11, []() -> Backend* { return new D3D11Backend(); });
    }
}
