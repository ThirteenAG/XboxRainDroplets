// ---------------------------------------------------------------------------
// Test application for the Direct3D 8 backend.
//
// Same scene, same UI and the same effect calls as the other applications, it
// only shows that the Direct3D 8 backend draws the same thing.
// ---------------------------------------------------------------------------

// this application is a Direct3D 8 one, which is all the effect has to know to
// pick the headers, the device type and the renderer of that API
#define XRD_ENABLE_D3D8 1
#define XRD_NO_D3DX 1
#include "xrd/xrd.h"

#include "XrdTest.h"

namespace
{
    IDirect3D8* pD3D = nullptr;
    IDirect3DDevice8* pDevice = nullptr;
    IDirect3DVertexBuffer8* pVertexBuffer = nullptr;
    // the depth of the frame a texture can be made of, see CreateDevice
    IDirect3DTexture8* pDepthTexture = nullptr;
    D3DPRESENT_PARAMETERS present{};
    constexpr int MaxVertices = 4096;

    struct ScreenVertex
    {
        float x, y, z, rhw;
        uint32_t color;
    };

    XrdTest::Window window;
    XrdTest::Camera camera;
    XrdTest::Ui ui;

    float deltaTime = 1.0f / 60.0f;
    int uiSelection = 0;

    bool InitializeDevice()
    {
        pD3D = Direct3DCreate8(D3D_SDK_VERSION);

        if (!pD3D)
            return false;

        D3DDISPLAYMODE displayMode{};
        pD3D->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &displayMode);

        printf("display format %u %ux%u\n", (unsigned)displayMode.Format, displayMode.Width, displayMode.Height);
        fflush(stdout);

        present.Windowed = TRUE;
        present.SwapEffect = D3DSWAPEFFECT_DISCARD;
        present.BackBufferFormat = displayMode.Format ? displayMode.Format : D3DFMT_X8R8G8B8;
        present.BackBufferWidth = window.width;
        present.BackBufferHeight = window.height;
        present.BackBufferCount = 1;
        present.hDeviceWindow = window.hwnd;
        // The two fullscreen fields have to stay zero in windowed mode, Direct3D
        // 8 rejects the device with an invalid call otherwise
        //
        // A depth buffer of its own, because every game this backend draws for has
        // one: the light of the frame is found with none of its own and puts back
        // what it found, and without one here that is not put to the test.
        present.EnableAutoDepthStencil = TRUE;
        present.AutoDepthStencilFormat = D3DFMT_D24S8;

        HRESULT hr = pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window.hwnd,
            D3DCREATE_HARDWARE_VERTEXPROCESSING, &present, &pDevice);

        printf("hardware device: %x, adapters %u\n", (unsigned)hr, pD3D->GetAdapterCount());
        fflush(stdout);

        if (FAILED(hr))
        {
            hr = pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window.hwnd,
                D3DCREATE_SOFTWARE_VERTEXPROCESSING, &present, &pDevice);

            printf("software device: %x\n", (unsigned)hr);
            fflush(stdout);

            if (FAILED(hr))
            {
                hr = pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_REF, window.hwnd,
                    D3DCREATE_SOFTWARE_VERTEXPROCESSING, &present, &pDevice);

                printf("reference device: %x\n", (unsigned)hr);
                fflush(stdout);

                if (FAILED(hr))
                    return false;
            }
        }

        const HRESULT hrBuffer = pDevice->CreateVertexBuffer(MaxVertices * sizeof(ScreenVertex), D3DUSAGE_WRITEONLY, D3DFVF_XYZRHW | D3DFVF_DIFFUSE, D3DPOOL_MANAGED, &pVertexBuffer);
        printf("device created, vertex buffer %x\n", (unsigned)hrBuffer);
        fflush(stdout);

        // A depth buffer a texture can be made of, the way the games that read the
        // depth of their own frame do it (see DepthStencil.ixx of the widescreen fix
        // of True Crime: New York City): the depth buffer a device hands out is of a
        // kind no texture can be made of, so the pass that reads the depth of the
        // frame and leaves the light of it to what is close to the camera has
        // nothing to read without one of these.
        if (pD3D->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8,
            D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_TEXTURE, (D3DFORMAT)MAKEFOURCC('I', 'N', 'T', 'Z')) == D3D_OK &&
            SUCCEEDED(pDevice->CreateTexture(window.width, window.height, 1, D3DUSAGE_DEPTHSTENCIL,
                (D3DFORMAT)MAKEFOURCC('I', 'N', 'T', 'Z'), D3DPOOL_DEFAULT, &pDepthTexture)))
        {
            IDirect3DSurface8* pDepthSurface = nullptr;
            IDirect3DSurface8* pBackBuffer = nullptr;

            if (SUCCEEDED(pDepthTexture->GetSurfaceLevel(0, &pDepthSurface)) &&
                SUCCEEDED(pDevice->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer)))
            {
                pDevice->SetRenderTarget(pBackBuffer, pDepthSurface);
                printf("depth texture created\n");
                fflush(stdout);
            }

            if (pBackBuffer) pBackBuffer->Release();
            if (pDepthSurface) pDepthSurface->Release();
        }

        return SUCCEEDED(hrBuffer);
    }

    void DrawRects(const XrdTest::Rect* pRects, int count)
    {
        BYTE* pVertices = nullptr;

        if (FAILED(pVertexBuffer->Lock(0, count * 6 * sizeof(ScreenVertex), &pVertices, 0)))
            return;

        ScreenVertex* pData = (ScreenVertex*)pVertices;
        int n = 0;

        for (int i = 0; i < count; i++)
        {
            const XrdTest::Rect& rect = pRects[i];
            const uint32_t color = Xrd::ColorFloat(rect.color.r, rect.color.g, rect.color.b, rect.color.a);
            const float x1 = rect.x;
            const float y1 = rect.y;
            const float x2 = rect.x + rect.width;
            const float y2 = rect.y + rect.height;

            const ScreenVertex corners[4] =
            {
                { x1, y1, 0.0f, 1.0f, color },
                { x2, y1, 0.0f, 1.0f, color },
                { x2, y2, 0.0f, 1.0f, color },
                { x1, y2, 0.0f, 1.0f, color },
            };

            const int order[6] = { 0, 1, 2, 0, 2, 3 };

            for (int v = 0; v < 6; v++)
                pData[n++] = corners[order[v]];
        }

        pVertexBuffer->Unlock();

        pDevice->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
        pDevice->SetStreamSource(0, pVertexBuffer, sizeof(ScreenVertex));
        pDevice->SetTexture(0, nullptr);
        pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
        pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
        pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
        pDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
        pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, count * 2);
    }

    void DrawWorld()
    {
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

        for (int i = 0; i < 40; i++)
        {
            const float z = fmodf((float)i * 2.5f + camera.z * 6.0f, 100.0f);
            const float y = (float)window.height * 0.62f + (float)window.height * 0.38f * (z / 100.0f) * (z / 100.0f);
            rects.push_back({ 0.0f, y, (float)window.width, 1.5f, { 0.30f, 0.34f, 0.30f, 1.0f } });
        }

        // --lens-light: the world is dark and two bright lamps sit right next to
        // the drop the check places at the top of the frame, one red and one green.
        // What a drop of clear water gathers out of the frame around it is then the
        // only light there is, and the drop is the same drop in the same place
        // either way, so running this once with and once without Refractions in the
        // ini is what the light gathering of this renderer looks like.
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

            // and four small ones, the size a rear light of a car has, at a range
            // a drop on the glass of a car that light belongs to is at, to see
            // whether a light that small is found by the gather at all
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
    XrdTest::Headless::ParseCommandLine("d3d8");

    if (XrdTest::Headless::Active())
        camera.autoYawSpeed = 0.0f;	// the pictures of the check are compared to each other

    if (!window.Create(L"Xbox Rain Droplets - Direct3D 8", 1280, 720))
        return 1;

    if (!InitializeDevice())
        return XrdTest::Headless::DeviceFailed();

    printf("renderer: %d\n", Xrd::Init(Xrd::RENDERER_D3D8, pDevice) ? 1 : 0);
    fflush(stdout);
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

    // and whether the drops left the drawing state of the application changed
    bool stateLeak = false;
    const bool expectD3D9 = strstr(GetCommandLineA(), "--expect-d3d9") != nullptr;
    const bool expectD3D8 = strstr(GetCommandLineA(), "--expect-d3d8") != nullptr;
    const bool resetCheck = strstr(GetCommandLineA(), "--reset-check") != nullptr;
    const bool targetCheck = strstr(GetCommandLineA(), "--target-check") != nullptr;
    int result = 0;

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

        if (resetCheck && frameIndex == 10)
        {
            Xrd::Reset();
            if (pDepthTexture) { pDepthTexture->Release(); pDepthTexture = nullptr; }
            const HRESULT hr = pDevice->Reset(&present);
            printf("[%s] device reset: %x\n", SUCCEEDED(hr) ? "PASS" : "FAIL", (unsigned)hr);
            if (FAILED(hr)) { result = XrdTest::Headless::EXIT_FAILED; break; }
        }

        if (frames < 3) { printf("frame: begin\n"); fflush(stdout); }
        if (SUCCEEDED(pDevice->BeginScene()))
        {
            pDevice->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255, 12, 12, 16), 1.0f, 0);

        if (frames < 3) { printf("frame: world\n"); fflush(stdout); }
            DrawWorld();
            // Controlled source-depth regression: the receiving background stays
            // far away while only the depth at an identical lamp changes.
            const bool sourceNear = strstr(GetCommandLineA(), "--source-near") != nullptr;
            const bool sourceFar = strstr(GetCommandLineA(), "--source-far") != nullptr;
            if (sourceNear || sourceFar)
            {
                if (!pDepthTexture) { printf("source depth test requires INTZ\n"); return 2; }
                pDevice->Clear(0, nullptr, D3DCLEAR_ZBUFFER, 0, 0, 0);
                const XrdTest::Rect background = { 0, 0, (float)window.width,
                    window.height * 0.45f, { 0.03f, 0.03f, 0.03f, 1 } };
                DrawRects(&background, 1);
                const int x = (int)(window.width * XrdTest::Headless::State().dropX + 30);
                const int y = (int)(window.height * 0.25f);
                const XrdTest::Rect lamp = { (float)x, (float)y - 8, 16, 16, { 1, 0.02f, 0.01f, 1 } };
                DrawRects(&lamp, 1);
                if (sourceNear)
                {
                    const D3DRECT area{ x - 12, y - 20, x + 28, y + 20 };
                    pDevice->Clear(1, &area, D3DCLEAR_ZBUFFER, 0, 1, 0);
                }
            }


            XrdTest::Headless::PrepareDrops(frameIndex, window.width, window.height);

            WaterDrops::Process();

            // AUDIT: what a batch of drops leaves changed of the drawing state of the
            // application it was drawn for. A state block puts the drawing state back,
            // and the target, the depth buffer and the viewport are not in one: a game
            // left without its depth buffer draws no more of its own world.
            IDirect3DSurface8* pBeforeTarget = nullptr;
            IDirect3DSurface8* pBeforeDepth = nullptr;
            D3DVIEWPORT8 beforeViewport{};
            pDevice->GetRenderTarget(&pBeforeTarget);
            pDevice->GetDepthStencilSurface(&pBeforeDepth);
            pDevice->GetViewport(&beforeViewport);

            // Draw to the caller's surface while another target is bound. This
            // exercises the D3D8-to-D3D9 surface conversion and restoration.
            IDirect3DSurface8* scratch = nullptr;
            Xrd::RenderTarget explicitTarget{};
            if (targetCheck)
            {
                D3DSURFACE_DESC desc{};
                pBeforeTarget->GetDesc(&desc);
                if (FAILED(pDevice->CreateRenderTarget(desc.Width, desc.Height, desc.Format,
                    D3DMULTISAMPLE_NONE, FALSE, &scratch)))
                {
                    pBeforeTarget->Release();
                    if (pBeforeDepth) pBeforeDepth->Release();
                    pDevice->EndScene();
                    result = XrdTest::Headless::EXIT_FAILED;
                    break;
                }
                explicitTarget.resource = pBeforeTarget;
                explicitTarget.size = { (int32_t)desc.Width, (int32_t)desc.Height };
                Xrd::SetTarget(&explicitTarget);
                pDevice->SetRenderTarget(scratch, pBeforeDepth);
                pDevice->SetViewport(&beforeViewport);
            }

        if (frames < 3) { printf("frame: drops\n"); fflush(stdout); }
            WaterDrops::Render();
            Xrd::SetTarget(nullptr);

            if (frameIndex == 5 || (resetCheck && frameIndex == 15))
            {
                const bool usesD3D9 = static_cast<Xrd::D3D8Backend*>(Xrd::pBackend)->UsesD3D9();
                printf("drawing through %s\n", usesD3D9 ? "D3D9" : "D3D8");
                if ((expectD3D9 && !usesD3D9) || (expectD3D8 && usesD3D9))
                { printf("[FAIL] unexpected drawing backend\n"); stateLeak = true; }
            }

            IDirect3DSurface8* pAfterTarget = nullptr;
            IDirect3DSurface8* pAfterDepth = nullptr;
            D3DVIEWPORT8 afterViewport{};
            pDevice->GetRenderTarget(&pAfterTarget);
            pDevice->GetDepthStencilSurface(&pAfterDepth);
            pDevice->GetViewport(&afterViewport);

            if (frames == 5)
            {
                const bool sameTarget = (scratch ? scratch : pBeforeTarget) == pAfterTarget;
                const bool sameDepth = pBeforeDepth == pAfterDepth;
                const bool sameViewport = beforeViewport.X == afterViewport.X && beforeViewport.Y == afterViewport.Y &&
                    beforeViewport.Width == afterViewport.Width && beforeViewport.Height == afterViewport.Height;

                if (sameTarget && sameDepth && sameViewport)
                {
                    printf("[PASS] the drops left the target, the depth buffer and the viewport of the game as they found them\n");
                }
                else
                {
                    printf("[FAIL] the drops left the target %s, the depth buffer %s, the viewport %s\n",
                        sameTarget ? "as it was" : "CHANGED", sameDepth ? "as it was" : "CHANGED (none left)",
                        sameViewport ? "as it was" : "CHANGED");
                    printf("[FAIL] viewport before %u %u %u x %u, after %u %u %u x %u\n",
                        beforeViewport.X, beforeViewport.Y, beforeViewport.Width, beforeViewport.Height,
                        afterViewport.X, afterViewport.Y, afterViewport.Width, afterViewport.Height);
                    stateLeak = true;
                }

                fflush(stdout);
            }

            if (scratch)
            {
                pDevice->SetRenderTarget(pBeforeTarget, pBeforeDepth);
                pDevice->SetViewport(&beforeViewport);
                scratch->Release();
            }

            if (pBeforeTarget) pBeforeTarget->Release();
            if (pBeforeDepth) pBeforeDepth->Release();
            if (pAfterTarget) pAfterTarget->Release();
            if (pAfterDepth) pAfterDepth->Release();

            DrawUi();

            pDevice->EndScene();
        }

        if (frames < 3) { printf("frame: present\n"); fflush(stdout); }
        pDevice->Present(nullptr, nullptr, nullptr, nullptr);

        // A game runs at 50 to 60 frames a second and the effect measures its
        // time in those frames, so the application waits for the rest of the
        // frame. Without this it would run at thousands of frames a second and
        // every drop would age in a fraction of a second.
        const float frameTime = std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - now).count();

        if (frameTime < 1.0f / 60.0f)
            Sleep((DWORD)((1.0f / 60.0f - frameTime) * 1000.0f));

        // a headless run takes the pictures of its last few frames and is over
        if (XrdTest::Headless::AfterPresent(window.hwnd, frameIndex))
        {
            result = stateLeak ? XrdTest::Headless::EXIT_FAILED : XrdTest::Headless::Result();
            break;
        }

        frameIndex++;
        frames++;

        if (std::chrono::duration<float>(now - lastReport).count() >= 1.0f)
        {
            sprintf_s(extra, "camera %.1f %.1f %.1f", camera.x, camera.y, camera.z);
            XrdTest::PrintStatus("d3d8", WaterDrops::ms_numDrops, frames, WaterDrops::bEnableSnow, extra);

            wchar_t title[160]{};
            swprintf_s(title, L"Xbox Rain Droplets - Direct3D 8  |  drops %d  fps %d", WaterDrops::ms_numDrops, frames);
            SetWindowTextW(window.hwnd, title);

            frames = 0;
            lastReport = now;
        }
    }

    WaterDrops::Shutdown();
    Xrd::Shutdown();

    if (pVertexBuffer) pVertexBuffer->Release();
    if (pDepthTexture) pDepthTexture->Release();
    if (pDevice) pDevice->Release();
    if (pD3D) pD3D->Release();

    return result;
}
