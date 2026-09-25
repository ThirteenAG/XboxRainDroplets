// ---------------------------------------------------------------------------
// Test application for the OpenGL backend.
//
// Same scene and the same UI as the other ones, only the little drawing code of
// the application itself differs. The drops do not: the effect and the renderer
// call are identical on every API.
//
// The drops are drawn after the world and before the UI, which is what puts
// them under the interface. OpenGL has no target to query, the frame is
// whatever is in the framebuffer that is bound, which is what the backend reads
// back into its copy of the scene.
// ---------------------------------------------------------------------------

#define XRD_ENABLE_OPENGL 1
#include "xrd/xrd.h"

#include "XrdTest.h"
#include "shaders/generated/tests.h"

#include <windows.h>
#include <GL/gl.h>

// the application draws with the same entry points the backend loaded, the 1.1
// header the Windows SDK ships has none of them
using namespace Xrd::GLFunctions;

namespace
{
    struct SimpleVertex
    {
        float x, y, z;
        float r, g, b, a;
    };

    HDC hdc = nullptr;
    HGLRC glContext = nullptr;

    GLuint program = 0;
    GLint uniformProjection = -1;
    GLint attribPosition = -1;
    GLint attribColor = -1;

    XrdTest::Window window;
    XrdTest::Camera camera;
    XrdTest::Ui ui;

    float deltaTime = 1.0f / 60.0f;
    int uiSelection = 0;

    std::vector<SimpleVertex> vertexBuffer;

    GLuint CompileShader(GLenum type, const char* source)
    {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        return compiled ? shader : 0;
    }

    bool InitializeContext()
    {
        hdc = GetDC(window.hwnd);

        PIXELFORMATDESCRIPTOR format{};
        format.nSize = sizeof(format);
        format.nVersion = 1;
        format.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        format.iPixelType = PFD_TYPE_RGBA;
        format.cColorBits = 32;
        format.cDepthBits = 24;

        const int pixelFormat = ChoosePixelFormat(hdc, &format);
        if (!pixelFormat || !SetPixelFormat(hdc, pixelFormat, &format))
            return false;

        glContext = wglCreateContext(hdc);
        if (!glContext || !wglMakeCurrent(hdc, glContext))
            return false;

        Xrd::GLFunctions::LoadAll();

        GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, Xrd::Shaders::OpenGLSimpleVertexSource);
        GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, Xrd::Shaders::OpenGLSimpleFragmentSource);

        if (!vertexShader || !fragmentShader)
            return false;

        program = glCreateProgram();
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glLinkProgram(program);

        GLint linked = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);

        if (!linked)
            return false;

        attribPosition = glGetAttribLocation(program, "position");
        attribColor = glGetAttribLocation(program, "color");
        uniformProjection = glGetUniformLocation(program, "projection");

        return attribPosition >= 0 && attribColor >= 0 && uniformProjection >= 0;
    }

    void DrawRects(const XrdTest::Rect* pRects, int count)
    {
        if (count <= 0)
            return;

        vertexBuffer.clear();
        vertexBuffer.reserve((size_t)count * 6);

        for (int i = 0; i < count; i++)
        {
            const XrdTest::Rect& rect = pRects[i];

            const SimpleVertex quad[4] =
            {
                { rect.x,              rect.y,              0.0f, rect.color.r, rect.color.g, rect.color.b, rect.color.a },
                { rect.x + rect.width, rect.y,              0.0f, rect.color.r, rect.color.g, rect.color.b, rect.color.a },
                { rect.x + rect.width, rect.y + rect.height, 0.0f, rect.color.r, rect.color.g, rect.color.b, rect.color.a },
                { rect.x,              rect.y + rect.height, 0.0f, rect.color.r, rect.color.g, rect.color.b, rect.color.a },
            };

            // two triangles per rectangle, OpenGL has no index buffer here
            vertexBuffer.push_back(quad[0]);
            vertexBuffer.push_back(quad[1]);
            vertexBuffer.push_back(quad[2]);
            vertexBuffer.push_back(quad[0]);
            vertexBuffer.push_back(quad[2]);
            vertexBuffer.push_back(quad[3]);
        }

        // the same orthographic matrix the other applications use, the origin is
        // in the top left corner and y grows downwards
        const float projection[16] =
        {
            2.0f / (float)window.width, 0.0f, 0.0f, 0.0f,
            0.0f, -2.0f / (float)window.height, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            -1.0f, 1.0f, 0.0f, 1.0f,
        };

        glUseProgram(program);
        glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, projection);

        const GLsizei stride = sizeof(SimpleVertex);
        glEnableVertexAttribArray(attribPosition);
        glVertexAttribPointer(attribPosition, 3, GL_FLOAT, GL_FALSE, stride, &vertexBuffer[0].x);
        glEnableVertexAttribArray(attribColor);
        glVertexAttribPointer(attribColor, 4, GL_FLOAT, GL_FALSE, stride, &vertexBuffer[0].r);

        glDisable(GL_TEXTURE_2D);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertexBuffer.size());

        glDisableVertexAttribArray(attribPosition);
        glDisableVertexAttribArray(attribColor);
        glUseProgram(0);
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
    XrdTest::Headless::ParseCommandLine("opengl");

    if (XrdTest::Headless::Active())
        camera.autoYawSpeed = 0.0f;	// the pictures of the check are compared to each other

    if (!window.Create(L"Xbox Rain Droplets - OpenGL", 1280, 720))
        return 1;

    if (!InitializeContext())
        return XrdTest::Headless::DeviceFailed();

    glViewport(0, 0, window.width, window.height);

    Xrd::Init(Xrd::RENDERER_OPENGL, hdc);

    // The drops are laid out the way a game lays out its frame, with the y of it
    // counting down from the top of it, and the framebuffer of OpenGL counts up
    // from the bottom, so the backend has to turn them over - where they are drawn
    // and what they sample. Without it a drop falls up the screen.
    Xrd::SetPresentSceneFlipY(true);

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

        glViewport(0, 0, window.width, window.height);
        glClearColor(12.0f / 255.0f, 12.0f / 255.0f, 16.0f / 255.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        DrawWorld();

        // the drops land on the scene, which is the same call as on every other
        // API, and they stay under the UI drawn after them
        XrdTest::Headless::PrepareDrops(frameIndex, window.width, window.height);

        WaterDrops::Process();
        WaterDrops::Render();

        DrawUi();

        SwapBuffers(hdc);

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
            XrdTest::PrintStatus("opengl", WaterDrops::ms_numDrops, frames, WaterDrops::bEnableSnow, extra);

            wchar_t title[160]{};
            swprintf_s(title, L"Xbox Rain Droplets - OpenGL  |  drops %d  fps %d", WaterDrops::ms_numDrops, frames);
            SetWindowTextW(window.hwnd, title);

            frames = 0;
            lastReport = now;
        }
    }

    WaterDrops::Shutdown();
    Xrd::Shutdown();

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(glContext);

    return 0;
}
