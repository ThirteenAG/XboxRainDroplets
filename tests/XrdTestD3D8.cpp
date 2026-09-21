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

        D3DPRESENT_PARAMETERS present{};
        present.Windowed = TRUE;
        present.SwapEffect = D3DSWAPEFFECT_DISCARD;
        present.BackBufferFormat = displayMode.Format ? displayMode.Format : D3DFMT_X8R8G8B8;
        present.BackBufferWidth = window.width;
        present.BackBufferHeight = window.height;
        present.BackBufferCount = 1;
        present.hDeviceWindow = window.hwnd;
        // the two fullscreen fields have to stay zero in windowed mode, Direct3D
        // 8 rejects the device with an invalid call otherwise

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
    if (!window.Create(L"Xbox Rain Droplets - Direct3D 8", 1280, 720))
        return 1;

    if (!InitializeDevice())
        return 1;

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
    char extra[128]{};

    while (window.running)
    {
        window.Pump();

        const auto now = std::chrono::high_resolution_clock::now();
        deltaTime = std::chrono::duration<float>(now - previous).count();
        previous = now;

        if (deltaTime > 0.1f)
            deltaTime = 0.1f;

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

        if (frames < 3) { printf("frame: begin\n"); fflush(stdout); }
        if (SUCCEEDED(pDevice->BeginScene()))
        {
            pDevice->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255, 12, 12, 16), 1.0f, 0);

        if (frames < 3) { printf("frame: world\n"); fflush(stdout); }
            DrawWorld();

            WaterDrops::Process();
        if (frames < 3) { printf("frame: drops\n"); fflush(stdout); }
            WaterDrops::Render();

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
    if (pDevice) pDevice->Release();
    if (pD3D) pD3D->Release();

    return 0;
}
