#pragma once
// ---------------------------------------------------------------------------
// Direct3D 12 backend.
//
// Direct3D 12 has no notion of "the render target that is bound right now", so
// this backend is the one that has to be told where to draw: either by handing
// it the swap chain, in which case the back buffer of the moment is used, or by
// setting a target explicitly with Xrd::SetTarget, which is what a game hook
// that runs before the UI does.
//
// The frame is copied out of the target and the drops are drawn back into it,
// exactly like on the other APIs. The copies and the layout of the image are
// expressed with resource barriers, the command list is submitted to the queue
// the swap chain belongs to, and a fence makes sure a command allocator is only
// reused once the GPU is done with it.
// ---------------------------------------------------------------------------

#include "xrdrender.h"
#include "xrdshaderbytecode.h"

#include <d3d12.h>
#include <dxgi1_4.h>

namespace Xrd
{
    class D3D12Backend : public Backend
    {
    public:
        ~D3D12Backend() override
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
                // the swap chain is the only way to the back buffer of the frame
                // that is being presented
                if (!pSwapChain3)
                    pSwapChain->QueryInterface(__uuidof(IDXGISwapChain3), (void**)&pSwapChain3);

                if (!pCommandQueue)
                {
                    ID3D12CommandQueue* pQueue = nullptr;
                    if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D12CommandQueue), (void**)&pQueue)))
                        pCommandQueue = pQueue;
                }

                if (!pDevice && pCommandQueue)
                    pCommandQueue->GetDevice(__uuidof(ID3D12Device), (void**)&pDevice);

                if (!pDevice)
                    pSwapChain->GetDevice(__uuidof(ID3D12Device), (void**)&pDevice);

                pSwapChain->Release();
            }
            else if (SUCCEEDED(pUnknown->QueryInterface(__uuidof(ID3D12Device), (void**)&pDevice)))
            {
                // the device was handed over directly
            }
            else if (SUCCEEDED(pUnknown->QueryInterface(__uuidof(ID3D12CommandQueue), (void**)&pCommandQueue)))
            {
                // the queue was handed over, which is what the swap chain gives
            }

            if (!pDevice && pCommandQueue)
                pCommandQueue->GetDevice(__uuidof(ID3D12Device), (void**)&pDevice);

            if (!pDevice)
                return false;

            return CreateCommandObjects();
        }

        void Shutdown() override
        {
            WaitForGpu();
            ReleaseResources();

            if (pFence) { pFence->Release(); pFence = nullptr; }
            if (pFenceEvent) { CloseHandle(pFenceEvent); pFenceEvent = nullptr; }
            if (pCommandList) { pCommandList->Release(); pCommandList = nullptr; }

            for (auto& frame : frames)
            {
                if (frame.pAllocator)
                {
                    frame.pAllocator->Release();
                    frame.pAllocator = nullptr;
                }
            }

            if (pCommandQueue) { pCommandQueue->Release(); pCommandQueue = nullptr; }
            if (pSwapChain3) { pSwapChain3->Release(); pSwapChain3 = nullptr; }
            if (pDevice) { pDevice->Release(); pDevice = nullptr; }
        }

        void Reset() override
        {
            // Direct3D 12 never resets a device, a resize only invalidates the
            // resources that were sized after the back buffer
            ReleaseResources();
        }

        // The swap chain of the emulator is replaced when its window is (a
        // fullscreen switch does it) and the new one is handed over at the next
        // present, see D3D11Backend::UpdateNative.
        bool UpdateNative(void* pNative) override
        {
            if (!pNative)
                return true;

            if (!pDevice || !pCommandQueue)
                return false;

            IDXGISwapChain* pSwapChain = nullptr;
            if (FAILED(reinterpret_cast<IUnknown*>(pNative)->QueryInterface(__uuidof(IDXGISwapChain), (void**)&pSwapChain)) || !pSwapChain)
                return true;

            ID3D12Device* pNewDevice = nullptr;
            pSwapChain->GetDevice(__uuidof(ID3D12Device), (void**)&pNewDevice);

            if (pNewDevice && (pNewDevice != pDevice))
            {
                pNewDevice->Release();
                pSwapChain->Release();
                return false;
            }

            if (pNewDevice)
                pNewDevice->Release();

            IDXGISwapChain3* pNewSwapChain3 = nullptr;
            pSwapChain->QueryInterface(__uuidof(IDXGISwapChain3), (void**)&pNewSwapChain3);
            pSwapChain->Release();

            if (pNewSwapChain3 != pSwapChain3)
            {
                if (pSwapChain3)
                    pSwapChain3->Release();

                pSwapChain3 = pNewSwapChain3;

                // the resources were sized after the back buffer of the old chain
                ReleaseResources();
            }
            else if (pNewSwapChain3)
            {
                pNewSwapChain3->Release();
            }

            return true;
        }

        bool IsActive() const override
        {
            return pDevice != nullptr && pCommandQueue != nullptr;
        }

        Size GetSize() const override
        {
            Size size = targetSize;

            ID3D12Resource* pResource = GetTargetResource();
            if (pResource)
            {
                D3D12_RESOURCE_DESC desc = pResource->GetDesc();
                size = { (int32_t)desc.Width, (int32_t)desc.Height };
                pResource->Release();
            }

            const_cast<D3D12Backend*>(this)->targetSize = size;
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

            ID3D12Resource* pTexture = nullptr;
            if (!CreateUploadTexture((UINT)textureWidth, (UINT)textureHeight, DXGI_FORMAT_R8G8B8A8_UNORM, pixels, &pTexture))
                return nullptr;

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
                ((ID3D12Resource*)pTexture->resource)->Release();

            delete pTexture;
        }

        void SetTarget(RenderTarget* pTarget) override
        {
            pTargetOverride = pTarget;
        }

        void SetTargetState(TargetState state) override
        {
            targetState = state;
        }

        // Backend::SetCommandContext: the list the effect records its commands into.
        // A frame of Direct3D 12 is recorded on one thread and executed by another, and
        // a command of the effect issued on its own from the recording thread lands
        // anywhere in the stream of the game, so what is left is to record into the very
        // list of the frame, at the point the draw of the game reaches, see the effect.
        // The list is borrowed, the game owns it, and a null says the effect records and
        // submits a list of its own, which is what every other call does.
        void SetCommandContext(void* pNative) override
        {
            pInjectedCommandList = (ID3D12GraphicsCommandList*)pNative;
        }

        // The queue of the game cannot be read back from the swap chain, DXGI
        // does not hand it out, so it is passed in from the outside.
        void SetCommandQueue(void* pQueue) override
        {
            if (pCommandQueue)
            {
                pCommandQueue->Release();
                pCommandQueue = nullptr;
            }

            pCommandQueue = (ID3D12CommandQueue*)pQueue;

            if (pCommandQueue)
            {
                pCommandQueue->AddRef();

                if (!pDevice)
                    pCommandQueue->GetDevice(__uuidof(ID3D12Device), (void**)&pDevice);
            }

            if (pDevice && !pCommandList)
                CreateCommandObjects();
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

        void SetSceneBlur(float blur, float atlasTiles, float refraction, float maskSize) override
        {
            sceneBlur = blur;
            sceneAtlasTiles = atlasTiles;
            sceneRefraction = refraction;
            sceneMaskSize = maskSize;
        }

        bool Prepare(int maxVertices) override
        {
            if (!IsActive()) return false;
            auto* target = GetTargetResource();
            if (!target) return false;
            const auto desc = target->GetDesc();
            const bool ready = EnsureDeviceObjects(desc.Format, PRIMITIVE_TRIANGLES) &&
                EnsureSceneTexture(desc) && EnsureVertexBuffer(maxVertices);
            target->Release();
            return ready;
        }

        void Render(const Vertex* pVertices, int numVertices, PrimitiveType primitive) override
        {
            if (!IsActive() || !pVertices || numVertices <= 0)
            {
                return;
            }

            const int numIndices = (primitive == PRIMITIVE_TRIANGLES) ? (numVertices / 4) * 6 : 0;

            if (primitive == PRIMITIVE_TRIANGLES && numIndices <= 0)
            {
                return;
            }

            ID3D12Resource* pTarget = GetTargetResource();
            if (!pTarget)
            {
                return;
            }

            const D3D12_RESOURCE_DESC targetDesc = pTarget->GetDesc();
            targetSize = { (int32_t)targetDesc.Width, (int32_t)targetDesc.Height };

            if (!EnsureDeviceObjects(targetDesc.Format, primitive) || !EnsureSceneTexture(targetDesc) || !EnsureVertexBuffer(numVertices))
            {
                pTarget->Release();
                return;
            }

            // The commands are recorded into the list of the frame itself when one was
            // given, and are submitted with it, which is what the drops of a frame are
            // placed with. Nothing of the list is reset or closed here, and no fence of
            // the effect is waited on, since the list does not belong to it.
            if (pInjectedCommandList)
            {
                ID3D12GraphicsCommandList* pOwnList = pCommandList;
                pCommandList = pInjectedCommandList;

                RecordCommands(pTarget, targetDesc, pVertices, numVertices, numIndices, primitive);

                pCommandList = pOwnList;
                pTarget->Release();
                return;
            }

            Frame& frame = frames[frameIndex];
            if (frame.fenceValue != 0 && pFence->GetCompletedValue() < frame.fenceValue)
            {
                pFence->SetEventOnCompletion(frame.fenceValue, pFenceEvent);
                WaitForSingleObject(pFenceEvent, INFINITE);
            }

            if (FAILED(frame.pAllocator->Reset()) || FAILED(pCommandList->Reset(frame.pAllocator, nullptr)))
            {
                pTarget->Release();
                return;
            }

            RecordCommands(pTarget, targetDesc, pVertices, numVertices, numIndices, primitive);

            pCommandList->Close();

            ID3D12CommandList* pLists[] = { pCommandList };
            pCommandQueue->ExecuteCommandLists(1, pLists);

            frame.fenceValue = ++fenceValue;
            pCommandQueue->Signal(pFence, frame.fenceValue);

            frameIndex = (frameIndex + 1) % FramesInFlight;

            pTarget->Release();
        }

    private:
        static constexpr int FramesInFlight = 2;
        static constexpr int MaxVertices = 64000;
        static constexpr int MaxIndices = (MaxVertices / 4) * 6;
        static constexpr int DescriptorsPerFrame = 4; // constant buffer and three textures
        // the most levels the chain of the copy of the frame has, see EnsureSceneTexture
        static constexpr UINT MaxMipLevels = 16;

        struct Constants
        {
            float projection[16];
            float uvOffset[4];
            float uvScale[4];
            float sceneComplement[4];
            // Direct3D 12 requires a constant buffer to be a multiple of 256
            // bytes, anything smaller is rejected by the device
            float padding[36];
        };

        static_assert(sizeof(Constants) == 256, "a Direct3D 12 constant buffer has to be 256 bytes aligned");

        struct Frame
        {
            ID3D12CommandAllocator* pAllocator = nullptr;
            UINT64 fenceValue = 0;
        };

        struct TargetView
        {
            ID3D12Resource* pResource = nullptr;
            D3D12_CPU_DESCRIPTOR_HANDLE handle{};
        };

        bool CreateCommandObjects()
        {
            D3D12_COMMAND_QUEUE_DESC queueDesc{};
            if (!pCommandQueue && FAILED(pDevice->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue), (void**)&pCommandQueue)))
                return false;

            for (auto& frame : frames)
            {
                if (FAILED(pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void**)&frame.pAllocator)))
                    return false;
            }

            // a command list needs an allocator, it only has to be closed before
            // it can be reset with another one
            if (FAILED(pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, frames[0].pAllocator, nullptr, __uuidof(ID3D12GraphicsCommandList), (void**)&pCommandList)))
                return false;

            pCommandList->Close();

            if (FAILED(pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void**)&pFence)))
                return false;

            pFenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            return pFenceEvent != nullptr;
        }

        bool CreateUploadTexture(UINT width, UINT height, DXGI_FORMAT format, const uint8_t* pixels, ID3D12Resource** ppTexture)
        {
            const UINT64 rowPitch = (UINT64)width * 4;
            const UINT64 bufferSize = rowPitch * height;

            D3D12_HEAP_PROPERTIES heapProperties{};
            heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

            D3D12_RESOURCE_DESC bufferDesc{};
            bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            bufferDesc.Width = bufferSize;
            bufferDesc.Height = 1;
            bufferDesc.DepthOrArraySize = 1;
            bufferDesc.MipLevels = 1;
            bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
            bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            bufferDesc.SampleDesc.Count = 1;

            ID3D12Resource* pUpload = nullptr;
            if (FAILED(pDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, __uuidof(ID3D12Resource), (void**)&pUpload)))
                return false;

            if (pixels)
            {
                void* pData = nullptr;
                if (SUCCEEDED(pUpload->Map(0, nullptr, &pData)))
                {
                    for (UINT y = 0; y < height; y++)
                        memcpy((uint8_t*)pData + y * rowPitch, pixels + (size_t)y * rowPitch, (size_t)rowPitch);

                    pUpload->Unmap(0, nullptr);
                }
            }

            D3D12_HEAP_PROPERTIES defaultProperties{};
            defaultProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

            D3D12_RESOURCE_DESC textureDesc{};
            textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            textureDesc.Width = width;
            textureDesc.Height = height;
            textureDesc.DepthOrArraySize = 1;
            textureDesc.MipLevels = 1;
            textureDesc.Format = format;
            textureDesc.SampleDesc.Count = 1;

            ID3D12Resource* pTexture = nullptr;
            if (FAILED(pDevice->CreateCommittedResource(&defaultProperties, D3D12_HEAP_FLAG_NONE, &textureDesc,
                D3D12_RESOURCE_STATE_COPY_DEST, nullptr, __uuidof(ID3D12Resource), (void**)&pTexture)))
            {
                pUpload->Release();
                return false;
            }

            D3D12_TEXTURE_COPY_LOCATION destination{};
            destination.pResource = pTexture;
            destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

            D3D12_TEXTURE_COPY_LOCATION source{};
            source.pResource = pUpload;
            source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            source.PlacedFootprint.Footprint.Format = format;
            source.PlacedFootprint.Footprint.Width = width;
            source.PlacedFootprint.Footprint.Height = height;
            source.PlacedFootprint.Footprint.Depth = 1;
            source.PlacedFootprint.Footprint.RowPitch = (UINT)rowPitch;

            // a one off command allocator, used before anything else is going on
            ID3D12CommandAllocator* pAllocator = nullptr;
            ID3D12GraphicsCommandList* pList = nullptr;
            bool bResult = false;

            if (SUCCEEDED(pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void**)&pAllocator)) &&
                SUCCEEDED(pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, pAllocator, nullptr, __uuidof(ID3D12GraphicsCommandList), (void**)&pList)))
            {
                pList->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);

                D3D12_RESOURCE_BARRIER barrier{};
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier.Transition.pResource = pTexture;
                barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
                barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
                pList->ResourceBarrier(1, &barrier);

                pList->Close();

                ID3D12CommandList* pLists[] = { pList };
                pCommandQueue->ExecuteCommandLists(1, pLists);

                frames[frameIndex].fenceValue = ++fenceValue;
                pCommandQueue->Signal(pFence, frames[frameIndex].fenceValue);
                pFence->SetEventOnCompletion(frames[frameIndex].fenceValue, pFenceEvent);
                WaitForSingleObject(pFenceEvent, INFINITE);

                bResult = true;
            }

            if (pList)
                pList->Release();

            if (pAllocator)
                pAllocator->Release();

            pUpload->Release();

            if (!bResult)
            {
                pTexture->Release();
                return false;
            }

            *ppTexture = pTexture;
            return true;
        }

        ID3D12Resource* GetTargetResource() const
        {
            if (pTargetOverride && pTargetOverride->resource)
            {
                auto* pResource = (ID3D12Resource*)pTargetOverride->resource;
                pResource->AddRef();
                return pResource;
            }

            if (!pSwapChain3 && !pTargetOverride)
                return nullptr;

            UINT index = pSwapChain3 ? pSwapChain3->GetCurrentBackBufferIndex() : 0;

            ID3D12Resource* pResource = nullptr;
            if (pSwapChain3 && SUCCEEDED(pSwapChain3->GetBuffer(index, __uuidof(ID3D12Resource), (void**)&pResource)) && pResource)
                return pResource;

            return nullptr;
        }

        bool EnsureDeviceObjects(DXGI_FORMAT targetFormat, PrimitiveType primitive)
        {
            (void)primitive;

            if (pPipelineState && pRootSignature && pipelineFormat == targetFormat)
                return true;

            ReleasePipeline();

            auto pVertexBlob = LoadShaderBytecode(IDR_DROP12VS);
            auto pPixelBlob = LoadShaderBytecode(IDR_DROP12PS);

            if (!pVertexBlob || !pPixelBlob)
                return false;

            D3D12_ROOT_PARAMETER parameters[2] = {};

            D3D12_DESCRIPTOR_RANGE ranges[2] = {};
            ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
            ranges[0].NumDescriptors = 1;
            ranges[0].BaseShaderRegister = 0;
            ranges[0].OffsetInDescriptorsFromTableStart = 0;
            ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            ranges[1].NumDescriptors = 2;
            ranges[1].BaseShaderRegister = 0;
            ranges[1].OffsetInDescriptorsFromTableStart = 1;

            parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            parameters[0].DescriptorTable.NumDescriptorRanges = 2;
            parameters[0].DescriptorTable.pDescriptorRanges = ranges;
            parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

            D3D12_STATIC_SAMPLER_DESC samplers[2] = {};
            for (int i = 0; i < 2; i++)
            {
                samplers[i].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
                samplers[i].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
                samplers[i].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
                samplers[i].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
                samplers[i].MaxLOD = D3D12_FLOAT32_MAX;
                samplers[i].ShaderRegister = (UINT)i;
                // The vertex shader samples the scene texture as well, to gather
                // the light around a drop, see VSMain in source/shaders/d3d10/drops.hlsl.
                samplers[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            }

            D3D12_ROOT_SIGNATURE_DESC rootDesc{};
            rootDesc.NumParameters = 1;
            rootDesc.pParameters = parameters;
            rootDesc.NumStaticSamplers = 2;
            rootDesc.pStaticSamplers = samplers;
            rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

            ID3DBlob* pSignatureBlob = nullptr;
            ID3DBlob* pErrorBlob = nullptr;

            using SerializeFn = long(WINAPI*)(const D3D12_ROOT_SIGNATURE_DESC*, D3D_ROOT_SIGNATURE_VERSION, ID3DBlob**, ID3DBlob**);
            using CreateRootSignatureFn = long(WINAPI*)(ID3D12Device*, UINT, const void*, SIZE_T, REFIID, void**);

            HMODULE hD3D12 = GetModuleHandleW(L"d3d12.dll");
            auto fnSerialize = hD3D12 ? (SerializeFn)GetProcAddress(hD3D12, "D3D12SerializeRootSignature") : nullptr;

            bool bResult = false;

            if (fnSerialize && SUCCEEDED(fnSerialize(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pSignatureBlob, &pErrorBlob)) && pSignatureBlob)
            {
                bResult = SUCCEEDED(pDevice->CreateRootSignature(0, pSignatureBlob->GetBufferPointer(), pSignatureBlob->GetBufferSize(), __uuidof(ID3D12RootSignature), (void**)&pRootSignature));
            }

            if (pErrorBlob)
                pErrorBlob->Release();

            if (pSignatureBlob)
                pSignatureBlob->Release();

            if (!bResult)
            {
                return false;
            }

            D3D12_INPUT_ELEMENT_DESC layout[] =
            {
                { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, x),     D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "COLOR",    0, DXGI_FORMAT_B8G8R8A8_UNORM,  0, offsetof(Vertex, color), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(Vertex, u0),    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(Vertex, u1),    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            };

            D3D12_BLEND_DESC blendDesc{};
            blendDesc.RenderTarget[0].BlendEnable = TRUE;
            blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
            blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
            blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
            blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
            blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
            blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
            blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

            D3D12_RASTERIZER_DESC rasterizerDesc{};
            rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
            rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
            rasterizerDesc.DepthClipEnable = FALSE;

            D3D12_DEPTH_STENCIL_DESC depthDesc{};
            depthDesc.DepthEnable = FALSE;
            depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
            depthDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
            depthDesc.StencilEnable = FALSE;
            depthDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
            depthDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
            depthDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
            depthDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
            depthDesc.BackFace = depthDesc.FrontFace;

            D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
            pipelineDesc.pRootSignature = pRootSignature;
            pipelineDesc.VS = { pVertexBlob.GetBufferPointer(), pVertexBlob.GetBufferSize() };
            pipelineDesc.PS = { pPixelBlob.GetBufferPointer(), pPixelBlob.GetBufferSize() };
            pipelineDesc.BlendState = blendDesc;
            pipelineDesc.SampleMask = UINT_MAX;
            pipelineDesc.RasterizerState = rasterizerDesc;
            pipelineDesc.DepthStencilState = depthDesc;
            pipelineDesc.InputLayout = { layout, ARRAYSIZE(layout) };
            pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            pipelineDesc.NumRenderTargets = 1;
            pipelineDesc.RTVFormats[0] = targetFormat;
            pipelineDesc.SampleDesc.Count = 1;
            pipelineDesc.SampleDesc.Quality = 0;

            bResult = SUCCEEDED(pDevice->CreateGraphicsPipelineState(&pipelineDesc, __uuidof(ID3D12PipelineState), (void**)&pPipelineState));


            if (bResult)
                pipelineFormat = targetFormat;

            return bResult;
        }

        // The concrete format a view of a texture of this format is made with, see
        // the Direct3D 11 backend: a typeless texture has no format of its own to
        // be read or drawn as.
        static DXGI_FORMAT ConcreteFormat(DXGI_FORMAT format)
        {
            switch (format)
            {
            case DXGI_FORMAT_R32G32B32A32_TYPELESS: return DXGI_FORMAT_R32G32B32A32_FLOAT;
            case DXGI_FORMAT_R16G16B16A16_TYPELESS: return DXGI_FORMAT_R16G16B16A16_FLOAT;
            case DXGI_FORMAT_R10G10B10A2_TYPELESS:  return DXGI_FORMAT_R10G10B10A2_UNORM;
            case DXGI_FORMAT_R8G8B8A8_TYPELESS:     return DXGI_FORMAT_R8G8B8A8_UNORM;
            case DXGI_FORMAT_B8G8R8A8_TYPELESS:     return DXGI_FORMAT_B8G8R8A8_UNORM;
            case DXGI_FORMAT_B8G8R8X8_TYPELESS:     return DXGI_FORMAT_B8G8R8X8_UNORM;
            default:
                break;
            }

            return format;
        }

        bool EnsureSceneTexture(const D3D12_RESOURCE_DESC& target)
        {
            if (pSceneTexture && sceneFormat == target.Format && sceneWidth == target.Width && sceneHeight == target.Height)
                return true;

            ReleaseSceneTexture();

            D3D12_HEAP_PROPERTIES heapProperties{};
            heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

            D3D12_RESOURCE_DESC desc = target;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.SampleDesc.Count = 1;
            desc.SampleDesc.Quality = 0;
            desc.Flags = D3D12_RESOURCE_FLAG_NONE;

            // The copy carries a chain of mip levels. A drop shows a window of the
            // frame far larger than itself, and read from the top level alone every
            // pixel of the drop is one pixel of the frame picked out of hundreds,
            // which is what the noise that crawled over the drops was: the sampler
            // reads the level whose texels are the size of the pixels of the drop
            // instead, which is the window averaged. Direct3D 12 has no call that
            // makes a chain, so every level of it is drawn out of the level above
            // after every copy, see GenerateSceneMips, which needs the copy to be a
            // render target and its format to be one that can be drawn into and
            // sampled with a filter.
            const DXGI_FORMAT viewFormat = ConcreteFormat(target.Format);

            D3D12_FEATURE_DATA_FORMAT_SUPPORT support{};
            support.Format = viewFormat;

            const D3D12_FORMAT_SUPPORT1 needed = D3D12_FORMAT_SUPPORT1_RENDER_TARGET | D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE | D3D12_FORMAT_SUPPORT1_MIP;
            sceneMips = SUCCEEDED(pDevice->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support))) &&
                (support.Support1 & needed) == needed && EnsureMipPipeline(viewFormat);

            sceneMipLevels = 1;

            if (sceneMips)
            {
                UINT64 largest = target.Width > target.Height ? target.Width : target.Height;

                while (largest > 1 && sceneMipLevels < MaxMipLevels)
                {
                    largest /= 2;
                    sceneMipLevels++;
                }

                desc.MipLevels = (UINT16)sceneMipLevels;
                desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
            }

            if (FAILED(pDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &desc,
                (D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
                nullptr, __uuidof(ID3D12Resource), (void**)&pSceneTexture)))
            {
                pSceneTexture = nullptr;

                // a chain the device would not make is not what keeps the drops off the frame
                if (!sceneMips)
                    return false;

                sceneMips = false;
                sceneMipLevels = 1;
                desc.MipLevels = 1;
                desc.Flags = D3D12_RESOURCE_FLAG_NONE;

                if (FAILED(pDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &desc,
                    (D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
                    nullptr, __uuidof(ID3D12Resource), (void**)&pSceneTexture)))
                {
                    pSceneTexture = nullptr;
                    return false;
                }
            }

            sceneFormat = target.Format;
            sceneWidth = target.Width;
            sceneHeight = target.Height;

            // one view of every level to read it by and one to draw it by, for the chain
            if (sceneMips && !EnsureMipViews(viewFormat))
            {
                // the chain is not made, the copy is read at its top level
                sceneMips = false;
            }

            return true;
        }

        // The root signature and the pipeline that draw a level of the chain out
        // of the level above it: one texture to read and a sampler that averages
        // four of its texels, see source/shaders/d3d10/mips.hlsl.
        bool EnsureMipPipeline(DXGI_FORMAT viewFormat)
        {
            if (pMipPipelineState && pMipRootSignature && mipPipelineFormat == viewFormat)
                return true;

            ReleaseMipPipeline();

            auto pVertexBlob = LoadShaderBytecode(IDR_MIP12VS);
            auto pPixelBlob = LoadShaderBytecode(IDR_MIP12PS);

            if (!pVertexBlob || !pPixelBlob)
                return false;

            D3D12_DESCRIPTOR_RANGE range{};
            range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            range.NumDescriptors = 1;
            range.BaseShaderRegister = 0;

            D3D12_ROOT_PARAMETER parameter{};
            parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            parameter.DescriptorTable.NumDescriptorRanges = 1;
            parameter.DescriptorTable.pDescriptorRanges = &range;
            parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

            D3D12_STATIC_SAMPLER_DESC sampler{};
            sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            sampler.MaxLOD = D3D12_FLOAT32_MAX;
            sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

            D3D12_ROOT_SIGNATURE_DESC rootDesc{};
            rootDesc.NumParameters = 1;
            rootDesc.pParameters = &parameter;
            rootDesc.NumStaticSamplers = 1;
            rootDesc.pStaticSamplers = &sampler;

            using SerializeFn = long(WINAPI*)(const D3D12_ROOT_SIGNATURE_DESC*, D3D_ROOT_SIGNATURE_VERSION, ID3DBlob**, ID3DBlob**);

            HMODULE hD3D12 = GetModuleHandleW(L"d3d12.dll");
            auto fnSerialize = hD3D12 ? (SerializeFn)GetProcAddress(hD3D12, "D3D12SerializeRootSignature") : nullptr;

            ID3DBlob* pSignatureBlob = nullptr;
            ID3DBlob* pErrorBlob = nullptr;
            bool bResult = false;

            if (fnSerialize && SUCCEEDED(fnSerialize(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pSignatureBlob, &pErrorBlob)) && pSignatureBlob)
                bResult = SUCCEEDED(pDevice->CreateRootSignature(0, pSignatureBlob->GetBufferPointer(), pSignatureBlob->GetBufferSize(), __uuidof(ID3D12RootSignature), (void**)&pMipRootSignature));

            if (pErrorBlob)
                pErrorBlob->Release();

            if (pSignatureBlob)
                pSignatureBlob->Release();

            if (!bResult)
                return false;

            D3D12_RASTERIZER_DESC rasterizerDesc{};
            rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
            rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
            rasterizerDesc.DepthClipEnable = FALSE;

            D3D12_BLEND_DESC blendDesc{};
            blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

            D3D12_DEPTH_STENCIL_DESC depthDesc{};
            depthDesc.DepthEnable = FALSE;
            depthDesc.StencilEnable = FALSE;

            D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
            pipelineDesc.pRootSignature = pMipRootSignature;
            pipelineDesc.VS = { pVertexBlob.GetBufferPointer(), pVertexBlob.GetBufferSize() };
            pipelineDesc.PS = { pPixelBlob.GetBufferPointer(), pPixelBlob.GetBufferSize() };
            pipelineDesc.BlendState = blendDesc;
            pipelineDesc.SampleMask = UINT_MAX;
            pipelineDesc.RasterizerState = rasterizerDesc;
            pipelineDesc.DepthStencilState = depthDesc;
            pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            pipelineDesc.NumRenderTargets = 1;
            pipelineDesc.RTVFormats[0] = viewFormat;
            pipelineDesc.SampleDesc.Count = 1;

            if (FAILED(pDevice->CreateGraphicsPipelineState(&pipelineDesc, __uuidof(ID3D12PipelineState), (void**)&pMipPipelineState)))
            {
                ReleaseMipPipeline();
                return false;
            }

            mipPipelineFormat = viewFormat;
            return true;
        }

        // A view of every level of the chain to read it by, in a heap the shader
        // can see, and one to draw it by.
        bool EnsureMipViews(DXGI_FORMAT viewFormat)
        {
            if (!pMipSrvHeap)
            {
                D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
                heapDesc.NumDescriptors = MaxMipLevels;
                heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
                heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

                if (FAILED(pDevice->CreateDescriptorHeap(&heapDesc, __uuidof(ID3D12DescriptorHeap), (void**)&pMipSrvHeap)))
                    return false;
            }

            if (!pMipRtvHeap)
            {
                D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
                heapDesc.NumDescriptors = MaxMipLevels;
                heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

                if (FAILED(pDevice->CreateDescriptorHeap(&heapDesc, __uuidof(ID3D12DescriptorHeap), (void**)&pMipRtvHeap)))
                    return false;
            }

            const UINT srvSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            const UINT rtvSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

            for (UINT level = 0; level < sceneMipLevels; level++)
            {
                D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
                srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                srvDesc.Format = viewFormat;
                srvDesc.Texture2D.MostDetailedMip = level;
                srvDesc.Texture2D.MipLevels = 1;

                D3D12_CPU_DESCRIPTOR_HANDLE srv = pMipSrvHeap->GetCPUDescriptorHandleForHeapStart();
                srv.ptr += (SIZE_T)level * srvSize;
                pDevice->CreateShaderResourceView(pSceneTexture, &srvDesc, srv);

                D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
                rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                rtvDesc.Format = viewFormat;
                rtvDesc.Texture2D.MipSlice = level;

                D3D12_CPU_DESCRIPTOR_HANDLE rtv = pMipRtvHeap->GetCPUDescriptorHandleForHeapStart();
                rtv.ptr += (SIZE_T)level * rtvSize;
                pDevice->CreateRenderTargetView(pSceneTexture, &rtvDesc, rtv);
            }

            return true;
        }

        // The chain of the copy of the frame, drawn level by level out of the level
        // above, once the top level holds the frame. Every level is a render target
        // while it is drawn and is read by the shaders otherwise, like the top one.
        void GenerateSceneMips()
        {
            if (!sceneMips || sceneMipLevels <= 1 || !pMipPipelineState || !pMipRootSignature || !pMipSrvHeap || !pMipRtvHeap)
                return;

            const UINT srvSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            const UINT rtvSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

            pCommandList->SetGraphicsRootSignature(pMipRootSignature);
            pCommandList->SetPipelineState(pMipPipelineState);

            ID3D12DescriptorHeap* pHeaps[] = { pMipSrvHeap };
            pCommandList->SetDescriptorHeaps(1, pHeaps);
            pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            UINT width = (UINT)sceneWidth;
            UINT height = (UINT)sceneHeight;

            for (UINT level = 1; level < sceneMipLevels; level++)
            {
                width = width > 1 ? width / 2 : 1;
                height = height > 1 ? height / 2 : 1;

                D3D12_RESOURCE_BARRIER barrier{};
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier.Transition.pResource = pSceneTexture;
                barrier.Transition.Subresource = level;
                barrier.Transition.StateBefore = (D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
                pCommandList->ResourceBarrier(1, &barrier);

                D3D12_CPU_DESCRIPTOR_HANDLE rtv = pMipRtvHeap->GetCPUDescriptorHandleForHeapStart();
                rtv.ptr += (SIZE_T)level * rtvSize;
                pCommandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

                D3D12_VIEWPORT viewport{};
                viewport.Width = (float)width;
                viewport.Height = (float)height;
                viewport.MaxDepth = 1.0f;
                pCommandList->RSSetViewports(1, &viewport);

                D3D12_RECT scissor{};
                scissor.right = (LONG)width;
                scissor.bottom = (LONG)height;
                pCommandList->RSSetScissorRects(1, &scissor);

                D3D12_GPU_DESCRIPTOR_HANDLE srv = pMipSrvHeap->GetGPUDescriptorHandleForHeapStart();
                srv.ptr += (UINT64)(level - 1) * srvSize;
                pCommandList->SetGraphicsRootDescriptorTable(0, srv);

                pCommandList->DrawInstanced(3, 1, 0, 0);

                barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
                barrier.Transition.StateAfter = (D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                pCommandList->ResourceBarrier(1, &barrier);
            }
        }

        void ReleaseMipPipeline()
        {
            if (pMipPipelineState) { pMipPipelineState->Release(); pMipPipelineState = nullptr; }
            if (pMipRootSignature) { pMipRootSignature->Release(); pMipRootSignature = nullptr; }
            mipPipelineFormat = DXGI_FORMAT_UNKNOWN;
        }

        bool EnsureVertexBuffer(int numVertices)
        {
            (void)numVertices;

            if (pVertexBufferUpload && pConstantBufferUpload && pDescriptorHeap)
                return true;

            ReleaseDeviceObjects();

            // one upload heap holds the vertices and the constant buffer, which
            // keeps the descriptor handling small. Both are given a slot for every
            // frame that can be in flight, since a frame recorded into a list of the
            // engine is executed with it and may still be reading its vertices while
            // the next one is recorded, see SetCommandContext.
            const UINT64 vertexSize = (UINT64)MaxVertices * sizeof(Vertex);
            const UINT64 constantSize = sizeof(Constants);

            D3D12_HEAP_PROPERTIES heapProperties{};
            heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

            D3D12_RESOURCE_DESC bufferDesc{};
            bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            bufferDesc.Width = vertexSize * FramesInFlight + constantSize * FramesInFlight;
            bufferDesc.Height = 1;
            bufferDesc.DepthOrArraySize = 1;
            bufferDesc.MipLevels = 1;
            bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
            bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            bufferDesc.SampleDesc.Count = 1;

            ID3D12Resource* pUpload = nullptr;
            if (FAILED(pDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, __uuidof(ID3D12Resource), (void**)&pUpload)))
                return false;

            pVertexBufferUpload = pUpload;
            pConstantBufferUpload = pUpload;

            if (FAILED(pUpload->Map(0, nullptr, (void**)&pMapped)))
            {
                ReleaseDeviceObjects();
                return false;
            }

            // the index buffer never changes
            D3D12_RESOURCE_DESC indexDesc = bufferDesc;
            indexDesc.Width = (UINT64)MaxIndices * sizeof(uint16_t);

            ID3D12Resource* pIndexUpload = nullptr;
            if (FAILED(pDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &indexDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, __uuidof(ID3D12Resource), (void**)&pIndexUpload)))
            {
                ReleaseDeviceObjects();
                return false;
            }

            pIndexBuffer = pIndexUpload;

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

            void* pIndexData = nullptr;
            if (SUCCEEDED(pIndexUpload->Map(0, nullptr, &pIndexData)))
            {
                memcpy(pIndexData, indices.data(), indices.size() * sizeof(uint16_t));
                pIndexUpload->Unmap(0, nullptr);
            }

            D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
            heapDesc.NumDescriptors = DescriptorsPerFrame * FramesInFlight;
            heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

            if (FAILED(pDevice->CreateDescriptorHeap(&heapDesc, __uuidof(ID3D12DescriptorHeap), (void**)&pDescriptorHeap)))
            {
                ReleaseDeviceObjects();
                return false;
            }

            descriptorSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

            // the render target views are kept apart, they are per back buffer
            D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
            rtvHeapDesc.NumDescriptors = 8;
            rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

            return SUCCEEDED(pDevice->CreateDescriptorHeap(&rtvHeapDesc, __uuidof(ID3D12DescriptorHeap), (void**)&pRtvHeap));
        }

        void UpdateDescriptorTable(ID3D12Resource* pTarget)
        {
            (void)pTarget;

            // the constant buffer of this frame
            D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = pDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
            D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = pDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

            const UINT offset = DescriptorsPerFrame * frameIndex;
            cpuHandle.ptr += (SIZE_T)offset * descriptorSize;
            gpuHandle.ptr += (UINT64)offset * descriptorSize;

            D3D12_CONSTANT_BUFFER_VIEW_DESC constantView{};
            constantView.BufferLocation = pConstantBufferUpload->GetGPUVirtualAddress() + (UINT64)MaxVertices * sizeof(Vertex) * FramesInFlight + (UINT64)frameIndex * sizeof(Constants);
            constantView.SizeInBytes = sizeof(Constants);
            pDevice->CreateConstantBufferView(&constantView, cpuHandle);

            D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = cpuHandle;
            srvHandle.ptr += descriptorSize;

            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = sceneMips ? (UINT)-1 : 1;

            srvDesc.Format = sceneFormat;
            pDevice->CreateShaderResourceView(pSceneTexture, &srvDesc, srvHandle);

            srvHandle.ptr += descriptorSize;
            srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            srvDesc.Texture2D.MipLevels = 1;

            if (pMaskTexture && pMaskTexture->resource)
                pDevice->CreateShaderResourceView((ID3D12Resource*)pMaskTexture->resource, &srvDesc, srvHandle);

            currentTableHandle = gpuHandle;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE GetRenderTargetView(ID3D12Resource* pResource)
        {
            for (auto& view : targetViews)
                if (view.pResource == pResource)
                    return view.handle;

            D3D12_CPU_DESCRIPTOR_HANDLE handle = pRtvHeap->GetCPUDescriptorHandleForHeapStart();
            handle.ptr += (SIZE_T)targetViewCount * pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

            D3D12_RENDER_TARGET_VIEW_DESC desc{};
            desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            desc.Format = sceneFormat;

            pDevice->CreateRenderTargetView(pResource, &desc, handle);

            targetViews[targetViewCount] = { pResource, handle };
            targetViewCount = (targetViewCount + 1) % 8;

            return handle;
        }

        void RecordCommands(ID3D12Resource* pTarget, const D3D12_RESOURCE_DESC& targetDesc,
            const Vertex* pVertices, int numVertices, int numIndices, PrimitiveType primitive)
        {
            UpdateDescriptorTable(pTarget);

            // What the target is being used for right now is the one thing
            // Direct3D 12 cannot find out on its own, the caller says it.
            const TargetState state = (pTargetOverride && pTargetOverride->state) ? pTargetOverride->state : targetState;
            const D3D12_RESOURCE_STATES stateBefore = (state == TARGET_STATE_PRESENT) ? D3D12_RESOURCE_STATE_PRESENT : D3D12_RESOURCE_STATE_RENDER_TARGET;

            D3D12_RESOURCE_BARRIER barriers[4] = {};

            // the frame that is on screen becomes the source of the copy
            barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barriers[0].Transition.pResource = pTarget;
            barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barriers[0].Transition.StateBefore = stateBefore;
            barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

            // only the top level of the copy is the frame, the chain below it is
            // drawn out of it afterwards, see GenerateSceneMips
            barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barriers[1].Transition.pResource = pSceneTexture;
            barriers[1].Transition.Subresource = 0;
            barriers[1].Transition.StateBefore = (D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

            pCommandList->ResourceBarrier(2, barriers);

            // A copy of the whole resource wants the same number of levels on both
            // sides, and the copy of the frame has a chain of them: the top level
            // is copied on its own.
            D3D12_TEXTURE_COPY_LOCATION destination{};
            destination.pResource = pSceneTexture;
            destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            destination.SubresourceIndex = 0;

            D3D12_TEXTURE_COPY_LOCATION source{};
            source.pResource = pTarget;
            source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            source.SubresourceIndex = 0;

            pCommandList->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);

            barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barriers[0].Transition.StateAfter = (D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            barriers[0].Transition.pResource = pSceneTexture;
            barriers[0].Transition.Subresource = 0;

            barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
            barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barriers[1].Transition.pResource = pTarget;
            barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            pCommandList->ResourceBarrier(2, barriers);

            // the chain of the copy, which is what a drop shows a window of the
            // frame far larger than itself smoothly with
            GenerateSceneMips();

            D3D12_CPU_DESCRIPTOR_HANDLE rtv = GetRenderTargetView(pTarget);
            pCommandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

            D3D12_VIEWPORT viewport{};
            viewport.Width = (float)targetDesc.Width;
            viewport.Height = (float)targetDesc.Height;
            viewport.MaxDepth = 1.0f;
            pCommandList->RSSetViewports(1, &viewport);

            D3D12_RECT scissor{};
            scissor.right = (LONG)targetDesc.Width;
            scissor.bottom = (LONG)targetDesc.Height;
            pCommandList->RSSetScissorRects(1, &scissor);

            pCommandList->SetGraphicsRootSignature(pRootSignature);
            pCommandList->SetPipelineState(pPipelineState);

            ID3D12DescriptorHeap* pHeaps[] = { pDescriptorHeap };
            pCommandList->SetDescriptorHeaps(1, pHeaps);
            pCommandList->SetGraphicsRootDescriptorTable(0, currentTableHandle);

            const UINT64 vertexOffset = (UINT64)frameIndex * MaxVertices * sizeof(Vertex);

            D3D12_VERTEX_BUFFER_VIEW vertexView{};
            vertexView.BufferLocation = pVertexBufferUpload->GetGPUVirtualAddress() + vertexOffset;
            vertexView.SizeInBytes = MaxVertices * sizeof(Vertex);
            vertexView.StrideInBytes = sizeof(Vertex);
            pCommandList->IASetVertexBuffers(0, 1, &vertexView);

            D3D12_INDEX_BUFFER_VIEW indexView{};
            indexView.BufferLocation = pIndexBuffer->GetGPUVirtualAddress();
            indexView.SizeInBytes = MaxIndices * sizeof(uint16_t);
            indexView.Format = DXGI_FORMAT_R16_UINT;
            pCommandList->IASetIndexBuffer(&indexView);
            pCommandList->IASetPrimitiveTopology(primitive == PRIMITIVE_TRIANGLES ? D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST : D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

            Constants constants{};
            if (projection == PROJECTION_SCREEN)
            {
                Matrix ortho = Matrix::OrthographicOffCenter(0.0f, (float)targetDesc.Width, (float)targetDesc.Height, 0.0f, 0.0f, 1.0f);
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
            constants.sceneComplement[2] = sceneBlur * 2.0f;
            constants.sceneComplement[3] = sceneRefraction;
            constants.uvScale[2] = sceneAtlasTiles;
            constants.uvScale[3] = sceneBlur;

            if (pMapped)
            {
                memcpy((uint8_t*)pMapped + vertexOffset, pVertices, (size_t)numVertices * sizeof(Vertex));
                memcpy((uint8_t*)pMapped + (UINT64)MaxVertices * sizeof(Vertex) * FramesInFlight + (size_t)frameIndex * sizeof(Constants), &constants, sizeof(Constants));
            }

            if (primitive == PRIMITIVE_TRIANGLES)
                pCommandList->DrawIndexedInstanced(numIndices, 1, 0, 0, 0);
            else
                pCommandList->DrawInstanced(numVertices, 1, 0, 0);

            if (stateBefore != D3D12_RESOURCE_STATE_RENDER_TARGET)
            {
                D3D12_RESOURCE_BARRIER finalBarrier{};
                finalBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                finalBarrier.Transition.pResource = pTarget;
                finalBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                finalBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
                finalBarrier.Transition.StateAfter = stateBefore;
                pCommandList->ResourceBarrier(1, &finalBarrier);
            }
        }

        void ReleaseSceneTexture()
        {
            if (pSceneTexture)
            {
                pSceneTexture->Release();
                pSceneTexture = nullptr;
            }

            if (pMipSrvHeap) { pMipSrvHeap->Release(); pMipSrvHeap = nullptr; }
            if (pMipRtvHeap) { pMipRtvHeap->Release(); pMipRtvHeap = nullptr; }

            sceneFormat = DXGI_FORMAT_UNKNOWN;
            sceneWidth = 0;
            sceneHeight = 0;
            sceneMips = false;
            sceneMipLevels = 1;
        }

        void ReleaseDeviceObjects()
        {
            if (pMapped && pVertexBufferUpload)
            {
                pVertexBufferUpload->Unmap(0, nullptr);
                pMapped = nullptr;
            }

            if (pIndexBuffer) { pIndexBuffer->Release(); pIndexBuffer = nullptr; }
            if (pVertexBufferUpload) { pVertexBufferUpload->Release(); pVertexBufferUpload = nullptr; }
            pConstantBufferUpload = nullptr;
            if (pDescriptorHeap) { pDescriptorHeap->Release(); pDescriptorHeap = nullptr; }
            if (pRtvHeap) { pRtvHeap->Release(); pRtvHeap = nullptr; }

            targetViewCount = 0;

            for (auto& view : targetViews)
                view = {};
        }

        void ReleasePipeline()
        {
            if (pPipelineState) { pPipelineState->Release(); pPipelineState = nullptr; }
            if (pRootSignature) { pRootSignature->Release(); pRootSignature = nullptr; }
            pipelineFormat = DXGI_FORMAT_UNKNOWN;
            ReleaseMipPipeline();
        }

        void ReleaseResources()
        {
            ReleaseSceneTexture();
            ReleaseDeviceObjects();
            ReleasePipeline();
        }

        void WaitForGpu()
        {
            if (!pCommandQueue || !pFence || !pFenceEvent)
                return;

            for (auto& frame : frames)
            {
                if (frame.fenceValue != 0)
                {
                    pCommandQueue->Signal(pFence, ++fenceValue);
                    pFence->SetEventOnCompletion(fenceValue, pFenceEvent);
                    WaitForSingleObject(pFenceEvent, INFINITE);
                    frame.fenceValue = 0;
                }
            }
        }

    private:
        ID3D12Device* pDevice = nullptr;
        ID3D12CommandQueue* pCommandQueue = nullptr;
        IDXGISwapChain3* pSwapChain3 = nullptr;
        ID3D12GraphicsCommandList* pCommandList = nullptr;
        ID3D12Fence* pFence = nullptr;
        HANDLE pFenceEvent = nullptr;
        UINT64 fenceValue = 0;
        Frame frames[FramesInFlight] = {};
        int frameIndex = 0;

        ID3D12RootSignature* pRootSignature = nullptr;
        ID3D12PipelineState* pPipelineState = nullptr;
        DXGI_FORMAT pipelineFormat = DXGI_FORMAT_UNKNOWN;

        ID3D12Resource* pSceneTexture = nullptr;
        DXGI_FORMAT sceneFormat = DXGI_FORMAT_UNKNOWN;
        UINT64 sceneWidth = 0;
        UINT64 sceneHeight = 0;
        // the chain of the copy of the frame, see EnsureSceneTexture and GenerateSceneMips
        bool sceneMips = false;
        UINT sceneMipLevels = 1;
        ID3D12RootSignature* pMipRootSignature = nullptr;
        ID3D12PipelineState* pMipPipelineState = nullptr;
        DXGI_FORMAT mipPipelineFormat = DXGI_FORMAT_UNKNOWN;
        ID3D12DescriptorHeap* pMipSrvHeap = nullptr;
        ID3D12DescriptorHeap* pMipRtvHeap = nullptr;

        ID3D12Resource* pVertexBufferUpload = nullptr;
        ID3D12Resource* pConstantBufferUpload = nullptr;
        ID3D12Resource* pIndexBuffer = nullptr;
        void* pMapped = nullptr;
        ID3D12DescriptorHeap* pDescriptorHeap = nullptr;
        ID3D12DescriptorHeap* pRtvHeap = nullptr;
        UINT descriptorSize = 0;
        D3D12_GPU_DESCRIPTOR_HANDLE currentTableHandle{};

        TargetView targetViews[8] = {};
        int targetViewCount = 0;

        Texture* pMaskTexture = nullptr;
        RenderTarget* pTargetOverride = nullptr;
        TargetState targetState = TARGET_STATE_PRESENT;

        // borrowed, the game owns it, see SetCommandContext
        ID3D12GraphicsCommandList* pInjectedCommandList = nullptr;

        Projection projection = PROJECTION_SCREEN;
        Matrix worldMatrix = Matrix::Identity();
        float width = 0.0f;
        float height = 0.0f;
        float uvOffsetX = 0.0f, uvScaleX = 1.0f;
        float uvOffsetY = 0.0f, uvScaleY = 1.0f;
        bool sceneComplement = false;
        // how blurred the drops are drawn and the shapes along a side of the atlas, see SetSceneBlur
        float sceneBlur = 0.0f;
        float sceneAtlasTiles = 2.0f;
        float sceneRefraction = 7.0f;
        float sceneMaskSize = 256.0f;
        bool sceneSampling = true;

        Size targetSize{};
    };

    namespace D3D12Factory
    {
        inline Detail::Register registrar(RENDERER_D3D12, []() -> Backend* { return new D3D12Backend(); });
    }
}
