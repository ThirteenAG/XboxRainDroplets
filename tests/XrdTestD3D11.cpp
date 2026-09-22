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
            const float x = (float)window.width * 0.5f;
            const float y = (float)window.height * 0.25f;

            const XrdTest::Rect lamps[] = {
                { x + 86.0f, y - 8.0f, 30.0f, 16.0f, { 1.0f, 0.05f, 0.02f, 1.0f } },
                { x - 116.0f, y - 8.0f, 30.0f, 16.0f, { 0.06f, 1.0f, 0.12f, 1.0f } },
            };

            DrawRects(lamps, 2);
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

        // A bead leaves water where it has been, and only once it has gone far
        // enough for the water to be a wake and not a pool on one spot. Which one
        // of the beads runs is not known in advance, so a bead that runs is asked
        // for until one is there.
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

        check(bead->trailCount == 0, "a bead that has only just started to run has left nothing yet");

        for (int i = 0; i < 200; i++)
            D::ProcessMoving();

        check(bead->trailCount > 0, "a bead that runs down the glass leaves water behind it");
        check(bead->trailY[bead->trailCount - 1] <= 0.0f, "the water is left behind the bead and not in front of it");
        check(bead->trailCount <= WaterDrop::TrailLength, "the water of one bead is a trail of a few places, not a pool");

        // How much water a bead left is a part of how visible the bead is and it
        // goes down with the bead, which is what the original code got wrong.
        D::ms_vertices.clear();
        D::AddToRenderList(bead);

        const uint8_t beadAlpha = (uint8_t)(D::ms_vertices.back().color >> 24);
        uint8_t brightest = 0;

        // the bead is the last quad that was added, the water is in front of it
        for (size_t i = 0; i + 4 < D::ms_vertices.size(); i += 4)
        {
            const uint8_t alpha = (uint8_t)(D::ms_vertices[i].color >> 24);

            if (alpha > brightest)
                brightest = alpha;
        }

        check(D::ms_vertices.size() > 4 && brightest < beadAlpha, "the water a bead left is drawn fainter than the bead");
        check(D::ms_vertices.size() == (size_t)(bead->trailCount + 1) * 4,
            "the water and the bead are drawn together and nothing else is");

        bead->alpha = 24;
        D::ms_vertices.clear();
        D::AddToRenderList(bead);

        uint8_t faded = 0;

        // the bead is the last quad that was added, the water is in front of it
        for (size_t i = 0; i + 4 < D::ms_vertices.size(); i++)
        {
            const uint8_t alpha = (uint8_t)(D::ms_vertices[i].color >> 24);

            if (alpha > faded)
                faded = alpha;
        }

        check(faded <= 24, "a bead that has almost faded out leaves water that can not be brighter than itself");
        bead->alpha = 255;

        // The water is on the glass, and the glass is what the camera moves: a
        // place stays where it was left on the screen while the camera drifts,
        // only the bead runs on to the next place by itself. The drift is small
        // enough that the bead does not run over another whole place in it.
        float was[WaterDrop::TrailLength] = {};
        const int32_t places = bead->trailCount;

        for (int32_t i = 0; i < places; i++)
            was[i] = bead->x + bead->trailX[i];

        D::ms_vec = { -5.0f, 2.0f, 0.0f };
        D::ProcessMoving();

        int32_t stayed = 0;

        for (int32_t i = 0; i < bead->trailCount; i++)
            for (int32_t j = 0; j < places; j++)
                if (fabsf((bead->x + bead->trailX[i]) - was[j]) < 0.001f)
                {
                    stayed++;
                    break;
                }

        check(places > 0 && stayed + 1 >= bead->trailCount,
            "the water stays where it was left on the screen while the camera drifts");

        // A bead that the camera drags across the screen in one frame has run over
        // more places of water than one, and all of them are drawn: the water of a
        // drag is left along the path, and not on the one spot the bead happens to
        // end up on, which is what a trail of dots is.
        D::Clear();
        D::ms_vec = {};
        auto* dragged = D::PlaceNew(1200.0f, 540.0f, (float)biggest, 20000.0f, true);
        D::NewDropMoving(dragged);
        D::ms_vec = { 200.0f, 0.0f, 0.0f };
        D::ProcessMoving();
        D::ms_vec = {};

        const float left = dragged->x;
        const float right = 1200.0f;
        int32_t onPath = 0;
        float nearest = 0.0f;
        float farthest = 0.0f;

        for (int32_t i = 0; i < dragged->trailCount; i++)
        {
            const float x = dragged->x + dragged->trailX[i];

            if (x >= left - 0.001f && x <= right + 0.001f)
                onPath++;

            if (i == dragged->trailCount - 1)
                nearest = x - left;

            if (i == 0)
                farthest = x - left;
        }

        check(dragged->trailCount >= D::TrailPlacesPerFrame && onPath == dragged->trailCount,
            "the path a bead is dragged over is drawn along with it, not one dot behind it");
        check(farthest > (right - left) * 0.7f && nearest > 0.0f &&
            nearest < (right - left) / (float)D::TrailPlacesPerFrame,
            "the water of a drag reaches back over the whole of it, thinnest at the far end");

        // The water dries out again, in the time of the effect and not in frames:
        // a second and a bit after the bead has gone, the place it left is dry.
        check(dragged->trailCount > 0, "the bead that was dragged left water behind it");

        // A frame of the effect's time of six units, so that the water of the
        // trail has dried out in a handful of frames instead of thousands. The
        // bead is held where it is, or a frame that long would have it run over a
        // new place of water on every one of them.
        timeStep = 6.0f;
        dragged->slide = 0.0f;

        int32_t dryingFrames = 0;

        while (dragged->trailCount > 0 && dryingFrames < 100)
        {
            D::ProcessMoving();
            dryingFrames++;
        }

        const int32_t expectedFrames = (int32_t)(D::TrailLife / D::GetTimeStepInMilliseconds()) + 2;

        timeStep = 1.0f / 60.0f;

        check(dryingFrames > 0 && dryingFrames <= expectedFrames,
            "the water of a trail dries out again instead of staying on the glass for good");

        // A bead that neither runs nor is moved leaves nothing at all: what is
        // left behind is the path of the bead, and it has none.
        D::ms_vec = {};
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

        check(hangingStill->trailCount == 0 && hangingStill->x == 960.0f,
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

        D::ms_vec = {};
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
