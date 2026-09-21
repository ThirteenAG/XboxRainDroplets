// ---------------------------------------------------------------------------
// Test application for the Direct3D 9 backend.
//
// It draws a small world, runs the drop effect over it and then draws a UI on
// top of everything, which is the placement the games use: the drops belong to
// the scene, the UI belongs after them.
// ---------------------------------------------------------------------------

#define XRD_ENABLE_D3D9 1
#include "xrd/xrd.h"

#include "XrdTest.h"

#include <d3d9.h>

namespace
{
    IDirect3D9* pD3D = nullptr;
    IDirect3DDevice9* pDevice = nullptr;
    IDirect3DVertexBuffer9* pVertexBuffer = nullptr;
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
        pD3D = Direct3DCreate9(D3D_SDK_VERSION);
        if (!pD3D)
            return false;

        D3DPRESENT_PARAMETERS present{};
        present.Windowed = TRUE;
        present.SwapEffect = D3DSWAPEFFECT_DISCARD;
        present.BackBufferFormat = D3DFMT_X8R8G8B8;
        present.BackBufferWidth = window.width;
        present.BackBufferHeight = window.height;
        present.BackBufferCount = 1;
        present.hDeviceWindow = window.hwnd;
        present.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

        if (FAILED(pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window.hwnd,
            D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED, &present, &pDevice)))
        {
            if (FAILED(pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window.hwnd,
                D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED, &present, &pDevice)))
                return false;
        }

        return SUCCEEDED(pDevice->CreateVertexBuffer(MaxVertices * sizeof(ScreenVertex), D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC, D3DFVF_XYZRHW | D3DFVF_DIFFUSE, D3DPOOL_DEFAULT, &pVertexBuffer, nullptr));
    }

    void DrawRects(const XrdTest::Rect* pRects, int count)
    {
        ScreenVertex* pVertices = nullptr;

        if (FAILED(pVertexBuffer->Lock(0, count * 6 * sizeof(ScreenVertex), (void**)&pVertices, D3DLOCK_DISCARD)))
            return;

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

            // a triangle list needs six vertices for one rectangle
            const int order[6] = { 0, 1, 2, 0, 2, 3 };

            for (int v = 0; v < 6; v++)
                pVertices[n++] = corners[order[v]];
        }

        pVertexBuffer->Unlock();

        pDevice->SetStreamSource(0, pVertexBuffer, 0, sizeof(ScreenVertex));
        pDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
        pDevice->SetTexture(0, nullptr);
        pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
        pDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
        pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        pDevice->SetVertexShader(nullptr);
        pDevice->SetPixelShader(nullptr);
        pDevice->DrawPrimitive(D3DPT_TRIANGLELIST, 0, count * 2);
    }

    // A colourful little world for the drops to refract. Everything is a screen
    // rectangle, moving with the camera, so it works on every API the same way.
    void DrawWorld()
    {
        std::vector<XrdTest::Rect> rects;

        XrdTest::Rect sky{};
        sky.x = 0.0f;
        sky.y = 0.0f;
        sky.width = (float)window.width;
        sky.height = (float)window.height * 0.62f;
        sky.color = { 0.35f, 0.45f, 0.65f, 1.0f };
        rects.push_back(sky);

        XrdTest::Rect ground{};
        ground.x = 0.0f;
        ground.y = sky.height;
        ground.width = (float)window.width;
        ground.height = (float)window.height - sky.height;
        ground.color = { 0.18f, 0.20f, 0.16f, 1.0f };
        rects.push_back(ground);

        // buildings that scroll with the yaw of the camera
        for (int i = 0; i < 48; i++)
        {
            const float offset = fmodf((float)i * 137.0f - camera.yaw * 900.0f, (float)window.width + 240.0f);
            const float x = offset - 120.0f;
            const float heightFraction = 0.18f + 0.32f * (float)((i * 37) % 100) / 100.0f;

            XrdTest::Rect building{};
            building.x = x;
            building.width = 40.0f + (float)((i * 53) % 60);
            building.height = (float)window.height * 0.62f * heightFraction;
            building.y = (float)window.height * 0.62f - building.height;
            building.color = { 0.10f + 0.35f * (float)((i * 17) % 100) / 100.0f, 0.12f, 0.22f + 0.3f * (float)((i * 29) % 100) / 100.0f, 1.0f };
            rects.push_back(building);

            XrdTest::Rect window1{};
            window1.x = building.x + 8.0f;
            window1.y = building.y + 10.0f;
            window1.width = building.width - 16.0f;
            window1.height = 6.0f;
            window1.color = { 0.95f, 0.85f, 0.45f, 1.0f };
            rects.push_back(window1);
        }

        // a few horizontal lines running towards the camera
        for (int i = 0; i < 40; i++)
        {
            const float z = fmodf((float)i * 2.5f + camera.z * 6.0f, 100.0f);
            const float y = sky.height + (float)window.height * 0.38f * (z / 100.0f) * (z / 100.0f);

            XrdTest::Rect line{};
            line.x = 0.0f;
            line.y = y;
            line.width = (float)window.width;
            line.height = 1.5f;
            line.color = { 0.30f, 0.34f, 0.30f, 1.0f };
            rects.push_back(line);
        }

        DrawRects(rects.data(), (int)rects.size());
    }

    void DrawUi()
    {
        DrawRects(ui.rects.data(), (int)ui.rects.size());

        // a marker that shows which button was hit, and the window title carries
        // the numbers, no font is involved
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
        WaterDrops::up = { -sinf(camera.yaw) * sinf(camera.pitch), cosPitch == 0.0f ? 1.0f : cosPitch, -cosf(camera.yaw) * sinf(camera.pitch) };
        WaterDrops::at = { sinf(camera.yaw) * cosPitch, sinf(camera.pitch), cosf(camera.yaw) * cosPitch };
        WaterDrops::pos = { camera.x, camera.y, camera.z };
    }
}

int main()
{
    if (!window.Create(L"Xbox Rain Droplets - Direct3D 9", 1280, 720))
        return 1;

    if (!InitializeDevice())
        return 1;

    // the effect talks to the renderer, which talks to the device
    Xrd::Init(Xrd::RENDERER_D3D9, pDevice);
    WaterDrops::fTimeStep = &deltaTime;
    // the games read this from the weather, a test wants a lot of rain
    WaterDrops::ms_rainIntensity = 4.0f;

    const bool uiActive[4] = { !WaterDrops::bEnableSnow, WaterDrops::bEnableSnow, WaterDrops::bGravity, WaterDrops::isPaused };
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

        if (SUCCEEDED(pDevice->BeginScene()))
        {
            pDevice->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255, 12, 12, 16), 1.0f, 0);

            DrawWorld();

            // the drops go over the world, exactly what the games do at the end
            // of the scene
            WaterDrops::Process();
            WaterDrops::Render();

            // and the UI covers them, which is the whole point of the placement
            DrawUi();

            pDevice->EndScene();
        }

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
            sprintf_s(extra, "camera %.1f %.1f %.1f  mask %s", camera.x, camera.y, camera.z, WaterDrops::ms_atlasUsed ? "atlas" : "fallback");
            XrdTest::PrintStatus("d3d9", WaterDrops::ms_numDrops, frames, WaterDrops::bEnableSnow, extra);

            wchar_t title[160]{};
            swprintf_s(title, L"Xbox Rain Droplets - Direct3D 9  |  drops %d  fps %d", WaterDrops::ms_numDrops, frames);
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
