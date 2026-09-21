// ---------------------------------------------------------------------------
// Test application for the Direct3D 10 backend.
//
// Same scene and the same UI as the other ones, only the little drawing code of
// the application itself differs. The drops do not: the effect and the renderer
// call are identical on every API.
//
// The drops are drawn after the world and before the UI, which is what puts
// them under the interface. Direct3D 10 hands them the render target that is
// bound at that moment without being asked.
// ---------------------------------------------------------------------------

#define XRD_ENABLE_D3D10 1
#include "xrd/xrd.h"

#include "XrdTest.h"

// the 10.1 header has to come before anything that pulls in d3d10.h
#include <d3d10_1.h>
#include <dxgi.h>

namespace
{
    struct SimpleVertex
    {
        float x, y, z;
        uint32_t color;
    };

    ID3D10Device* pDevice = nullptr;
    IDXGISwapChain* pSwapChain = nullptr;
    ID3D10RenderTargetView* pBackBufferView = nullptr;

    ID3D10VertexShader* pVertexShader = nullptr;
    ID3D10PixelShader* pPixelShader = nullptr;
    ID3D10InputLayout* pInputLayout = nullptr;
    ID3D10Buffer* pVertexBuffer = nullptr;
    ID3D10Buffer* pConstantBuffer = nullptr;
    ID3D10BlendState* pBlendState = nullptr;

    XrdTest::Window window;
    XrdTest::Camera camera;
    XrdTest::Ui ui;

    float deltaTime = 1.0f / 60.0f;
    int uiSelection = 0;
    constexpr int MaxVertices = 8192;

    struct SimpleConstants
    {
        float projection[16];
    };

    // Direct3D 10 rejects a B8G8R8A8 input layout, so the colours of the vertices
    // are stored as RGBA bytes, which means the red and the blue of the 0xAARRGGBB
    // of the effect have to trade places.
    uint32_t ColorToRgba(float r, float g, float b, float a)
    {
        const uint32_t c = Xrd::ColorFloat(r, g, b, a);
        return (c & 0xFF00FF00u) | ((c & 0x00FF0000u) >> 16) | ((c & 0x000000FFu) << 16);
    }

    bool InitializeDevice()
    {
        DXGI_SWAP_CHAIN_DESC swapChainDesc{};
        swapChainDesc.BufferCount = 2;
        swapChainDesc.BufferDesc.Width = window.width;
        swapChainDesc.BufferDesc.Height = window.height;
        swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.OutputWindow = window.hwnd;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.Windowed = TRUE;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        HRESULT hr = D3D10CreateDeviceAndSwapChain(nullptr, D3D10_DRIVER_TYPE_HARDWARE, nullptr, 0, D3D10_SDK_VERSION,
            &swapChainDesc, &pSwapChain, &pDevice);

        // A build server has no graphics card; the software rasterizer of the
        // system draws the scene there instead.
        if (FAILED(hr))
        {
            printf("[d3d10] no hardware device (%08x), falling back to WARP\n", (unsigned)hr);
            fflush(stdout);

            hr = D3D10CreateDeviceAndSwapChain(nullptr, D3D10_DRIVER_TYPE_WARP, nullptr, 0, D3D10_SDK_VERSION,
                &swapChainDesc, &pSwapChain, &pDevice);
        }

        if (FAILED(hr))
        {
            printf("[d3d10] device and swap chain failed: %08x\n", (unsigned)hr);
            fflush(stdout);
            return false;
        }

        ID3D10Texture2D* pBackBuffer = nullptr;
        HRESULT hrGet = pSwapChain->GetBuffer(0, __uuidof(ID3D10Texture2D), (void**)&pBackBuffer);
        if (FAILED(hrGet)) { printf("[d3d10] GetBuffer %08x\n", (unsigned)hrGet); fflush(stdout); return false; }

        HRESULT hrRtv = pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &pBackBufferView);
        printf("[d3d10] rtv %08x\n", (unsigned)hrRtv); fflush(stdout);
        const bool bTargetOk = SUCCEEDED(hrRtv);
        pBackBuffer->Release();

        if (!bTargetOk)
            return false;

        ID3DBlob* pVertexBlob = Xrd::CompileShader(Xrd::Shaders::D3D11SimpleSource, "SimpleVS", "vs_4_0");
        ID3DBlob* pPixelBlob = Xrd::CompileShader(Xrd::Shaders::D3D11SimpleSource, "SimplePS", "ps_4_0");

        if (!pVertexBlob || !pPixelBlob) { printf("[d3d10] compile failed\n"); fflush(stdout); return false; }

        D3D10_INPUT_ELEMENT_DESC layout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(SimpleVertex, x), D3D10_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM,  0, offsetof(SimpleVertex, color), D3D10_INPUT_PER_VERTEX_DATA, 0 },
        };

        HRESULT hrVs = pDevice->CreateVertexShader(pVertexBlob->GetBufferPointer(), pVertexBlob->GetBufferSize(), &pVertexShader);
        HRESULT hrIl = pDevice->CreateInputLayout(layout, ARRAYSIZE(layout), pVertexBlob->GetBufferPointer(), pVertexBlob->GetBufferSize(), &pInputLayout);
        HRESULT hrPs = pDevice->CreatePixelShader(pPixelBlob->GetBufferPointer(), pPixelBlob->GetBufferSize(), &pPixelShader);
        printf("[d3d10] vs %08x il %08x ps %08x\n", (unsigned)hrVs, (unsigned)hrIl, (unsigned)hrPs);
        fflush(stdout);

        bool bResult = SUCCEEDED(hrVs) && SUCCEEDED(hrIl) && SUCCEEDED(hrPs);

        pVertexBlob->Release();
        pPixelBlob->Release();

        if (!bResult) { printf("[d3d10] shaders or layout failed\n"); fflush(stdout); return false; }

        D3D10_BUFFER_DESC bufferDesc{};
        bufferDesc.ByteWidth = MaxVertices * sizeof(SimpleVertex);
        bufferDesc.Usage = D3D10_USAGE_DYNAMIC;
        bufferDesc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
        bufferDesc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
        HRESULT hrVb = pDevice->CreateBuffer(&bufferDesc, nullptr, &pVertexBuffer);
        printf("[d3d10] vb %08x\n", (unsigned)hrVb); fflush(stdout);
        bResult = bResult && SUCCEEDED(hrVb);

        D3D10_BUFFER_DESC constantDesc{};
        constantDesc.ByteWidth = sizeof(SimpleConstants);
        constantDesc.Usage = D3D10_USAGE_DYNAMIC;
        constantDesc.BindFlags = D3D10_BIND_CONSTANT_BUFFER;
        constantDesc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
        HRESULT hrCb = pDevice->CreateBuffer(&constantDesc, nullptr, &pConstantBuffer);
        printf("[d3d10] cb %08x\n", (unsigned)hrCb); fflush(stdout);
        bResult = bResult && SUCCEEDED(hrCb);

        D3D10_BLEND_DESC blendDesc{};
        blendDesc.BlendEnable[0] = TRUE;
        blendDesc.SrcBlend = D3D10_BLEND_SRC_ALPHA;
        blendDesc.DestBlend = D3D10_BLEND_INV_SRC_ALPHA;
        blendDesc.BlendOp = D3D10_BLEND_OP_ADD;
        blendDesc.SrcBlendAlpha = D3D10_BLEND_SRC_ALPHA;
        blendDesc.DestBlendAlpha = D3D10_BLEND_INV_SRC_ALPHA;
        blendDesc.BlendOpAlpha = D3D10_BLEND_OP_ADD;
        blendDesc.RenderTargetWriteMask[0] = D3D10_COLOR_WRITE_ENABLE_ALL;
        HRESULT hrBs = pDevice->CreateBlendState(&blendDesc, &pBlendState);
        printf("[d3d10] bs %08x bResult %d\n", (unsigned)hrBs, (int)bResult); fflush(stdout);
        bResult = bResult && SUCCEEDED(hrBs);

        return bResult;
    }

    void DrawRects(const XrdTest::Rect* pRects, int count)
    {
        if (count <= 0 || count * 6 > MaxVertices)
            return;

        SimpleVertex* pVertices = nullptr;
        if (FAILED(pVertexBuffer->Map(D3D10_MAP_WRITE_DISCARD, 0, (void**)&pVertices)))
            return;

        int n = 0;

        for (int i = 0; i < count; i++)
        {
            const XrdTest::Rect& rect = pRects[i];
            const uint32_t color = ColorToRgba(rect.color.r, rect.color.g, rect.color.b, rect.color.a);

            const SimpleVertex quad[4] =
            {
                { rect.x,                    rect.y,                    0.0f, color },
                { rect.x + rect.width,       rect.y,                    0.0f, color },
                { rect.x + rect.width,       rect.y + rect.height,      0.0f, color },
                { rect.x,                    rect.y + rect.height,      0.0f, color },
            };

            for (int v = 0; v < 4; v++)
                pVertices[n++] = quad[v];
        }

        pVertexBuffer->Unmap();

        SimpleConstants constants{};
        Xrd::Matrix ortho = Xrd::Matrix::OrthographicOffCenter(0.0f, (float)window.width, (float)window.height, 0.0f, 0.0f, 1.0f);
        memcpy(constants.projection, ortho.m, sizeof(constants.projection));

        SimpleConstants* pMappedConstants = nullptr;
        if (FAILED(pConstantBuffer->Map(D3D10_MAP_WRITE_DISCARD, 0, (void**)&pMappedConstants)))
            return;

        memcpy(pMappedConstants, &constants, sizeof(constants));
        pConstantBuffer->Unmap();

        const UINT stride = sizeof(SimpleVertex);
        const UINT offset = 0;
        pDevice->IASetVertexBuffers(0, 1, &pVertexBuffer, &stride, &offset);
        pDevice->IASetInputLayout(pInputLayout);
        pDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pDevice->VSSetShader(pVertexShader);
        pDevice->VSSetConstantBuffers(0, 1, &pConstantBuffer);
        pDevice->PSSetShader(pPixelShader);

        static ID3D10Buffer* pIndexBuffer = nullptr;
        if (!pIndexBuffer)
        {
            std::vector<uint16_t> indices(MaxVertices / 4 * 6);
            for (int i = 0; i < MaxVertices / 4; i++)
            {
                indices[i * 6 + 0] = (uint16_t)(i * 4 + 0);
                indices[i * 6 + 1] = (uint16_t)(i * 4 + 1);
                indices[i * 6 + 2] = (uint16_t)(i * 4 + 2);
                indices[i * 6 + 3] = (uint16_t)(i * 4 + 0);
                indices[i * 6 + 4] = (uint16_t)(i * 4 + 2);
                indices[i * 6 + 5] = (uint16_t)(i * 4 + 3);
            }

            D3D10_BUFFER_DESC desc{};
            desc.ByteWidth = (UINT)(indices.size() * sizeof(uint16_t));
            desc.Usage = D3D10_USAGE_IMMUTABLE;
            desc.BindFlags = D3D10_BIND_INDEX_BUFFER;

            D3D10_SUBRESOURCE_DATA data{};
            data.pSysMem = indices.data();
            pDevice->CreateBuffer(&desc, &data, &pIndexBuffer);
        }

        pDevice->IASetIndexBuffer(pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
        pDevice->OMSetBlendState(pBlendState, nullptr, 0xFFFFFFFF);
        pDevice->DrawIndexed(count * 6, 0, 0);
    }

    void DrawWorld()
    {
        std::vector<XrdTest::Rect> rects;

        rects.push_back({ 0.0f, 0.0f, (float)window.width, (float)window.height * 0.62f, { 0.35f, 0.45f, 0.65f, 1.0f } });
        rects.push_back({ 0.0f, (float)window.height * 0.62f, (float)window.width, (float)window.height * 0.38f, { 0.18f, 0.20f, 0.16f, 1.0f } });
        rects.push_back({ 0.0f, (float)window.height * 0.62f - 3.0f, (float)window.width, 3.0f, { 0.75f, 0.70f, 0.45f, 1.0f } });

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

        for (int i = 0; i < 40; i++)
        {
            const float z = fmodf((float)i * 2.5f + camera.z * 6.0f, 100.0f);
            const float y = (float)window.height * 0.62f + (float)window.height * 0.38f * (z / 100.0f) * (z / 100.0f);
            rects.push_back({ 0.0f, y, (float)window.width, 1.5f, { 0.30f, 0.34f, 0.30f, 1.0f } });
        }

        DrawRects(rects.data(), (int)rects.size());
    }

    void DrawUi()
    {
        DrawRects(ui.rects.data(), (int)ui.rects.size());

        if (uiSelection >= 0 && (size_t)uiSelection + 1 < ui.rects.size())
        {
            XrdTest::Rect marker = ui.rects[uiSelection + 1];
            marker.x = marker.x + marker.width - 26.0f;
            marker.width = 16.0f;
            marker.y += 9.0f;
            marker.height = 16.0f;
            marker.color = { 1.0f, 1.0f, 1.0f, 1.0f };
            DrawRects(&marker, 1);
        }
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
    XrdTest::Headless::ParseCommandLine("d3d10");

    if (XrdTest::Headless::Active())
        camera.autoYawSpeed = 0.0f;	// the pictures of the check are compared to each other

    if (!window.Create(L"Xbox Rain Droplets - Direct3D 10", 1280, 720))
    {
        printf("[d3d10] window creation failed\n");
        fflush(stdout);
        return 1;
    }

    if (!InitializeDevice())
    {
        printf("[d3d10] device initialisation failed\n");
        fflush(stdout);
        return XrdTest::Headless::DeviceFailed();
    }

    printf("[d3d10] renderer: %d\n", (int)Xrd::Init(Xrd::RENDERER_D3D10, pSwapChain));
    fflush(stdout);    WaterDrops::fTimeStep = &deltaTime;
    // the games read this from the weather, a test wants a lot of rain
    WaterDrops::ms_rainIntensity = 4.0f;

    bool active[4] = { true, false, true, false };
    ui.Build(XrdTest::g_uiLabels, 4, active, (float)window.width);

    auto previous = std::chrono::high_resolution_clock::now();
    auto lastReport = previous;
    int frames = 0;
    int frameIndex = 0;		// headless: how many frames the run has drawn
    char extra[128]{};

    D3D10_VIEWPORT viewport{};
    viewport.Width = (UINT)window.width;
    viewport.Height = (UINT)window.height;
    viewport.MaxDepth = 1.0f;

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

        const float clear[4] = { 12.0f / 255.0f, 12.0f / 255.0f, 16.0f / 255.0f, 1.0f };
        pDevice->ClearRenderTargetView(pBackBufferView, clear);
        pDevice->OMSetRenderTargets(1, &pBackBufferView, nullptr);
        pDevice->RSSetViewports(1, &viewport);

        printf("[d3d10] frame: world done\n"); fflush(stdout);
        DrawWorld();
        printf("[d3d10] frame: world drawn\n"); fflush(stdout);

        // the drops land on the scene, which is the same call as on every other
        // API, and they stay under the UI drawn after them
        XrdTest::Headless::PrepareDrops(frameIndex, window.width, window.height);

        WaterDrops::Process();
        printf("[d3d10] frame: processed\n"); fflush(stdout);
        WaterDrops::Render();
        printf("[d3d10] frame: rendered\n"); fflush(stdout);

        DrawUi();
        printf("[d3d10] frame: ui drawn\n"); fflush(stdout);

        pSwapChain->Present(0, 0);

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
            XrdTest::PrintStatus("d3d10", WaterDrops::ms_numDrops, frames, WaterDrops::bEnableSnow, extra);

            wchar_t title[160]{};
            swprintf_s(title, L"Xbox Rain Droplets - Direct3D 10  |  drops %d  fps %d", WaterDrops::ms_numDrops, frames);
            SetWindowTextW(window.hwnd, title);

            frames = 0;
            lastReport = now;
        }
    }

    WaterDrops::Shutdown();
    Xrd::Shutdown();

    return 0;
}
