#pragma once
// ---------------------------------------------------------------------------
// Shared bits of the test applications.
//
// Every application creates its own device and draws its own little scene, the
// UI included, but they all use the same window, the same fake camera and the
// same way of feeding the drop effect, which is what makes them comparable.
//
// The UI matters: it is drawn by the application *after* the drops, so it is
// easy to see that the drops ended up behind it, which is how they are meant to
// be used in a game.
// ---------------------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>
#include <chrono>

namespace XrdTest
{
    struct Color
    {
        float r, g, b, a;
    };

    struct Rect
    {
        float x, y, width, height;
        Color color;

        bool Contains(float px, float py) const
        {
            return px >= x && px <= x + width && py >= y && py <= y + height;
        }
    };

    // -----------------------------------------------------------------------
    // window
    // -----------------------------------------------------------------------
    struct Window
    {
        HWND hwnd = nullptr;
        int width = 1280;
        int height = 720;
        bool running = true;
        bool keys[256] = {};
        float mouseX = 0.0f;
        float mouseY = 0.0f;
        bool mouseDown = false;
        bool clicked = false;
        float wheel = 0.0f;

        static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
        {
            Window* pWindow = (Window*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

            switch (message)
            {
            case WM_CREATE:
                SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCT*)lParam)->lpCreateParams);
                return 0;

            case WM_CLOSE:
            case WM_DESTROY:
                if (pWindow)
                    pWindow->running = false;

                PostQuitMessage(0);
                return 0;

            case WM_SIZE:
                if (pWindow && wParam != SIZE_MINIMIZED)
                {
                    pWindow->width = LOWORD(lParam);
                    pWindow->height = HIWORD(lParam);
                }
                return 0;

            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
                if (pWindow)
                    pWindow->keys[wParam & 0xFF] = true;
                return 0;

            case WM_KEYUP:
            case WM_SYSKEYUP:
                if (pWindow)
                    pWindow->keys[wParam & 0xFF] = false;
                return 0;

            case WM_MOUSEMOVE:
                if (pWindow)
                {
                    pWindow->mouseX = (float)GET_X_LPARAM(lParam);
                    pWindow->mouseY = (float)GET_Y_LPARAM(lParam);
                }
                return 0;

            case WM_LBUTTONDOWN:
                if (pWindow)
                {
                    pWindow->mouseDown = true;
                    pWindow->clicked = true;
                }
                return 0;

            case WM_LBUTTONUP:
                if (pWindow)
                    pWindow->mouseDown = false;
                return 0;

            case WM_MOUSEWHEEL:
                if (pWindow)
                    pWindow->wheel = (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA;
                return 0;
            }

            return DefWindowProc(hwnd, message, wParam, lParam);
        }

        bool Create(const wchar_t* title, int windowWidth, int windowHeight)
        {
            width = windowWidth;
            height = windowHeight;

            WNDCLASSEXW windowClass{};
            windowClass.cbSize = sizeof(windowClass);
            windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
            windowClass.lpfnWndProc = WndProc;
            windowClass.hInstance = GetModuleHandleW(nullptr);
            windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
            windowClass.lpszClassName = L"XrdTestWindow";

            if (!RegisterClassExW(&windowClass))
                return false;

            RECT rect = { 0, 0, width, height };
            AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

            hwnd = CreateWindowExW(0, windowClass.lpszClassName, title, WS_OVERLAPPEDWINDOW,
                CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top,
                nullptr, nullptr, windowClass.hInstance, this);

            if (!hwnd)
                return false;

            ShowWindow(hwnd, SW_SHOW);
            UpdateWindow(hwnd);

            GetClientRect(hwnd, &rect);
            width = rect.right - rect.left;
            height = rect.bottom - rect.top;
            return true;
        }

        void Pump()
        {
            MSG message{};
            while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&message);
                DispatchMessage(&message);
            }
        }
    };

    // -----------------------------------------------------------------------
    // fake camera
    //
    // The drops slide in the direction the camera moves, so the effect needs a
    // camera position and the basis of its orientation. A camera flying through
    // a little world drives that: WASD moves, the mouse looks around, and the
    // basis is handed to the effect every frame.
    // -----------------------------------------------------------------------
    struct Camera
    {
        float yaw = 0.0f;
        float pitch = 0.0f;
        float x = 0.0f;
        float y = 0.0f;
        float z = -6.0f;
        float speed = 6.0f;
        float autoYawSpeed = 0.35f;
        bool autoMove = true;
        bool dragging = false;
        float lastMouseX = 0.0f;
        float lastMouseY = 0.0f;
        float left = 1.0f, right = 0.0f;
        float moveX = 0.0f, moveY = 0.0f, moveZ = 0.0f;

        void Update(Window& window, float deltaTime)
        {
            if (window.clicked && window.mouseY > 120.0f)
            {
                dragging = true;
                lastMouseX = window.mouseX;
                lastMouseY = window.mouseY;
            }

            if (!window.mouseDown)
                dragging = false;

            if (dragging)
            {
                yaw -= (window.mouseX - lastMouseX) * 0.005f;
                pitch -= (window.mouseY - lastMouseY) * 0.005f;
                lastMouseX = window.mouseX;
                lastMouseY = window.mouseY;
            }

            if (autoMove)
                yaw += autoYawSpeed * deltaTime;

            if (window.keys['A'])
                x -= speed * deltaTime;

            if (window.keys['D'])
                x += speed * deltaTime;

            if (window.keys['W'])
                z += speed * deltaTime;

            if (window.keys['S'])
                z -= speed * deltaTime;

            if (window.keys['Q'])
                y -= speed * deltaTime;

            if (window.keys['E'])
                y += speed * deltaTime;

            // how far the camera travelled since the previous frame, the effect
            // turns that into the direction the drops slide in
            moveX = x - previousX;
            moveY = y - previousY;
            moveZ = z - previousZ;
            previousX = x;
            previousY = y;
            previousZ = z;
        }

    private:
        float previousX = 0.0f;
        float previousY = 0.0f;
        float previousZ = 0.0f;
    };

    // -----------------------------------------------------------------------
    // the panel the applications draw over the drops
    // -----------------------------------------------------------------------
    struct Ui
    {
        std::vector<Rect> rects;
        std::vector<const char*> labels;

        void Build(const char* const* pLabels, int count, const bool* pActive, float screenWidth)
        {
            (void)screenWidth;

            rects.clear();
            labels.clear();

            const float x = 20.0f;
            float y = 20.0f;
            const float width = 300.0f;
            const float height = 34.0f;

            // the panel behind the buttons
            Rect panel{};
            panel.x = x - 10.0f;
            panel.y = y - 10.0f;
            panel.width = width + 20.0f;
            panel.height = (height + 6.0f) * count + 20.0f;
            panel.color = { 0.05f, 0.06f, 0.08f, 0.75f };
            rects.push_back(panel);

            for (int i = 0; i < count; i++)
            {
                Rect button{};
                button.x = x;
                button.y = y;
                button.width = width;
                button.height = height;
                button.color = pActive[i] ? Color{ 0.20f, 0.55f, 0.95f, 1.0f } : Color{ 0.16f, 0.17f, 0.20f, 1.0f };
                rects.push_back(button);
                labels.push_back(pLabels[i]);
                y += height + 6.0f;
            }
        }

        // -1 when nothing of the panel was hit
        int HitTest(float px, float py) const
        {
            for (size_t i = 1; i < rects.size(); i++)
                if (rects[i].Contains(px, py))
                    return (int)(i - 1);

            return -1;
        }
    };

    inline const char* const g_uiLabels[] =
    {
        "Rain (1)",
        "Snow (2)",
        "Gravity (3)",
        "Paused (4)",
    };

    inline void PrintStatus(const char* app, int numDrops, int fps, bool snow, const char* extra)
    {
        char buffer[256]{};
        sprintf_s(buffer, "[%s] drops: %d  fps: %d  %s%s\n", app, numDrops, fps, snow ? "snow " : "rain ", extra ? extra : "");
        OutputDebugStringA(buffer);
        printf("%s", buffer);
        fflush(stdout);
    }
}
