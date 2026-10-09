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
#include "ShaderCompiler.h"
#include "shaders/generated/tests.h"

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
    // the mock of the drive sorts and culls its own faces, nothing is culled here
    ID3D11RasterizerState* pRasterizerState = nullptr;

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

        D3D11_RASTERIZER_DESC rasterizerDesc{};
        rasterizerDesc.FillMode = D3D11_FILL_SOLID;
        rasterizerDesc.CullMode = D3D11_CULL_NONE;
        rasterizerDesc.DepthClipEnable = TRUE;
        bResult = bResult && SUCCEEDED(pDevice->CreateRasterizerState(&rasterizerDesc, &pRasterizerState));

        return bResult;
    }

    // quads of the window, four corners each, drawn in the order they come
    void DrawQuads(const XrdTest::Mock::Quad* pQuads, int count)
    {
        // more than the buffer holds goes in several draws
        while (count * 4 > MaxVertices)
        {
            const int part = MaxVertices / 4;
            DrawQuads(pQuads, part);
            pQuads += part;
            count -= part;
        }

        if (count <= 0)
            return;

        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(pContext->Map(pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
            return;

        SimpleVertex* pVertices = (SimpleVertex*)mapped.pData;
        int n = 0;

        for (int i = 0; i < count; i++)
        {
            const XrdTest::Mock::Quad& quad = pQuads[i];

            for (int v = 0; v < 4; v++)
                pVertices[n++] = { quad.x[v], quad.y[v], 0.0f, Xrd::ColorFloat(quad.color[v].r, quad.color[v].g, quad.color[v].b, quad.color[v].a) };
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
        pContext->RSSetState(pRasterizerState);
        pContext->DrawIndexed(count * 6, 0, 0);
    }

    void DrawRects(const XrdTest::Rect* pRects, int count)
    {
        std::vector<XrdTest::Mock::Quad> quads;
        quads.reserve((size_t)(std::max)(count, 0));

        for (int i = 0; i < count; i++)
        {
            const XrdTest::Rect& rect = pRects[i];
            quads.push_back({ { rect.x, rect.x + rect.width, rect.x + rect.width, rect.x },
                { rect.y, rect.y, rect.y + rect.height, rect.y + rect.height }, { rect.color, rect.color, rect.color, rect.color }, 0.0f });
        }

        DrawQuads(quads.data(), (int)quads.size());
    }

    // the mock of a drive, see XrdTest::Mock
    XrdTest::Mock::World mock;

    void DrawMockWorld()
    {
        static std::vector<XrdTest::Mock::Quad> flat;
        static std::vector<XrdTest::Mock::Quad> scene;
        flat.clear();
        scene.clear();
        XrdTest::Mock::Build(mock, camera, window.width, window.height, flat, scene);
        DrawQuads(flat.data(), (int)flat.size());
        DrawQuads(scene.data(), (int)scene.size());
    }

    void DrawWorld()
    {
        if (mock.active)
        {
            DrawMockWorld();
            return;
        }

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

        // the right vector the way the games hand it over: RenderWare's, which
        // points to the left of the screen
        WaterDrops::right = { -cosf(camera.yaw), 0.0f, sinf(camera.yaw) };
        WaterDrops::up = { -sinf(camera.yaw) * sinf(camera.pitch), cosPitch, -cosf(camera.yaw) * sinf(camera.pitch) };
        WaterDrops::at = { sinf(camera.yaw) * cosPitch, sinf(camera.pitch), cosf(camera.yaw) * cosPitch };
        WaterDrops::pos = { camera.x, camera.y, camera.z };
    }

    // -----------------------------------------------------------------------
    // --trail-check: the model of the drops, without a device
    //
    // The drops are the lens rain of Forza Horizon 4, taken from a capture of
    // the game (see the air over the lens in xrd.h). What it does does not need
    // a graphics API to be looked at, only the drops the effect makes:
    //
    //   - a drop lands at rest, lives one second and fades from the moment it lands
    //     lands, and seven drops in eight are small,
    //   - a drop sits while the camera stands, runs outwards from the middle of
    //     the picture while the camera drives, faster and faster and the faster
    //     the bigger it is, and inwards while it backs up,
    //   - the camera that moves sideways or turns pushes the drops sideways, and
    //     they come to rest again when it stops turning, some of them leaving a
    //     little water behind them,
    //   - a place in the pool that is free again is not still being moved.
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
        D::MinSize = 4;
        D::MaxSize = 19;

        const float scale = D::ms_scaling;
        const int32_t smallest = (int32_t)(D::MinSize * scale);
        const int32_t biggest = (int32_t)(D::MaxSize * scale);

        const auto air = [&](float x, float y, float z)
        {
            D::ms_air[0] = x;
            D::ms_air[1] = y;
            D::ms_air[2] = z;
            D::ms_lookAir[0] = D::ms_lookAir[1] = 0.0f;
        };

        const auto calm = [&]()
        {
            D::ms_vec = {};
            D::ms_vecLen = 0.0f;
            air(0.0f, 0.0f, 0.0f);
            D::Clear();
        };

        // a drop of a known make: whether it leaves water is what the check asks
        const auto drop = [&](float x, float y, float size, bool trail)
        {
            auto* d = D::PlaceNew(x, y, size, 2000.0f, true);

            if (d)
            {
                d->trail = trail;
                d->fallTop = 0.0f;  // no gravity, see the check of it
                d->time = 1000.0f;  // past its splash
                D::NewDropMoving(d);
            }

            return d;
        };

        const auto run = [&](int frames)
        {
            for (int i = 0; i < frames; i++)
                D::ProcessMoving();
        };

        // the rain itself: what lands and how
        calm();
        D::ms_initialised = true;
        D::SpawnDrops(2000, 0xFF, 0xFF, 0xFF);

        int32_t landed = 0, smallCount = 0, bigCount = 0, still = 0, oneSecond = 0;
        float minRotation = 10.0f, maxRotation = -10.0f;
        const float smallTop = smallest + (biggest - smallest) * 0.32f + 0.01f;
        const float bigBottom = smallest + (biggest - smallest) * 0.62f - 0.01f;

        for (const auto& d : D::ms_drops)
        {
            if (!d.active)
                continue;

            landed++;
            smallCount += d.size >= smallest - 0.01f && d.size <= smallTop ? 1 : 0;
            bigCount += d.size >= bigBottom && d.size <= biggest + 0.01f ? 1 : 0;
            still += d.vel[0] == 0.0f && d.vel[1] == 0.0f && d.vel[2] == 0.0f ? 1 : 0;
            oneSecond += d.ttl >= D::LifeMinSeconds * 2000.0f && d.ttl <= D::LifeMaxSeconds * 2000.0f ? 1 : 0;
            minRotation = (std::min)(minRotation, d.rotation);
            maxRotation = (std::max)(maxRotation, d.rotation);
        }

        printf("[d3d11] %d drops landed: %d small, %d big\n", landed, smallCount, bigCount);
        check(landed > 400 && smallCount + bigCount == landed, "a drop of the rain is either a small one or a big one");
        check(bigCount > landed / 20 && bigCount < landed / 4, "and about one in eight is bigCount");
        check(still == landed && oneSecond == landed, "every drop lands at rest and lives one second");
        check(minRotation < -2.5f && maxRotation > 2.5f, "and lands turned any way");

        // The rain of the weather lands evenly over the picture, the way Forza's
        // does; what thins the middle is the run of the drops outwards, so a lens
        // driven at the speed of the capture of Forza (34 units a second) has the
        // middle (within 0.2 of the half sizes of the picture) at about 0.28 of
        // the density near the edge (0.8 to 1.0), which is what the capture has.
        const auto densities = [&](float& middle, float& edge)
        {
            float inMiddle = 0.0f, nearEdge = 0.0f;
            for (const auto& d : D::ms_drops)
            {
                if (!d.active || d.isTrace) continue;
                const float r = hypotf(d.x / 960.0f - 1.0f, d.y / 540.0f - 1.0f);
                inMiddle += r < 0.2f ? 1.0f : 0.0f;
                nearEdge += r >= 0.8f && r < 1.0f ? 1.0f : 0.0f;
            }
            middle = inMiddle / (0.2f * 0.2f);
            edge = nearEdge / (1.0f - 0.64f);
        };
        float standMiddle = 0.0f, standEdge = 0.0f, driveMiddle = 0.0f, driveEdge = 0.0f;
        for (int round = 0; round < 6; round++)
        {
            calm();
            D::SpawnDrops(2000, 0xFF, 0xFF, 0xFF, true);
            float m, e;
            densities(m, e);
            standMiddle += m; standEdge += e;
        }
        calm();
        timeStep = 1.0f / 60.0f;
        for (int i = 0; i < 60 * 8; i++)
        {
            air(0.0f, 0.0f, -34.0f);
            D::FillScreenMovingRate(1.0f, false, true);
            D::ProcessMoving();
            D::Fade();
            if (i >= 60 * 5 && (i % 30) == 0)
            {
                float m, e;
                densities(m, e);
                driveMiddle += m; driveEdge += e;
            }
        }
        const float standRatio = standMiddle / (std::max)(standEdge, 1.0f);
        const float driveRatio = driveMiddle / (std::max)(driveEdge, 1.0f);
        printf("[d3d11] the rain in the middle against near the edge: %.2f as it lands, %.2f at 34 units a second (Forza 0.28)\n", standRatio, driveRatio);
        check(standRatio > 0.75f && standRatio < 1.3f, "the rain of the weather lands evenly over the picture, the way Forza's does");
        check(driveRatio > 0.15f && driveRatio < 0.45f, "and a lens driven into it has the middle thinned by the run of the drops, the way Forza's is");
        calm();
        D::SpawnDrops(1500, 0xFF, 0xFF, 0xFF, false);
        int32_t splashMiddle = 0;
        for (const auto& d : D::ms_drops)
            splashMiddle += d.active && hypotf(d.x / 960.0f - 1.0f, d.y / 540.0f - 1.0f) < 0.3f ? 1 : 0;
        check(splashMiddle > 10, "and a splash of the game lands in the middle as well");

        // A drop fades from the moment it lands until it is gone.
        calm();
        auto* fading = D::PlaceNew(960.0f, 540.0f, (float)biggest, 2000.0f, true);
        timeStep = 0.25f;
        D::Fade();
        const uint8_t quarter = fading->alpha;
        D::Fade();
        const uint8_t half = fading->alpha;
        timeStep = 1.0f / 60.0f;
        check(quarter > 180 && quarter < 200 && half > 120 && half < 135, "a drop fades from the moment it lands, a quarter of the way in a quarter of a second");

        // And it splashes, bigger for the first moment.
        calm();
        auto* splash = D::PlaceNew(960.0f, 540.0f, 40.0f, 2000.0f, true);
        const float atLanding = D::DrawnSize(splash);
        splash->time = 100.0f;
        check(atLanding > 40.0f * 1.35f && D::DrawnSize(splash) == 40.0f, "a drop is bigger the moment it lands and its own size a frame later");

        // A camera that stands leaves the drops where they are.
        calm();
        auto* sitting = drop(700.0f, 300.0f, (float)biggest, false);
        run(60);
        check(sitting->x == 700.0f && sitting->y == 300.0f, "a drop sits where it landed while the camera stands");

        // A camera that drives makes the drops run outwards from the middle,
        // faster and faster, and the big ones faster than the small ones.
        calm();
        air(0.0f, 0.0f, -30.0f);
        auto* left = drop(500.0f, 540.0f, (float)biggest, false);
        auto* right = drop(1500.0f, 540.0f, (float)biggest, false);
        auto* top = drop(960.0f, 200.0f, (float)biggest, false);
        auto* little = drop(500.0f, 800.0f, (float)smallest, false);
        run(15);
        const float leftEarly = 500.0f - left->x;
        run(15);
        const float leftLate = 500.0f - left->x - leftEarly;
        printf("[d3d11] at 30 units a second a big drop ran %.1f px in a quarter of a second and %.1f px in the next\n", leftEarly, leftLate);
        check(left->x < 500.0f && right->x > 1500.0f && top->y < 200.0f, "a camera that drives makes the drops run outwards from the middle of the picture");
        check(fabsf(left->y - 540.0f) < 0.5f && fabsf(right->y - 540.0f) < 0.5f, "and straight outwards");
        check(leftLate > leftEarly * 2.0f, "faster and faster");
        check(500.0f - little->x < (500.0f - left->x) * 0.5f, "and a small drop well behind a big one");

        calm();
        air(0.0f, 0.0f, 30.0f);
        auto* backing = drop(400.0f, 540.0f, (float)biggest, false);
        run(30);
        check(backing->x > 400.0f, "air from behind the lens would draw the drops in");

        // but a camera that backs up, or looks back while driving, has its lens
        // in the lee: no air comes at it from behind and the drops stay put
        calm();
        D::bRadial = false;
        D::right = { -1.0f, 0.0f, 0.0f };
        D::up = { 0.0f, 1.0f, 0.0f };
        D::at = { 0.0f, 0.0f, 1.0f };
        D::pos = { 0.0f, 0.0f, 0.0f };
        D::ms_haveLastFwd = false;
        D::CalculateMovement();
        D::pos = { 0.0f, 0.0f, -30.0f * timeStep };
        D::CalculateMovement();
        check(D::ms_air[2] == 0.0f, "a camera that goes backwards has no air come at its lens from behind");
        D::ms_haveLastFwd = false;

        // A camera that goes to the right has the air come from the right and
        // pushes the drops to the left; one that goes up pushes them down.
        calm();
        air(-30.0f, 0.0f, 0.0f);
        auto* pushed = drop(960.0f, 540.0f, (float)biggest, false);
        run(30);
        check(pushed->x < 950.0f && fabsf(pushed->y - 540.0f) < 0.5f, "air from the right pushes a drop to the left");
        calm();
        air(0.0f, -30.0f, 0.0f);
        auto* dropped = drop(960.0f, 540.0f, (float)biggest, false);
        run(30);
        check(dropped->y > 550.0f && fabsf(dropped->x - 960.0f) < 0.5f, "air from above pushes a drop down");

        // What the camera does is what the air is measured from.
        calm();
        D::bRadial = false;
        D::fSpeedAdjuster = 1.0f;
        // the right vector the way the games hand it over, pointing to the left
        // of the screen: the right of the screen is +x here
        D::right = { -1.0f, 0.0f, 0.0f };
        D::up = { 0.0f, 1.0f, 0.0f };
        D::at = { 0.0f, 0.0f, 1.0f };
        D::pos = { 0.0f, 0.0f, 0.0f };
        D::ms_haveLastFwd = false;
        D::CalculateMovement();
        D::pos = { 0.0f, 0.0f, 20.0f * timeStep };
        D::CalculateMovement();
        check(fabsf(D::ms_air[2] + 20.0f) < 0.01f && fabsf(D::ms_air[0]) < 0.01f, "a camera that drives forward has the air come at the lens head on");
        D::pos = { D::pos.x + 5.0f * timeStep, 0.0f, D::pos.z };
        D::CalculateMovement();
        // sideways to the way it looks, so half of it, see the facing in CalculateMovement
        check(fabsf(D::ms_air[0] + 2.5f) < 0.01f && fabsf(D::ms_air[2]) < 0.01f, "and one that goes to the right has it come from the right, half of it");

        D::CalculateMovement();
        const float angle = 0.02f;
        D::right = { -cosf(angle), 0.0f, sinf(angle) };
        D::at = { sinf(angle), 0.0f, cosf(angle) };
        D::CalculateMovement();
        const float expected = angle * D::LookArm / timeStep;
        printf("[d3d11] a view that turns right by %.3f rad in a frame makes air of %.1f units/s, expected %.1f\n", angle, D::ms_air[0], expected);
        check(D::ms_air[0] > expected * 0.9f && D::ms_air[0] < expected * 1.1f && D::ms_lookAir[0] == D::ms_air[0],
            "a view that turns to the right swings the lens through the air, so the drops go right");

        // A camera that drives round a corner is turned by its car: the drops feel
        // only the air in its view, the way Forza's do, and no swing of the lens.
        D::right = { -1.0f, 0.0f, 0.0f };
        D::at = { 0.0f, 0.0f, 1.0f };
        D::pos = { 0.0f, 0.0f, 0.0f };
        D::ms_haveLastFwd = false;
        D::CalculateMovement();
        D::right = { -cosf(angle), 0.0f, sinf(angle) };
        D::at = { sinf(angle), 0.0f, cosf(angle) };
        D::pos = { D::at.x * 30.0f * timeStep, 0.0f, D::at.z * 30.0f * timeStep };
        D::CalculateMovement();
        check(D::ms_lookAir[0] == 0.0f && fabsf(D::ms_air[0]) < 1.0f,
            "a camera that drives round a corner does not swing its lens through the air");

        D::right = { -1.0f, 0.0f, 0.0f };
        D::at = { 0.0f, 0.0f, 1.0f };
        D::pos = { 0.0f, 0.0f, 0.0f };
        D::ms_haveLastFwd = false;

        // A drop the turning camera shoved comes to rest again when the turning
        // stops, and a drop that leaves water leaves it while it is dragged.
        calm();
        auto* shoved = drop(960.0f, 540.0f, (float)biggest, true);
        auto* clean = drop(960.0f, 300.0f, (float)biggest, false);
        // the air of a turn of the camera, at the length of the arm it swings on,
        // see LookArm
        for (int i = 0; i < 20; i++)
        {
            air(900.0f, 0.0f, 0.0f);
            D::ms_lookAir[0] = 900.0f;
            D::ProcessMoving();
        }
        const float shovedTo = shoved->x;
        int32_t water = 0;
        for (auto& d : D::ms_drops)
            water += d.active && d.isTrace ? 1 : 0;
        air(0.0f, 0.0f, 0.0f);
        run(60);
        const float coasted = shoved->x - shovedTo;
        run(60);
        printf("[d3d11] a turn moved a drop %.1f px, it coasted %.1f px after and %.2f px in the second after that; it left %d drops of water\n",
            shovedTo - 960.0f, coasted, shoved->x - shovedTo - coasted, water);
        check(shovedTo > 980.0f && clean->x > 980.0f, "the turning camera drags the drops sideways");
        check(shoved->x - shovedTo - coasted < 1.0f, "and they come to rest again when it stops");
        check(water > 2, "a drop that leaves water leaves a trail of it when it is dragged");

        int32_t cleanWater = 0;
        for (auto& d : D::ms_drops)
            if (d.active && d.isTrace && fabsf(d.y - 300.0f) < 5.0f)
                cleanWater++;
        check(cleanWater == 0, "and one that leaves none leaves none");

        // a drop that runs outwards in the stream leaves no water
        calm();
        air(0.0f, 0.0f, -40.0f);
        drop(400.0f, 540.0f, (float)biggest, true);
        run(40);
        int32_t streamWater = 0;
        for (auto& d : D::ms_drops)
            streamWater += d.active && d.isTrace ? 1 : 0;
        check(streamWater == 0, "a drop that the stream of a drive blows outwards leaves no water");

        // Trails come from some of the drops, more of the big ones.
        calm();
        int32_t trailSmall = 0, trailBig = 0;
        for (int i = 0; i < 400; i++)
        {
            auto* d = D::PlaceNew(960.0f, 540.0f, (float)((i % 2) ? smallest : biggest), 2000.0f, true);
            ((i % 2) ? trailSmall : trailBig) += d->trail ? 1 : 0;
            D::Clear();
        }
        check(trailSmall > 10 && trailSmall < 100 && trailBig > trailSmall && trailBig < 180, "some drops leave water when dragged, more of the big ones, not all");

        // With gravity on, a share of the drops, picked at random, runs slowly
        // down the glass; with it off, none does.
        calm();
        int32_t falling = 0, fallingSmall = 0, fallingBig = 0;
        float fastestFall = 0.0f;
        for (int i = 0; i < 400; i++)
        {
            auto* d = D::PlaceNew(960.0f, 540.0f, (float)((i % 2) ? smallest : biggest), 2000.0f, true);
            if (d->fallTop > 0.0f)
            {
                falling++;
                ((i % 2) ? fallingSmall : fallingBig)++;
                fastestFall = (std::max)(fastestFall, d->fallTop);
            }
            D::Clear();
        }
        check(falling > 400 / 5 && falling < 400 / 2 && fallingSmall > 30 && fallingBig > 30,
            "with gravity on, about a third of the drops, of every size, run down the glass");
        check(fastestFall <= D::GravityFastest * scale + 0.01f, "and slowly");
        calm();
        auto* runner = drop(960.0f, 300.0f, (float)biggest, false);
        runner->fallTop = D::GravityFastest * scale;
        run(60);
        check(runner->y > 300.0f + D::GravityFastest * scale * 0.6f && fabsf(runner->x - 960.0f) < 0.01f, "a drop gravity has runs down the glass");
        {
            calm();
            auto* draining = drop(960.0f, 200.0f, (float)biggest, true);
            draining->fallTop = D::GravityFastest * scale;
            const float full = draining->size;
            run(60);
            check(draining->size < full && draining->size >= D::TrailSmallest * (float)smallest - 0.01f,
                "a drop that leaves a trail loses the water it leaves, and gets smaller");
        }
        D::bGravity = false;
        const float stoppedAt = runner->y;
        run(30);
        check(runner->y == stoppedAt, "and nothing runs once gravity is turned off");
        D::bGravity = true;

        // A drop that leaves the picture is gone.
        calm();
        air(0.0f, 0.0f, -60.0f);
        auto* leaving = drop(30.0f, 540.0f, (float)biggest, false);
        run(120);
        check(!leaving->active && D::ms_numDropsMoving == 0, "a drop that runs off the picture is gone");

        // a drop that expired is not moved any more: the place it had in the pool
        // is handed to the drop below, which must not then move twice per frame
        calm();
        auto* expired = D::PlaceNew(960.0f, 540.0f, (float)biggest, 1.0f, true);
        D::NewDropMoving(expired);
        D::Fade();
        check(D::ms_numDropsMoving == 0, "a drop that expired is taken out of the list of the drops that move");
        auto* replacement = D::PlaceNew(960.0f, 540.0f, (float)biggest, 2000.0f, true);
        D::NewDropMoving(replacement);
        check(replacement != nullptr && D::ms_numDrops == 1 && D::ms_numDropsMoving == 1,
            "the drop that comes after an expired one moves once, not twice");
        D::Clear();
        check(D::ms_numDrops == 0 && D::ms_numDropsMoving == 0 && D::ms_numTraces == 0 && D::ms_dropsMoving[0].drop == nullptr,
            "clearing the drops clears the list of the drops that move as well");

        // The same air moves a drop about the same way at any frame rate.
        const auto simulate = [&](int hz)
        {
            calm();
            timeStep = 1.0f / hz;
            air(0.0f, 0.0f, -30.0f);
            auto* d = drop(600.0f, 400.0f, (float)biggest, false);
            for (int i = 0; i < hz / 2; ++i)
                D::ProcessMoving();
            return std::pair<float, float>(d->x, d->y);
        };
        const auto at30 = simulate(30), at60 = simulate(60), at144 = simulate(144);
        printf("[d3d11] half a second of the same drive at 30, 60 and 144 Hz: x %.1f %.1f %.1f\n", at30.first, at60.first, at144.first);
        check(fabsf(at30.first - at144.first) < (600.0f - at144.first) * 0.12f + 0.5f,
            "the drive moves a drop about the same way at 30, 60 and 144 Hz");
        timeStep = 1.0f / 60.0f;

        // Every drop of the effect, the water of the drops included, is one quad of
        // the vertex buffer of a backend, see MaxQuads.
        calm();
        for (int32_t i = 0; i < 400; i++)
            drop(120.0f + (float)(i % 20) * 52.0f, 150.0f + (float)(i / 20) * 26.0f, (float)biggest, true);
        for (int32_t i = 0; i < 60; i++)
        {
            air(80.0f, 20.0f, 0.0f);
            D::ms_lookAir[0] = 80.0f;
            D::ProcessMoving();
        }
        D::ms_vertices.clear();
        for (auto& d : D::ms_drops)
            if (d.active)
                D::AddToRenderList(&d);
        const int32_t quads = (int32_t)(D::ms_vertices.size() / 4);
        check(quads == D::ms_numDrops, "every drop of the rain, the water it left included, is one quad of the buffer");
        check(quads <= D::MaxQuads, "and a screen full of drops and of their water fits in the vertex buffer of a backend");

        // The rain is a rate: a frame adds its share of a second, whatever the
        // frame rate is, and a light rain still adds its drops over time.
        const auto rainFor = [&](int hz, float amount)
        {
            calm();
            timeStep = 1.0f / hz;
            for (int i = 0; i < hz; ++i)
                D::FillScreenMovingRate(amount, false, true);
            return D::ms_numDrops;
        };
        const int rain30 = rainFor(30, 0.2f), rain144 = rainFor(144, 0.2f), drizzle = rainFor(60, 0.02f);
        printf("[d3d11] a second of rain at 30 and 144 Hz: %d and %d drops, and %d of a drizzle\n", rain30, rain144, drizzle);
        check(rain30 > 0 && abs(rain30 - rain144) <= (std::max)(rain30, rain144) / 4,
            "a second of rain is the same rain at 30 and at 144 frames a second");
        check(drizzle > 0, "and a drizzle that adds less than a drop a frame still rains");
        D::ms_forwardSpeed = 30.0f;
        const int driven = rainFor(60, 0.2f);
        D::ms_forwardSpeed = 0.0f;
        const int standing = rainFor(60, 0.2f);
        printf("[d3d11] a second of rain standing and driving: %d and %d drops\n", standing, driven);
        check(driven == standing, "Forza's rain lands at a rate of its own, whatever the speed of the camera");

        // A splash of a game is the drops it asks for, standing or driving.
        calm();
        D::FillScreenMoving(1.0f);
        const int32_t splashStanding = D::ms_numDrops;
        check(splashStanding >= 19, "a splash of a game is as many drops standing as driving");

        // A screen full of drops a game asks for is drops of the rain like any
        // other: they move and live as long as the rest.
        calm();
        D::FillScreen(50);
        int32_t filledLong = 0;
        for (const auto& d : D::ms_drops)
            filledLong += d.active && d.ttl >= D::LifeMinSeconds * 2000.0f ? 1 : 0;
        check(D::ms_numDrops == 50 && D::ms_numDropsMoving == 50 && filledLong == 50,
            "a screen full of drops a game asks for moves and lives like the rain");

        // A flake of snow sticks where it lands: nothing moves it.
        calm();
        D::bEnableSnow = true;
        D::SpawnDrops(40, 235, 235, 235, true);
        check(D::ms_numDrops > 0 && D::ms_numDropsMoving == 0, "snow lands on the lens and nothing moves it");
        D::bEnableSnow = false;

        // Blood sticks where it lands, leaves nothing behind and fades away.
        calm();
        timeStep = 1.0f / 60.0f;
        D::FillScreenMoving(1.0f, true);
        WaterDrop* splat = nullptr;
        for (auto& d : D::ms_drops)
            if (d.active) { splat = &d; break; }
        check(splat && splat->blood && D::ms_numDropsMoving == 0, "blood lands on the lens and nothing moves it");
        if (splat)
        {
            const float sx = splat->x, sy = splat->y;
            air(0.0f, 0.0f, -60.0f);
            D::ms_lookAir[0] = 80.0f;
            for (int i = 0; i < 60; i++)
            {
                D::ProcessMoving();
                D::Fade();
            }
            check(splat->x == sx && splat->y == sy && splat->alpha == 255 && D::ms_numTraces == 0,
                "the air and the turning camera do not move blood, and it stays whole for a while");
            for (int i = 0; i < 60 * 6; i++)
                D::Fade();
            check(!splat->active, "and then it fades away");
        }

        // The water of the trails leaves a share of the pool to the rain, and the
        // rain takes the place of the water of a trail when nothing else is free.
        calm();
        timeStep = 1.0f / 60;
        auto* source = D::PlaceNew(960.0f, 540.0f, (float)biggest, 20000.0f, true);
        const int32_t pool = (int32_t)D::ms_drops.size();
        for (int32_t i = 0; i < pool * 2; i++)
            D::PlaceNew(100.0f, 100.0f, (float)smallest, 20000.0f, true, source->r, source->g, source->b, true);
        check(D::ms_numTraces == pool - D::TraceReserve(), "the water of the trails fills the pool up to the share it leaves to the rain");
        int32_t placed = 0;
        for (int32_t i = 0; i < pool; i++)
            if (D::PlaceNew(200.0f, 200.0f, (float)biggest, 20000.0f, true))
                placed++;
        check(placed == pool - 1 && D::ms_numDrops == pool && D::ms_numTraces == 0,
            "and a drop of the rain takes the place of the water of a trail when nothing else is free");

        D::Clear();
        D::ms_initialised = false;
        D::fps = 0;
        D::ms_vec = {};
        air(0.0f, 0.0f, 0.0f);
        D::MinSize = 4;
        D::MaxSize = 19;
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

    bool active[5] = { true, false, true, false, false };

    // --drive: the mock of the car and its road, see XrdTest::Mock
    if (XrdTest::Headless::State().drive)
    {
        mock.active = true;
        mock.speed = XrdTest::Headless::State().driveSpeed;
        mock.swing = XrdTest::Headless::State().swing;
        active[4] = true;
    }

    if (XrdTest::Headless::State().snow)
    {
        WaterDrops::SetSnow(true);
        active[0] = false;
        active[1] = true;
    }

    const auto enterDrive = [&]()
    {
        camera.yaw = 0.0f;
        camera.pitch = -0.07f;
        mock.active = true;
    };

    if (mock.active)
        enterDrive();

    ui.Build(XrdTest::g_uiLabels, 5, active, (float)window.width);

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

        // the drive: the car goes on and the camera hangs behind it, see XrdTest::Mock.
        // The rain of it is the rain of a game and not the downpour of a test.
        // --snow: the ini is read on the first frame and says what the drops are,
        // so the snow is asked for again until it holds
        if (XrdTest::Headless::State().snow && !WaterDrops::bEnableSnow)
            WaterDrops::SetSnow(true);

        if (mock.active)
        {
            XrdTest::Mock::Update(mock, camera, window, deltaTime, XrdTest::Headless::Active());
            // the camera of the mock looks along the road, which the effect takes
            // for a camera that looks up into the rain, so a game's worth of rain
            // is a third of what the games hand over at their heaviest
            WaterDrops::ms_rainIntensity = 0.6f;
        }
        else
            WaterDrops::ms_rainIntensity = 4.0f;

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
                case 4:
                    active[4] = !active[4];
                    mock.active = active[4];
                    if (mock.active)
                        enterDrive();
                    else
                        camera.autoMove = true;
                    break;
                }

                ui.Build(XrdTest::g_uiLabels, 5, active, (float)window.width);
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
            sprintf_s(extra, "camera %.1f %.1f %.1f  traces %d", camera.x, camera.y, camera.z, WaterDrops::ms_numTraces);
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
