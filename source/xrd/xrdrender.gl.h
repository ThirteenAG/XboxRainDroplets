#pragma once
// ---------------------------------------------------------------------------
// The OpenGL backend.
//
// OpenGL is the odd one out among the APIs of the games: there is no notion of
// "the target the game is rendering into" to query from a swap chain, the frame
// is whatever is in the current framebuffer and it is read back with a texture
// copy. Everything else is the same idea as the other backends: copy what is on
// screen, draw the atlas of drop shapes over it with the shader of the family
// and put the state of the game back afterwards.
//
// Contexts are the other difference: a game can have several of them and the
// functions of a modern context are not in opengl32.dll, they come from
// wglGetProcAddress. Both are handled here, the entry points are loaded once.
//
// What the caller passes to Init is the device context (HDC) that is being
// presented, which is what the OpenGL hook of the games hands over.
//
// Notes on the coordinate systems: the effect draws in pixels with the origin
// in the top left corner, which is what the orthographic matrix of Xrd::Matrix
// produces, and OpenGL has its origin in the bottom left corner, so the scene
// is sampled with a flipped v. The z of the snow lands in the lower half of the
// depth range, which does not matter because the depth test stays off.
// ---------------------------------------------------------------------------

#include "xrdrender.h"
#include "xrdshaders.h"

#include <windows.h>
#include <GL/gl.h>
#include <cstddef>
#include <cstdint>
#include <vector>

// pulled in by whoever includes this header, the games and the wrapper alike
#pragma comment(lib, "opengl32.lib")

namespace Xrd
{
    // The gl.h that ships with the Windows SDK stops at OpenGL 1.1, everything
    // newer is declared here. The values are the ones of the specification and
    // never change, which is why the extension headers of the drivers are not
    // needed.
    #ifndef GL_TEXTURE0
    #define GL_TEXTURE0 0x84C0
    #define GL_TEXTURE1 0x84C1
    #define GL_ACTIVE_TEXTURE 0x84E0
    #define GL_CLAMP_TO_EDGE 0x812F
    #define GL_CURRENT_PROGRAM 0x8B8D
    #define GL_VERTEX_SHADER 0x8B31
    #define GL_FRAGMENT_SHADER 0x8B30
    #define GL_COMPILE_STATUS 0x8B81
    #define GL_LINK_STATUS 0x8B82
    #define GL_SRC_ALPHA 0x0302
    #define GL_ONE_MINUS_SRC_ALPHA 0x0303
    #define GL_BLEND_SRC_RGB 0x80C9
    #define GL_BLEND_DST_RGB 0x80C8
    #define GL_BLEND_SRC_ALPHA 0x80CB
    #define GL_BLEND_DST_ALPHA 0x80CA
    #define GL_RGBA 0x1908
    #define GL_LINEAR 0x2601
    #define GL_TEXTURE_2D 0x0DE1
    #define GL_TEXTURE_BINDING_2D 0x8069
    #define GL_TEXTURE_MIN_FILTER 0x2801
    #define GL_TEXTURE_MAG_FILTER 0x2800
    #define GL_TEXTURE_WRAP_S 0x2802
    #define GL_TEXTURE_WRAP_T 0x2803
    #define GL_UNSIGNED_BYTE 0x1401
    #define GL_UNSIGNED_SHORT 0x1403
    #define GL_FLOAT 0x1406
    #define GL_TRIANGLES 0x0004
    #define GL_TRIANGLE_STRIP 0x0005
    #define GL_BLEND 0x0BE2
    #define GL_DEPTH_TEST 0x0B71
    #define GL_CULL_FACE 0x0B44
    #define GL_SCISSOR_TEST 0x0C11
    #define GL_VERTEX_ARRAY_BINDING 0x85B5
    #define GL_READ_FRAMEBUFFER 0x8CA8
    #define GL_READ_FRAMEBUFFER_BINDING 0x8CAA
    #define GL_COLOR_BUFFER_BIT 0x00004000
    #define GL_DEPTH_BUFFER_BIT 0x00000100
    #define GL_VIEWPORT 0x0BA2
    #endif

    // kept out of the block above on purpose, the sanity check of a header is
    // not worth losing a definition over
    #ifndef GL_ARRAY_BUFFER
    #define GL_ARRAY_BUFFER 0x8892
    #endif
    #ifndef GL_ELEMENT_ARRAY_BUFFER
    #define GL_ELEMENT_ARRAY_BUFFER 0x8893
    #endif
    #ifndef GL_ARRAY_BUFFER_BINDING
    #define GL_ARRAY_BUFFER_BINDING 0x8894
    #endif
    #ifndef GL_ELEMENT_ARRAY_BUFFER_BINDING
    #define GL_ELEMENT_ARRAY_BUFFER_BINDING 0x8895
    #endif
    #ifndef GL_STREAM_DRAW
    #define GL_STREAM_DRAW 0x88E0
    #endif
    #ifndef GL_FRAMEBUFFER
    #define GL_FRAMEBUFFER 0x8D40
    #endif
    #ifndef GL_DRAW_FRAMEBUFFER_BINDING
    #define GL_DRAW_FRAMEBUFFER_BINDING 0x8CA6
    #endif
    #ifndef GL_DRAW_FRAMEBUFFER
    #define GL_DRAW_FRAMEBUFFER 0x8CA9
    #endif
    #ifndef GL_COLOR_ATTACHMENT0
    #define GL_COLOR_ATTACHMENT0 0x8CE0
    #endif
    #ifndef GL_FRAMEBUFFER_COMPLETE
    #define GL_FRAMEBUFFER_COMPLETE 0x8CD5
    #endif

    namespace GLFunctions
    {
        inline HMODULE module = nullptr;

        inline void* Load(const char* name)
        {
            void* pResult = (void*)wglGetProcAddress(name);

            if (!pResult || pResult == (void*)1 || pResult == (void*)2 || pResult == (void*)3 || pResult == (void*)-1)
            {
                if (!module)
                    module = LoadLibraryA("opengl32.dll");

                pResult = module ? (void*)GetProcAddress(module, name) : nullptr;
            }

            return pResult;
        }

        // the entry points the backend uses, all of them from a 2.0 context
        inline GLuint(APIENTRY* glCreateShader)(GLenum type) = nullptr;
        inline void(APIENTRY* glShaderSource)(GLuint shader, GLsizei count, const char* const* string, const GLint* length) = nullptr;
        inline void(APIENTRY* glCompileShader)(GLuint shader) = nullptr;
        inline void(APIENTRY* glGetShaderiv)(GLuint shader, GLenum pname, GLint* params) = nullptr;
        inline void(APIENTRY* glGetShaderInfoLog)(GLuint shader, GLsizei bufSize, GLsizei* length, char* infoLog) = nullptr;
        inline GLuint(APIENTRY* glCreateProgram)() = nullptr;
        inline void(APIENTRY* glAttachShader)(GLuint program, GLuint shader) = nullptr;
        inline void(APIENTRY* glDeleteShader)(GLuint shader) = nullptr;
        inline void(APIENTRY* glDeleteProgram)(GLuint program) = nullptr;
        inline void(APIENTRY* glLinkProgram)(GLuint program) = nullptr;
        inline void(APIENTRY* glGetProgramiv)(GLuint program, GLenum pname, GLint* params) = nullptr;
        inline void(APIENTRY* glUseProgram)(GLuint program) = nullptr;
        inline GLint(APIENTRY* glGetUniformLocation)(GLuint program, const char* name) = nullptr;
        inline void(APIENTRY* glUniformMatrix4fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) = nullptr;
        inline void(APIENTRY* glUniform1i)(GLint location, GLint value) = nullptr;
        inline void(APIENTRY* glUniform1f)(GLint location, GLfloat value) = nullptr;
        inline void(APIENTRY* glUniform2f)(GLint location, GLfloat x, GLfloat y) = nullptr;
        inline GLint(APIENTRY* glGetAttribLocation)(GLuint program, const char* name) = nullptr;
        inline void(APIENTRY* glEnableVertexAttribArray)(GLuint index) = nullptr;
        inline void(APIENTRY* glDisableVertexAttribArray)(GLuint index) = nullptr;
        inline void(APIENTRY* glVertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer) = nullptr;
        inline void(APIENTRY* glActiveTexture)(GLenum texture) = nullptr;
        // a context of version 3 and newer does not draw without a vertex array
        // object, and the applications that create one are the ones that also
        // have a render target of their own bound while presenting
        inline void(APIENTRY* glGenVertexArrays)(GLsizei n, GLuint* arrays) = nullptr;
        inline void(APIENTRY* glDeleteVertexArrays)(GLsizei n, const GLuint* arrays) = nullptr;
        inline void(APIENTRY* glBindVertexArray)(GLuint array) = nullptr;
        inline void(APIENTRY* glBindFramebuffer)(GLenum target, GLuint framebuffer) = nullptr;
        // a frame of the caller that is still being drawn is a texture, and a
        // texture is not something a draw goes into: a framebuffer is made for it
        inline void(APIENTRY* glGenFramebuffers)(GLsizei n, GLuint* framebuffers) = nullptr;
        inline void(APIENTRY* glDeleteFramebuffers)(GLsizei n, const GLuint* framebuffers) = nullptr;
        inline void(APIENTRY* glFramebufferTexture2D)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) = nullptr;
        inline GLenum(APIENTRY* glCheckFramebufferStatus)(GLenum target) = nullptr;
        inline void(APIENTRY* glBlendFuncSeparate)(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) = nullptr;
        // vertices and indices have to come out of buffers, a context of
        // version 3 and newer does not read them from client memory
        inline void(APIENTRY* glGenBuffers)(GLsizei n, GLuint* buffers) = nullptr;
        inline void(APIENTRY* glDeleteBuffers)(GLsizei n, const GLuint* buffers) = nullptr;
        inline void(APIENTRY* glBindBuffer)(GLenum target, GLuint buffer) = nullptr;
        inline void(APIENTRY* glBufferData)(GLenum target, ptrdiff_t size, const void* data, GLenum usage) = nullptr;

        inline void LoadAll()
        {
            static bool loaded = false;

            if (loaded)
                return;

            loaded = true;

            glCreateShader = (decltype(glCreateShader))Load("glCreateShader");
            glShaderSource = (decltype(glShaderSource))Load("glShaderSource");
            glCompileShader = (decltype(glCompileShader))Load("glCompileShader");
            glGetShaderiv = (decltype(glGetShaderiv))Load("glGetShaderiv");
            glGetShaderInfoLog = (decltype(glGetShaderInfoLog))Load("glGetShaderInfoLog");
            glCreateProgram = (decltype(glCreateProgram))Load("glCreateProgram");
            glAttachShader = (decltype(glAttachShader))Load("glAttachShader");
            glDeleteShader = (decltype(glDeleteShader))Load("glDeleteShader");
            glDeleteProgram = (decltype(glDeleteProgram))Load("glDeleteProgram");
            glLinkProgram = (decltype(glLinkProgram))Load("glLinkProgram");
            glGetProgramiv = (decltype(glGetProgramiv))Load("glGetProgramiv");
            glUseProgram = (decltype(glUseProgram))Load("glUseProgram");
            glGetUniformLocation = (decltype(glGetUniformLocation))Load("glGetUniformLocation");
            glUniformMatrix4fv = (decltype(glUniformMatrix4fv))Load("glUniformMatrix4fv");
            glUniform1i = (decltype(glUniform1i))Load("glUniform1i");
            glUniform1f = (decltype(glUniform1f))Load("glUniform1f");
            glUniform2f = (decltype(glUniform2f))Load("glUniform2f");
            glGetAttribLocation = (decltype(glGetAttribLocation))Load("glGetAttribLocation");
            glEnableVertexAttribArray = (decltype(glEnableVertexAttribArray))Load("glEnableVertexAttribArray");
            glDisableVertexAttribArray = (decltype(glDisableVertexAttribArray))Load("glDisableVertexAttribArray");
            glVertexAttribPointer = (decltype(glVertexAttribPointer))Load("glVertexAttribPointer");
            glActiveTexture = (decltype(glActiveTexture))Load("glActiveTexture");
            glGenVertexArrays = (decltype(glGenVertexArrays))Load("glGenVertexArrays");
            glDeleteVertexArrays = (decltype(glDeleteVertexArrays))Load("glDeleteVertexArrays");
            glBindVertexArray = (decltype(glBindVertexArray))Load("glBindVertexArray");
            glBindFramebuffer = (decltype(glBindFramebuffer))Load("glBindFramebuffer");
            glGenFramebuffers = (decltype(glGenFramebuffers))Load("glGenFramebuffers");
            glDeleteFramebuffers = (decltype(glDeleteFramebuffers))Load("glDeleteFramebuffers");
            glFramebufferTexture2D = (decltype(glFramebufferTexture2D))Load("glFramebufferTexture2D");
            glCheckFramebufferStatus = (decltype(glCheckFramebufferStatus))Load("glCheckFramebufferStatus");
            glBlendFuncSeparate = (decltype(glBlendFuncSeparate))Load("glBlendFuncSeparate");
            glGenBuffers = (decltype(glGenBuffers))Load("glGenBuffers");
            glDeleteBuffers = (decltype(glDeleteBuffers))Load("glDeleteBuffers");
            glBindBuffer = (decltype(glBindBuffer))Load("glBindBuffer");
            glBufferData = (decltype(glBufferData))Load("glBufferData");
        }

        inline bool Ready()
        {
            LoadAll();
            return glCreateShader && glShaderSource && glCompileShader && glCreateProgram && glLinkProgram && glUseProgram &&
                glActiveTexture && glEnableVertexAttribArray && glVertexAttribPointer && glGenBuffers && glBindBuffer && glBufferData;
        }
    }

    class OpenGLBackend : public Backend
    {
    public:
        bool Init(void* pNative) override
        {
            // the caller may hand over the device context or nothing at all, the
            // context that is current is the one that matters
            hdc = (HDC)pNative;
            if (!hdc)
                hdc = wglGetCurrentDC();

            if (!wglGetCurrentContext())
                return false;

            if (!GLFunctions::Ready())
                return false;

            // the vertex array object the draws go through is made on demand, see
            // EnsureDeviceObjects
            viewport[0] = viewport[1] = viewport[2] = viewport[3] = 0;
            active = true;
            return true;
        }

        // OpenGL gets a new device context with every window the game is drawn
        // into, and a fullscreen switch of an emulator is one. The objects of the
        // backend belong to the context, so it is followed rather than rebuilt.
        bool UpdateNative(void* pNative) override
        {
            if (!active)
                return false;

            HDC newHdc = (HDC)pNative;
            if (!newHdc)
                newHdc = wglGetCurrentDC();

            if (newHdc != hdc)
            {
                hdc = newHdc;
                viewport[0] = viewport[1] = viewport[2] = viewport[3] = 0;
            }

            return true;
        }

        void Shutdown() override
        {
            ReleaseDeviceObjects();

            hdc = nullptr;
            active = false;
        }

        // The context the objects of the backend belong to is gone with the window
        // it was made for, so they are forgotten: a name of a dead context means
        // nothing to the one that is current now (and deleting it there would
        // delete whatever happens to carry it).
        void Detach() override
        {
            vertexArray = 0;
            sceneTexture = nullptr;
            sceneWidth = 0;
            sceneHeight = 0;
            program = 0;
            targetTexture = 0;
            targetFramebuffer = 0;
            targetFramebufferTexture = 0;
            targetSize = {};
            viewport[0] = viewport[1] = viewport[2] = viewport[3] = 0;
            active = false;
        }

        void Reset() override
        {
            ReleaseDeviceObjects();
        }

        bool IsActive() const override
        {
            return active && wglGetCurrentContext() != nullptr;
        }

        Size GetSize() const override
        {
            // A frame of the application that is being drawn into is the size of the
            // target that was handed over, and not the size of the viewport that is
            // bound at the moment: in the middle of a frame of the application that
            // viewport is the one of the draw before this one, which is a pass of its
            // own and regularly smaller than the frame (a quarter sized pass of a post
            // processing, for one). The frame starts at the corner of its own texture.
            if (targetSize.width > 0 && targetSize.height > 0)
            {
                viewport[0] = 0;
                viewport[1] = 0;
                viewport[2] = targetSize.width;
                viewport[3] = targetSize.height;
                return targetSize;
            }

            GLint vp[4]{};
            glGetIntegerv(GL_VIEWPORT, vp);

            if (vp[2] > 0 && vp[3] > 0)
            {
                viewport[0] = vp[0];
                viewport[1] = vp[1];
                viewport[2] = vp[2];
                viewport[3] = vp[3];
                return { vp[2], vp[3] };
            }

            if (hdc)
            {
                HWND window = WindowFromDC(hdc);
                RECT rect{};

                if (window && GetClientRect(window, &rect))
                    return { (int32_t)(rect.right - rect.left), (int32_t)(rect.bottom - rect.top) };
            }

            return { (int32_t)viewport[2], (int32_t)viewport[3] };
        }

        void SetMaskTexture(Texture* pMask) override
        {
            pMaskTexture = pMask;
        }

        Texture* CreateTexture(int textureWidth, int textureHeight, const uint8_t* pixels) override
        {
            if (!active || textureWidth <= 0 || textureHeight <= 0)
                return nullptr;

            GLuint id = 0;
            glGenTextures(1, &id);

            if (!id)
                return nullptr;

            // the texture belongs to the backend, the state of the caller is
            // handed back untouched
            GLint savedActiveTexture = GL_TEXTURE0;
            GLint savedTexture = 0;
            glGetIntegerv(GL_ACTIVE_TEXTURE, &savedActiveTexture);
            GLFunctions::glActiveTexture(GL_TEXTURE0);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &savedTexture);

            glBindTexture(GL_TEXTURE_2D, id);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureWidth, textureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

            glBindTexture(GL_TEXTURE_2D, (GLuint)savedTexture);
            GLFunctions::glActiveTexture((GLenum)savedActiveTexture);

            Texture* pTexture = new Texture{};
            pTexture->resource = (void*)(uintptr_t)id;
            pTexture->size = { textureWidth, textureHeight };
            pTexture->ownsResource = true;
            return pTexture;
        }

        void DestroyTexture(Texture* pTexture) override
        {
            if (!pTexture)
                return;

            // a name of a context that is gone means nothing to the one that is
            // current now, see Detach
            if (pTexture->ownsResource && pTexture->resource && active)
            {
                GLuint id = (GLuint)(uintptr_t)pTexture->resource;
                glDeleteTextures(1, &id);
            }

            if (pTexture == pMaskTexture)
                pMaskTexture = nullptr;

            delete pTexture;
        }

        void SetTarget(RenderTarget* pTarget) override
        {
            // OpenGL draws into a framebuffer and not into the texture of one, and
            // the caller hands over the texture of the frame of its game: the
            // framebuffer that carries it is made for it, see Render. A caller that
            // hands over nothing draws into the framebuffer that is bound, which is
            // the window for a present call.
            const GLuint texture = (pTarget && pTarget->resource) ? (GLuint)(uintptr_t)pTarget->resource : 0;

            if (texture != targetTexture)
            {
                ReleaseTargetFramebuffer();
                targetTexture = texture;
            }

            targetSize = (pTarget && pTarget->size.width > 0 && pTarget->size.height > 0) ? pTarget->size : Size{};
        }

        void SetTargetState(TargetState state) override
        {
            // A frame that is still being drawn lives in the framebuffer that is
            // bound, only a finished frame is the one of the window. It is what the
            // copy of the frame behind the drops is read from.
            captureBoundFramebuffer = (state == TARGET_STATE_RENDER_TARGET);
        }

        void SetProjection(Projection projection, const Matrix* pWorld, float width, float height) override
        {
            if (projection == PROJECTION_WORLD && pWorld)
            {
                memcpy(projectionMatrix.m, pWorld->m, sizeof(projectionMatrix.m));
            }
            else
            {
                // The drops are laid out in the frame of the game the way a game lays out a frame,
                // with the y of it going down from the top left, and a framebuffer of OpenGL goes
                // up from the bottom left.
                //
                // A frame that is drawn into lies the same way up as the y of the drops does, and
                // a frame that is presented is the other way round: what is read out of the window
                // is the frame of the game turned over, so both the drops and the read of the frame
                // are turned over with it, see SetPresentSceneFlipY. Without that a drop falls up
                // the screen: the y of it lands at the bottom of the window and its own fall moves
                // it to the top of it.
                if (presentSceneFlipY && !captureBoundFramebuffer)
                    projectionMatrix = Matrix::OrthographicOffCenter(0.0f, width, height, 0.0f, 0.0f, 1.0f);
                else
                    projectionMatrix = Matrix::OrthographicOffCenter(0.0f, width, 0.0f, height, 0.0f, 1.0f);
            }
        }

        void SetSceneUVScale(float offsetX, float scaleX, float offsetY, float scaleY) override
        {
            uvOffsetX = offsetX;
            uvScaleX = scaleX;

            // A frame of a game is read into the texture of the drops the way it lies in the
            // framebuffer of OpenGL, which goes up from the bottom left, and the drops are drawn
            // with the y turned over to match, see SetProjection: the read of the frame follows it
            // and is not turned over on its own.
            uvOffsetY = offsetY;
            uvScaleY = scaleY;

            // A frame that is presented is no frame of a game here: it was read out of the window,
            // which the window of OpenGL lies the other way up in, so the copy of it is turned
            // over against the frame the game drew and everything that follows the frame (which is
            // what a drop is moved along by) is turned over with it.
            if (presentSceneFlipY && !captureBoundFramebuffer)
            {
                uvOffsetY = offsetY + scaleY;
                uvScaleY = -scaleY;
            }
        }

        void SetPresentSceneFlipY(bool enabled) override
        {
            presentSceneFlipY = enabled;
        }

        void SetSceneComplement(bool enabled) override
        {
            sceneComplement = enabled;
        }

        void SetSceneSampling(bool enabled) override
        {
            sceneSampling = enabled;
        }

        bool Prepare(int maxVertices) override
        {
            if (!active || !EnsureDeviceObjects()) return false;
            const auto size = GetSize();
            if (size.width <= 0 || size.height <= 0) return false;
            EnsureIndexArray(maxVertices);
            return EnsureSceneTexture(size);
        }

        void Render(const Vertex* pVertices, int numVertices, PrimitiveType primitive) override
        {
            if (!active || !pVertices || numVertices <= 0)
                return;

            const int numIndices = (primitive == PRIMITIVE_TRIANGLES) ? (numVertices / 4) * 6 : 0;

            if (primitive == PRIMITIVE_TRIANGLES && numIndices <= 0)
                return;

            if (!EnsureDeviceObjects())
                return;

            Size size = GetSize();
            if (size.width <= 0 || size.height <= 0)
                return;

            // A frame of a game is handed over as a texture, and neither a draw nor
            // the copy of the frame behind the drops can name a texture: both go
            // through a framebuffer, so the one that carries the frame is made and
            // bound here, before the copy below. The framebuffer that was bound is
            // put back at the end, so that the application finds the state it left
            // behind, and its own idea of which one is bound.
            GLint savedFrameDrawFramebuffer = 0;
            GLint savedFrameReadFramebuffer = 0;
            GLint savedViewport[4] = {};
            bool frameFramebufferComplete = false;

            if (captureBoundFramebuffer && GLFunctions::glBindFramebuffer)
            {
                glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &savedFrameDrawFramebuffer);
                glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &savedFrameReadFramebuffer);
                glGetIntegerv(GL_VIEWPORT, savedViewport);

                frameFramebufferComplete = EnsureTargetFramebuffer();

                if (frameFramebufferComplete)
                    GLFunctions::glBindFramebuffer(GL_FRAMEBUFFER, targetFramebuffer);
                else
                    GLFunctions::glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)savedFrameDrawFramebuffer);

                // The viewport of the application here is the one of the draw before
                // this one, and the drops cover the whole frame: it is set to the frame
                // and put back at the end, like the framebuffer above.
                if (targetSize.width > 0 && targetSize.height > 0)
                    glViewport(0, 0, (GLsizei)targetSize.width, (GLsizei)targetSize.height);
            }

            if (!EnsureSceneTexture(size))
            {
                if (captureBoundFramebuffer && GLFunctions::glBindFramebuffer)
                {
                    glViewport(savedViewport[0], savedViewport[1], savedViewport[2], savedViewport[3]);
                    GLFunctions::glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)savedFrameDrawFramebuffer);
                    GLFunctions::glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)savedFrameReadFramebuffer);
                }

                return;
            }

            // Everything the draw touches is saved and put back by hand. The
            // attribute stack of the fixed function pipeline cannot be used for
            // this: it does not exist in a core profile context, which is what
            // the newer applications create, and asking for it there is an error.
            GLint savedProgram = 0;
            GLint savedActiveTexture = GL_TEXTURE0;
            GLint savedTexture0 = 0;
            GLint savedTexture1 = 0;
            GLint savedBlendSrcRgb = GL_ONE, savedBlendDstRgb = GL_ZERO;
            GLint savedBlendSrcAlpha = GL_ONE, savedBlendDstAlpha = GL_ZERO;
            GLint savedVertexArray = 0;
            GLint savedReadFramebuffer = 0;
            GLint savedArrayBuffer = 0;
            GLint savedElementBuffer = 0;

            glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &savedArrayBuffer);
            glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &savedElementBuffer);

            glGetIntegerv(GL_CURRENT_PROGRAM, &savedProgram);
            glGetIntegerv(GL_ACTIVE_TEXTURE, &savedActiveTexture);

            GLFunctions::glActiveTexture(GL_TEXTURE0);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &savedTexture0);
            GLFunctions::glActiveTexture(GL_TEXTURE1);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &savedTexture1);

            glGetIntegerv(GL_BLEND_SRC_RGB, &savedBlendSrcRgb);
            glGetIntegerv(GL_BLEND_DST_RGB, &savedBlendDstRgb);
            glGetIntegerv(GL_BLEND_SRC_ALPHA, &savedBlendSrcAlpha);
            glGetIntegerv(GL_BLEND_DST_ALPHA, &savedBlendDstAlpha);

            const GLboolean savedBlend = glIsEnabled(GL_BLEND);
            const GLboolean savedDepthTest = glIsEnabled(GL_DEPTH_TEST);
            const GLboolean savedCullFace = glIsEnabled(GL_CULL_FACE);
            const GLboolean savedScissorTest = glIsEnabled(GL_SCISSOR_TEST);

            // The application is in the middle of a frame of its own here, and what it
            // had set up for the draw before this one is not what a draw of the drops
            // can use: colour writes can be off, and a stencil test of the application
            // fails for every pixel of them.
            GLint savedColorMask[4] = {};
            glGetIntegerv(GL_COLOR_WRITEMASK, savedColorMask);

            const GLboolean savedStencilTest = glIsEnabled(GL_STENCIL_TEST);

            if (GLFunctions::glBindVertexArray)
                glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &savedVertexArray);

            // the frame that is on screen is the one of the window, an
            // application that presents from a target of its own has that one
            // bound for reading and it is not what the drops refract. A frame that
            // is still being drawn is the other way round: it is the one that is
            // bound, which is where the drops are being drawn into as well.
            if (!captureBoundFramebuffer && GLFunctions::glBindFramebuffer && GLFunctions::glBindVertexArray)
            {
                glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &savedReadFramebuffer);

                if (savedReadFramebuffer != 0)
                    GLFunctions::glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            }

            // copy of the frame behind the drops, into the scene texture
            GLFunctions::glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, (GLuint)(uintptr_t)sceneTexture);
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, viewport[0], viewport[1], (GLsizei)size.width, (GLsizei)size.height);

            glDisable(GL_DEPTH_TEST);
            glDisable(GL_SCISSOR_TEST);
            glDisable(GL_CULL_FACE);
            glDisable(GL_STENCIL_TEST);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            GLFunctions::glUseProgram(program);

            GLFunctions::glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, &projectionMatrix.m[0][0]);
            GLFunctions::glUniform2f(uniformUvOffset, uvOffsetX, uvOffsetY);
            GLFunctions::glUniform2f(uniformUvScale, uvScaleX, uvScaleY);
            GLFunctions::glUniform1f(uniformSceneComplement, sceneSampling ? (sceneComplement ? 1.0f : 0.0f) : -1.0f);

            GLFunctions::glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, (GLuint)(uintptr_t)sceneTexture);
            GLFunctions::glUniform1i(uniformSceneTexture, 0);

            GLFunctions::glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, (pMaskTexture && pMaskTexture->resource) ? (GLuint)(uintptr_t)pMaskTexture->resource : 0);
            GLFunctions::glUniform1i(uniformMaskTexture, 1);
            GLFunctions::glActiveTexture(GL_TEXTURE0);

            // a context of version 3 and newer draws only through a vertex array
            // object, the one of the caller is bound again afterwards
            if (GLFunctions::glBindVertexArray)
                GLFunctions::glBindVertexArray(vertexArray);

            // neither the vertices nor the indices may come from client memory,
            // so both go into a buffer first
            const GLsizei stride = sizeof(Vertex);

            GLFunctions::glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
            GLFunctions::glBufferData(GL_ARRAY_BUFFER, (ptrdiff_t)((size_t)numVertices * stride), pVertices, GL_STREAM_DRAW);

            GLFunctions::glEnableVertexAttribArray(attribPosition);
            GLFunctions::glVertexAttribPointer(attribPosition, 3, GL_FLOAT, GL_FALSE, stride, (const void*)(uintptr_t)offsetof(Vertex, x));
            GLFunctions::glEnableVertexAttribArray(attribColor);
            GLFunctions::glVertexAttribPointer(attribColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (const void*)(uintptr_t)offsetof(Vertex, color));
            GLFunctions::glEnableVertexAttribArray(attribAtlas);
            GLFunctions::glVertexAttribPointer(attribAtlas, 2, GL_FLOAT, GL_FALSE, stride, (const void*)(uintptr_t)offsetof(Vertex, u0));
            GLFunctions::glEnableVertexAttribArray(attribScene);
            GLFunctions::glVertexAttribPointer(attribScene, 2, GL_FLOAT, GL_FALSE, stride, (const void*)(uintptr_t)offsetof(Vertex, u1));

            if (primitive == PRIMITIVE_TRIANGLES)
            {
                EnsureIndexArray(numVertices);

                GLFunctions::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
                GLFunctions::glBufferData(GL_ELEMENT_ARRAY_BUFFER, (ptrdiff_t)((size_t)numIndices * sizeof(uint16_t)), indices.data(), GL_STREAM_DRAW);
                glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_SHORT, nullptr);
            }
            else
            {
                glDrawArrays(GL_TRIANGLE_STRIP, 0, numVertices);
            }

            GLFunctions::glDisableVertexAttribArray(attribPosition);
            GLFunctions::glDisableVertexAttribArray(attribColor);
            GLFunctions::glDisableVertexAttribArray(attribAtlas);
            GLFunctions::glDisableVertexAttribArray(attribScene);

            if (GLFunctions::glBindVertexArray)
                GLFunctions::glBindVertexArray((GLuint)savedVertexArray);

            GLFunctions::glBindBuffer(GL_ARRAY_BUFFER, (GLuint)savedArrayBuffer);
            GLFunctions::glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, (GLuint)savedElementBuffer);

            if (captureBoundFramebuffer && GLFunctions::glBindFramebuffer)
            {
                GLFunctions::glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)savedFrameDrawFramebuffer);
                GLFunctions::glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)savedFrameReadFramebuffer);
                glViewport(savedViewport[0], savedViewport[1], savedViewport[2], savedViewport[3]);
            }

            if (GLFunctions::glBindFramebuffer && savedReadFramebuffer != 0)
                GLFunctions::glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)savedReadFramebuffer);

            GLFunctions::glUseProgram((GLuint)savedProgram);

            if (savedBlend)
                glEnable(GL_BLEND);
            else
                glDisable(GL_BLEND);

            if (GLFunctions::glBlendFuncSeparate)
                GLFunctions::glBlendFuncSeparate((GLenum)savedBlendSrcRgb, (GLenum)savedBlendDstRgb, (GLenum)savedBlendSrcAlpha, (GLenum)savedBlendDstAlpha);
            else
                glBlendFunc((GLenum)savedBlendSrcRgb, (GLenum)savedBlendDstRgb);

            if (savedDepthTest)
                glEnable(GL_DEPTH_TEST);
            else
                glDisable(GL_DEPTH_TEST);

            if (savedCullFace)
                glEnable(GL_CULL_FACE);
            else
                glDisable(GL_CULL_FACE);

            if (savedScissorTest)
                glEnable(GL_SCISSOR_TEST);
            else
                glDisable(GL_SCISSOR_TEST);

            glColorMask((GLboolean)savedColorMask[0], (GLboolean)savedColorMask[1], (GLboolean)savedColorMask[2], (GLboolean)savedColorMask[3]);

            if (savedStencilTest)
                glEnable(GL_STENCIL_TEST);
            else
                glDisable(GL_STENCIL_TEST);

            GLFunctions::glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, (GLuint)savedTexture0);
            GLFunctions::glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, (GLuint)savedTexture1);
            GLFunctions::glActiveTexture((GLenum)savedActiveTexture);
        }

    private:
        bool EnsureDeviceObjects()
        {
            if (!GLFunctions::Ready())
                return false;

            // Every draw of this backend goes through one vertex array object, a
            // modern context draws nothing without it. It is made here and not in
            // Init because Reset() releases it along with the rest of the device
            // objects, and a core profile context refuses to draw with the default
            // vertex array: without it every frame after a window change (a
            // fullscreen switch is one, it changes the size of the frame) would be
            // drawn without the drops and never recover.
            if (GLFunctions::glGenVertexArrays && !vertexArray)
            {
                GLint savedVertexArray = 0;
                glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &savedVertexArray);
                GLFunctions::glGenVertexArrays(1, &vertexArray);
                GLFunctions::glBindVertexArray((GLuint)savedVertexArray);
            }

            if (program)
                return true;

            GLuint vertexShader = Compile(GL_VERTEX_SHADER, Shaders::OpenGLVertexSource);
            GLuint fragmentShader = Compile(GL_FRAGMENT_SHADER, Shaders::OpenGLFragmentSource);

            if (!vertexShader || !fragmentShader)
            {
                // a core profile context refuses the shaders of a compatibility
                // one, so the other pair is tried before giving up
                if (vertexShader)
                {
                    GLFunctions::glDeleteShader(vertexShader);
                    vertexShader = 0;
                }

                if (fragmentShader)
                {
                    GLFunctions::glDeleteShader(fragmentShader);
                    fragmentShader = 0;
                }

                vertexShader = Compile(GL_VERTEX_SHADER, Shaders::OpenGLVertexSourceCore);
                fragmentShader = Compile(GL_FRAGMENT_SHADER, Shaders::OpenGLFragmentSourceCore);
            }

            if (!vertexShader || !fragmentShader)
                return false;

            program = GLFunctions::glCreateProgram();
            GLFunctions::glAttachShader(program, vertexShader);
            GLFunctions::glAttachShader(program, fragmentShader);
            GLFunctions::glLinkProgram(program);

            GLint linked = 0;
            GLFunctions::glGetProgramiv(program, GL_LINK_STATUS, &linked);

            if (!linked)
            {
                program = 0;
                return false;
            }

            attribPosition = GLFunctions::glGetAttribLocation(program, "position");
            attribColor = GLFunctions::glGetAttribLocation(program, "color");
            attribAtlas = GLFunctions::glGetAttribLocation(program, "atlas");
            attribScene = GLFunctions::glGetAttribLocation(program, "scene");

            uniformProjection = GLFunctions::glGetUniformLocation(program, "projection");
            uniformSceneTexture = GLFunctions::glGetUniformLocation(program, "sceneTexture");
            uniformMaskTexture = GLFunctions::glGetUniformLocation(program, "maskTexture");
            uniformUvOffset = GLFunctions::glGetUniformLocation(program, "uvOffset");
            uniformUvScale = GLFunctions::glGetUniformLocation(program, "uvScale");
            uniformSceneComplement = GLFunctions::glGetUniformLocation(program, "sceneComplement");

            // the vertices and the indices of every draw come out of these
            GLFunctions::glGenBuffers(1, &vertexBuffer);
            GLFunctions::glGenBuffers(1, &indexBuffer);

            if (!vertexBuffer || !indexBuffer)
            {
                ReleaseDeviceObjects();
                return false;
            }

            return attribPosition >= 0 && attribColor >= 0 && attribAtlas >= 0 && attribScene >= 0;
        }

        GLuint Compile(GLenum type, const char* source)
        {
            GLuint shader = GLFunctions::glCreateShader(type);
            GLFunctions::glShaderSource(shader, 1, &source, nullptr);
            GLFunctions::glCompileShader(shader);

            GLint compiled = 0;
            GLFunctions::glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

            if (!compiled)
            {
                GLFunctions::glDeleteShader(shader);
                return 0;
            }

            return shader;
        }

        // The framebuffer that carries the frame of the caller, made from the texture
        // it handed over, see SetTarget and Render. It is made once per texture: a
        // frame of a game is the same texture every frame, and a window change makes
        // a new one.
        bool EnsureTargetFramebuffer()
        {
            if (!targetTexture || !GLFunctions::glBindFramebuffer || !GLFunctions::glGenFramebuffers || !GLFunctions::glFramebufferTexture2D)
                return false;

            if (targetFramebuffer && targetFramebufferTexture == targetTexture)
                return true;

            ReleaseTargetFramebuffer();
            GLFunctions::glGenFramebuffers(1, &targetFramebuffer);

            if (!targetFramebuffer)
                return false;

            GLFunctions::glBindFramebuffer(GL_FRAMEBUFFER, targetFramebuffer);
            GLFunctions::glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, targetTexture, 0);

            // a texture of an application is not always one a framebuffer can carry
            // (a different format, a level that is not there): without a complete
            // framebuffer there is nothing to draw the drops into, and the caller is
            // left with the framebuffer that is bound
            if (GLFunctions::glCheckFramebufferStatus && GLFunctions::glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            {
                ReleaseTargetFramebuffer();
                return false;
            }

            targetFramebufferTexture = targetTexture;
            return true;
        }

        void ReleaseTargetFramebuffer()
        {
            if (targetFramebuffer)
            {
                // a name of a context that is gone means nothing to the one that is
                // current now, see Detach
                if (active && GLFunctions::glDeleteFramebuffers)
                    GLFunctions::glDeleteFramebuffers(1, &targetFramebuffer);

                targetFramebuffer = 0;
            }

            targetFramebufferTexture = 0;
        }

        bool EnsureSceneTexture(Size size)
        {
            if (sceneTexture && sceneWidth == size.width && sceneHeight == size.height)
                return true;

            if (sceneTexture)
            {
                GLuint id = (GLuint)(uintptr_t)sceneTexture;
                glDeleteTextures(1, &id);
                sceneTexture = nullptr;
            }

            GLuint id = 0;
            glGenTextures(1, &id);

            if (!id)
                return false;

            GLint savedActiveTexture = GL_TEXTURE0;
            GLint savedTexture = 0;
            glGetIntegerv(GL_ACTIVE_TEXTURE, &savedActiveTexture);
            GLFunctions::glActiveTexture(GL_TEXTURE0);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &savedTexture);

            glBindTexture(GL_TEXTURE_2D, id);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)size.width, (GLsizei)size.height,
                0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

            glBindTexture(GL_TEXTURE_2D, (GLuint)savedTexture);
            GLFunctions::glActiveTexture((GLenum)savedActiveTexture);

            sceneTexture = (void*)(uintptr_t)id;
            sceneWidth = size.width;
            sceneHeight = size.height;
            return true;
        }

        void EnsureIndexArray(int numVertices)
        {
            const int quads = (numVertices / 4) + 1;

            if ((int)indices.size() >= quads * 6)
                return;

            indices.resize((size_t)quads * 6);

            for (int i = 0; i < quads; i++)
            {
                indices[i * 6 + 0] = (uint16_t)(i * 4 + 0);
                indices[i * 6 + 1] = (uint16_t)(i * 4 + 1);
                indices[i * 6 + 2] = (uint16_t)(i * 4 + 2);
                indices[i * 6 + 3] = (uint16_t)(i * 4 + 0);
                indices[i * 6 + 4] = (uint16_t)(i * 4 + 2);
                indices[i * 6 + 5] = (uint16_t)(i * 4 + 3);
            }
        }

        void ReleaseDeviceObjects()
        {
            if (!active)
                return;

            ReleaseTargetFramebuffer();

            if (vertexArray)
            {
                if (GLFunctions::glDeleteVertexArrays)
                    GLFunctions::glDeleteVertexArrays(1, &vertexArray);

                vertexArray = 0;
            }

            if (vertexBuffer && GLFunctions::glDeleteBuffers)
            {
                GLFunctions::glDeleteBuffers(1, &vertexBuffer);
                vertexBuffer = 0;
            }

            if (indexBuffer && GLFunctions::glDeleteBuffers)
            {
                GLFunctions::glDeleteBuffers(1, &indexBuffer);
                indexBuffer = 0;
            }

            if (sceneTexture)
            {
                GLuint id = (GLuint)(uintptr_t)sceneTexture;
                glDeleteTextures(1, &id);
                sceneTexture = nullptr;
            }

            if (program)
            {
                GLFunctions::glUseProgram(0);
                GLFunctions::glDeleteProgram(program);
                program = 0;
            }

            sceneWidth = 0;
            sceneHeight = 0;
        }

        HDC hdc = nullptr;
        bool active = false;
        // the frame being drawn is in the framebuffer that is bound, see
        // SetTargetState
        bool captureBoundFramebuffer = false;
        // the texture of the frame the caller handed over, and the framebuffer that
        // carries it while the drops are drawn into it, see SetTarget
        GLuint targetTexture = 0;
        GLuint targetFramebuffer = 0;
        GLuint targetFramebufferTexture = 0;
        // the size of that frame, which is what the drops are scaled and placed with
        // and is not the size of the viewport the application has bound, see GetSize
        Size targetSize{};
        mutable GLint viewport[4]{};

        GLuint vertexArray = 0;

        GLuint vertexBuffer = 0;
        GLuint indexBuffer = 0;

        GLuint program = 0;
        GLint attribPosition = -1;
        GLint attribColor = -1;
        GLint attribAtlas = -1;
        GLint attribScene = -1;
        GLint uniformProjection = -1;
        GLint uniformSceneTexture = -1;
        GLint uniformMaskTexture = -1;
        GLint uniformUvOffset = -1;
        GLint uniformUvScale = -1;
        GLint uniformSceneComplement = -1;

        void* sceneTexture = nullptr;
        int32_t sceneWidth = 0;
        int32_t sceneHeight = 0;

        Matrix projectionMatrix = Matrix::Identity();
        float uvOffsetX = 0.0f;
        float uvOffsetY = 0.0f;
        float uvScaleX = 1.0f;
        float uvScaleY = -1.0f;

        bool sceneComplement = false;
        bool sceneSampling = true;

        // See SetPresentSceneFlipY: a frame that was read out of a window is the other way up
        // than a frame a game drew, and only a caller that presents from a window asks for this.
        bool presentSceneFlipY = false;

        Texture* pMaskTexture = nullptr;
        std::vector<uint16_t> indices;
    };

    namespace Detail
    {
        inline Backend* CreateOpenGL()
        {
            return new OpenGLBackend();
        }

        inline Register registerOpenGL(RENDERER_OPENGL, CreateOpenGL);
    }
}
