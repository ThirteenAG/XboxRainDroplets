#pragma once
// ---------------------------------------------------------------------------
// One immediate mode drawing interface for every backend.
//
// The effect only ever does the same three things:
//   1. it asks which graphics API is running and what the window size is,
//   2. it builds a list of quads,
//   3. it hands that list over and expects the drops to show up on whatever the
//      game is currently rendering into.
//
// Step 3 is the important part: a backend renders into the target that is
// bound at the moment Render is called, and it copies that same target into a
// texture for the drops to sample. That way the very same call works at the end
// of the frame (the back buffer, drops on top of the world but under nothing),
// in the middle of it (the scene render target of the game, which lands the
// drops under the UI) and in a game specific hook, without the effect knowing
// anything about it.
// ---------------------------------------------------------------------------

#include "xrdcommon.h"

namespace Xrd
{
    // -----------------------------------------------------------------------
    // what a backend has to provide
    // -----------------------------------------------------------------------
    class Backend
    {
    public:
        virtual ~Backend() = default;

        // pNative is the device of the API (IDirect3DDevice8/9, ID3D10Device,
        // ID3D11Device, ID3D12Device, a GL/HDC pair, a VkDevice) or the swap
        // chain when the API is swap chain based.
        virtual bool Init(void* pNative) = 0;
        virtual void Shutdown() = 0;

        // Called when the window was resized or the device was reset: every
        // resource that depends on the back buffer size has to go.
        virtual void Reset() = 0;

        virtual bool IsActive() const = 0;
        virtual Size GetSize() const = 0;

        // The atlas of drop shapes, sampled with the alpha of the drop.
        virtual void SetMaskTexture(Texture* pMask) = 0;
        virtual Texture* CreateTexture(int width, int height, const uint8_t* pixels) = 0;
        virtual void DestroyTexture(Texture* pTexture) = 0;

        // nullptr means "whatever the API currently renders into".
        virtual void SetTarget(RenderTarget* pTarget) = 0;

        // Only the APIs without implicit state use this.
        virtual void SetTargetState(TargetState state) = 0;

        // Direct3D 12 renders on the queue of the game, because that is the only
        // way to guarantee the drops land in the right order. The other APIs
        // have no use for it.
        virtual void SetCommandQueue(void* /*pQueue*/) {}

        // A renderer that records its frame on another thread records it into a
        // context of its own and has it executed by the thread that owns the device,
        // and a draw that is issued on the context of the device from outside would
        // land at an arbitrary point inside that frame: it would be drawn into a
        // buffer the game is done with, or not yet at, and it would be there while
        // the game has its own record of the state it set. The context the game is
        // recording into is handed over here, and what is recorded into it is part
        // of the frame, in the order it was recorded in. Only the API with such
        // contexts uses it, nullptr means the context of the device.
        virtual void SetCommandContext(void* /*pContext*/) {}

        // A device or a swap chain of the game can be replaced without a resize or
        // a reset being reported, a fullscreen switch of an emulator does exactly
        // that, and the new one is what Init is handed next. A backend that keeps
        // such an object has to follow it, the alternative is drawing into a
        // surface that is never presented again.
        //
        // Returning false means the change is too large to be absorbed and the
        // backend is built again from scratch.
        virtual bool UpdateNative(void* /*pNative*/) { return true; }

        // A device can be destroyed without anybody saying so, which is what
        // happens when an emulator switches its renderer. Everything the backend
        // holds then belongs to a device that is gone and must only be forgotten,
        // never given back. Called before Shutdown, which afterwards finds
        // nothing to release.
        virtual void Detach() {}

        // Whether the drops are drawn with a shader that gathers the light of the
        // frame around them, out of a device that may not be able to run one at
        // all, see WaterDrops::GatheredLight. Only the backend of Direct3D 8
        // answers this with something that is not known before the device is: its
        // drops are drawn with a shader of model 1, which it builds itself.
        virtual bool GathersLight() const { return false; }

        // Sets the transform used by the vertices. Width and height are the
        // target size, they are what the corners of the screen map to.
        virtual void SetProjection(Projection projection, const Matrix* pWorld, float width, float height) = 0;

        // The scene texture is sampled with the texture coordinates of the
        // vertices. On the console versions the frame can be letterboxed, the
        // games then scale those coordinates.
        virtual void SetSceneUVScale(float offsetX, float scaleX, float offsetY, float scaleY) = 0;

        // Snow keeps what is behind it bright instead of darkening it, exactly
        // like the second texture stage of the original code.
        virtual void SetSceneComplement(bool enabled) = 0;

        // The snow particles of the original code did not look at the frame at
        // all, they only blended their own texture on top of it. Turning the
        // sampling off is the same as clearing the second texture stage.
        virtual void SetSceneSampling(bool enabled) = 0;

        // A frame that is presented has been turned over to be the right way up
        // on the screen, and a copy of it that is read out of the window is the
        // other way round than the frame itself: an OpenGL window lies the other
        // way up than a frame of a game. Everything that follows the frame when
        // the drops move is turned over with this, or a drop moves up the screen
        // instead of down it. Only the OpenGL backend reads a window, the rest
        // hand the frame over themselves and have nothing to do here.
        virtual void SetPresentSceneFlipY(bool /*enabled*/) {}

        // Copies the current target into the scene texture, applies the states
        // and draws the whole batch in one go. Triangels are expected to come
        // in quads, the index buffer of the backend turns them into triangles.
        virtual void Render(const Vertex* pVertices, int numVertices, PrimitiveType primitive) = 0;
    };

    // -----------------------------------------------------------------------
    // the current backend
    // -----------------------------------------------------------------------
    inline RendererId currentRenderer = RENDERER_NONE;
    inline Backend* pBackend = nullptr;

    // Every backend is built for one device, and everything that came out of that
    // device is useless to the next one. This counts the backends there have been,
    // so the effect can tell that its textures belong to a device that is gone.
    inline uint32_t backendGeneration = 0;

    inline uint32_t GetBackendGeneration()
    {
        return backendGeneration;
    }

    // The queue of the game can be handed over before the renderer exists, which
    // is how the present hook of Direct3D 12 does it. It is passed on to the
    // backend that is created next, so it never outlives a frame.
    inline void*& PendingCommandQueue()
    {
        static void* pQueue = nullptr;
        return pQueue;
    }

    // Backends are declared here so the dispatcher below does not have to know
    // which ones were compiled in.
    namespace Detail
    {
        struct Entry
        {
            RendererId id;
            Backend* (*create)();
        };

        inline std::vector<Entry>& Entries()
        {
            static std::vector<Entry> entries;
            return entries;
        }

        struct Register
        {
            Register(RendererId id, Backend* (*create)())
            {
                Entries().push_back({ id, create });
            }
        };

        inline Backend* Create(RendererId id)
        {
            for (auto& entry : Entries())
                if (entry.id == id)
                    return entry.create();

            return nullptr;
        }
    }

    // -----------------------------------------------------------------------
    // entry points used by the effect
    // -----------------------------------------------------------------------
    inline void Shutdown();

    inline bool Init(RendererId id, void* pNative)
    {
        if (pBackend && currentRenderer == id)
        {
            // The backend of the renderer is kept, but the object it was built on
            // may have been replaced since, see UpdateNative.
            if (pBackend->IsActive() && pBackend->UpdateNative(pNative))
                return true;
        }

        // The renderer of the game was switched, which an emulator does while a
        // game runs. The device of the backend that is left went with the one that
        // was left behind, so everything it holds is forgotten here instead of
        // being handed back to a driver that no longer owns it.
        if (pBackend)
            pBackend->Detach();

        Shutdown();

        pBackend = Detail::Create(id);
        if (!pBackend)
            return false;

        currentRenderer = id;

        // Direct3D 12 needs the queue of the game to submit on, and the caller
        // hands it over before the renderer is initialized.
        if (id == RENDERER_D3D12 && PendingCommandQueue())
            pBackend->SetCommandQueue(PendingCommandQueue());

        if (!pBackend->Init(pNative))
        {
            pBackend->Shutdown();
            delete pBackend;
            pBackend = nullptr;
            currentRenderer = RENDERER_NONE;
            return false;
        }

        backendGeneration++;
        return true;
    }

    inline void Shutdown()
    {
        if (pBackend)
        {
            pBackend->Shutdown();
            delete pBackend;
            pBackend = nullptr;
        }

        currentRenderer = RENDERER_NONE;
    }

    inline void Reset()
    {
        if (pBackend)
            pBackend->Reset();
    }

    inline bool IsActive()
    {
        return pBackend && pBackend->IsActive();
    }

    inline RendererId GetRenderer()
    {
        return currentRenderer;
    }

    inline Size GetSize()
    {
        return pBackend ? pBackend->GetSize() : Size{};
    }

    inline void SetMaskTexture(Texture* pMask)
    {
        if (pBackend)
            pBackend->SetMaskTexture(pMask);
    }

    inline Texture* CreateTexture(int width, int height, const uint8_t* pixels)
    {
        return pBackend ? pBackend->CreateTexture(width, height, pixels) : nullptr;
    }

    inline void DestroyTexture(Texture* pTexture)
    {
        if (!pTexture)
            return;

        if (pBackend)
            pBackend->DestroyTexture(pTexture);
        else
            delete pTexture;
    }

    inline void SetTarget(RenderTarget* pTarget)
    {
        if (pBackend)
            pBackend->SetTarget(pTarget);
    }

    // Tells the renderer where the target is in its life when it is not obvious,
    // Direct3D 12 and Vulkan need it, the other APIs ignore it.
    inline void SetTargetState(TargetState state)
    {
        if (pBackend)
            pBackend->SetTargetState(state);
    }

    // Hands the graphics queue of the game over, Direct3D 12 and Vulkan use it
    // for their submissions.
    inline void SetCommandQueue(void* pQueue)
    {
        PendingCommandQueue() = pQueue;

        if (pBackend)
            pBackend->SetCommandQueue(pQueue);
    }

    // The context the commands are recorded into, nullptr for the one of the device,
    // see Backend::SetCommandContext. The pointer is only borrowed by the renderer.
    inline void SetCommandContext(void* pContext)
    {
        if (pBackend)
            pBackend->SetCommandContext(pContext);
    }

    // The device the backend was created with is gone, see Backend::Detach.
    inline void Detach()
    {
        if (pBackend)
            pBackend->Detach();
    }

    inline void SetProjection(Projection projection, const Matrix* pWorld = nullptr)
    {
        if (!pBackend)
            return;

        Size size = pBackend->GetSize();
        pBackend->SetProjection(projection, pWorld, (float)size.width, (float)size.height);
    }

    inline void SetSceneUVScale(float offsetX, float scaleX, float offsetY, float scaleY)
    {
        if (pBackend)
            pBackend->SetSceneUVScale(offsetX, scaleX, offsetY, scaleY);
    }

    inline void SetSceneComplement(bool enabled)
    {
        if (pBackend)
            pBackend->SetSceneComplement(enabled);
    }

    inline void SetPresentSceneFlipY(bool enabled)
    {
        if (pBackend)
            pBackend->SetPresentSceneFlipY(enabled);
    }

    inline void SetSceneSampling(bool enabled)
    {
        if (pBackend)
            pBackend->SetSceneSampling(enabled);
    }

    // Whether the drops are drawn with a shader that gathers the light of the
    // frame around them, see Backend::GathersLight.
    inline bool GathersLight()
    {
        return pBackend && pBackend->GathersLight();
    }

    inline void Render(const Vertex* pVertices, int numVertices, PrimitiveType primitive = PRIMITIVE_TRIANGLES)
    {
        if (numVertices > 0 && pBackend)
            pBackend->Render(pVertices, numVertices, primitive);
    }
}
