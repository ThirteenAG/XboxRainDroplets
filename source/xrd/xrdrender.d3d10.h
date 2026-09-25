#pragma once
// ---------------------------------------------------------------------------
// Direct3D 10 and 10.1 backend.
//
// The same behaviour as the Direct3D 11 one, on the interfaces a Direct3D 10
// game has. Both feature levels share it, 10.1 only differs in the shader model
// the driver accepts.
// ---------------------------------------------------------------------------

#include "xrdrender.h"
#include "xrdshaderbytecode.h"

#include <d3d10_1.h>

namespace Xrd
{
    class D3D10Backend : public Backend
    {
    public:
        ~D3D10Backend() override
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
                pSwapChain->GetDevice(__uuidof(ID3D10Device), (void**)&pDevice);
                pSwapChain->Release();
            }
            else if (SUCCEEDED(pUnknown->QueryInterface(__uuidof(ID3D10Device), (void**)&pDevice)))
            {
                // the device was handed over directly
            }

            return pDevice != nullptr;
        }

        void Shutdown() override
        {
            ReleaseResources();

            if (pDevice)
            {
                pDevice->Release();
                pDevice = nullptr;
            }
        }

        void Reset() override
        {
            ReleaseSceneTexture();
        }

        bool IsActive() const override
        {
            return pDevice != nullptr;
        }

        Size GetSize() const override
        {
            Size size = targetSize;

            ID3D10RenderTargetView* pTarget = nullptr;
            ID3D10Resource* pResource = nullptr;

            if (GetTarget(&pTarget, &pResource) && pResource)
            {
                D3D10_TEXTURE2D_DESC desc{};
                ((ID3D10Texture2D*)pResource)->GetDesc(&desc);
                size = { (int32_t)desc.Width, (int32_t)desc.Height };
            }

            if (pTarget)
                pTarget->Release();

            if (pResource)
                pResource->Release();

            const_cast<D3D10Backend*>(this)->targetSize = size;
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

            D3D10_TEXTURE2D_DESC desc{};
            desc.Width = textureWidth;
            desc.Height = textureHeight;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D10_USAGE_IMMUTABLE;
            desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;

            D3D10_SUBRESOURCE_DATA data{};
            data.pSysMem = pixels;
            data.SysMemPitch = (UINT)textureWidth * 4;

            ID3D10Texture2D* pTexture = nullptr;
            if (FAILED(pDevice->CreateTexture2D(&desc, pixels ? &data : nullptr, &pTexture)))
                return nullptr;

            ID3D10ShaderResourceView* pView = nullptr;
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
                ((ID3D10Texture2D*)pTexture->resource)->Release();

            if (pTexture->extra)
                ((ID3D10ShaderResourceView*)pTexture->extra)->Release();

            delete pTexture;
        }

        void SetTarget(RenderTarget* pTarget) override
        {
            pTargetOverride = pTarget;
        }

        void SetTargetState(TargetState state) override
        {
            // Direct3D 10 has no resource states
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

        bool Prepare(int maxVertices) override
        {
            (void)maxVertices;
            if (!pDevice || !EnsureDeviceObjects()) return false;
            ID3D10RenderTargetView* target = nullptr;
            ID3D10Resource* resource = nullptr;
            bool ready = GetTarget(&target, &resource) && resource;
            if (ready)
            {
                D3D10_TEXTURE2D_DESC desc{};
                ((ID3D10Texture2D*)resource)->GetDesc(&desc);
                ready = EnsureSceneTexture(desc);
            }
            if (resource) resource->Release();
            if (target) target->Release();
            return ready;
        }

        void Render(const Vertex* pVertices, int numVertices, PrimitiveType primitive) override
        {
            if (!pDevice || !pVertices || numVertices <= 0)
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


            ID3D10RenderTargetView* pTarget = nullptr;
            ID3D10Resource* pResource = nullptr;

            if (!GetTarget(&pTarget, &pResource) || !pResource)
            {
                if (pTarget)
                    pTarget->Release();

                return;
            }

            D3D10_TEXTURE2D_DESC desc{};
            ((ID3D10Texture2D*)pResource)->GetDesc(&desc);
            targetSize = { (int32_t)desc.Width, (int32_t)desc.Height };

            if (!EnsureSceneTexture(desc))
            {
                pResource->Release();
                pTarget->Release();
                return;
            }


            // The state of the application has to be taken before anything of it
            // is changed: the shader resource slots are emptied for the copy of
            // the scene below, and a state that is put back afterwards with empty
            // slots in it leaves the next draw of the application without the
            // textures its pixel shader reads.
            SavedState state{};
            CaptureState(state);

            // Our own scene texture has to be out of the shader resource slots
            // while it is written to, and the vertex shader reads it as well, see
            // VSMain.
            ID3D10ShaderResourceView* pNullViews[2] = {};
            pDevice->PSSetShaderResources(0, 2, pNullViews);
            pDevice->VSSetShaderResources(0, 1, pNullViews);

            if (desc.SampleDesc.Count > 1)
                pDevice->ResolveSubresource(pSceneTexture, 0, pResource, 0, desc.Format);
            else
                pDevice->CopyResource(pSceneTexture, pResource);

            pResource->Release();


            ApplyState(desc, pTarget);


            void* pVertexData = nullptr;
            if (SUCCEEDED(pVertexBuffer->Map(D3D10_MAP_WRITE_DISCARD, 0, &pVertexData)))
            {
                Vertex* pDest = (Vertex*)pVertexData;

                for (int i = 0; i < numVertices; i++)
                {
                    pDest[i] = pVertices[i];
                    pDest[i].color = (pDest[i].color & 0xFF00FF00u)
                        | ((pDest[i].color & 0x00FF0000u) >> 16)
                        | ((pDest[i].color & 0x000000FFu) << 16);
                }
                pVertexBuffer->Unmap();

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

                pDevice->UpdateSubresource(pConstantBuffer, 0, nullptr, &constants, 0, 0);

                const UINT stride = sizeof(Vertex);
                const UINT offset = 0;
                pDevice->IASetVertexBuffers(0, 1, &pVertexBuffer, &stride, &offset);
                pDevice->IASetIndexBuffer(pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
                pDevice->IASetPrimitiveTopology(primitive == PRIMITIVE_TRIANGLES ? D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST : D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
                pDevice->IASetInputLayout(pInputLayout);

                pDevice->VSSetShader(pVertexShader);
                pDevice->PSSetShader(pPixelShader);
                pDevice->VSSetConstantBuffers(0, 1, &pConstantBuffer);
                pDevice->PSSetConstantBuffers(0, 1, &pConstantBuffer);

                ID3D10ShaderResourceView* pViews[2] = { pSceneTextureView, nullptr };
                if (pMaskTexture && pMaskTexture->extra)
                    pViews[1] = (ID3D10ShaderResourceView*)pMaskTexture->extra;

                pDevice->PSSetShaderResources(0, 2, pViews);
                pDevice->PSSetSamplers(0, 1, &pSampler);

                // The vertex shader gathers the light around a drop out of the
                // scene texture, which is slot 0 of its resources and the same
                // sampler the pixel shader uses.
                pDevice->VSSetShaderResources(0, 1, pViews);
                pDevice->VSSetSamplers(0, 1, &pSampler);

                if (primitive == PRIMITIVE_TRIANGLES)
                    pDevice->DrawIndexed(numIndices, 0, 0);
                else
                    pDevice->Draw(numVertices, 0);


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
            ID3D10RenderTargetView* pRenderTarget = nullptr;
            ID3D10DepthStencilView* pDepthStencil = nullptr;
            ID3D10BlendState* pBlendState = nullptr;
            float blendFactor[4] = {};
            UINT sampleMask = 0;
            ID3D10DepthStencilState* pDepthStencilState = nullptr;
            UINT stencilRef = 0;
            ID3D10RasterizerState* pRasterizerState = nullptr;
            D3D10_RECT scissors[D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
            UINT scissorCount = 0;
            D3D10_VIEWPORT viewports[D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
            UINT viewportCount = 0;
            ID3D10VertexShader* pVertexShader = nullptr;
            ID3D10PixelShader* pPixelShader = nullptr;
            ID3D10InputLayout* pInputLayout = nullptr;
            ID3D10Buffer* pVertexBuffer = nullptr;
            UINT vertexStride = 0;
            UINT vertexOffset = 0;
            ID3D10Buffer* pIndexBuffer = nullptr;
            DXGI_FORMAT indexFormat = DXGI_FORMAT_UNKNOWN;
            UINT indexOffset = 0;
            D3D10_PRIMITIVE_TOPOLOGY topology = D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            ID3D10Buffer* pVSConstants = nullptr;
            ID3D10Buffer* pPSConstants = nullptr;
            ID3D10ShaderResourceView* pViews[2] = {};
            ID3D10SamplerState* pSampler = nullptr;
            ID3D10ShaderResourceView* pVSView = nullptr;
            ID3D10SamplerState* pVSSampler = nullptr;
        };

        bool GetTarget(ID3D10RenderTargetView** ppTarget, ID3D10Resource** ppResource) const
        {
            *ppTarget = nullptr;
            *ppResource = nullptr;

            if (pTargetOverride && pTargetOverride->resource)
            {
                *ppTarget = (ID3D10RenderTargetView*)pTargetOverride->resource;
                (*ppTarget)->AddRef();
            }
            else if (pDevice)
            {
                pDevice->OMGetRenderTargets(1, ppTarget, nullptr);
            }

            if (!*ppTarget)
                return false;

            (*ppTarget)->GetResource(ppResource);
            return *ppResource != nullptr;
        }

        bool EnsureDeviceObjects()
        {
            if (pVertexShader)
                return true;

            auto pVertexBlob = LoadShaderBytecode(IDR_DROP10VS);
            auto pPixelBlob = LoadShaderBytecode(IDR_DROP10PS);

            if (!pVertexBlob || !pPixelBlob)
                return false;

            bool bResult = SUCCEEDED(pDevice->CreateVertexShader(pVertexBlob.GetBufferPointer(), pVertexBlob.GetBufferSize(), &pVertexShader));

            D3D10_INPUT_ELEMENT_DESC layout[] =
            {
                { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, x),     D3D10_INPUT_PER_VERTEX_DATA, 0 },
                // Direct3D 10 refuses a B8G8R8A8 input layout, Direct3D 11 takes it,
                // so the colour is declared as RGBA and swapped while uploading
                { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM,  0, offsetof(Vertex, color), D3D10_INPUT_PER_VERTEX_DATA, 0 },
                { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(Vertex, u0),    D3D10_INPUT_PER_VERTEX_DATA, 0 },
                { "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(Vertex, u1),    D3D10_INPUT_PER_VERTEX_DATA, 0 },
            };

            bResult = bResult && SUCCEEDED(pDevice->CreateInputLayout(layout, ARRAYSIZE(layout), pVertexBlob.GetBufferPointer(), pVertexBlob.GetBufferSize(), &pInputLayout));
            bResult = bResult && SUCCEEDED(pDevice->CreatePixelShader(pPixelBlob.GetBufferPointer(), pPixelBlob.GetBufferSize(), &pPixelShader));


            if (!bResult)
                return false;

            D3D10_BUFFER_DESC bufferDesc{};
            bufferDesc.ByteWidth = MaxVertices * sizeof(Vertex);
            bufferDesc.Usage = D3D10_USAGE_DYNAMIC;
            bufferDesc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
            bufferDesc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;

                        if (FAILED(pDevice->CreateBuffer(&bufferDesc, nullptr, &pVertexBuffer)))
                return false;

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

            D3D10_BUFFER_DESC indexDesc{};
            indexDesc.ByteWidth = (UINT)(indices.size() * sizeof(uint16_t));
            indexDesc.Usage = D3D10_USAGE_IMMUTABLE;
            indexDesc.BindFlags = D3D10_BIND_INDEX_BUFFER;

            D3D10_SUBRESOURCE_DATA indexData{};
            indexData.pSysMem = indices.data();

            if (FAILED(pDevice->CreateBuffer(&indexDesc, &indexData, &pIndexBuffer)))
                return false;

            D3D10_BUFFER_DESC constantDesc{};
            constantDesc.ByteWidth = sizeof(Constants);
            constantDesc.Usage = D3D10_USAGE_DEFAULT;
            constantDesc.BindFlags = D3D10_BIND_CONSTANT_BUFFER;

            if (FAILED(pDevice->CreateBuffer(&constantDesc, nullptr, &pConstantBuffer)))
                return false;

            D3D10_SAMPLER_DESC samplerDesc{};
            samplerDesc.Filter = D3D10_FILTER_MIN_MAG_MIP_LINEAR;
            samplerDesc.AddressU = D3D10_TEXTURE_ADDRESS_CLAMP;
            samplerDesc.AddressV = D3D10_TEXTURE_ADDRESS_CLAMP;
            samplerDesc.AddressW = D3D10_TEXTURE_ADDRESS_CLAMP;
            samplerDesc.ComparisonFunc = D3D10_COMPARISON_NEVER;
            samplerDesc.MaxLOD = D3D10_FLOAT32_MAX;

            if (FAILED(pDevice->CreateSamplerState(&samplerDesc, &pSampler)))
                return false;

            D3D10_BLEND_DESC blendDesc{};
            blendDesc.BlendEnable[0] = TRUE;
            blendDesc.SrcBlend = D3D10_BLEND_SRC_ALPHA;
            blendDesc.DestBlend = D3D10_BLEND_INV_SRC_ALPHA;
            blendDesc.BlendOp = D3D10_BLEND_OP_ADD;
            blendDesc.SrcBlendAlpha = D3D10_BLEND_SRC_ALPHA;
            blendDesc.DestBlendAlpha = D3D10_BLEND_INV_SRC_ALPHA;
            blendDesc.BlendOpAlpha = D3D10_BLEND_OP_ADD;
            blendDesc.RenderTargetWriteMask[0] = D3D10_COLOR_WRITE_ENABLE_ALL;

            if (FAILED(pDevice->CreateBlendState(&blendDesc, &pBlendState)))
                return false;

            D3D10_DEPTH_STENCIL_DESC depthDesc{};
            depthDesc.DepthEnable = FALSE;
            depthDesc.DepthWriteMask = D3D10_DEPTH_WRITE_MASK_ZERO;
            depthDesc.StencilEnable = FALSE;

            if (FAILED(pDevice->CreateDepthStencilState(&depthDesc, &pDepthStencilState)))
                return false;

            D3D10_RASTERIZER_DESC rasterizerDesc{};
            rasterizerDesc.FillMode = D3D10_FILL_SOLID;
            rasterizerDesc.CullMode = D3D10_CULL_NONE;
            rasterizerDesc.DepthClipEnable = FALSE;
            rasterizerDesc.ScissorEnable = FALSE;

            HRESULT hrrs = pDevice->CreateRasterizerState(&rasterizerDesc, &pRasterizerState);
            return SUCCEEDED(hrrs);
        }

        bool EnsureSceneTexture(const D3D10_TEXTURE2D_DESC& target)
        {
            if (pSceneTexture && sceneFormat == target.Format && sceneWidth == target.Width && sceneHeight == target.Height)
                return true;

            ReleaseSceneTexture();

            D3D10_TEXTURE2D_DESC desc = target;
            desc.Usage = D3D10_USAGE_DEFAULT;
            desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
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

            if (FAILED(pDevice->CreateShaderResourceView(pSceneTexture, nullptr, &pSceneTextureView)))
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

        void ReleaseResources()
        {
            ReleaseSceneTexture();

            if (pVertexShader) { pVertexShader->Release(); pVertexShader = nullptr; }
            if (pPixelShader) { pPixelShader->Release(); pPixelShader = nullptr; }
            if (pInputLayout) { pInputLayout->Release(); pInputLayout = nullptr; }
            if (pVertexBuffer) { pVertexBuffer->Release(); pVertexBuffer = nullptr; }
            if (pIndexBuffer) { pIndexBuffer->Release(); pIndexBuffer = nullptr; }
            if (pConstantBuffer) { pConstantBuffer->Release(); pConstantBuffer = nullptr; }
            if (pSampler) { pSampler->Release(); pSampler = nullptr; }
            if (pBlendState) { pBlendState->Release(); pBlendState = nullptr; }
            if (pDepthStencilState) { pDepthStencilState->Release(); pDepthStencilState = nullptr; }
            if (pRasterizerState) { pRasterizerState->Release(); pRasterizerState = nullptr; }
        }

        void CaptureState(SavedState& state)
        {
            pDevice->OMGetRenderTargets(1, &state.pRenderTarget, &state.pDepthStencil);
            pDevice->OMGetBlendState(&state.pBlendState, state.blendFactor, &state.sampleMask);
            pDevice->OMGetDepthStencilState(&state.pDepthStencilState, &state.stencilRef);
            pDevice->RSGetState(&state.pRasterizerState);
            pDevice->RSGetScissorRects(&state.scissorCount, nullptr);
            if (state.scissorCount > D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)
                state.scissorCount = D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
            pDevice->RSGetScissorRects(&state.scissorCount, state.scissors);
            pDevice->RSGetViewports(&state.viewportCount, nullptr);
            if (state.viewportCount > D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)
                state.viewportCount = D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
            pDevice->RSGetViewports(&state.viewportCount, state.viewports);

            pDevice->VSGetShader(&state.pVertexShader);
            pDevice->PSGetShader(&state.pPixelShader);
            pDevice->IAGetInputLayout(&state.pInputLayout);
            pDevice->IAGetVertexBuffers(0, 1, &state.pVertexBuffer, &state.vertexStride, &state.vertexOffset);
            pDevice->IAGetIndexBuffer(&state.pIndexBuffer, &state.indexFormat, &state.indexOffset);
            pDevice->IAGetPrimitiveTopology(&state.topology);
            pDevice->VSGetConstantBuffers(0, 1, &state.pVSConstants);
            pDevice->PSGetConstantBuffers(0, 1, &state.pPSConstants);
            pDevice->PSGetShaderResources(0, 2, state.pViews);
            pDevice->PSGetSamplers(0, 1, &state.pSampler);
            pDevice->VSGetShaderResources(0, 1, &state.pVSView);
            pDevice->VSGetSamplers(0, 1, &state.pVSSampler);
        }

        void ApplyState(const D3D10_TEXTURE2D_DESC& desc, ID3D10RenderTargetView* pTarget)
        {
            viewport.TopLeftX = 0;
            viewport.TopLeftY = 0;
            viewport.Width = desc.Width;
            viewport.Height = desc.Height;
            viewport.MinDepth = 0.0f;
            viewport.MaxDepth = 1.0f;

            pDevice->OMSetRenderTargets(1, &pTarget, nullptr);
            pDevice->OMSetBlendState(pBlendState, nullptr, 0xFFFFFFFF);
            pDevice->OMSetDepthStencilState(pDepthStencilState, 0);
            pDevice->RSSetState(pRasterizerState);
            pDevice->RSSetViewports(1, &viewport);
        }

        void RestoreState(SavedState& state)
        {
            ID3D10RenderTargetView* pTargets[1] = { state.pRenderTarget };
            pDevice->OMSetRenderTargets(state.pRenderTarget ? 1 : 0, pTargets, state.pDepthStencil);
            pDevice->OMSetBlendState(state.pBlendState, state.blendFactor, state.sampleMask);
            pDevice->OMSetDepthStencilState(state.pDepthStencilState, state.stencilRef);

            if (state.pRasterizerState)
                pDevice->RSSetState(state.pRasterizerState);

            if (state.viewportCount)
                pDevice->RSSetViewports(state.viewportCount, state.viewports);

            if (state.scissorCount)
                pDevice->RSSetScissorRects(state.scissorCount, state.scissors);

            pDevice->VSSetShader(state.pVertexShader);
            pDevice->PSSetShader(state.pPixelShader);
            pDevice->IASetInputLayout(state.pInputLayout);
            pDevice->IASetVertexBuffers(0, 1, &state.pVertexBuffer, &state.vertexStride, &state.vertexOffset);
            pDevice->IASetIndexBuffer(state.pIndexBuffer, state.indexFormat, state.indexOffset);
            pDevice->IASetPrimitiveTopology(state.topology);
            pDevice->VSSetConstantBuffers(0, 1, &state.pVSConstants);
            pDevice->PSSetConstantBuffers(0, 1, &state.pPSConstants);
            pDevice->PSSetShaderResources(0, 2, state.pViews);
            pDevice->PSSetSamplers(0, 1, &state.pSampler);
            pDevice->VSSetShaderResources(0, 1, &state.pVSView);
            pDevice->VSSetSamplers(0, 1, &state.pVSSampler);

            if (state.pRenderTarget) state.pRenderTarget->Release();
            if (state.pDepthStencil) state.pDepthStencil->Release();
            if (state.pBlendState) state.pBlendState->Release();
            if (state.pDepthStencilState) state.pDepthStencilState->Release();
            if (state.pRasterizerState) state.pRasterizerState->Release();
            if (state.pVertexShader) state.pVertexShader->Release();
            if (state.pPixelShader) state.pPixelShader->Release();
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

        ID3D10Device* pDevice = nullptr;

        ID3D10VertexShader* pVertexShader = nullptr;
        ID3D10PixelShader* pPixelShader = nullptr;
        ID3D10InputLayout* pInputLayout = nullptr;
        ID3D10Buffer* pVertexBuffer = nullptr;
        ID3D10Buffer* pIndexBuffer = nullptr;
        ID3D10Buffer* pConstantBuffer = nullptr;
        ID3D10SamplerState* pSampler = nullptr;
        ID3D10BlendState* pBlendState = nullptr;
        ID3D10DepthStencilState* pDepthStencilState = nullptr;
        ID3D10RasterizerState* pRasterizerState = nullptr;

        ID3D10Texture2D* pSceneTexture = nullptr;
        ID3D10ShaderResourceView* pSceneTextureView = nullptr;
        DXGI_FORMAT sceneFormat = DXGI_FORMAT_UNKNOWN;
        UINT sceneWidth = 0;
        UINT sceneHeight = 0;

        D3D10_VIEWPORT viewport{};

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

    namespace D3D10Factory
    {
        inline Detail::Register registrar10(RENDERER_D3D10, []() -> Backend* { return new D3D10Backend(); });
        inline Detail::Register registrar101(RENDERER_D3D10_1, []() -> Backend* { return new D3D10Backend(); });
    }
}
