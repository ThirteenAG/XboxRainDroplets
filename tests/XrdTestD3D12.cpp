// ---------------------------------------------------------------------------
// Test application for the Direct3D 12 backend.
//
// Direct3D 12 has no implicit render target, so this is the case where the
// effect has to be pointed at one: the application hands the swap chain over
// and the backend uses the back buffer of the moment. Handing over the swap
// chain works exactly like it does on Direct3D 11, which is the point of the
// exercise.
// ---------------------------------------------------------------------------

#define XRD_ENABLE_D3D12 1
#include "xrd/xrd.h"

#include "XrdTest.h"
#include "ShaderCompiler.h"
#include "shaders/generated/tests.h"

#include <d3d12.h>

namespace
{
    struct SimpleVertex
    {
        float x, y, z;
        uint32_t color;
    };

    ID3D12Device* pDevice = nullptr;
    ID3D12InfoQueue* pInfoQueue = nullptr;
    ID3D12CommandQueue* pQueue = nullptr;
    IDXGISwapChain3* pSwapChain = nullptr;
    ID3D12DescriptorHeap* pRtvHeap = nullptr;
    ID3D12Resource* pBackBuffers[2] = {};
    UINT rtvSize = 0;

    ID3D12RootSignature* pRootSignature = nullptr;
    ID3D12PipelineState* pPipelineState = nullptr;
    ID3D12Resource* pVertexBuffer = nullptr;
    ID3D12Resource* pIndexBuffer = nullptr;
    void* pVertexMapped = nullptr;
    ID3D12DescriptorHeap* pConstantHeap = nullptr;
    ID3D12Resource* pConstantBuffer = nullptr;
    void* pConstantMapped = nullptr;

    ID3D12CommandAllocator* pAllocators[2] = {};
    ID3D12GraphicsCommandList* pCommandList = nullptr;
    ID3D12Fence* pFence = nullptr;
    HANDLE hFenceEvent = nullptr;
    UINT64 fenceValues[2] = {};
    UINT64 fenceValue = 0;

    XrdTest::Window window;
    XrdTest::Camera camera;
    XrdTest::Ui ui;

    float deltaTime = 1.0f / 60.0f;
    int uiSelection = 0;
    constexpr int MaxVertices = 8192;
    constexpr int MaxIndices = MaxVertices / 4 * 6;

    struct RootConstants
    {
        float projection[16];
    };

    ID3D12Resource* CreateUploadBuffer(UINT64 size, const void* data, void** ppMapped)
    {
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = size;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.SampleDesc.Count = 1;

        ID3D12Resource* pResource = nullptr;
        if (FAILED(pDevice->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, __uuidof(ID3D12Resource), (void**)&pResource)))
            return nullptr;

        void* pMapped = nullptr;
        if (SUCCEEDED(pResource->Map(0, nullptr, &pMapped)))
        {
            if (data)
                memcpy(pMapped, data, (size_t)size);

            if (ppMapped)
                *ppMapped = pMapped;
            else
                pResource->Unmap(0, nullptr);
        }

        return pResource;
    }

    bool InitializeDevice()
    {

        IDXGIFactory4* pFactory = nullptr;
        if (FAILED(CreateDXGIFactory2(0, __uuidof(IDXGIFactory4), (void**)&pFactory)))
            return false;

        // the debug layer explains why a pipeline state or a root signature is
        // rejected, which is worth the trouble while this backend is new
        ID3D12Debug* pDebug = nullptr;
        if (SUCCEEDED(D3D12GetDebugInterface(__uuidof(ID3D12Debug), (void**)&pDebug)) && pDebug)
        {
            pDebug->EnableDebugLayer();
            pDebug->Release();
        }

        // the first adapter is not necessarily one Direct3D 12 can use, so every
        // one of them is tried, with the software device as the last resort
        IDXGIAdapter1* pAdapter = nullptr;
        for (UINT i = 0; ; i++)
        {
            pAdapter = nullptr;

            if (pFactory->EnumAdapters1(i, &pAdapter) == DXGI_ERROR_NOT_FOUND || !pAdapter)
                break;

            DXGI_ADAPTER_DESC1 desc{};
            pAdapter->GetDesc1(&desc);


            if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) &&
                SUCCEEDED(D3D12CreateDevice(pAdapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), (void**)&pDevice)))
            {
                pAdapter->Release();
                pAdapter = nullptr;
                break;
            }

            pAdapter->Release();
            pAdapter = nullptr;
        }

        if (!pDevice)
        {
            if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), (void**)&pDevice)))
            {
                if (pAdapter)
                    pAdapter->Release();

                pFactory->Release();
                return false;
            }
        }

        if (pAdapter)
        {
            pAdapter->Release();
            pAdapter = nullptr;
        }


        D3D12_COMMAND_QUEUE_DESC queueDesc{};
        if (FAILED(pDevice->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue), (void**)&pQueue)))
        {
            pFactory->Release();
            return false;
        }

        DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
        swapChainDesc.BufferCount = 2;
        swapChainDesc.Width = window.width;
        swapChainDesc.Height = window.height;
        swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

        IDXGISwapChain1* pSwapChain1 = nullptr;
        if (FAILED(pFactory->CreateSwapChainForHwnd(pQueue, window.hwnd, &swapChainDesc, nullptr, nullptr, &pSwapChain1)))
        {
            pFactory->Release();
            return false;
        }

        pSwapChain1->QueryInterface(__uuidof(IDXGISwapChain3), (void**)&pSwapChain);
        pSwapChain1->Release();

        pFactory->Release();


        if (!pSwapChain)
            return false;

        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
        rtvHeapDesc.NumDescriptors = 2;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

        if (FAILED(pDevice->CreateDescriptorHeap(&rtvHeapDesc, __uuidof(ID3D12DescriptorHeap), (void**)&pRtvHeap)))
            return false;

        rtvSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        D3D12_CPU_DESCRIPTOR_HANDLE handle = pRtvHeap->GetCPUDescriptorHandleForHeapStart();
        for (int i = 0; i < 2; i++)
        {
            if (FAILED(pSwapChain->GetBuffer(i, __uuidof(ID3D12Resource), (void**)&pBackBuffers[i])))
                return false;

            pDevice->CreateRenderTargetView(pBackBuffers[i], nullptr, handle);
            handle.ptr += rtvSize;
        }

        // the application draws with one colour per vertex, the matrix arrives as
        // root constants so no descriptor table is needed for it
        D3D12_ROOT_PARAMETER parameters[2] = {};
        parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        parameters[0].Constants.ShaderRegister = 0;
        parameters[0].Constants.Num32BitValues = 16;
        parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        parameters[1].Descriptor.ShaderRegister = 1;
        parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        D3D12_ROOT_SIGNATURE_DESC rootDesc{};
        rootDesc.NumParameters = 2;
        rootDesc.pParameters = parameters;
        rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ID3DBlob* pSignature = nullptr;
        ID3DBlob* pError = nullptr;

        using SerializeFn = long(WINAPI*)(const D3D12_ROOT_SIGNATURE_DESC*, D3D_ROOT_SIGNATURE_VERSION, ID3DBlob**, ID3DBlob**);
        HMODULE hD3D12 = GetModuleHandleW(L"d3d12.dll");
        auto fnSerialize = hD3D12 ? (SerializeFn)GetProcAddress(hD3D12, "D3D12SerializeRootSignature") : nullptr;

        if (!fnSerialize || FAILED(fnSerialize(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pSignature, &pError)))
            return false;

        const bool bRootOk = SUCCEEDED(pDevice->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), __uuidof(ID3D12RootSignature), (void**)&pRootSignature));


        if (pError)
            pError->Release();

        pSignature->Release();

        if (!bRootOk)
            return false;

        ID3DBlob* pVertexBlob = Xrd::CompileShader(Xrd::Shaders::D3D11SimpleSource, "SimpleVS", "vs_5_0");
        ID3DBlob* pPixelBlob = Xrd::CompileShader(Xrd::Shaders::D3D11SimpleSource, "SimplePS", "ps_5_0");

        if (!pVertexBlob || !pPixelBlob)
            return false;

        D3D12_INPUT_ELEMENT_DESC layout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(SimpleVertex, x), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_B8G8R8A8_UNORM,  0, offsetof(SimpleVertex, color), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
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
        pipelineDesc.VS = { pVertexBlob->GetBufferPointer(), pVertexBlob->GetBufferSize() };
        pipelineDesc.PS = { pPixelBlob->GetBufferPointer(), pPixelBlob->GetBufferSize() };
        pipelineDesc.BlendState = blendDesc;
        pipelineDesc.SampleMask = UINT_MAX;
        pipelineDesc.RasterizerState = rasterizerDesc;
        pipelineDesc.DepthStencilState = depthDesc;
        pipelineDesc.InputLayout = { layout, ARRAYSIZE(layout) };
        pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pipelineDesc.NumRenderTargets = 1;
        pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        pipelineDesc.SampleDesc.Count = 1;
        pipelineDesc.SampleDesc.Quality = 0;

        const HRESULT hrPipeline = pDevice->CreateGraphicsPipelineState(&pipelineDesc, __uuidof(ID3D12PipelineState), (void**)&pPipelineState);
        const bool bPipelineOk = SUCCEEDED(hrPipeline);

        // the debug layer explains what it does not like, worth keeping for a
        // test application
        pDevice->QueryInterface(__uuidof(ID3D12InfoQueue), (void**)&pInfoQueue);

        pVertexBlob->Release();
        pPixelBlob->Release();

        if (!bPipelineOk)
            return false;

        pVertexBuffer = CreateUploadBuffer(MaxVertices * sizeof(SimpleVertex), nullptr, &pVertexMapped);

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

        pIndexBuffer = CreateUploadBuffer(indices.size() * sizeof(uint16_t), indices.data(), nullptr);
        pConstantBuffer = CreateUploadBuffer(sizeof(RootConstants), nullptr, &pConstantMapped);

        for (int i = 0; i < 2; i++)
            if (FAILED(pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void**)&pAllocators[i])))
                return false;

        if (FAILED(pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, pAllocators[0], nullptr, __uuidof(ID3D12GraphicsCommandList), (void**)&pCommandList)))
            return false;

        pCommandList->Close();

        // a constant buffer view still has to exist for the second root parameter
        D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc{};
        cbvHeapDesc.NumDescriptors = 1;
        cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        pDevice->CreateDescriptorHeap(&cbvHeapDesc, __uuidof(ID3D12DescriptorHeap), (void**)&pConstantHeap);

        if (FAILED(pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void**)&pFence)))
            return false;

        hFenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);

        if (!pVertexBuffer || !pIndexBuffer || !pConstantBuffer || !hFenceEvent)
            return false;

        return true;
    }

    void WaitForFrame(int index)
    {
        if (fenceValues[index] != 0 && pFence->GetCompletedValue() < fenceValues[index])
        {
            pFence->SetEventOnCompletion(fenceValues[index], hFenceEvent);
            WaitForSingleObject(hFenceEvent, INFINITE);
        }
    }

    void DumpDebugLayer(const char* where)
    {
        if (!pInfoQueue)
            return;

        const UINT64 count = pInfoQueue->GetNumStoredMessages();

        for (UINT64 i = 0; i < count; i++)
        {
            SIZE_T length = 0;
            if (FAILED(pInfoQueue->GetMessage(i, nullptr, &length)) || length == 0)
                continue;

            std::vector<char> buffer(length);
            D3D12_MESSAGE* pMessage = (D3D12_MESSAGE*)buffer.data();
            if (SUCCEEDED(pInfoQueue->GetMessage(i, pMessage, &length)) && pMessage->pDescription)
            {
                printf("[%s] [%d] %s\n", where, (int)pMessage->Severity, pMessage->pDescription);
                fflush(stdout);
            }
        }

        pInfoQueue->ClearStoredMessages();
    }

    void TransitionTarget(ID3D12CommandAllocator* pAllocator, ID3D12Resource* pTarget, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
    {
        pAllocator->Reset();
        pCommandList->Reset(pAllocator, nullptr);

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = pTarget;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = before;
        barrier.Transition.StateAfter = after;
        pCommandList->ResourceBarrier(1, &barrier);

        pCommandList->Close();

        ID3D12CommandList* pLists[] = { pCommandList };
        pQueue->ExecuteCommandLists(1, pLists);
    }

    void DrawRects(const XrdTest::Rect* pRects, int count, ID3D12CommandAllocator* pAllocator, ID3D12Resource* pTarget)
    {
        if (count <= 0 || count * 6 > MaxVertices)
            return;

        SimpleVertex* pVertices = (SimpleVertex*)pVertexMapped;
        int n = 0;

        for (int i = 0; i < count; i++)
        {
            const XrdTest::Rect& rect = pRects[i];
            const uint32_t color = Xrd::ColorFloat(rect.color.r, rect.color.g, rect.color.b, rect.color.a);

            const SimpleVertex quad[4] =
            {
                { rect.x,              rect.y,               0.0f, color },
                { rect.x + rect.width, rect.y,               0.0f, color },
                { rect.x + rect.width, rect.y + rect.height, 0.0f, color },
                { rect.x,              rect.y + rect.height, 0.0f, color },
            };

            // the index buffer of this application expects four vertices per
            // rectangle, the same layout the renderer uses
            for (int v = 0; v < 4; v++)
                pVertices[n++] = quad[v];
        }

        RootConstants constants{};
        Xrd::Matrix ortho = Xrd::Matrix::OrthographicOffCenter(0.0f, (float)window.width, (float)window.height, 0.0f, 0.0f, 1.0f);
        memcpy(constants.projection, ortho.m, sizeof(constants.projection));
        memcpy(pConstantMapped, &constants, sizeof(constants));

        pAllocator->Reset();
        pCommandList->Reset(pAllocator, pPipelineState);

        pCommandList->SetGraphicsRootSignature(pRootSignature);
        pCommandList->SetGraphicsRoot32BitConstants(0, 16, constants.projection, 0);

        D3D12_CPU_DESCRIPTOR_HANDLE rtv = pRtvHeap->GetCPUDescriptorHandleForHeapStart();
        rtv.ptr += (SIZE_T)(pSwapChain->GetCurrentBackBufferIndex()) * rtvSize;
        pCommandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

        D3D12_VIEWPORT viewport{};
        viewport.Width = (float)window.width;
        viewport.Height = (float)window.height;
        viewport.MaxDepth = 1.0f;
        pCommandList->RSSetViewports(1, &viewport);

        D3D12_RECT scissor{};
        scissor.right = window.width;
        scissor.bottom = window.height;
        pCommandList->RSSetScissorRects(1, &scissor);

        D3D12_VERTEX_BUFFER_VIEW vertexView{};
        vertexView.BufferLocation = pVertexBuffer->GetGPUVirtualAddress();
        vertexView.SizeInBytes = MaxVertices * sizeof(SimpleVertex);
        vertexView.StrideInBytes = sizeof(SimpleVertex);
        pCommandList->IASetVertexBuffers(0, 1, &vertexView);

        D3D12_INDEX_BUFFER_VIEW indexView{};
        indexView.BufferLocation = pIndexBuffer->GetGPUVirtualAddress();
        indexView.SizeInBytes = MaxIndices * sizeof(uint16_t);
        indexView.Format = DXGI_FORMAT_R16_UINT;
        pCommandList->IASetIndexBuffer(&indexView);
        pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        pCommandList->DrawIndexedInstanced(count * 6, 1, 0, 0, 0);

        (void)pTarget;

        pCommandList->Close();

        ID3D12CommandList* pLists[] = { pCommandList };
        pQueue->ExecuteCommandLists(1, pLists);
    }

    void ApplyCameraToDrops()
    {
        const float cosPitch = cosf(camera.pitch);

        WaterDrops::right = { cosf(camera.yaw), 0.0f, -sinf(camera.yaw) };
        WaterDrops::up = { -sinf(camera.yaw) * sinf(camera.pitch), cosPitch, -cosf(camera.yaw) * sinf(camera.pitch) };
        WaterDrops::at = { sinf(camera.yaw) * cosPitch, sinf(camera.pitch), cosf(camera.yaw) * cosPitch };
        WaterDrops::pos = { camera.x, camera.y, camera.z };
    }
}

int main()
{
    // --headless: a run for a build server, see XrdTest::Headless
    XrdTest::Headless::ParseCommandLine("d3d12");

    if (XrdTest::Headless::Active())
        camera.autoYawSpeed = 0.0f;	// the pictures of the check are compared to each other

    if (!window.Create(L"Xbox Rain Droplets - Direct3D 12", 1280, 720))
    {
        return 1;
    }

    if (!InitializeDevice())
        return XrdTest::Headless::DeviceFailed();


    // the swap chain is all the effect needs, exactly like on Direct3D 11, but
    // Direct3D 12 has to be told what the target is being used for: here the
    // drops are drawn in the middle of the frame, so everything the application
    // draws afterwards, the UI, covers them
    // Direct3D 12 needs to be told two things the other APIs find out on their
    // own: which queue to submit on, so the drops land after whatever the
    // application already submitted, and where the target is in its life, here
    // in the middle of the frame so the UI covers them.
    Xrd::Init(Xrd::RENDERER_D3D12, pDevice);
    Xrd::SetCommandQueue(pQueue);
    Xrd::SetTargetState(Xrd::TARGET_STATE_RENDER_TARGET);

    WaterDrops::fTimeStep = &deltaTime;
    // the games read this from the weather, a test wants a lot of rain
    WaterDrops::ms_rainIntensity = 4.0f;

    bool active[4] = { true, false, true, false };
    ui.Build(XrdTest::g_uiLabels, 4, active, (float)window.width);

    auto previous = std::chrono::high_resolution_clock::now();
    auto lastReport = previous;
    int frames = 0;
    int frameIndex = 0;		// headless: how many frames the run has drawn
    char extra[128]{};

    while (window.running)
    {
        window.Pump();

        const auto now = std::chrono::high_resolution_clock::now();
        deltaTime = std::chrono::duration<float>(now - previous).count();
        previous = now;

        if (deltaTime > 0.1f)
            deltaTime = 0.1f;

        if (XrdTest::Headless::Active())
            deltaTime = XrdTest::Headless::DeltaTime();

        camera.Update(window, deltaTime);

        if (window.clicked)
        {
            const int hit = ui.HitTest(window.mouseX, window.mouseY);

            if (hit >= 0)
            {
                uiSelection = hit;

                switch (hit)
                {
                case 0: active[0] = true;  active[1] = false; WaterDrops::SetSnow(false); break;
                case 1: active[0] = false; active[1] = true;  WaterDrops::SetSnow(true); break;
                case 2: active[2] = !active[2]; WaterDrops::bGravity = active[2]; break;
                case 3: active[3] = !active[3]; WaterDrops::isPaused = active[3]; break;
                }

                ui.Build(XrdTest::g_uiLabels, 4, active, (float)window.width);
            }

            window.clicked = false;
        }

        ApplyCameraToDrops();

        const UINT index = pSwapChain->GetCurrentBackBufferIndex();
        WaitForFrame(index);

        ID3D12Resource* pTarget = pBackBuffers[index];
        ID3D12CommandAllocator* pAllocator = pAllocators[index];

        Xrd::RenderTarget dropTarget{};
        dropTarget.resource = pTarget;
        dropTarget.state = Xrd::TARGET_STATE_RENDER_TARGET;
        Xrd::SetTarget(&dropTarget);

        TransitionTarget(pAllocator, pTarget, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

        // the scene the drops are drawn over
        std::vector<XrdTest::Rect> rects;
        rects.push_back({ 0.0f, 0.0f, (float)window.width, (float)window.height * 0.62f, { 0.35f, 0.45f, 0.65f, 1.0f } });
        rects.push_back({ 0.0f, (float)window.height * 0.62f, (float)window.width, (float)window.height * 0.38f, { 0.18f, 0.20f, 0.16f, 1.0f } });

        for (int i = 0; i < 48; i++)
        {
            const float offset = fmodf((float)i * 137.0f - camera.yaw * 900.0f, (float)window.width + 240.0f);
            const float x = offset - 120.0f;
            const float heightFraction = 0.18f + 0.32f * (float)((i * 37) % 100) / 100.0f;
            const float width = 40.0f + (float)((i * 53) % 60);
            const float height = (float)window.height * 0.62f * heightFraction;

            rects.push_back({ x, (float)window.height * 0.62f - height, width, height,
                { 0.10f + 0.35f * (float)((i * 17) % 100) / 100.0f, 0.12f, 0.22f + 0.3f * (float)((i * 29) % 100) / 100.0f, 1.0f } });
        }

        DrawRects(rects.data(), (int)rects.size(), pAllocator, pTarget);

        // the drops, into the back buffer of the moment
        XrdTest::Headless::PrepareDrops(frameIndex, window.width, window.height);

        WaterDrops::Process();
        WaterDrops::Render();


        // and the UI on top of them
        DrawRects(ui.rects.data(), (int)ui.rects.size(), pAllocator, pTarget);

        TransitionTarget(pAllocator, pTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

        pSwapChain->Present(0, 0);
        fenceValues[index] = ++fenceValue;
        pQueue->Signal(pFence, fenceValue);

        // A game runs at 50 to 60 frames a second and the effect measures its
        // time in those frames, so the application waits for the rest of the
        // frame. Without this it would run at thousands of frames a second and
        // every drop would age in a fraction of a second.
        const float frameTime = std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - now).count();

        if (frameTime < 1.0f / 60.0f)
            Sleep((DWORD)((1.0f / 60.0f - frameTime) * 1000.0f));

        // a headless run takes the pictures of its last few frames and is over
        if (XrdTest::Headless::AfterPresent(window.hwnd, frameIndex))
            return XrdTest::Headless::Result();

        frameIndex++;
        frames++;

        if (std::chrono::duration<float>(now - lastReport).count() >= 1.0f)
        {
            sprintf_s(extra, "camera %.1f %.1f %.1f", camera.x, camera.y, camera.z);
            XrdTest::PrintStatus("d3d12", WaterDrops::ms_numDrops, frames, WaterDrops::bEnableSnow, extra);

            wchar_t title[160]{};
            swprintf_s(title, L"Xbox Rain Droplets - Direct3D 12  |  drops %d  fps %d", WaterDrops::ms_numDrops, frames);
            SetWindowTextW(window.hwnd, title);

            frames = 0;
            lastReport = now;
        }
    }

    WaterDrops::Shutdown();
    Xrd::Shutdown();

    return 0;
}
