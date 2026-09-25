#pragma once
// ---------------------------------------------------------------------------
// The backend that draws with the drawing of the emulator.
//
// An emulator that runs a plugin of its own process can hand it the drawing of
// its backend where the frame of a game is in between its world and its UI, see
// PPSSPP_RegisterBeforeUIDrawDraw and GPUCommon.h of PPSSPP. That drawing is the
// Draw::DrawContext of Common/GPU/thin3d.h, which every backend of the emulator
// implements (OpenGL, Direct3D 11, Vulkan, ...), so one backend of the effect
// serves all of them: no hook has to know which API the emulator picked, and
// nothing has to be guessed about the order of the drawing of a frame.
//
// Three things are what such a drawing is used for here:
//
//   1. the copy of the frame behind the drops, which is the picture they
//      refract. A framebuffer cannot be sampled while it is being drawn into, so
//      the frame is blitted into a framebuffer of our own, which is then bound as
//      a texture,
//   2. the frame itself, bound as the render target, so that the drops land in
//      the frame and not wherever the emulator left its own binding,
//   3. the pipeline of the drops: the shaders of source/shaders/thin3d/glsl.h, the layout
//      of Xrd::Vertex, one dynamic constant buffer, the copy of the frame and the
//      atlas of the drop shapes as the two textures, and the one blend the effect
//      uses.
//
// The drawing, the frame and the size of it are handed over for every frame the
// emulator reports, see UpdateNative: a frame of a game can be resized, and a
// renderer that was switched brings a new drawing with it.
//
// The objects of the drawing (textures, buffers, pipelines, ...) are reference
// counted and only the emulator has the code that gives one back, so it hands
// that over with the rest, see Thin3DTarget::release.
// ---------------------------------------------------------------------------

#include "xrdrender.h"
#include "../shaders/thin3d/glsl.h"
#include "../shaders/generated/thin3d.h"

// The drawing of the emulator. The header of it is the copy in this repository,
// so that the build of the plugin needs no checkout of the emulator: it is the
// interface of the drawing itself, and the emulator has to be one of a version
// that interface still fits, see external/ppsspp/README.md. Every call below
// goes through the vtable of the drawing.
#include "Common/GPU/thin3d.h"

#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

namespace Xrd
{
    // What the emulator hands over at the point between the world and the UI of
    // a frame of a game: the PPSSPPBeforeUIDrawTarget of the emulator with the
    // types its two pointers stand for. It is what Init and UpdateNative are
    // handed, see Thin3DBackend.
    struct Thin3DTarget
    {
        Draw::DrawContext* pDrawing = nullptr;      // the drawing of the backend of the emulator
        Draw::Framebuffer* pFrame = nullptr;        // the frame of the moment
        int width = 0;                              // its size, which is what the effect lays the drops out for
        int height = 0;
        void (*release)(void* object) = nullptr;    // gives an object of that drawing back
    };

    // A line for the log of the host: which drawing is there, which language it
    // wants its shaders in and what went wrong if something did. Temporary, for
    // the first runs of this backend.
    inline char Thin3DStatus[256] = "the backend of the drawing of the emulator is not initialized";

    inline const char* Thin3DGetStatus()
    {
        return Thin3DStatus;
    }

    class Thin3DBackend : public Backend
    {
    public:
        ~Thin3DBackend() override
        {
            Shutdown();
        }

        bool Init(void* pNative) override
        {
            if (!pNative)
                return false;

            UpdateTarget((const Thin3DTarget*)pNative);

            if (!pDrawing || !pFrame)
            {
                snprintf(Thin3DStatus, sizeof(Thin3DStatus), "the emulator handed over no %s", pDrawing ? "frame" : "drawing");
                return false;
            }

            snprintf(Thin3DStatus, sizeof(Thin3DStatus), "the drawing %p of the emulator wants its shaders in %s, the frame %p is %dx%d",
                (void*)pDrawing, ShaderLanguageName(pDrawing->GetShaderLanguageDesc().shaderLanguage),
                (void*)pFrame, pFrame->Width(), pFrame->Height());

            return true;
        }

        // Every frame is handed over again, and a frame of a game can be resized
        // while it runs: the frame and the size are taken, the drawing is only
        // taken if it is the same one. A new drawing means a new device, and
        // everything of this backend was built on the old one (false).
        bool UpdateNative(void* pNative) override
        {
            if (!pNative)
                return false;

            const Thin3DTarget* pTarget = (const Thin3DTarget*)pNative;

            if (pTarget->pDrawing != pDrawing)
                return false;

            UpdateTarget(pTarget);
            return true;
        }

        // The device the drawing belongs to is gone. Nothing of this backend may
        // be given back to it any more, so it is only forgotten.
        void Detach() override
        {
            ForgetObjects();

            pDrawing = nullptr;
            pFrame = nullptr;
            releaseObject = nullptr;
            target = Thin3DTarget{};
        }

        void Shutdown() override
        {
            // Only a drawing that is still there can take its own objects back.
            if (pDrawing && releaseObject)
            {
                releaseObject(pipeline);
                releaseObject(inputLayout);
                releaseObject(vertexShader);
                releaseObject(pixelShader);
                releaseObject(blendState);
                releaseObject(depthState);
                releaseObject(rasterState);
                releaseObject(samplers[0]);
                releaseObject(samplers[1]);
                releaseObject(vertexBuffer);
                releaseObject(indexBuffer);
                releaseObject(sceneFramebuffer);
            }

            ForgetObjects();
        }

        // The frame of a game was resized or the window it is shown in was: the
        // copy of the frame has to follow the new size, which happens with the
        // next frame, see EnsureScene.
        void Reset() override
        {
            ReleaseScene();
        }

        bool IsActive() const override
        {
            return pDrawing != nullptr && pFrame != nullptr;
        }

        Size GetSize() const override
        {
            if (target.width <= 0 || target.height <= 0)
                return Size{};

            return Size{ target.width, target.height };
        }

        void SetMaskTexture(Texture* pMask) override
        {
            pMaskTexture = pMask;
        }

        Texture* CreateTexture(int width, int height, const uint8_t* pixels) override
        {
            if (!pDrawing || width <= 0 || height <= 0)
                return nullptr;

            Draw::TextureDesc desc{};
            desc.type = Draw::TextureType::LINEAR2D;
            desc.format = Draw::DataFormat::R8G8B8A8_UNORM;
            desc.width = width;
            desc.height = height;
            desc.depth = 1;
            desc.mipLevels = 1;
            desc.generateMips = false;
            desc.swizzle = Draw::TextureSwizzle::DEFAULT;
            desc.tag = "xrd texture";

            if (pixels)
                desc.initData.push_back(pixels);

            Draw::Texture* pTexture = pDrawing->CreateTexture(desc);

            if (!pTexture)
            {
                snprintf(Thin3DStatus, sizeof(Thin3DStatus), "the drawing of the emulator made no texture of %dx%d", width, height);
                return nullptr;
            }

            // Only the effect ever passes this back, so the drawing keeps the
            // texture and the wrapper stays what the effect sees.
            Texture* pWrapper = new Texture();
            pWrapper->resource = pTexture;
            pWrapper->size = Size{ width, height };
            return pWrapper;
        }

        void DestroyTexture(Texture* pTexture) override
        {
            if (!pTexture)
                return;

            Release(pTexture->resource);
            delete pTexture;
        }

        // The frame of the game is what the drops are drawn into and it is handed
        // over with every frame, so there is nothing to be picked here: a game
        // hook cannot hand another target to this backend.
        void SetTarget(RenderTarget* /*pTarget*/) override {}

        // The drawing of the emulator keeps track of where its target is in its
        // life itself, nothing has to be told.
        void SetTargetState(TargetState /*state*/) override {}

        void SetProjection(Projection projection, const Matrix* pWorld, float width, float height) override
        {
            if (projection == PROJECTION_WORLD && pWorld)
            {
                memcpy(projectionMatrix.m, pWorld->m, sizeof(projectionMatrix.m));
                return;
            }

            // The drops are laid out the way a game lays out a frame, with the y
            // going down from the top left. Which way that is in a framebuffer
            // depends on the backend of the emulator: OpenGL and Vulkan put the
            // first row of one where the y goes up, Direct3D puts it where it goes
            // down, which is why the emulator turns the y of Direct3D over in the
            // shaders of its own, see Shader.h of the emulator (viewportYSign).
            // This is that same turn, done with the matrix instead of with the
            // shader, so that a drop falls down on every backend.
            if (UsesDirect3DConvention())
                projectionMatrix = Matrix::OrthographicOffCenter(0.0f, width, height, 0.0f, 0.0f, 1.0f);
            else
                projectionMatrix = Matrix::OrthographicOffCenter(0.0f, width, 0.0f, height, 0.0f, 1.0f);
        }

        void SetSceneUVScale(float offsetX, float scaleX, float offsetY, float scaleY) override
        {
            uvOffsetX = offsetX;
            uvScaleX = scaleX;
            uvOffsetY = offsetY;
            uvScaleY = scaleY;
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
            if (!IsActive() || !EnsureDeviceObjects() || !EnsureScene()) return false;
            staging.reserve(maxVertices);
            indices.reserve((maxVertices / 4) * 6);
            return EnsureBuffers(maxVertices, (maxVertices / 4) * 6);
        }

        void Render(const Vertex* pVertices, int numVertices, PrimitiveType primitive) override
        {
            if (!IsActive() || !pVertices || numVertices <= 0)
                return;

            const Size size = GetSize();

            if (size.width <= 0 || size.height <= 0)
            {
                snprintf(Thin3DStatus, sizeof(Thin3DStatus), "the frame of the emulator has no size");
                return;
            }

            if (!EnsureDeviceObjects() || !EnsureScene())
                return;

            Draw::Texture* pMask = pMaskTexture ? (Draw::Texture*)pMaskTexture->resource : nullptr;

            if (!pMask)
                return;

            // -----------------------------------------------------------------
            // the copy of the frame behind the drops
            // -----------------------------------------------------------------
            // The copy is taken in the size of the framebuffer of the frame
            // itself, and the texture coordinates of the drops walk over it from
            // one corner to the other, so nothing of it is scaled here.
            //
            // Which call a framebuffer can be copied with is a question for the
            // drawing, exactly like the emulator asks it itself, see
            // FramebufferManagerCommon::BlitFramebuffer: the drawing of Direct3D 11
            // has no blit at all (its BlitFramebuffer crashes on purpose) and only
            // copies, the ones of OpenGL and Vulkan do both.
            const Draw::DeviceCaps& caps = pDrawing->GetDeviceCaps();

            if (caps.framebufferBlitSupported)
            {
                pDrawing->BlitFramebuffer(pFrame, 0, 0, sceneSize.width, sceneSize.height,
                    sceneFramebuffer, 0, 0, sceneSize.width, sceneSize.height,
                    Draw::Aspect::COLOR_BIT, Draw::FB_BLIT_LINEAR, "xrd scene");
            }
            else if (caps.framebufferCopySupported)
            {
                pDrawing->CopyFramebufferImage(pFrame, 0, 0, 0, 0,
                    sceneFramebuffer, 0, 0, 0, 0,
                    sceneSize.width, sceneSize.height, 1,
                    Draw::Aspect::COLOR_BIT, "xrd scene");
            }
            else
            {
                snprintf(Thin3DStatus, sizeof(Thin3DStatus), "the drawing of the emulator can neither blit nor copy a framebuffer, so the drops have nothing to refract");
                return;
            }

            // -----------------------------------------------------------------
            // the drops themselves, in the frame of the game
            // -----------------------------------------------------------------
            Draw::RenderPassInfo pass{};
            pass.color = Draw::RPAction::KEEP;      // the world of the frame is in it, it is not cleared
            pass.depth = Draw::RPAction::KEEP;
            pass.stencil = Draw::RPAction::KEEP;
            pass.tag = "xrd drops";

            pDrawing->BindFramebufferAsRenderTarget(pFrame, pass, "xrd drops");

            // The drops cover the whole frame and are drawn over whatever the last
            // draw of the game left behind: neither its viewport nor its scissor
            // is what they can use.
            Draw::Viewport viewport{ 0.0f, 0.0f, (float)size.width, (float)size.height, 0.0f, 1.0f };
            pDrawing->SetViewport(viewport);
            pDrawing->SetScissorRect(0, 0, size.width, size.height);

            if (!UploadGeometry(pVertices, numVertices, primitive))
                return;

            Constants constants{};
            memcpy(constants.projection, projectionMatrix.m, sizeof(constants.projection));
            constants.uvOffset[0] = uvOffsetX;
            constants.uvOffset[1] = uvOffsetY;
            constants.uvScale[0] = uvScaleX;
            constants.uvScale[1] = uvScaleY;
            constants.sceneComplement[0] = sceneComplement ? 1.0f : 0.0f;
            constants.sceneComplement[1] = sceneSampling ? 1.0f : 0.0f;

            pDrawing->BindPipeline(pipeline);
            pDrawing->UpdateDynamicUniformBuffer(&constants, sizeof(constants));
            pDrawing->BindSamplerStates(0, 2, samplers);

            // The copy of the frame is a framebuffer and not an ordinary texture,
            // and only the drawing of the emulator can bind one of those:
            // BindFramebufferAsTexture is that. The atlas of the drop shapes is an
            // ordinary texture and goes in the slot next to it.
            pDrawing->BindFramebufferAsTexture(sceneFramebuffer, 0, Draw::Aspect::COLOR_BIT, 0);
            pDrawing->BindTextures(1, 1, &pMask);

            pDrawing->BindVertexBuffer(vertexBuffer, 0);
            pDrawing->BindIndexBuffer(indexBuffer, 0);
            pDrawing->DrawIndexed(indexCount, 0);

            // Temporary, with Thin3DStatus: that the drawing was asked for and made it
            // through is the other half of what a run that shows nothing has to answer.
            if (!drawnOnce)
            {
                drawnOnce = true;
                snprintf(Thin3DStatus, sizeof(Thin3DStatus), "the drops of %d vertices are drawn into the frame %p",
                    numVertices, (void*)pFrame);
            }
        }

    private:
        struct Constants
        {
            float projection[16];
            float uvOffset[4];
            float uvScale[4];
            float sceneComplement[4];
        };

        void UpdateTarget(const Thin3DTarget* pTarget)
        {
            target = *pTarget;
            pDrawing = pTarget->pDrawing;
            pFrame = pTarget->pFrame;
            releaseObject = pTarget->release;
        }

        // Direct3D needs the turn described in SetProjection, OpenGL and Vulkan do
        // not.
        bool UsesDirect3DConvention() const
        {
            return pDrawing && pDrawing->GetShaderLanguageDesc().shaderLanguage == ShaderLanguage::HLSL_D3D11;
        }

        void Release(void* pObject)
        {
            if (pObject && releaseObject)
                releaseObject(pObject);
        }

        void ForgetObjects()
        {
            pipeline = nullptr;
            inputLayout = nullptr;
            vertexShader = nullptr;
            pixelShader = nullptr;
            blendState = nullptr;
            depthState = nullptr;
            rasterState = nullptr;
            samplers[0] = nullptr;
            samplers[1] = nullptr;
            vertexBuffer = nullptr;
            indexBuffer = nullptr;
            sceneFramebuffer = nullptr;
            sceneSize = Size{};
            vertexCapacity = 0;
            indexCapacity = 0;
            indexCount = 0;
        }

        // -----------------------------------------------------------------------
        // the pipeline of the drops
        // -----------------------------------------------------------------------
        bool EnsureDeviceObjects()
        {
            if (pipeline)
                return true;

            if (!CreateShaders() || !CreatePipeline())
            {
                // Whatever was made before the failure is given back, so that the
                // next frame builds it again instead of keeping half a pipeline.
                Release(vertexShader);
                Release(pixelShader);
                vertexShader = nullptr;
                pixelShader = nullptr;
                return false;
            }

            return true;
        }

        bool CreateShaders()
        {
            const ShaderLanguage language = pDrawing->GetShaderLanguageDesc().shaderLanguage;

            switch (language)
            {
            case ShaderLanguage::HLSL_D3D11:
                vertexShader = pDrawing->CreateShaderModule(ShaderStage::Vertex, language,
                    (const uint8_t*)Shaders::Thin3D::D3D11VertexSource, strlen(Shaders::Thin3D::D3D11VertexSource), "xrd vertex");
                pixelShader = pDrawing->CreateShaderModule(ShaderStage::Fragment, language,
                    (const uint8_t*)Shaders::Thin3D::D3D11PixelSource, strlen(Shaders::Thin3D::D3D11PixelSource), "xrd pixel");
                break;

            case ShaderLanguage::GLSL_VULKAN:
                // The drawing of the emulator compiles the GLSL of a Vulkan shader itself, see
                // GLSLtoSPV in Common/GPU/Vulkan/VulkanContext.cpp, so this is the same kind of
                // source as the one of OpenGL and not a compiled module.
                vertexShader = pDrawing->CreateShaderModule(ShaderStage::Vertex, language,
                    (const uint8_t*)Shaders::Thin3D::VulkanVertexSource, strlen(Shaders::Thin3D::VulkanVertexSource), "xrd vertex");
                pixelShader = pDrawing->CreateShaderModule(ShaderStage::Fragment, language,
                    (const uint8_t*)Shaders::Thin3D::VulkanPixelSource, strlen(Shaders::Thin3D::VulkanPixelSource), "xrd pixel");
                break;

            default:
            {
                // OpenGL, whose shaders are written for the version and the words
                // the context it runs on actually accepts.
                const std::string vertexSource = Shaders::Thin3D::BuildVertexSource(pDrawing->GetShaderLanguageDesc());
                const std::string pixelSource = Shaders::Thin3D::BuildFragmentSource(pDrawing->GetShaderLanguageDesc());

                vertexShader = pDrawing->CreateShaderModule(ShaderStage::Vertex, language,
                    (const uint8_t*)vertexSource.c_str(), vertexSource.size(), "xrd vertex");
                pixelShader = pDrawing->CreateShaderModule(ShaderStage::Fragment, language,
                    (const uint8_t*)pixelSource.c_str(), pixelSource.size(), "xrd pixel");
                break;
            }
            }

            if (!vertexShader || !pixelShader)
            {
                snprintf(Thin3DStatus, sizeof(Thin3DStatus), "the drawing of the emulator refused the shaders of the drops (%s)",
                    ShaderLanguageName(language));
                return false;
            }

            return true;
        }

        bool CreatePipeline()
        {
            // The colour of a vertex is an uint32 in memory and the drawing of the
            // emulator reads it the way the effect writes it, see the comment at
            // the top of source/shaders/thin3d/glsl.h. The vertex buffer is filled with the
            // two channels swapped, see UploadGeometry, which is why this layout
            // says RGBA while every other backend of the effect says BGRA.
            Draw::InputLayoutDesc layout{};
            layout.stride = sizeof(Vertex);
            layout.attributes.push_back({ Draw::SEM_POSITION, Draw::DataFormat::R32G32B32_FLOAT, (int)offsetof(Vertex, x) });
            layout.attributes.push_back({ Draw::SEM_COLOR0, Draw::DataFormat::R8G8B8A8_UNORM, (int)offsetof(Vertex, color) });
            layout.attributes.push_back({ Draw::SEM_TEXCOORD0, Draw::DataFormat::R32G32B32A32_FLOAT, (int)offsetof(Vertex, u0) });
            inputLayout = pDrawing->CreateInputLayout(layout);

            // A drop darkens what is behind it, exactly like it does on every
            // other backend of the effect: src alpha, one minus src alpha.
            Draw::BlendStateDesc blend{};
            blend.enabled = true;
            blend.colorMask = Draw::COLOR_MASK_R | Draw::COLOR_MASK_G | Draw::COLOR_MASK_B | Draw::COLOR_MASK_A;
            blend.srcCol = Draw::BlendFactor::SRC_ALPHA;
            blend.dstCol = Draw::BlendFactor::ONE_MINUS_SRC_ALPHA;
            blend.eqCol = Draw::BlendOp::ADD;
            blend.srcAlpha = Draw::BlendFactor::SRC_ALPHA;
            blend.dstAlpha = Draw::BlendFactor::ONE_MINUS_SRC_ALPHA;
            blend.eqAlpha = Draw::BlendOp::ADD;
            blend.logicEnabled = false;
            blend.logicOp = Draw::LogicOp::LOGIC_COPY;
            blendState = pDrawing->CreateBlendState(blend);

            // A drop is drawn over the world of a frame and never hides behind it,
            // so no depth buffer is looked at - and the game may well have left a
            // stencil test that every pixel of the drops fails.
            Draw::DepthStencilStateDesc depth{};
            depth.depthTestEnabled = false;
            depth.depthWriteEnabled = false;
            depth.depthCompare = Draw::Comparison::LESS;
            depth.stencilEnabled = false;
            depth.stencil.failOp = Draw::StencilOp::KEEP;
            depth.stencil.passOp = Draw::StencilOp::KEEP;
            depth.stencil.depthFailOp = Draw::StencilOp::KEEP;
            depth.stencil.compareOp = Draw::Comparison::ALWAYS;
            depthState = pDrawing->CreateDepthStencilState(depth);

            Draw::RasterStateDesc raster{};
            raster.cull = Draw::CullMode::NONE;
            raster.frontFace = Draw::Facing::CCW;
            rasterState = pDrawing->CreateRasterState(raster);

            Draw::SamplerStateDesc samplerDesc{};
            samplerDesc.magFilter = Draw::TextureFilter::LINEAR;
            samplerDesc.minFilter = Draw::TextureFilter::LINEAR;
            samplerDesc.mipFilter = Draw::TextureFilter::NEAREST;
            samplerDesc.maxAniso = 1.0f;
            samplerDesc.wrapU = Draw::TextureAddressMode::CLAMP_TO_EDGE;
            samplerDesc.wrapV = Draw::TextureAddressMode::CLAMP_TO_EDGE;
            samplerDesc.wrapW = Draw::TextureAddressMode::CLAMP_TO_EDGE;
            samplerDesc.shadowCompareEnabled = false;
            samplerDesc.shadowCompareFunc = Draw::Comparison::ALWAYS;
            samplerDesc.borderColor = Draw::BorderColor::DONT_CARE;
            samplers[0] = pDrawing->CreateSamplerState(samplerDesc);
            samplers[1] = pDrawing->CreateSamplerState(samplerDesc);

            if (!inputLayout || !blendState || !depthState || !rasterState || !samplers[0] || !samplers[1])
            {
                snprintf(Thin3DStatus, sizeof(Thin3DStatus), "the drawing of the emulator made no state of the drops");
                return false;
            }

            // One constant buffer of four members for every language, and the
            // names of them, which is all a backend without constant buffers has
            // to look its uniforms up by (OpenGL).
            static const UniformBufferDesc constantsDesc{ sizeof(Constants), {
                { "projection", 0, -1, UniformType::MATRIX4X4, 0 },
                { "uvOffset", 0, -1, UniformType::FLOAT4, 64 },
                { "uvScale", 0, -1, UniformType::FLOAT4, 80 },
                { "sceneComplement", 0, -1, UniformType::FLOAT4, 96 },
            } };

            static const SamplerDef samplerDefs[2] = {
                { 0, "sceneTexture" },
                { 1, "maskTexture" },
            };

            Draw::PipelineDesc desc{
                Draw::Primitive::TRIANGLE_LIST,
                { vertexShader, pixelShader },
                inputLayout,
                depthState,
                blendState,
                rasterState,
                &constantsDesc,
                Slice<SamplerDef>(samplerDefs, 2),
            };

            pipeline = pDrawing->CreateGraphicsPipeline(desc, "xrd drops");

            if (!pipeline)
            {
                snprintf(Thin3DStatus, sizeof(Thin3DStatus), "the drawing of the emulator made no pipeline of the drops (%s)",
                    ShaderLanguageName(pDrawing->GetShaderLanguageDesc().shaderLanguage));
                return false;
            }

            return true;
        }

        // -----------------------------------------------------------------------
        // the copy of the frame behind the drops
        // -----------------------------------------------------------------------
        bool EnsureScene()
        {
            const int width = pFrame->Width();
            const int height = pFrame->Height();

            if (sceneFramebuffer && sceneSize.width == width && sceneSize.height == height)
                return true;

            ReleaseScene();

            if (width <= 0 || height <= 0)
                return false;

            // The drawing of the emulator makes the textures of a framebuffer
            // itself, and binds one of them as a texture on its own, see
            // BindFramebufferAsTexture.
            Draw::FramebufferDesc desc{};
            desc.width = width;
            desc.height = height;
            desc.depth = 1;
            desc.numLayers = 1;
            desc.multiSampleLevel = 0;
            desc.z_stencil = false;
            desc.tag = "xrd scene";
            sceneFramebuffer = pDrawing->CreateFramebuffer(desc);

            if (!sceneFramebuffer)
            {
                snprintf(Thin3DStatus, sizeof(Thin3DStatus), "the drawing of the emulator made no framebuffer for the copy of the frame");
                return false;
            }

            // The framebuffer is never drawn into: the frame is copied over the
            // whole of it and it is then read as a texture, so there is nothing to
            // bind and nothing of it that has to be told it is not to be kept.
            sceneSize = Size{ width, height };
            return true;
        }

        void ReleaseScene()
        {
            Release(sceneFramebuffer);
            sceneFramebuffer = nullptr;
            sceneSize = Size{};
        }

        // -----------------------------------------------------------------------
        // the vertices and the indices of one batch
        // -----------------------------------------------------------------------

        // The effect hands over quads (four vertices that make two triangles) or
        // a triangle strip, while the pipeline of this backend draws a list of
        // triangles: the indices are made here, six per quad and three for every
        // vertex of a strip after the first two.
        bool UploadGeometry(const Vertex* pVertices, int numVertices, PrimitiveType primitive)
        {
            int quads = 0;
            int count = 0;

            if (primitive == PRIMITIVE_TRIANGLES)
            {
                quads = numVertices / 4;

                if (quads <= 0)
                    return false;

                count = quads * 6;
            }
            else
            {
                if (numVertices < 3)
                    return false;

                count = (numVertices - 2) * 3;
            }

            if (!EnsureBuffers(numVertices, count))
                return false;

            // A D3DCOLOR is BGRA in memory and the drawing of the emulator reads
            // the colour of a vertex as RGBA, so the two channels are swapped
            // while the buffer is filled. Nothing else about a vertex changes.
            staging.resize((size_t)numVertices);

            for (int i = 0; i < numVertices; i++)
            {
                const Vertex& source = pVertices[i];
                Vertex& vertex = staging[i];

                vertex = source;
                vertex.color = (source.color & 0xFF00FF00u) | ((source.color & 0x00FF0000u) >> 16) | ((source.color & 0x000000FFu) << 16);
            }

            if (quads > 0)
            {
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
            else
            {
                for (int i = 0; i < numVertices - 2; i++)
                {
                    indices[i * 3 + 0] = (uint16_t)(i + 0);
                    indices[i * 3 + 1] = (uint16_t)(i + 1);
                    indices[i * 3 + 2] = (uint16_t)(i + 2);
                }
            }

            pDrawing->UpdateBuffer(vertexBuffer, (const uint8_t*)staging.data(), 0, (size_t)numVertices * sizeof(Vertex), Draw::UpdateBufferFlags::UPDATE_DISCARD);
            pDrawing->UpdateBuffer(indexBuffer, (const uint8_t*)indices.data(), 0, (size_t)count * sizeof(uint16_t), Draw::UpdateBufferFlags::UPDATE_DISCARD);

            indexCount = count;
            return true;
        }

        // Buffers that are big enough are kept: the effect draws a batch of the
        // same size for a whole session, so growing happens once.
        bool EnsureBuffers(int vertexCount, int count)
        {
            if (vertexBuffer && indexBuffer && vertexCount <= vertexCapacity && count <= indexCapacity)
                return true;

            if (vertexCount > 65535)
            {
                // The indices of the drawing of the emulator are 16 bit, like the
                // ones of every other backend of the effect.
                snprintf(Thin3DStatus, sizeof(Thin3DStatus), "a batch of %d vertices is more than the indices of the drops can name", vertexCount);
                return false;
            }

            if (!vertexBuffer || vertexCount > vertexCapacity)
            {
                Release(vertexBuffer);
                vertexBuffer = nullptr;

                // Some room to grow: a batch that grew once grows again.
                vertexCapacity = vertexCount + 512;
                vertexBuffer = pDrawing->CreateBuffer((size_t)vertexCapacity * sizeof(Vertex),
                    Draw::BufferUsageFlag::VERTEXDATA | Draw::BufferUsageFlag::DYNAMIC);

                if (!vertexBuffer)
                {
                    vertexCapacity = 0;
                    snprintf(Thin3DStatus, sizeof(Thin3DStatus), "the drawing of the emulator made no vertex buffer");
                    return false;
                }

                staging.reserve((size_t)vertexCapacity);
            }

            if (!indexBuffer || count > indexCapacity)
            {
                Release(indexBuffer);
                indexBuffer = nullptr;

                indexCapacity = count + 1024;
                indexBuffer = pDrawing->CreateBuffer((size_t)indexCapacity * sizeof(uint16_t),
                    Draw::BufferUsageFlag::INDEXDATA | Draw::BufferUsageFlag::DYNAMIC);

                if (!indexBuffer)
                {
                    indexCapacity = 0;
                    snprintf(Thin3DStatus, sizeof(Thin3DStatus), "the drawing of the emulator made no index buffer");
                    return false;
                }

                indices.resize((size_t)indexCapacity);
            }

            return true;
        }

        static const char* ShaderLanguageName(ShaderLanguage language)
        {
            switch (language)
            {
            case ShaderLanguage::GLSL_1xx: return "the GLSL of an old OpenGL";
            case ShaderLanguage::GLSL_3xx: return "the GLSL of OpenGL 3 and newer";
            case ShaderLanguage::GLSL_VULKAN: return "the SPIR-V of Vulkan";
            case ShaderLanguage::HLSL_D3D11: return "the HLSL of Direct3D 11";
            default: return "an unknown language";
            }
        }

        Thin3DTarget target{};
        Draw::DrawContext* pDrawing = nullptr;
        Draw::Framebuffer* pFrame = nullptr;
        void (*releaseObject)(void* object) = nullptr;

        Draw::ShaderModule* vertexShader = nullptr;
        Draw::ShaderModule* pixelShader = nullptr;
        Draw::Pipeline* pipeline = nullptr;
        Draw::InputLayout* inputLayout = nullptr;
        Draw::BlendState* blendState = nullptr;
        Draw::DepthStencilState* depthState = nullptr;
        Draw::RasterState* rasterState = nullptr;
        Draw::SamplerState* samplers[2] = { nullptr, nullptr };

        Draw::Buffer* vertexBuffer = nullptr;
        Draw::Buffer* indexBuffer = nullptr;
        int vertexCapacity = 0;
        int indexCapacity = 0;
        int indexCount = 0;
        bool drawnOnce = false;

        Draw::Framebuffer* sceneFramebuffer = nullptr;
        Size sceneSize{};

        Texture* pMaskTexture = nullptr;

        Matrix projectionMatrix = Matrix::Identity();
        float uvOffsetX = 0.0f, uvScaleX = 1.0f;
        float uvOffsetY = 0.0f, uvScaleY = 1.0f;
        bool sceneComplement = false;
        bool sceneSampling = true;

        std::vector<Vertex> staging;
        std::vector<uint16_t> indices;
    };

    namespace Thin3DFactory
    {
        inline Detail::Register registrar(RENDERER_THIN3D, []() -> Backend* { return new Thin3DBackend(); });
    }
}
