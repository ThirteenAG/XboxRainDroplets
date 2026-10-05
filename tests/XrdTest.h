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
#include <algorithm>
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
    // the mock of a drive
    //
    // The drops of a camera on a car are what the effect is compared against, so
    // the applications can show one: a car on a road through a wood at dusk, in
    // the rain, with the camera behind it the way the camera of a racing game
    // hangs behind the car. The car drives on by itself, the mouse swings the
    // camera round it, W and S change the speed. The world is built out of
    // shaded polygons projected on the CPU and drawn back to front: a lit car
    // with its wing and its wheels, round trees, a wet road and hills in the
    // haze, which is about what a game of the generation the effect comes from
    // drew, and enough for the drops to have something to show.
    // -----------------------------------------------------------------------
    namespace Mock
    {
        // one quad of the picture, in pixels of the window, a colour at every
        // corner, the farthest first
        struct Quad
        {
            float x[4];
            float y[4];
            Color color[4];
            float depth;
        };

        struct World
        {
            bool active = false;
            float speed = 30.0f;    // units of the world a second, about metres
            bool swing = false;     // the camera swings round the car by itself, see --swing
            float carX = 0.0f;
            float carZ = 0.0f;
            float time = 0.0f;
        };

        // the camera as the scene sees it
        struct View
        {
            float px, py, pz;
            float right[3], up[3], at[3];
            float focal;
            int width, height;
        };

        // the light of the dusk: from behind the camera, above and a little to
        // the left, so that what faces the camera is lit
        inline const float LightDir[3] = { -0.30f, 0.80f, -0.52f };
        // the haze everything far away fades into
        inline const Color Haze = { 0.62f, 0.66f, 0.71f, 1.0f };

        // --night: the world is dark and only what gives light
        // of its own keeps its colour, the tail lights of the car and the lamps
        // along the road, which are what is drawn in a red of full strength
        inline bool Night = false;
        inline const float NightShade = 0.18f;

        inline float Hash(int n)
        {
            unsigned v = (unsigned)n * 2654435761u;
            v ^= v >> 13;
            v *= 0x5bd1e995u;
            v ^= v >> 15;
            return (float)(v & 0xFFFF) / 65535.0f;
        }

        inline Color Mul(Color c, float f)
        {
            return { c.r * f, c.g * f, c.b * f, c.a };
        }

        inline Color Mix(Color a, Color b, float t)
        {
            return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t };
        }

        inline void Update(World& world, Camera& camera, const Window& window, float dt, bool headless)
        {
            if (!headless)
            {
                if (window.keys['W'])
                    world.speed = (std::min)(70.0f, world.speed + 20.0f * dt);

                if (window.keys['S'])
                    world.speed = (std::max)(0.0f, world.speed - 20.0f * dt);
            }

            world.time += dt;
            world.carZ += world.speed * dt;
            // the car wanders over its lane a little, the way a driver steers
            world.carX = sinf(world.time * 0.35f) * 1.2f;

            // The camera hangs behind the car and looks at it, and the mouse swings
            // it round the car: it then travels sideways through the air, which is
            // what the wind of a swung camera is measured from.
            camera.autoMove = false;

            if (world.swing)
                camera.yaw = sinf(world.time * 1.2f) * 0.6f;

            const float distance = 5.2f;
            camera.x = world.carX - sinf(camera.yaw) * distance;
            camera.y = 1.45f;
            camera.z = world.carZ - cosf(camera.yaw) * distance;
        }

        inline View MakeView(const Camera& camera, int width, int height)
        {
            View view{};
            const float cosPitch = cosf(camera.pitch);
            view.px = camera.x;
            view.py = camera.y;
            view.pz = camera.z;
            view.right[0] = cosf(camera.yaw); view.right[1] = 0.0f; view.right[2] = -sinf(camera.yaw);
            view.up[0] = -sinf(camera.yaw) * sinf(camera.pitch); view.up[1] = cosPitch; view.up[2] = -cosf(camera.yaw) * sinf(camera.pitch);
            view.at[0] = sinf(camera.yaw) * cosPitch; view.at[1] = sinf(camera.pitch); view.at[2] = cosf(camera.yaw) * cosPitch;
            // a wide lens, the one of a racing game
            view.focal = (float)height * 0.85f;
            view.width = width;
            view.height = height;
            return view;
        }

        inline bool Project(const View& view, const float* p, float& sx, float& sy, float& depth)
        {
            const float dx = p[0] - view.px;
            const float dy = p[1] - view.py;
            const float dz = p[2] - view.pz;
            const float cx = dx * view.right[0] + dy * view.right[1] + dz * view.right[2];
            const float cy = dx * view.up[0] + dy * view.up[1] + dz * view.up[2];
            const float cz = dx * view.at[0] + dy * view.at[1] + dz * view.at[2];

            if (cz < 0.25f)
                return false;

            sx = (float)view.width * 0.5f + view.focal * cx / cz;
            sy = (float)view.height * 0.5f - view.focal * cy / cz;
            depth = cz;
            return true;
        }

        // how much the haze takes of something this far away
        inline float Fog(float depth)
        {
            return 1.0f - expf(-depth / 240.0f);
        }

        // the colour of a face in the light of the dusk, by the way it faces
        inline Color Lit(Color color, const float* normal)
        {
            const float length = sqrtf(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2]);
            const float d = length > 0.0f ? (normal[0] * LightDir[0] + normal[1] * LightDir[1] + normal[2] * LightDir[2]) / length : 0.0f;
            return Mul(color, 0.45f + 0.55f * (std::max)(d, 0.0f));
        }

        // A quad of the world with a colour at every corner. A face that is told
        // to is only drawn when it faces the camera: its normal is turned to point
        // away from the middle it was given, so the winding of it does not matter.
        // A quad that reaches behind the camera is cut off at the lens, so that
        // the road under the camera is there to the bottom of the picture.
        inline void PushQuad(std::vector<Quad>& out, const View& view, const float (&p)[4][3], const Color (&colors)[4], const float* middle)
        {
            if (middle)
            {
                const float ax = p[1][0] - p[0][0], ay = p[1][1] - p[0][1], az = p[1][2] - p[0][2];
                const float bx = p[2][0] - p[0][0], by = p[2][1] - p[0][1], bz = p[2][2] - p[0][2];
                float nx = ay * bz - az * by, ny = az * bx - ax * bz, nz = ax * by - ay * bx;
                const float ox = p[0][0] - middle[0], oy = p[0][1] - middle[1], oz = p[0][2] - middle[2];

                if (nx * ox + ny * oy + nz * oz < 0.0f)
                {
                    nx = -nx; ny = -ny; nz = -nz;
                }

                const float vx = p[0][0] - view.px, vy = p[0][1] - view.py, vz = p[0][2] - view.pz;

                if (nx * vx + ny * vy + nz * vz > 0.0f)
                    return;
            }

            // the corners in the space of the camera
            const float nearPlane = 0.3f;
            float c[4][3];
            int behind = 0;

            for (int i = 0; i < 4; i++)
            {
                const float dx = p[i][0] - view.px, dy = p[i][1] - view.py, dz = p[i][2] - view.pz;
                c[i][0] = dx * view.right[0] + dy * view.right[1] + dz * view.right[2];
                c[i][1] = dx * view.up[0] + dy * view.up[1] + dz * view.up[2];
                c[i][2] = dx * view.at[0] + dy * view.at[1] + dz * view.at[2];
                behind += c[i][2] < nearPlane ? 1 : 0;
            }

            if (behind == 4)
                return;

            // the polygon that is left in front of the lens, up to five corners
            float q[6][3];
            Color qc[6];
            int count = 0;

            for (int i = 0; i < 4; i++)
            {
                const int j = (i + 1) % 4;
                const bool inA = c[i][2] >= nearPlane, inB = c[j][2] >= nearPlane;

                if (inA)
                {
                    memcpy(q[count], c[i], sizeof(q[count]));
                    qc[count++] = colors[i];
                }

                if (inA != inB)
                {
                    const float f = (nearPlane - c[i][2]) / (c[j][2] - c[i][2]);
                    q[count][0] = c[i][0] + (c[j][0] - c[i][0]) * f;
                    q[count][1] = c[i][1] + (c[j][1] - c[i][1]) * f;
                    q[count][2] = nearPlane;
                    qc[count++] = Mix(colors[i], colors[j], f);
                }
            }

            if (count < 3)
                return;

            // one quad of the first four corners, and one more of what is left
            for (int first = 0; first < count - 2; first += 2)
            {
                Quad quad{};
                int used = 0;

                for (int k = 0; k < 4; k++)
                {
                    const int idx = k == 0 ? 0 : (std::min)(first + k, count - 1);
                    const float* v = q[idx];
                    quad.x[k] = (float)view.width * 0.5f + view.focal * v[0] / v[2];
                    quad.y[k] = (float)view.height * 0.5f - view.focal * v[1] / v[2];
                    quad.depth += v[2] * 0.25f;
                    quad.color[k] = Mix(qc[idx], { Haze.r, Haze.g, Haze.b, qc[idx].a }, Fog(v[2]));
                    used++;
                }

                out.push_back(quad);

                if (first + 3 >= count - 1)
                    break;
            }
        }

        inline void PushQuad(std::vector<Quad>& out, const View& view, const float (&p)[4][3], Color color, const float* middle)
        {
            const Color colors[4] = { color, color, color, color };
            PushQuad(out, view, p, colors, middle);
        }

        // the normal of a face, turned to point away from a middle
        inline void Normal(const float (&p)[4][3], const float* middle, float* n)
        {
            const float ax = p[1][0] - p[0][0], ay = p[1][1] - p[0][1], az = p[1][2] - p[0][2];
            const float bx = p[2][0] - p[0][0], by = p[2][1] - p[0][1], bz = p[2][2] - p[0][2];
            n[0] = ay * bz - az * by; n[1] = az * bx - ax * bz; n[2] = ax * by - ay * bx;

            if (middle)
            {
                const float ox = p[0][0] - middle[0], oy = p[0][1] - middle[1], oz = p[0][2] - middle[2];

                if (n[0] * ox + n[1] * oy + n[2] * oz < 0.0f)
                {
                    n[0] = -n[0]; n[1] = -n[1]; n[2] = -n[2];
                }
            }
        }

        // a flat quad on the ground, between two distances along the road and two
        // offsets across it, of one colour at the near edge and another at the far
        inline void Ground(std::vector<Quad>& out, const View& view, float x0, float x1, float z0, float z1, float y, Color nearColor, Color farColor)
        {
            const float p[4][3] = { { x0, y, z0 }, { x0, y, z1 }, { x1, y, z1 }, { x1, y, z0 } };
            const Color colors[4] = { nearColor, farColor, farColor, nearColor };
            PushQuad(out, view, p, colors, nullptr);
        }

        // a quad that stands up and faces the camera, darker at the foot
        inline void Billboard(std::vector<Quad>& out, const View& view, float x, float y, float z, float width, float height, Color foot, Color top)
        {
            const float rx = view.right[0] * width * 0.5f, rz = view.right[2] * width * 0.5f;
            const float p[4][3] = { { x - rx, y, z - rz }, { x - rx, y + height, z - rz }, { x + rx, y + height, z + rz }, { x + rx, y, z + rz } };
            const Color colors[4] = { foot, top, top, foot };
            PushQuad(out, view, p, colors, nullptr);
        }

        // A round shape that faces the camera: an ellipse of a colour in its
        // middle and another at its edge, which is what a crown of leaves, a bush
        // or a cloud is made of. It is drawn as a fan of pieces round the middle.
        inline void Disc(std::vector<Quad>& out, const View& view, float x, float y, float z, float radiusX, float radiusY, Color middle, Color edge, int segments = 10)
        {
            for (int i = 0; i < segments; i++)
            {
                const float a0 = (float)i / segments * 6.2831853f;
                const float a1 = (float)(i + 1) / segments * 6.2831853f;
                const float p[4][3] = {
                    { x, y, z },
                    { x + view.right[0] * cosf(a1) * radiusX, y + sinf(a1) * radiusY, z + view.right[2] * cosf(a1) * radiusX },
                    { x + view.right[0] * cosf(a0) * radiusX, y + sinf(a0) * radiusY, z + view.right[2] * cosf(a0) * radiusX },
                    { x + view.right[0] * cosf(a0) * radiusX, y + sinf(a0) * radiusY, z + view.right[2] * cosf(a0) * radiusX },
                };
                const Color colors[4] = { middle, edge, edge, edge };
                PushQuad(out, view, p, colors, nullptr);
            }

        }

        // A solid of the world: a profile in the plane along the road (z) and up
        // (y), pulled out across the road from one x to another. The sides are
        // quads between the points of the profile, the two ends are fans, every
        // face is lit by the way it faces and only drawn when it faces the camera.
        // A box, the body of a car, a wing and a wheel are all one of these.
        inline void Prism(std::vector<Quad>& out, const View& view, const float* profile, int points, float x0, float x1, Color color, Color endColor)
        {
            float middle[3] = { (x0 + x1) * 0.5f, 0.0f, 0.0f };

            for (int i = 0; i < points; i++)
            {
                middle[2] += profile[i * 2] / points;
                middle[1] += profile[i * 2 + 1] / points;
            }

            for (int i = 0; i < points; i++)
            {
                const int j = (i + 1) % points;
                const float p[4][3] = {
                    { x0, profile[i * 2 + 1], profile[i * 2] }, { x0, profile[j * 2 + 1], profile[j * 2] },
                    { x1, profile[j * 2 + 1], profile[j * 2] }, { x1, profile[i * 2 + 1], profile[i * 2] } };
                float n[3];
                Normal(p, middle, n);
                PushQuad(out, view, p, Lit(color, n), middle);
            }

            for (int end = 0; end < 2; end++)
            {
                const float x = end ? x1 : x0;
                const float n[3] = { end ? 1.0f : -1.0f, 0.0f, 0.0f };
                const Color lit = Lit(endColor, n);

                for (int i = 0; i < points; i++)
                {
                    const int j = (i + 1) % points;
                    const float p[4][3] = {
                        { x, middle[1], middle[2] }, { x, profile[i * 2 + 1], profile[i * 2] },
                        { x, profile[j * 2 + 1], profile[j * 2] }, { x, profile[j * 2 + 1], profile[j * 2] } };
                    PushQuad(out, view, p, lit, middle);
                }
            }
        }

        inline void Box(std::vector<Quad>& out, const View& view, float cx, float cy, float cz, float sx, float sy, float sz, Color color)
        {
            const float profile[8] = { cz - sz * 0.5f, cy - sy * 0.5f, cz - sz * 0.5f, cy + sy * 0.5f, cz + sz * 0.5f, cy + sy * 0.5f, cz + sz * 0.5f, cy - sy * 0.5f };
            Prism(out, view, profile, 4, cx - sx * 0.5f, cx + sx * 0.5f, color, color);
        }

        // a wheel: a tyre pulled out across its width, with a lighter hub
        inline void Wheel(std::vector<Quad>& out, const View& view, float x, float y, float z, float radius, float width)
        {
            float profile[24];

            for (int i = 0; i < 12; i++)
            {
                const float a = (float)i / 12.0f * 6.2831853f;
                profile[i * 2] = z + cosf(a) * radius;
                profile[i * 2 + 1] = y + sinf(a) * radius;
            }

            Prism(out, view, profile, 12, x - width * 0.5f, x + width * 0.5f, { 0.05f, 0.05f, 0.05f, 1.0f }, { 0.30f, 0.30f, 0.32f, 1.0f });
        }

        // The car, a hatchback of the rally kind: a body with a sloping nose and
        // a high tail, a cabin with its glass, a wing on two struts with its end
        // plates, four wheels, the lights and the plate, and a diffuser under
        // the bumper. Its own z runs forward along the road.
        inline void Car(std::vector<Quad>& out, const View& view, float cx, float cz)
        {
            const Color paint{ 0.13f, 0.17f, 0.30f, 1.0f };
            const Color trim{ 0.07f, 0.08f, 0.10f, 1.0f };
            const Color glass{ 0.10f, 0.12f, 0.16f, 1.0f };

            // the body, in profile from the rear bumper over the nose and back under
            const float body[] = {
                cz - 2.25f, 0.32f, cz - 2.28f, 0.70f, cz - 2.15f, 1.00f, cz - 1.40f, 1.04f, cz + 1.30f, 1.00f,
                cz + 2.05f, 0.80f, cz + 2.30f, 0.50f, cz + 2.20f, 0.30f, cz - 2.00f, 0.30f };
            Prism(out, view, body, 9, cx - 0.92f, cx + 0.92f, paint, paint);

            // the cabin and its glass
            const float cabin[] = { cz - 1.45f, 1.02f, cz - 0.75f, 1.52f, cz + 0.55f, 1.55f, cz + 1.25f, 1.02f };
            Prism(out, view, cabin, 4, cx - 0.78f, cx + 0.78f, glass, glass);
            // the pillars and the roof, a little wider than the glass
            Box(out, view, cx, 1.56f, cz - 0.1f, 1.62f, 0.05f, 1.35f, paint);

            // the wing, its struts and its end plates
            const float wing[] = { cz - 2.30f, 1.50f, cz - 2.30f, 1.56f, cz - 1.90f, 1.60f, cz - 1.86f, 1.48f };
            Prism(out, view, wing, 4, cx - 0.85f, cx + 0.85f, trim, trim);
            Box(out, view, cx - 0.50f, 1.26f, cz - 2.05f, 0.05f, 0.48f, 0.28f, trim);
            Box(out, view, cx + 0.50f, 1.26f, cz - 2.05f, 0.05f, 0.48f, 0.28f, trim);
            Box(out, view, cx - 0.85f, 1.52f, cz - 2.08f, 0.03f, 0.26f, 0.50f, trim);
            Box(out, view, cx + 0.85f, 1.52f, cz - 2.08f, 0.03f, 0.26f, 0.50f, trim);

            // the wheels, a little outside the body
            for (int wx = -1; wx <= 1; wx += 2)
                for (int wz = -1; wz <= 1; wz += 2)
                    Wheel(out, view, cx + (float)wx * 0.88f, 0.34f, cz + (float)wz * 1.45f, 0.34f, 0.30f);

            // the lights: a dark housing, a bright middle and a glow round it
            for (int side = -1; side <= 1; side += 2)
            {
                const float lx = cx + (float)side * 0.62f;
                const float housing[4][3] = { { lx - 0.30f, 0.76f, cz - 2.29f }, { lx - 0.30f, 0.92f, cz - 2.29f }, { lx + 0.30f, 0.92f, cz - 2.29f }, { lx + 0.30f, 0.76f, cz - 2.29f } };
                PushQuad(out, view, housing, { 0.55f, 0.06f, 0.03f, 1.0f }, nullptr);
                const float lamp[4][3] = { { lx - 0.26f, 0.79f, cz - 2.30f }, { lx - 0.26f, 0.89f, cz - 2.30f }, { lx + 0.26f, 0.89f, cz - 2.30f }, { lx + 0.26f, 0.79f, cz - 2.30f } };
                PushQuad(out, view, lamp, { 1.0f, 0.22f, 0.12f, 1.0f }, nullptr);
                Disc(out, view, lx, 0.84f, cz - 2.31f, 0.45f, 0.18f, { 1.0f, 0.25f, 0.10f, 0.45f }, { 1.0f, 0.2f, 0.1f, 0.0f }, 8);
            }

            // the plate and the diffuser
            const float plate[4][3] = { { cx - 0.27f, 0.48f, cz - 2.30f }, { cx - 0.27f, 0.62f, cz - 2.30f }, { cx + 0.27f, 0.62f, cz - 2.30f }, { cx + 0.27f, 0.48f, cz - 2.30f } };
            PushQuad(out, view, plate, { 0.92f, 0.78f, 0.22f, 1.0f }, nullptr);
            Box(out, view, cx, 0.24f, cz - 2.20f, 1.70f, 0.14f, 0.30f, trim);

            for (int i = -2; i <= 2; i++)
                Box(out, view, cx + (float)i * 0.32f, 0.24f, cz - 2.30f, 0.03f, 0.14f, 0.10f, trim);
        }

        // a tree: a trunk and a crown of discs, round or pointed
        inline void Tree(std::vector<Quad>& out, const View& view, float x, float z, int seed)
        {
            const float trunk = 1.2f + Hash(seed + 2) * 2.5f;
            const float crown = 4.0f + Hash(seed + 3) * 9.0f;
            const float spread = 2.0f + Hash(seed + 4) * 3.5f;
            const float tone = 0.75f + 0.5f * Hash(seed + 5);
            const bool pointed = Hash(seed + 10) > 0.6f;
            const Color dark{ 0.06f * tone, 0.11f * tone, 0.05f * tone, 1.0f };
            const Color light{ 0.16f * tone, 0.27f * tone, 0.11f * tone, 1.0f };

            Billboard(out, view, x, 0.0f, z, 0.35f, trunk + crown * 0.3f, { 0.10f, 0.07f, 0.05f, 1.0f }, { 0.20f, 0.15f, 0.10f, 1.0f });

            if (pointed)
            {
                const int tiers = 4;

                for (int t = 0; t < tiers; t++)
                {
                    const float f = (float)t / tiers;
                    const float y = trunk + crown * f * 0.8f;
                    const float r = spread * (1.0f - f * 0.75f) * 0.7f;
                    const float rx = view.right[0] * r, rz = view.right[2] * r;
                    const float p[4][3] = { { x - rx, y, z - rz }, { x, y + crown * 0.42f, z }, { x, y + crown * 0.42f, z }, { x + rx, y, z + rz } };
                    const Color colors[4] = { dark, light, light, dark };
                    PushQuad(out, view, p, colors, nullptr);
                }
            }
            else
            {
                Disc(out, view, x, trunk + crown * 0.28f, z, spread, crown * 0.32f, Mix(dark, light, 0.4f), dark, 10);
                Disc(out, view, x + spread * 0.3f, trunk + crown * 0.5f, z, spread * 0.75f, crown * 0.26f, Mix(dark, light, 0.6f), dark, 10);
                Disc(out, view, x - spread * 0.3f, trunk + crown * 0.55f, z, spread * 0.7f, crown * 0.25f, Mix(dark, light, 0.6f), dark, 10);
                Disc(out, view, x, trunk + crown * 0.78f, z, spread * 0.55f, crown * 0.22f, light, Mix(dark, light, 0.3f), 10);
            }
        }

        // The picture of the drive: the sky, the clouds and the hills, which are
        // flat on the window, and everything else, which is projected and sorted
        // by its depth.
        inline void Build(const World& world, const Camera& camera, int width, int height, std::vector<Quad>& flat, std::vector<Quad>& scene)
        {
            const View view = MakeView(camera, width, height);
            const float w = (float)width;
            const float h = (float)height;

            // where the ground meets the sky
            float horizon = h * 0.5f;
            {
                const float distant[3] = { view.px + view.at[0] * 5000.0f, 0.0f, view.pz + view.at[2] * 5000.0f };
                float sx = 0.0f, depth = 0.0f;
                Project(view, distant, sx, horizon, depth);
            }

            const auto push = [&](float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, Color c0, Color c1, Color c2, Color c3)
            {
                flat.push_back({ { x0, x1, x2, x3 }, { y0, y1, y2, y3 }, { c0, c1, c2, c3 }, 1.0e9f });
            };

            // the sky of a dusk under cloud, brightest at the horizon
            const Color zenith{ 0.30f, 0.36f, 0.50f, 1.0f };
            const Color skyline{ 0.70f, 0.72f, 0.75f, 1.0f };
            const Color between = Mix(zenith, skyline, 0.55f);
            push(0.0f, -h, w, -h, w, horizon * 0.45f, 0.0f, horizon * 0.45f, zenith, zenith, between, between);
            push(0.0f, horizon * 0.45f, w, horizon * 0.45f, w, horizon + 2.0f, 0.0f, horizon + 2.0f, between, between, skyline, skyline);

            // the clouds, drifting with the view
            for (int i = 0; i < 14; i++)
            {
                const float cx = fmodf(Hash(i * 3 + 7) * w * 2.0f - camera.yaw * w * 0.6f + w * 4.0f, w * 2.0f) - w * 0.5f;
                const float cy = horizon * (0.12f + 0.5f * Hash(i * 5 + 1));
                const float rx = w * (0.08f + 0.14f * Hash(i * 7 + 2));
                const float ry = rx * (0.18f + 0.15f * Hash(i * 11 + 3));
                const Color light = Mix(skyline, { 0.86f, 0.87f, 0.88f, 1.0f }, 0.6f);
                const Color dim = Mix(zenith, skyline, 0.4f);

                for (int s = 0; s < 12; s++)
                {
                    const float a0 = (float)s / 12.0f * 6.2831853f, a1 = (float)(s + 1) / 12.0f * 6.2831853f;
                    Color edge = Mix(dim, light, 0.5f + 0.5f * sinf(a0));
                    edge.a = 0.0f;
                    Color mid = light;
                    mid.a = 0.75f;
                    push(cx, cy, cx + cosf(a0) * rx, cy + sinf(a0) * ry, cx + cosf(a1) * rx, cy + sinf(a1) * ry, cx + cosf(a1) * rx, cy + sinf(a1) * ry, mid, edge, edge, edge);
                }
            }

            // the ground under the horizon, and two ranges of hills in the haze
            const Color farGrass = Mix({ 0.19f, 0.25f, 0.12f, 1.0f }, Haze, 0.85f);
            push(0.0f, horizon, w, horizon, w, h, 0.0f, h, farGrass, farGrass, farGrass, farGrass);

            for (int range = 0; range < 2; range++)
            {
                const Color hill = range == 0 ? Mix({ 0.22f, 0.28f, 0.33f, 1.0f }, Haze, 0.75f) : Mix({ 0.16f, 0.22f, 0.20f, 1.0f }, Haze, 0.55f);
                const float scale = range == 0 ? 0.12f : 0.06f;
                const int stations = 48;
                float lastX = 0.0f, lastY = 0.0f;

                for (int i = 0; i <= stations; i++)
                {
                    const float x = w * ((float)i / stations - 0.5f) * 1.3f + w * 0.5f - camera.yaw * w * (0.25f + 0.15f * range);
                    const float y = horizon - h * scale * (0.3f + 0.7f * (0.5f + 0.5f * sinf((float)i * (0.9f + range * 0.4f) + range * 2.0f) * Hash(i * 13 + range * 7 + 1)));

                    if (i > 0)
                        push(lastX, lastY, x, y, x, horizon + 3.0f, lastX, horizon + 3.0f, hill, hill, Mix(hill, skyline, 0.5f), Mix(hill, skyline, 0.5f));

                    lastX = x;
                    lastY = y;
                }
            }

            // the road and the verges, in pieces along it: a piece that is near the
            // camera is short, the far ones can be long
            const float roadHalf = 3.6f;
            const float start = floorf((camera.z - 20.0f) / 4.0f) * 4.0f;
            const Color grass{ 0.19f, 0.25f, 0.12f, 1.0f };
            const Color gravel{ 0.36f, 0.33f, 0.28f, 1.0f };
            const Color asphalt{ 0.20f, 0.21f, 0.23f, 1.0f };
            const Color wet{ 0.33f, 0.35f, 0.39f, 1.0f };
            const Color line{ 0.82f, 0.82f, 0.78f, 1.0f };

            for (float z = start; z < camera.z + 420.0f; )
            {
                const float length = z < camera.z + 40.0f ? 4.0f : z < camera.z + 120.0f ? 8.0f : 24.0f;
                const float z1 = z + length;
                const int cell = (int)floorf(z / 4.0f);
                const float g = 0.88f + 0.24f * Hash(cell * 3 + 1);
                const float g1 = 0.88f + 0.24f * Hash(cell * 3 + 4);

                Ground(scene, view, -80.0f, -roadHalf - 0.7f, z, z1, 0.0f, Mul(grass, g), Mul(grass, g1));
                Ground(scene, view, roadHalf + 0.7f, 80.0f, z, z1, 0.0f, Mul(grass, g1), Mul(grass, g));
                Ground(scene, view, -roadHalf - 0.7f, -roadHalf, z, z1, 0.001f, gravel, gravel);
                Ground(scene, view, roadHalf, roadHalf + 0.7f, z, z1, 0.001f, gravel, gravel);
                const float a = 0.95f + 0.1f * Hash(cell * 5 + 2);
                Ground(scene, view, -roadHalf, roadHalf, z, z1, 0.002f, Mul(asphalt, a), Mul(asphalt, a));
                // the sky in the wet of the road, between the tracks of the tyres
                Ground(scene, view, -1.3f, 1.3f, z, z1, 0.003f, Mul(wet, a), Mul(wet, a));
                Ground(scene, view, -3.3f, -2.4f, z, z1, 0.003f, Mul(wet, a * 0.8f), Mul(wet, a * 0.8f));
                Ground(scene, view, 2.4f, 3.3f, z, z1, 0.003f, Mul(wet, a * 0.8f), Mul(wet, a * 0.8f));
                // the lines: solid at the edges, dashed down the middle
                Ground(scene, view, -roadHalf + 0.2f, -roadHalf + 0.35f, z, z1, 0.004f, line, line);
                Ground(scene, view, roadHalf - 0.35f, roadHalf - 0.2f, z, z1, 0.004f, line, line);

                for (float d = floorf(z / 12.0f) * 12.0f; d < z1; d += 12.0f)
                {
                    const float d0 = (std::max)(d, z), d1 = (std::min)(d + 4.0f, z1);

                    if (d1 > d0)
                        Ground(scene, view, -0.08f, 0.08f, d0, d1, 0.004f, line, line);
                }

                z = z1;
            }

            // the wood on both sides, the bushes and the posts along the road
            for (int side = -1; side <= 1; side += 2)
            {
                for (int k = (int)floorf((camera.z - 15.0f) / 5.0f); k < (int)((camera.z + 300.0f) / 5.0f); k++)
                {
                    const int seed = k * 17 + side * 101;
                    const float z = (float)k * 5.0f + Hash(seed) * 4.0f;
                    const float x = (float)side * (6.0f + Hash(seed + 1) * 24.0f);

                    Tree(scene, view, x, z, seed);

                    if (Hash(seed + 6) > 0.4f)
                    {
                        const float bx = (float)side * (roadHalf + 1.6f + Hash(seed + 7) * 3.0f);
                        const float br = 0.6f + Hash(seed + 8) * 0.9f;
                        Disc(scene, view, bx, br * 0.5f, z + 2.0f, br, br * 0.7f, { 0.16f, 0.24f, 0.10f, 1.0f }, { 0.07f, 0.12f, 0.05f, 1.0f }, 8);
                    }

                    if ((k % 6) == 0)
                        Billboard(scene, view, (float)side * (roadHalf + 1.0f), 0.0f, (float)k * 5.0f, 0.12f, 1.0f, { 0.6f, 0.6f, 0.6f, 1.0f }, { 0.9f, 0.9f, 0.9f, 1.0f });

                    // the lamps along the road at night, on poles on either side
                    if (Night && (k % 8) == (side > 0 ? 0 : 4))
                    {
                        const float lx = (float)side * (roadHalf + 1.4f);
                        const float lz = (float)k * 5.0f;
                        Billboard(scene, view, lx, 0.0f, lz, 0.15f, 6.2f, { 0.3f, 0.3f, 0.3f, 1.0f }, { 0.35f, 0.35f, 0.35f, 1.0f });
                        Disc(scene, view, lx - (float)side * 0.6f, 6.1f, lz, 0.45f, 0.22f, { 1.0f, 0.88f, 0.62f, 1.0f }, { 1.0f, 0.80f, 0.50f, 1.0f }, 10);
                        Disc(scene, view, lx - (float)side * 0.6f, 6.1f, lz - 0.01f, 1.6f, 1.2f, { 1.0f, 0.75f, 0.45f, 0.25f }, { 1.0f, 0.7f, 0.4f, 0.0f }, 12);
                    }
                }
            }

            Car(scene, view, world.carX, world.carZ);

            if (Night)
            {
                for (auto* quads : { &flat, &scene })
                    for (auto& quad : *quads)
                        for (auto& c : quad.color)
                            if (c.r < 0.99f)
                                c = { c.r * NightShade, c.g * NightShade, c.b * NightShade * 1.25f, c.a };
            }

            std::stable_sort(scene.begin(), scene.end(), [](const Quad& a, const Quad& b) { return a.depth > b.depth; });
        }
    }

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
        "Drive (5)",
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
            bool startupCheck = false;
            bool startupFailed = false;
            std::chrono::steady_clock::time_point frameStart{};
            bool noRefractions = false;
            float dropX = 0.5f;         // where across the frame the drop of the check is placed
            // And where down it, which is what the checks are about. A drop shows
            // the frame around it and little else, so it is placed over the skyline
            // of the test scene, where the frame has something to show, and not
            // over the flat sky above it.
            float dropY = 0.35f;
            bool trailCheck = false;    // no device at all, only the CPU side of the effect
            bool trailView = false;     // drops the camera drags in a circle, and the water they leave
            // The camera drives forward through the rain at the speed of a car,
            // which is what makes the drops stream away from the middle of the
            // frame, and the pictures of the last frames of the run are kept one
            // by one, so that the paths of the drops can be followed over them.
            bool drive = false;
            float driveSpeed = 20.0f;   // units of the world a second
            int sequence = 0;           // how many of the last frames are kept
            bool swing = false;         // the camera of the drive swings round the car
            bool snow = false;          // the drops on the lens are flakes of snow
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
                else if (wcscmp(argv[i], L"--startup-check") == 0)
                    run.startupCheck = true;
                else if (wcscmp(argv[i], L"--frames") == 0 && i + 1 < argc)
                {
                    run.frameCount = _wtoi(argv[++i]);
                }
                else if (wcscmp(argv[i], L"--lens-light") == 0)
                {
                    run.lensLight = true;
                }
                else if (wcscmp(argv[i], L"--drop-x") == 0 && i + 1 < argc)
                {
                    run.dropX = (float)_wtof(argv[++i]);
                }
                else if (wcscmp(argv[i], L"--drop-y") == 0 && i + 1 < argc)
                {
                    run.dropY = (float)_wtof(argv[++i]);
                }
                else if (wcscmp(argv[i], L"--no-refractions") == 0)
                {
                    run.noRefractions = true;
                }
                else if (wcscmp(argv[i], L"--trail-check") == 0)
                {
                    run.trailCheck = true;
                }
                else if (wcscmp(argv[i], L"--trail-view") == 0)
                {
                    run.trailView = true;
                }
                else if (wcscmp(argv[i], L"--drive") == 0)
                {
                    run.drive = true;
                }
                else if (wcscmp(argv[i], L"--night") == 0)
                {
                    Mock::Night = true;
                }
                else if (wcscmp(argv[i], L"--drive-speed") == 0 && i + 1 < argc)
                {
                    run.driveSpeed = (float)_wtof(argv[++i]);
                }
                else if (wcscmp(argv[i], L"--sequence") == 0 && i + 1 < argc)
                {
                    run.sequence = _wtoi(argv[++i]);
                }
                else if (wcscmp(argv[i], L"--swing") == 0)
                {
                    run.swing = true;
                }
                else if (wcscmp(argv[i], L"--snow") == 0)
                {
                    run.snow = true;
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

            run.frameStart = std::chrono::steady_clock::now();

            // a drive keeps its rain to the end, the pictures of it are the rain
            if (run.drive)
                return;

            if (run.startupCheck && !IsPictureFrame(frameIndex))
            {
                // Preparation must happen without drawing a drop, both on a
                // fresh device and after resources have been invalidated.
                if (frameIndex == 12) WaterDrops::Reset();
                WaterDrops::Clear();
                WaterDrops::bForceRain = false;
                WaterDrops::ms_rainIntensity = 0.0f;
                return;
            }

            // The trail view is a handful of drops and a camera that is dragged
            // round in a circle with the mouse, so that what a drop leaves behind
            // it while it is dragged is on the pictures. Nothing of the run is
            // checked, the pictures are what is looked at.
            if (run.trailView)
            {
                WaterDrops::ms_rainIntensity = 0.0f;

                if (frameIndex == 1)
                {
                    WaterDrops::Clear();

                    for (int i = 0; i < 40; i++)
                    {
                        const float angle = (float)i * 0.1570796f;
                        auto* drop = WaterDrops::PlaceNew(
                            width * (0.5f + 0.42f * cosf(angle)),
                            height * (0.5f + 0.42f * sinf(angle)),
                            height / 22.0f, 60000.0f, false);

                        if (drop)
                            WaterDrops::NewDropMoving(drop);
                    }
                }

                // A camera that sweeps from side to side, which is what dragging
                // the mouse does to the drops of a game, and a bead of the rain that
                // runs down the glass by itself as well: the water of both is on the
                // pictures.
                WaterDrops::pos.x = sinf((float)frameIndex * 0.05f) * 30.0f;
                WaterDrops::bGravity = true;

                return;
            }

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
                WaterDrops::PlaceNew(width * run.dropX, height * run.dropY, height / 5.0f, 60000.0f, false);
            else if (picture == 2)
                WaterDrops::PlaceNew(width * run.dropX, height * (1.0f - run.dropY), height / 5.0f, 60000.0f, false);

            // The crop the Direct3D games of the Definitive Edition ask the
            // refraction to sample, which is what the check with the lamps is run
            // with: a light is where it is on the screen, and the field a drop
            // looks its light up in must not be moved by this.
            if (run.noRefractions)
                WaterDrops::bRefractions = false;
            if (run.lensLight)
            {
                WaterDrops::SetXUVScale(0.125f, 0.875f);
                for (auto& drop : WaterDrops::ms_drops)
                    if (drop.active) drop.uv_index = 0;
            }
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
                // The pixels are the colour of the frame and nothing else: what the
                // readback holds in the alpha of the target is how much the drops
                // covered it, which no screen shows, and a picture that kept it would
                // show the drops over the white of the viewer instead of the frame.
                Gdiplus::Bitmap bitmap(image.width, image.height, PixelFormat32bppRGB);
                Gdiplus::Rect rect(0, 0, image.width, image.height);
                Gdiplus::BitmapData data{};

                if (bitmap.LockBits(&rect, Gdiplus::ImageLockModeWrite, PixelFormat32bppRGB, &data) == Gdiplus::Ok)
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
        inline void WritePicture(const Image& image, const std::wstring& base, const wchar_t* suffix);

        inline bool AfterPresent(HWND hwnd, int frameIndex)
        {
            Run& run = State();

            if (!run.active)
                return false;

            if (run.startupCheck)
            {
                if (frameIndex == 5 || frameIndex == 20)
                {
                    const bool ready = WaterDrops::ms_renderPrepared && WaterDrops::ms_numDrops == 0;
                    run.startupFailed |= !ready;
                    printf("[%s] resources prepared on a dry frame%s\n", ready ? "PASS" : "FAIL",
                        frameIndex == 20 ? " after reset" : "");
                }
                if (frameIndex == 0 || frameIndex == run.frameCount - PICTURE_FRAMES + 1)
                {
                    const auto ms = std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - run.frameStart).count();
                    printf("[%s] %s processing through Present: %.3f ms\n", run.app,
                        frameIndex == 0 ? "dry initialization" : "first droplet", ms);
                }
            }

            // the frames of a sequence are kept one by one, see --sequence
            if (run.sequence > 0 && frameIndex >= run.frameCount - run.sequence && !run.screenshot.empty())
            {
                Image frame;

                if (CaptureWindow(hwnd, frame))
                {
                    wchar_t suffix[32]{};
                    swprintf_s(suffix, L"-seq-%03d", frameIndex - (run.frameCount - run.sequence));
                    WritePicture(frame, run.screenshot, suffix);
                }
            }

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

            if (run.startupFailed)
            {
                report("FAILED: first-drop resources were not prepared during dry frames");
                return EXIT_FAILED;
            }

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

            if (run.drive)
            {
                WritePicture(run.bottomDrop, run.screenshot, L"-drive");
                report("the rain of a drive is on the pictures");
                return EXIT_PASSED;
            }

            if (run.trailView)
            {
                WritePicture(run.empty, run.screenshot, L"-empty");
                WritePicture(run.topDrop, run.screenshot, L"-top");
                WritePicture(run.bottomDrop, run.screenshot, L"-bottom");

                report("the trail of a dragged drop is on the pictures");
                return EXIT_PASSED;
            }

            const Changed top = Difference(run.empty, run.topDrop);
            const Changed bottom = Difference(run.empty, run.bottomDrop);

            const int width = run.empty.width;
            const int height = run.empty.height;

            // the drop has to be drawn at all, where the x of the drop said it would
            // be, and in the half of the frame the y of it said
            const auto landed = [&](const Changed& changed, double expected)
            {
                if (changed.count < 200)
                    return false;

                if (fabs(changed.centerX - width * run.dropX) > width * 0.15)
                    return false;

                return fabs(changed.centerY - height * expected) <= height * 0.15;
            };

            const bool topOk = landed(top, run.dropY);
            const bool bottomOk = landed(bottom, 1.0 - run.dropY);

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
