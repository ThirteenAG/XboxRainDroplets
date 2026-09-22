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
#include <shellapi.h>
#include <gdiplus.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
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

    // -----------------------------------------------------------------------
    // headless runs
    //
    // The applications are made to be looked at, which a build server can not do.
    // Started with --headless one draws a fixed number of frames without input,
    // takes pictures of the last few of them and checks where the drops of the
    // effect landed in them: the y of a game counts down from the top of the
    // frame and a backend that draws the drops the other way up is exactly what
    // the pictures catch. The last three frames carry one known drop each (and no
    // rain at all), so the picture of a frame on its own says where that drop
    // ended up. What can not be checked on the machine - there is no device for
    // the API at all, the window can not be read back - is a skip, not a failure.
    // -----------------------------------------------------------------------
    namespace Headless
    {
        constexpr int EXIT_PASSED = 0;
        constexpr int EXIT_FAILED = 1;
        constexpr int EXIT_SKIPPED = 2;

        // how many frames of the run carry the known drops, at the end of it
        constexpr int PICTURE_FRAMES = 3;

        struct Image
        {
            int width = 0;
            int height = 0;
            std::vector<uint32_t> pixels;	// 0x00RRGGBB, the top row first

            bool Empty() const { return width <= 0 || height <= 0 || pixels.empty(); }
        };

        struct Run
        {
            bool active = false;
            bool finished = false;
            bool lensLight = false;     // the world is dark and one light is behind the drop of the check
            bool trailCheck = false;    // no device at all, only the CPU side of the effect
            int frameCount = 180;
            int frameIndex = 0;
            std::wstring screenshot;
            const char* app = "";
            Image empty, topDrop, bottomDrop;
            std::string reason;			// why it was skipped or what went wrong
        };

        inline Run& State()
        {
            static Run run;
            return run;
        }

        inline bool Active()
        {
            return State().active;
        }

        inline const char* AppName()
        {
            return State().app;
        }

        inline int FrameCount()
        {
            return State().frameCount;
        }

        // A run has to be the same on every machine, so it does not use the time
        // it took to draw a frame.
        inline float DeltaTime()
        {
            return 1.0f / 60.0f;
        }

        inline void ParseCommandLine(const char* app)
        {
            Run& run = State();
            run.app = app;

            int argc = 0;
            LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

            if (!argv)
                return;

            for (int i = 1; i < argc; i++)
            {
                if (wcscmp(argv[i], L"--headless") == 0)
                {
                    run.active = true;
                }
                else if (wcscmp(argv[i], L"--frames") == 0 && i + 1 < argc)
                {
                    run.frameCount = _wtoi(argv[++i]);
                }
                else if (wcscmp(argv[i], L"--lens-light") == 0)
                {
                    run.lensLight = true;
                }
                else if (wcscmp(argv[i], L"--trail-check") == 0)
                {
                    run.trailCheck = true;
                }
                else if (wcscmp(argv[i], L"--screenshot") == 0 && i + 1 < argc)
                {
                    run.screenshot = argv[++i];
                }
            }

            LocalFree(argv);

            // the pictures are taken at the end of the run and a run of a few
            // frames has no drops to look at yet
            if (run.frameCount < PICTURE_FRAMES + 30)
                run.frameCount = PICTURE_FRAMES + 30;
        }

        // A headless run that can not have a device (no driver for the API, no
        // display) is a skip: the application says so and the caller stops.
        inline int DeviceFailed()
        {
            Run& run = State();

            if (!run.active)
                return EXIT_FAILED;

            run.reason = "no device for this API on this machine";
            printf("[%s] SKIP: %s\n", run.app[0] ? run.app : "test", run.reason.c_str());
            fflush(stdout);
            return EXIT_SKIPPED;
        }
        inline bool IsPictureFrame(int frameIndex)
        {
            return frameIndex >= State().frameCount - PICTURE_FRAMES;
        }

        // Called before the drops are processed: the last three frames of the run
        // are the ones the pictures are taken of, and what is on them is known.
        inline void PrepareDrops(int frameIndex, int width, int height)
        {
            Run& run = State();

            if (!run.active)
                return;

            if (!IsPictureFrame(frameIndex))
                return;

            // no rain of its own on the frames of the check, only the drop that is
            // placed here, so the difference between the pictures is that drop
            WaterDrops::ms_rainIntensity = 0.0f;
            WaterDrops::Clear();

            const int picture = frameIndex - (run.frameCount - PICTURE_FRAMES);

            // the y of a drop counts down from the top of the frame, and a drop of a
            // fifth of the height in the middle of it is easy to find again
            if (picture == 1)
                WaterDrops::PlaceNew(width * 0.5f, height * 0.25f, height / 5.0f, 60000.0f, false);
            else if (picture == 2)
                WaterDrops::PlaceNew(width * 0.5f, height * 0.75f, height / 5.0f, 60000.0f, false);

            // The crop the Direct3D games of the Definitive Edition ask the
            // refraction to sample, which is what the check with the lamps is run
            // with: a light is where it is on the screen, and the field a drop
            // looks its light up in must not be moved by this.
            if (run.lensLight)
                WaterDrops::SetXUVScale(0.125f, 0.875f);
        }

        inline bool CaptureWindow(HWND hwnd, Image& image)
        {
            RECT rect{};

            if (!GetClientRect(hwnd, &rect))
                return false;

            const int width = rect.right - rect.left;
            const int height = rect.bottom - rect.top;

            if (width <= 0 || height <= 0)
                return false;

            HDC hdc = GetDC(hwnd);
            HDC memory = CreateCompatibleDC(hdc);
            HBITMAP bitmap = CreateCompatibleBitmap(hdc, width, height);
            HGDIOBJ previous = SelectObject(memory, bitmap);

            bool ok = BitBlt(memory, 0, 0, width, height, hdc, 0, 0, SRCCOPY) != FALSE;

            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = width;
            info.bmiHeader.biHeight = -height;	// the top row of the window comes first
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 32;
            info.bmiHeader.biCompression = BI_RGB;

            image.pixels.resize((size_t)width * height);

            if (ok)
                ok = GetDIBits(hdc, bitmap, 0, height, image.pixels.data(), &info, DIB_RGB_COLORS) != 0;

            SelectObject(memory, previous);
            DeleteObject(bitmap);
            DeleteDC(memory);
            ReleaseDC(hwnd, hdc);

            if (!ok)
                return false;

            image.width = width;
            image.height = height;
            return true;
        }

        inline bool SameImage(const Image& a, const Image& b)
        {
            return a.pixels == b.pixels;
        }

        inline bool GetPngEncoder(CLSID& clsid)
        {
            UINT count = 0;
            UINT size = 0;

            if (Gdiplus::GetImageEncodersSize(&count, &size) != Gdiplus::Ok || size == 0)
                return false;

            std::vector<uint8_t> buffer(size);
            Gdiplus::ImageCodecInfo* pCodecs = (Gdiplus::ImageCodecInfo*)buffer.data();

            if (Gdiplus::GetImageEncoders(count, size, pCodecs) != Gdiplus::Ok)
                return false;

            for (UINT i = 0; i < count; i++)
            {
                if (wcscmp(pCodecs[i].MimeType, L"image/png") == 0)
                {
                    clsid = pCodecs[i].Clsid;
                    return true;
                }
            }

            return false;
        }

        // The picture of a frame, next to the file the run was asked for. It is
        // written even when the check fails, so a build server has something to
        // show for it.
        inline bool SaveImage(const Image& image, const std::wstring& path)
        {
            if (image.Empty())
                return false;

            Gdiplus::GdiplusStartupInput input;
            ULONG_PTR token = 0;

            if (Gdiplus::GdiplusStartup(&token, &input, nullptr) != Gdiplus::Ok)
                return false;

            bool result = false;

            {
                Gdiplus::Bitmap bitmap(image.width, image.height, PixelFormat32bppARGB);
                Gdiplus::Rect rect(0, 0, image.width, image.height);
                Gdiplus::BitmapData data{};

                if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeWrite, PixelFormat32bppARGB, &data) == Gdiplus::Ok)
                {
                    for (int y = 0; y < image.height; y++)
                        memcpy((uint8_t*)data.Scan0 + (size_t)y * data.Stride, image.pixels.data() + (size_t)y * image.width, (size_t)image.width * 4);

                    bitmap.UnlockBits(&data);

                    CLSID encoder{};

                    if (GetPngEncoder(encoder))
                        result = bitmap.Save(path.c_str(), &encoder, nullptr) == Gdiplus::Ok;
                }
            }

            Gdiplus::GdiplusShutdown(token);
            return result;
        }

        struct Changed
        {
            int count = 0;
            double centerX = 0.0;
            double centerY = 0.0;
        };

        // What the known drop changed in the frame, and where that is: a drop at a
        // quarter of the height of the frame has to change the upper part of the
        // picture, one at three quarters the lower part.
        inline Changed Difference(const Image& a, const Image& b)
        {
            Changed changed;

            if (a.Empty() || b.Empty() || a.width != b.width || a.height != b.height)
                return changed;

            double sumX = 0.0;
            double sumY = 0.0;

            for (int y = 0; y < a.height; y++)
            {
                const uint32_t* pRow = &a.pixels[(size_t)y * a.width];
                const uint32_t* qRow = &b.pixels[(size_t)y * b.width];

                for (int x = 0; x < a.width; x++)
                {
                    const int dr = abs((int)((pRow[x] >> 16) & 0xFF) - (int)((qRow[x] >> 16) & 0xFF));
                    const int dg = abs((int)((pRow[x] >> 8) & 0xFF) - (int)((qRow[x] >> 8) & 0xFF));
                    const int db = abs((int)(pRow[x] & 0xFF) - (int)(qRow[x] & 0xFF));

                    if (dr > 8 || dg > 8 || db > 8)
                    {
                        changed.count++;
                        sumX += x;
                        sumY += y;
                    }
                }
            }

            if (changed.count > 0)
            {
                changed.centerX = sumX / changed.count;
                changed.centerY = sumY / changed.count;
            }

            return changed;
        }

        // Called once the frame was presented. The last three frames of a run are
        // the ones the pictures are taken of, and true is returned when the run is
        // over and the result is there to be asked for.
        inline bool AfterPresent(HWND hwnd, int frameIndex)
        {
            Run& run = State();

            if (!run.active)
                return false;

            if (IsPictureFrame(frameIndex))
            {
                Image& image = (frameIndex == run.frameCount - PICTURE_FRAMES) ? run.empty
                    : (frameIndex == run.frameCount - PICTURE_FRAMES + 1) ? run.topDrop : run.bottomDrop;

                if (!CaptureWindow(hwnd, image))
                {
                    run.finished = true;
                    run.reason = "the window could not be read back on this machine";
                    return true;
                }
            }

            run.frameIndex = frameIndex + 1;
            run.finished = run.frameIndex >= run.frameCount;
            return run.finished;
        }

        inline void WritePicture(const Image& image, const std::wstring& base, const wchar_t* suffix)
        {
            if (image.Empty() || base.empty())
                return;

            SaveImage(image, base + suffix + L".png");
        }

        // What the pictures of the run say, and the exit code that goes with it.
        inline int Result()
        {
            Run& run = State();
            const char* app = run.app[0] ? run.app : "test";

            const auto report = [&](const char* text)
            {
                printf("[%s] %s\n", app, text);
                fflush(stdout);
            };

            if (!run.active)
                return EXIT_PASSED;

            if (!run.reason.empty())
            {
                WritePicture(run.empty, run.screenshot, L"-empty");
                WritePicture(run.topDrop, run.screenshot, L"-top");
                WritePicture(run.bottomDrop, run.screenshot, L"-bottom");

                report(run.reason.c_str());
                return EXIT_SKIPPED;
            }

            // A window that could not be read back at all comes back as one colour,
            // which is not the fault of the effect.
            if (run.empty.Empty() || SameImage(run.empty, run.topDrop) || SameImage(run.empty, run.bottomDrop))
            {
                report("SKIP: the window reads back empty, nothing could be checked");
                return EXIT_SKIPPED;
            }

            const Changed top = Difference(run.empty, run.topDrop);
            const Changed bottom = Difference(run.empty, run.bottomDrop);

            const int width = run.empty.width;
            const int height = run.empty.height;

            // the drop has to be drawn at all, in the middle of the frame, and in
            // the half of it the y of the drop said
            const auto landed = [&](const Changed& changed, double expected)
            {
                if (changed.count < 200)
                    return false;

                if (fabs(changed.centerX - width * 0.5) > width * 0.15)
                    return false;

                return fabs(changed.centerY - height * expected) <= height * 0.15;
            };

            const bool topOk = landed(top, 0.25);
            const bool bottomOk = landed(bottom, 0.75);

            WritePicture(run.empty, run.screenshot, L"-empty");
            WritePicture(run.topDrop, run.screenshot, L"-top");
            WritePicture(run.bottomDrop, run.screenshot, L"-bottom");

            if (topOk && bottomOk)
            {
                report("PASSED: a drop at the top of the frame is drawn in the upper half of it, one at the bottom in the lower half");
                return EXIT_PASSED;
            }

            char buffer[256]{};
            sprintf_s(buffer, "FAILED: the drops are not where the frame says they are (top: %d px at %.0f,%.0f - bottom: %d px at %.0f,%.0f, frame %dx%d)",
                top.count, top.centerX, top.centerY, bottom.count, bottom.centerX, bottom.centerY, width, height);
            report(buffer);

            return EXIT_FAILED;
        }
    }
}
