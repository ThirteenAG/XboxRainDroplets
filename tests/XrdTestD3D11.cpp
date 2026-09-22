// ---------------------------------------------------------------------------
// Test application for the Direct3D 11 backend.
//
// Same scene and the same UI as the Direct3D 9 one, only the little drawing
// code of the application itself differs. The drops do not: the effect and the
// renderer call are identical on both APIs.
// ---------------------------------------------------------------------------

#define XRD_ENABLE_D3D11 1
#include "xrd/xrd.h"

#include "XrdTest.h"

#include <d3d11.h>

namespace
{
    struct SimpleVertex
    {
        float x, y, z;
        uint32_t color;
    };

    ID3D11Device* pDevice = nullptr;
    ID3D11DeviceContext* pContext = nullptr;
    IDXGISwapChain* pSwapChain = nullptr;
    ID3D11RenderTargetView* pBackBufferView = nullptr;

    ID3D11VertexShader* pVertexShader = nullptr;
    ID3D11PixelShader* pPixelShader = nullptr;
    ID3D11InputLayout* pInputLayout = nullptr;
    ID3D11Buffer* pVertexBuffer = nullptr;
    ID3D11Buffer* pConstantBuffer = nullptr;
    ID3D11BlendState* pBlendState = nullptr;

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

        D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };

        HRESULT hrDevice = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, ARRAYSIZE(levels),
            D3D11_SDK_VERSION, &swapChainDesc, &pSwapChain, &pDevice, nullptr, &pContext);

        // A build server has no graphics card; the software rasterizer of the
        // system draws the scene there instead.
        if (FAILED(hrDevice))
        {
            printf("[d3d11] no hardware device (%08x), falling back to the WARP rasterizer\n", (unsigned)hrDevice);
            fflush(stdout);

            hrDevice = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, ARRAYSIZE(levels),
                D3D11_SDK_VERSION, &swapChainDesc, &pSwapChain, &pDevice, nullptr, &pContext);
        }

        if (FAILED(hrDevice))
        {
            printf("[d3d11] device and swap chain failed: %08x\n", (unsigned)hrDevice);
            fflush(stdout);
            return false;
        }

        ID3D11Texture2D* pBackBuffer = nullptr;
        if (FAILED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer)))
            return false;

        const bool bTargetOk = SUCCEEDED(pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &pBackBufferView));
        pBackBuffer->Release();

        if (!bTargetOk)
            return false;

        ID3DBlob* pVertexBlob = Xrd::CompileShader(Xrd::Shaders::D3D11SimpleSource, "SimpleVS", "vs_4_0");
        ID3DBlob* pPixelBlob = Xrd::CompileShader(Xrd::Shaders::D3D11SimpleSource, "SimplePS", "ps_4_0");

        if (!pVertexBlob || !pPixelBlob)
            return false;

        D3D11_INPUT_ELEMENT_DESC layout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(SimpleVertex, x), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_B8G8R8A8_UNORM,  0, offsetof(SimpleVertex, color), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        bool bResult = SUCCEEDED(pDevice->CreateVertexShader(pVertexBlob->GetBufferPointer(), pVertexBlob->GetBufferSize(), nullptr, &pVertexShader))
            && SUCCEEDED(pDevice->CreateInputLayout(layout, ARRAYSIZE(layout), pVertexBlob->GetBufferPointer(), pVertexBlob->GetBufferSize(), &pInputLayout))
            && SUCCEEDED(pDevice->CreatePixelShader(pPixelBlob->GetBufferPointer(), pPixelBlob->GetBufferSize(), nullptr, &pPixelShader));

        pVertexBlob->Release();
        pPixelBlob->Release();

        if (!bResult)
            return false;

        D3D11_BUFFER_DESC bufferDesc{};
        bufferDesc.ByteWidth = MaxVertices * sizeof(SimpleVertex);
        bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        bResult = bResult && SUCCEEDED(pDevice->CreateBuffer(&bufferDesc, nullptr, &pVertexBuffer));

        D3D11_BUFFER_DESC constantDesc{};
        constantDesc.ByteWidth = sizeof(SimpleConstants);
        constantDesc.Usage = D3D11_USAGE_DYNAMIC;
        constantDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        constantDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        bResult = bResult && SUCCEEDED(pDevice->CreateBuffer(&constantDesc, nullptr, &pConstantBuffer));

        D3D11_BLEND_DESC blendDesc{};
        blendDesc.RenderTarget[0].BlendEnable = TRUE;
        blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        bResult = bResult && SUCCEEDED(pDevice->CreateBlendState(&blendDesc, &pBlendState));

        return bResult;
    }

    void DrawRects(const XrdTest::Rect* pRects, int count)
    {
        if (count <= 0 || count * 6 > MaxVertices)
            return;

        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(pContext->Map(pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
            return;

        SimpleVertex* pVertices = (SimpleVertex*)mapped.pData;
        int n = 0;

        for (int i = 0; i < count; i++)
        {
            const XrdTest::Rect& rect = pRects[i];
            const uint32_t color = Xrd::ColorFloat(rect.color.r, rect.color.g, rect.color.b, rect.color.a);

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

        pContext->Unmap(pVertexBuffer, 0);

        SimpleConstants constants{};
        Xrd::Matrix ortho = Xrd::Matrix::OrthographicOffCenter(0.0f, (float)window.width, (float)window.height, 0.0f, 0.0f, 1.0f);
        memcpy(constants.projection, ortho.m, sizeof(constants.projection));

        D3D11_MAPPED_SUBRESOURCE constantMapped{};
        if (FAILED(pContext->Map(pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &constantMapped)))
            return;

        memcpy(constantMapped.pData, &constants, sizeof(constants));
        pContext->Unmap(pConstantBuffer, 0);

        const UINT stride = sizeof(SimpleVertex);
        const UINT offset = 0;
        pContext->IASetVertexBuffers(0, 1, &pVertexBuffer, &stride, &offset);
        pContext->IASetInputLayout(pInputLayout);
        pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pContext->VSSetShader(pVertexShader, nullptr, 0);
        pContext->VSSetConstantBuffers(0, 1, &pConstantBuffer);
        pContext->PSSetShader(pPixelShader, nullptr, 0);

        // the quads are drawn as four vertices each without an index buffer, so
        // a small internal one is used instead
        static ID3D11Buffer* pIndexBuffer = nullptr;
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

            D3D11_BUFFER_DESC desc{};
            desc.ByteWidth = (UINT)(indices.size() * sizeof(uint16_t));
            desc.Usage = D3D11_USAGE_IMMUTABLE;
            desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

            D3D11_SUBRESOURCE_DATA data{};
            data.pSysMem = indices.data();
            pDevice->CreateBuffer(&desc, &data, &pIndexBuffer);
        }

        pContext->IASetIndexBuffer(pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
        pContext->OMSetBlendState(pBlendState, nullptr, 0xFFFFFFFF);
        pContext->DrawIndexed(count * 6, 0, 0);
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

        // --lens-light: the world is dark and two bright lamps sit right next to
        // the drop the check places at the top of the frame, one red and one
        // green. What a drop of clear water gathers out of the frame around it is
        // then the only light there is, and the drop is the same drop in the same
        // place either way, so running this once with and once without Refractions
        // in the ini is what the light gathering looks like.
        if (XrdTest::Headless::State().lensLight)
        {
            for (auto& rect : rects)
            {
                rect.color.r *= 0.35f;
                rect.color.g *= 0.35f;
                rect.color.b *= 0.35f;
            }
        }

        DrawRects(rects.data(), (int)rects.size());

        if (XrdTest::Headless::State().lensLight)
        {
            const float x = (float)window.width * XrdTest::Headless::State().dropX;
            const float y = (float)window.height * 0.25f;

            const XrdTest::Rect lamps[] = {
                { x + 86.0f, y - 8.0f, 30.0f, 16.0f, { 1.0f, 0.05f, 0.02f, 1.0f } },
                { x - 116.0f, y - 8.0f, 30.0f, 16.0f, { 0.06f, 1.0f, 0.12f, 1.0f } },
            };

            DrawRects(lamps, 2);

            const XrdTest::Rect tailLights[] = {
                { x + 6.0f, y + 10.0f, 10.0f, 5.0f, { 1.0f, 0.10f, 0.05f, 1.0f } },
                { x + 18.0f, y - 6.0f, 10.0f, 5.0f, { 1.0f, 0.10f, 0.05f, 1.0f } },
                { x + 34.0f, y + 14.0f, 10.0f, 5.0f, { 1.0f, 0.10f, 0.05f, 1.0f } },
                { x - 14.0f, y + 8.0f, 10.0f, 5.0f, { 0.10f, 1.0f, 0.15f, 1.0f } },
            };

            DrawRects(tailLights, 4);
        }
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

    // The world space snow of the consoles. It takes the camera twice, exactly
    // like the games hand it over: the matrix of the camera orients and places
    // the flakes, the view matrix projects them.
    bool snowEnabled = false; // T turns the snow on, Y turns it off

    void DrawSnow()
    {
        const float cosPitch = cosf(camera.pitch);
        const float sinPitch = sinf(camera.pitch);
        const float cosYaw = cosf(camera.yaw);
        const float sinYaw = sinf(camera.yaw);

        static RwMatrix cam{};
        static RwMatrix view{};

        cam.right = { cosYaw, 0.0f, -sinYaw };
        cam.up = { -sinYaw * sinPitch, cosPitch, -cosYaw * sinPitch };
        cam.at = { sinYaw * cosPitch, sinPitch, cosYaw * cosPitch };
        cam.pos = { camera.x, camera.y, camera.z };

        view.right = { 1.0f, 0.0f, 0.0f };
        view.up = { 0.0f, 1.0f, 0.0f };
        view.at = { 0.0f, 0.0f, 1.0f };
        view.pos = { -camera.x, -camera.y, -camera.z };

        float timeStep = deltaTime;
        CSnow::targetSnow = 1.0f;
        CSnow::AddSnow(window.width, window.height, &cam, &view, &timeStep, false);
    }

    void ApplyCameraToDrops()
    {
        const float cosPitch = cosf(camera.pitch);

        WaterDrops::right = { cosf(camera.yaw), 0.0f, -sinf(camera.yaw) };
        WaterDrops::up = { -sinf(camera.yaw) * sinf(camera.pitch), cosPitch, -cosf(camera.yaw) * sinf(camera.pitch) };
        WaterDrops::at = { sinf(camera.yaw) * cosPitch, sinf(camera.pitch), cosf(camera.yaw) * cosPitch };
        WaterDrops::pos = { camera.x, camera.y, camera.z };
    }

    // -----------------------------------------------------------------------
    // the water a running drop leaves behind it, without a device
    //
    // Three things the effect has to get right do not need a graphics API to be
    // looked at, only the drops it makes and the vertices they become:
    //
    //   - what runs down the glass and what hangs on it is decided by how much
    //     water there is in a bead, and never by a setting,
    //   - the water a bead leaves is only ever a part of how visible the bead is,
    //     so a bead that has almost faded out can not leave a trail that shows up
    //     brighter than the bead itself. The original code drew every trace at
    //     full opacity, which is why a drop that was about to fade out painted a
    //     bright smear as soon as the camera moved,
    //   - a place in the pool that is free again is not still being moved, see
    //     WaterDrops::DetachMoving. The original code left the entry that points
    //     at a drop behind when the drop expired, and the next drop to take that
    //     place was moved twice.
    // -----------------------------------------------------------------------
    int CheckTrails()
    {
        using D = WaterDrops;

        bool passed = true;
        const auto check = [&](bool ok, const char* message)
        {
            printf("[%s] %s\n", ok ? "PASS" : "FAIL", message);
            fflush(stdout);
            passed &= ok;
        };

        // the seconds per frame the games hand over, 60 frames a second
        float timeStep = 1.0f / 60.0f;

        D::ms_fbWidth = 1920;
        D::ms_fbHeight = 1080;
        D::ms_scaling = D::ms_fbHeight / 480.0f;
        D::ms_movingEnabled = true;
        D::ms_vec = {};
        D::bGravity = true;
        D::bEnableSnow = false;
        D::bRefractions = true;
        D::fTimeStep = &timeStep;

        const int32_t smallest = (int32_t)(D::MinSize * D::ms_scaling);
        const int32_t biggest = (int32_t)(D::MaxSize * D::ms_scaling);
        const float slowest = D::gravity / D::gdivmin;
        const float fastest = D::gravity / D::gdivmax;

        // What runs down the glass and what hangs on it: one bead in two is held
        // where it is by the surface tension and never runs, whatever its size is,
        // which is what makes beads of every size be seen both hanging and
        // running. The only thing that then moves a bead that hangs is the camera.
        int32_t hangingBeads = 0;
        int32_t runningBeads = 0;
        float slowestRunner = 1000.0f;
        float fastestRunner = 0.0f;

        D::Clear();

        for (int i = 0; i < 400; i++)
        {
            auto* bead = D::PlaceNew(960.0f, 540.0f, (float)((i % 2) ? smallest : biggest), 20000.0f, true);

            if (bead->slide == 0.0f)
            {
                hangingBeads++;
            }
            else
            {
                runningBeads++;

                if (bead->slide < slowestRunner)
                    slowestRunner = bead->slide;

                if (bead->slide > fastestRunner)
                    fastestRunner = bead->slide;
            }
        }

        check(hangingBeads > 400 / 4 && hangingBeads < 400 * 3 / 4,
            "about one bead in two hangs on the glass where it is");
        check(runningBeads > 400 / 4 && runningBeads < 400 * 3 / 4,
            "and the other one runs down it");
        check(slowestRunner >= slowest - 0.001f && fastestRunner <= fastest + 0.001f,
            "how fast a bead runs is in the range the effect has always given a drop");

        // both halves of the beads are of every size, and not the small ones that
        // hang and the big ones that run
        D::Clear();

        int32_t hangingSmall = 0;
        int32_t runningSmall = 0;
        int32_t hangingBig = 0;
        int32_t runningBig = 0;

        for (int i = 0; i < 400; i++)
        {
            const bool tiny = (i % 2) != 0;
            auto* bead = D::PlaceNew(960.0f, 540.0f, (float)(tiny ? smallest : biggest), 20000.0f, true);

            if (tiny)
                (bead->slide == 0.0f ? hangingSmall : runningSmall)++;
            else
                (bead->slide == 0.0f ? hangingBig : runningBig)++;
        }

        check(hangingSmall > 200 / 4 && runningSmall > 200 / 4 && hangingBig > 200 / 4 && runningBig > 200 / 4,
            "the beads that hang and the beads that run are of every size");

        D::bGravity = false;
        D::Clear();
        auto* weightless = D::PlaceNew(400.0f, 540.0f, (float)biggest, 20000.0f, true);
        check(weightless->slide == 0.0f, "nothing runs down the glass while gravity is turned off");
        D::bGravity = true;

        // A bead leaves a drop of water where it has been, and it is a drop of the
        // rain like any other: it is put in the pool and drawn by the same code as
        // the bead it came from, with the shape and the colour of it. Which one of
        // the beads runs is not known in advance, so a bead that runs is asked for
        // until one is there.
        D::ms_vec = {};
        D::Clear();
        auto* bead = D::PlaceNew(960.0f, 300.0f, (float)biggest, 20000.0f, true);

        for (int i = 0; i < 200 && bead->slide == 0.0f; i++)
        {
            D::Clear();
            bead = D::PlaceNew(960.0f, 300.0f, (float)biggest, 20000.0f, true);
        }

        D::NewDropMoving(bead);

        for (int i = 0; i < 5; i++)
            D::ProcessMoving();

        WaterDrop* trace = nullptr;

        for (auto& drop : D::ms_drops)
            if (drop.active && &drop != bead)
                trace = &drop;

        check(D::ms_numDrops > 1, "a bead that runs down the glass leaves drops of water behind it");
        check(trace && trace->size == (float)(D::MinSize * D::ms_scaling),
            "and what it leaves is a drop of the smallest size the rain has");
        check(trace && trace->r == bead->r && trace->g == bead->g && trace->b == bead->b,
            "drawn in the colour of the drop it came from");
        check(trace && trace->slide == 0.0f && trace->alpha == 255,
            "and what is on the glass does not run down it");

        // It is not one of the drops that move, so it never travels and it never
        // leaves anything of its own: a tail of the rain is a chain of drops and not
        // a haze of drops of drops.
        int32_t movers = 0;

        for (auto& moving : D::ms_dropsMoving)
            if (moving.drop == trace)
                movers++;

        check(movers == 0, "the drop a bead left is not one of the drops that move");

        const float leftAt = trace ? trace->x : 0.0f;

        D::ms_vec = { -5.0f, 2.0f, 0.0f };

        for (int i = 0; i < 20; i++)
            D::ProcessMoving();

        D::ms_vec = {};

        check(trace && trace->x == leftAt,
            "and the glass holds it where it was left while the bead runs on from it");

        // The water a drop leaves is that drop and never brighter than it: a drop that
        // has almost faded out leaves water that is as faded as it is, which is what
        // the trails of a drop that is nearly gone looked like before.
        D::Clear();
        D::ms_vec = {};
        D::ms_vecLen = 0.0f;
        auto* faint = D::PlaceNew(600.0f, 300.0f, (float)biggest, 20000.0f, true);

        if (faint)
        {
            faint->slide = D::gravity / D::gdivmax;
            faint->alpha = 24;
            D::NewDropMoving(faint);

            for (int i = 0; i < 20; i++)
                D::ProcessMoving();

            uint8_t brightestWater = 0;

            for (auto& drop : D::ms_drops)
                if (drop.active && &drop != faint)
                    brightestWater = (std::max)(brightestWater, drop.alpha);

            check(D::ms_numDrops > 1 && brightestWater <= 24,
                "and the water a bead that has almost faded out leaves is as faint as the bead is");
        }

        // Every drop that moves leaves water behind it, however short its own life is:
        // how much of a tail a drop has is how long the water it left stays on the
        // glass, and not how long the drop itself lives.
        D::Clear();
        D::ms_vec = {};
        D::ms_vecLen = 0.0f;
        auto* shortLived = D::PlaceNew(600.0f, 300.0f, (float)biggest, 2000.0f, true);

        if (shortLived)
        {
            shortLived->slide = D::gravity / D::gdivmax;
            D::NewDropMoving(shortLived);

            for (int i = 0; i < 20; i++)
                D::ProcessMoving();

            check(D::ms_numDrops > 1, "and a bead that lives for a short time leaves water while it is there");
        }

        // The tail of a bead is a chain of drops along the path it has run, and a
        // bead the camera drags across the screen is followed by the whole of that
        // path: what the eye reads as a streak is the chain itself.
        D::Clear();
        D::ms_vec = {};
        D::ms_vecLen = 0.0f;
        auto* dragged = D::PlaceNew(1200.0f, 540.0f, (float)biggest, 20000.0f, true);
        D::NewDropMoving(dragged);

        const float right = dragged->x;

        for (int32_t i = 0; i < 8; i++)
        {
            D::ms_vec = { 40.0f, 0.0f, 0.0f };
            // what CalculateMovement measures the camera with, which a check that
            // moves the drops by hand has to hand over itself
            D::ms_vecLen = 40.0f;
            D::ProcessMoving();
        }

        D::ms_vec = {};
        D::ms_vecLen = 0.0f;

        int32_t onTheGlass = 0;
        int32_t alongPath = 0;
        float nearest = 0.0f;
        float farthest = 0.0f;

        for (auto& drop : D::ms_drops)
            if (drop.active && &drop != dragged)
            {
                onTheGlass++;
                alongPath += drop.x >= dragged->x - 0.001f && drop.x <= right + 0.001f ? 1 : 0;
                nearest = (std::max)(nearest, drop.x);
                farthest = farthest == 0.0f ? drop.x : (std::min)(farthest, drop.x);
            }

        check(onTheGlass > 1 && alongPath == onTheGlass,
            "the path a bead is dragged over is left as a chain of drops along it, not one dot behind it");
        check(farthest < nearest && right - farthest > (right - dragged->x) * 0.3f,
            "and the chain of it reaches back over the path the bead was dragged");

        // How long the water of a bead stays on the glass is the length of the tail
        // of that bead, and it is rolled for every bead of the rain on its own: a
        // bead whose water dries quickly is followed by a short chain and one whose
        // water stays is followed by a long one.
        D::Clear();
        D::ms_vec = {};
        D::ms_vecLen = 0.0f;
        auto* shortTailed = D::PlaceNew(400.0f, 300.0f, (float)biggest, 20000.0f, true);
        auto* longTailed = D::PlaceNew(400.0f, 900.0f, (float)biggest, 20000.0f, true);

        shortTailed->traceTtl = 200.0f;
        longTailed->traceTtl = 4000.0f;
        shortTailed->slide = D::gravity / D::gdivmax;
        longTailed->slide = D::gravity / D::gdivmax;

        D::NewDropMoving(shortTailed);
        D::NewDropMoving(longTailed);

        for (int32_t i = 0; i < 4; i++)
            D::ProcessMoving();

        int32_t shortDrops = 0;
        int32_t longDrops = 0;

        for (auto& drop : D::ms_drops)
            if (drop.active && &drop != shortTailed && &drop != longTailed)
                (drop.y < 600.0f ? shortDrops : longDrops)++;

        // the water is no longer being added to: what is on the glass now is what
        // there was, and all that is left to happen to it is that it dries
        shortTailed->slide = 0.0f;
        longTailed->slide = 0.0f;

        for (int32_t i = 0; i < 30; i++)
        {
            D::ProcessMoving();
            D::Fade();
        }

        int32_t shortLeft = 0;
        int32_t longLeft = 0;

        for (auto& drop : D::ms_drops)
            if (drop.active && &drop != shortTailed && &drop != longTailed)
                (drop.y < 600.0f ? shortLeft : longLeft)++;

        printf("[d3d11] a bead whose water dries quickly left %d of %d drops of a tail, one whose water stays left %d of %d\n",
            shortLeft, shortDrops, longLeft, longDrops);

        check(shortDrops > 0 && longDrops > 0, "a bead that runs leaves a chain of drops of water behind it");
        check(shortLeft == 0 && longLeft == longDrops,
            "the water of a bead dries out, and the tail of a bead whose water dries quickly is gone while the other is still there");

        // A bead that neither runs nor is moved leaves nothing at all: what is left
        // behind is the path of the bead, and it has none.
        D::ms_vec = {};
        D::ms_vecLen = 0.0f;
        D::Clear();
        auto* hangingStill = D::PlaceNew(960.0f, 540.0f, (float)biggest, 20000.0f, true);

        for (int i = 0; i < 200 && hangingStill->slide != 0.0f; i++)
        {
            D::Clear();
            hangingStill = D::PlaceNew(960.0f, 540.0f, (float)biggest, 20000.0f, true);
        }

        D::NewDropMoving(hangingStill);

        for (int i = 0; i < 400; i++)
            D::ProcessMoving();

        check(D::ms_numDrops == 1 && hangingStill->x == 960.0f,
            "a bead that hangs on the glass leaves no water while the camera is still");

        // a drop that expired is not moved any more: the place it had in the pool
        // is handed to the drop below, which must not then move twice per frame
        D::Clear();
        D::ms_vec = {};
        auto* expired = D::PlaceNew(960.0f, 540.0f, (float)biggest, 1.0f, true);
        D::NewDropMoving(expired);
        D::Fade();

        check(D::ms_numDropsMoving == 0, "a drop that expired is taken out of the list of the drops that move");

        auto* replacement = D::PlaceNew(960.0f, 540.0f, (float)biggest, 20000.0f, true);
        D::NewDropMoving(replacement);
        check(replacement == expired && D::ms_numDropsMoving == 1,
            "the drop that takes the place of an expired one moves once, not twice");

        D::Clear();
        check(D::ms_numDrops == 0 && D::ms_numDropsMoving == 0 && D::ms_dropsMoving[0].drop == nullptr,
            "clearing the drops clears the list of the drops that move as well");

        // Every bead of the rain is given the water it leaves of its own when it is
        // placed, and it is rolled for every one of them: the tails of one shower
        // are of every length there is and no two beads look the same.
        float shortest = 1.0e9f;
        float longest = 0.0f;

        for (int32_t i = 0; i < 500; i++)
        {
            D::Clear();
            auto* rolled = D::PlaceNew(960.0f, 540.0f, (float)biggest, 20000.0f, true);

            shortest = (std::min)(shortest, rolled->traceTtl);
            longest = (std::max)(longest, rolled->traceTtl);
        }

        check(shortest > 0.0f && longest > shortest * 3.0f,
            "the water one bead leaves is on the glass for several times as long as the water of another");

        // Every drop of the effect, the water of the drops included, is one quad of
        // the vertex buffer of a backend, so a pool of drops that fits in it is a
        // buffer that can not be written past its end, see MaxQuads.
        D::Clear();
        D::ms_vec = {};
        D::ms_vecLen = 0.0f;

        const int32_t dense = 400;

        for (int32_t i = 0; i < dense; i++)
        {
            auto* denseBead = D::PlaceNew(120.0f + (float)(i % 20) * 52.0f, 150.0f + (float)(i / 20) * 26.0f,
                (float)biggest, 20000.0f, true);

            if (!denseBead)
                break;

            D::NewDropMoving(denseBead);
        }

        for (int32_t i = 0; i < 120; i++)
        {
            D::ms_vec = { 11.0f, 3.0f, 0.0f };
            D::ms_vecLen = 11.0f;
            D::ProcessMoving();
        }

        D::ms_vec = {};
        D::ms_vecLen = 0.0f;

        D::ms_vertices.clear();

        for (auto& drop : D::ms_drops)
            if (drop.active)
                D::AddToRenderList(&drop);

        const int32_t quads = (int32_t)(D::ms_vertices.size() / 4);

        check(quads == D::ms_numDrops, "every drop of the rain, the water it left included, is one quad of the buffer");
        check(quads <= D::MaxQuads, "and a screen full of drops and of their water fits in the vertex buffer of a backend");

        // A bead of the size a game on a console or in an emulator runs with travels
        // a small part of itself in a second, and what it leaves is the chain of the
        // drops of the path it has run: the step of the ini is what says how close
        // together they are, and the chain is what the eye reads as a streak. The
        // frame time is a real one here, and the camera does not move, so the only
        // thing that moves the bead is the bead itself.
        D::fps = 60;
        D::ms_vec = {};
        D::ms_vecLen = 0.0f;
        D::ms_fbWidth = 1920.0f;
        D::ms_fbHeight = 1080.0f;
        D::ms_scaling = D::ms_fbHeight / 480.0f;
        // the drops of the ini of PPSSPP, which is the biggest a host is set to
        D::MinSize = 50;
        D::MaxSize = 50;
        D::Clear();

        auto* runner = D::PlaceNew(D::ms_fbWidth * 0.5f, D::ms_fbHeight * 0.4f, D::MaxSize * D::ms_scaling, 20000.0f, true);

        if (runner)
        {
            runner->slide = D::gravity / D::gdivmax;
            runner->traceTtl = 1000.0f;
            D::NewDropMoving(runner);

            for (int i = 0; i < 60; i++)
                D::ProcessMoving();

            int32_t behind = 0;
            float farthestBehind = 0.0f;

            for (auto& drop : D::ms_drops)
                if (drop.active && &drop != runner)
                {
                    behind++;
                    farthestBehind = (std::max)(farthestBehind, runner->y - drop.y);
                }

            printf("[d3d11] a bead of %.0f px that ran for 60 frames of %.1f ms left %d drops behind it, %.0f px of water\n",
                runner->size, D::GetTimeStepInMilliseconds(), behind, farthestBehind);

            check(behind >= 8, "a bead that only runs down the glass is followed by a chain of drops and not by nothing");
            check(farthestBehind > 8.0f, "and the chain of it reaches back over the path the bead has run");
        }

        // Equal elapsed time must give equal motion, including above 120 Hz.
        D::MinSize = 4;
        D::MaxSize = 15;
        struct MotionResult { float x, y, size; int traces; };
        const auto simulate = [&](int hz)
        {
            D::Clear();
            timeStep = 1.0f / hz;
            D::ms_vec = { -30.0f / hz, 0, 0 };
            auto* d = D::PlaceNew(600, 300, 30, 20000, true);
            d->slide = 0.25f;
            D::NewDropMoving(d);
            for (int i = 0; i < hz; ++i) D::ProcessMoving();
            return MotionResult{d->x, d->y, d->size, D::ms_numDrops - 1};
        };
        const auto at30 = simulate(30), at60 = simulate(60), at144 = simulate(144);
        check(fabsf(at30.x - at144.x) < 0.02f && fabsf(at30.y - at144.y) < 0.02f
            && fabsf(at60.size - at144.size) < 0.01f,
            "camera travel, gravity and shrink agree at 30, 60 and 144 Hz");
        check(abs(at30.traces - at144.traces) <= 1 && abs(at60.traces - at144.traces) <= 1,
            "trail density follows distance rather than frame rate");

        D::Clear();
        timeStep = 1.0f / 60;
        D::ms_vec = { -100, 0, 0 };
        auto* sweep = D::PlaceNew(600, 300, 30, 20000, true);
        sweep->slide = 0;
        D::NewDropMoving(sweep);
        D::ProcessMoving();
        int deposits = D::ms_numDrops - 1;
        float first = 10000, last = 0;
        bool sameShape = true;
        for (const auto& d : D::ms_drops)
            if (d.active && &d != sweep)
            {
                first = (std::min)(first, d.x);
                last = (std::max)(last, d.x);
                sameShape &= d.uv_index == sweep->uv_index;
            }
        check(deposits > 1 && deposits <= 8 && last - first > 70 && sameShape,
            "fast sweeps cover the path with bounded deposits and a consistent atlas shape");
        D::ms_vec = {};
        D::ProcessMoving();
        check(D::ms_numDrops - 1 == deposits, "stopping leaves no deferred burst of traces");
        const float stoppedX = sweep->x, stoppedSize = sweep->size;
        timeStep = 0;
        D::ms_vec = { -100, 100, 0 };
        D::ProcessMoving();
        check(sweep->x == stoppedX && sweep->size == stoppedSize && D::ms_numDrops - 1 == deposits,
            "zero elapsed time neither moves drops nor creates traces");
        timeStep = 1.0f / 60;
        D::ms_vec = {};
        sweep->slide = 0.25f;
        D::bGravity = false;
        const float stoppedY = sweep->y;
        D::ProcessMoving();
        check(sweep->y == stoppedY, "disabling gravity also stops existing runners");
        D::bGravity = true;

        D::fps = 0;
        D::ms_vec = {};
        D::MinSize = 4;
        D::MaxSize = 15;
        D::fTimeStep = nullptr;
        return passed ? 0 : 1;
    }
}

int main()
{
    // --headless: a run for a build server, see XrdTest::Headless
    XrdTest::Headless::ParseCommandLine("d3d11");

    // --trail-check: the trail of a running drop, which needs no device at all
    if (XrdTest::Headless::State().trailCheck)
        return CheckTrails();

    if (XrdTest::Headless::Active())
        camera.autoYawSpeed = 0.0f;	// the pictures of the check are compared to each other

    if (!window.Create(L"Xbox Rain Droplets - Direct3D 11", 1280, 720))
        return 1;

    if (!InitializeDevice())
        return XrdTest::Headless::DeviceFailed();

    Xrd::Init(Xrd::RENDERER_D3D11, pSwapChain);
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

    D3D11_VIEWPORT viewport{};
    viewport.Width = (float)window.width;
    viewport.Height = (float)window.height;
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
        pContext->ClearRenderTargetView(pBackBufferView, clear);
        pContext->OMSetRenderTargets(1, &pBackBufferView, nullptr);
        pContext->RSSetViewports(1, &viewport);

        DrawWorld();

        // the drops land on the scene, which is the same call as on Direct3D 9
        XrdTest::Headless::PrepareDrops(frameIndex, window.width, window.height);

        WaterDrops::Process();
        WaterDrops::Render();

        if (window.keys['T'])
        {
            snowEnabled = true;
        }
        else if (window.keys['Y'])
        {
            snowEnabled = false;

            if (snowEnabled == false)
                CSnow::targetSnow = 0.0f;
        }

        if (snowEnabled || CSnow::Snow > 0.0f)
            DrawSnow();

        DrawUi();

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
            XrdTest::PrintStatus("d3d11", WaterDrops::ms_numDrops, frames, WaterDrops::bEnableSnow, extra);

            wchar_t title[160]{};
            swprintf_s(title, L"Xbox Rain Droplets - Direct3D 11  |  drops %d  fps %d", WaterDrops::ms_numDrops, frames);
            SetWindowTextW(window.hwnd, title);

            frames = 0;
            lastReport = now;
        }
    }

    WaterDrops::Shutdown();
    Xrd::Shutdown();

    return 0;
}
