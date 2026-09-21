#pragma once
// ---------------------------------------------------------------------------
// The Vulkan backend.
//
// Vulkan is the API with the least implicit state of all of them: no implicit
// target, no implicit queue, no implicit layout. Everything the effect needs is
// therefore handed over by the caller:
//
//   Init()            a Xrd::VulkanInitInfo: the device, the queue and the
//                     family the queue belongs to, which is what a command pool
//                     is created for
//   SetTarget()       a Xrd::RenderTarget: the image to draw into, its format
//                     and its size, and whether it is being presented or is in
//                     the middle of a frame
//
// The image is read into an internal copy first, which is what the drops
// refract, and then drawn into with a render pass that loads what is already
// there. Both of those are layouts of one image, so the backend transitions it
// itself:
//
//   presenting (a present hook):        PRESENT          -> TRANSFER_SRC
//                                       TRANSFER_SRC     -> COLOR_ATTACHMENT
//                                       COLOR_ATTACHMENT -> PRESENT
//   in the middle of a frame:           COLOR_ATTACHMENT -> TRANSFER_SRC
//                                       TRANSFER_SRC     -> COLOR_ATTACHMENT
//
// That second case only works when the caller is not inside a render pass of
// its own, Vulkan has no way to transition an image while one is open. A hook
// around the present call is the natural place, and that is where the wrapper
// draws them.
//
// Nothing here links against the Vulkan loader: vulkan-1.dll is opened at
// runtime and every entry point comes out of vkGetDeviceProcAddr, so building
// the project needs no SDK and no import library, only the headers that ship
// with the repository (external/vulkan/include).
// ---------------------------------------------------------------------------

#include "xrdrender.h"
#include "xrdspirv.h"

#include <vulkan/vulkan.h>
#include <vector>

namespace Xrd
{
    // -----------------------------------------------------------------------
    // what the caller tells the backend about the device it is running on
    // -----------------------------------------------------------------------
    struct VulkanInitInfo
    {
        VkInstance instance = VK_NULL_HANDLE;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        VkQueue queue = VK_NULL_HANDLE;
        uint32_t queueFamilyIndex = 0;

        // Whether the images of the swap chain may be read back, see
        // AllowReadingSwapchainImages. Without it the effect has nothing to
        // refract and leaves the frame alone.
        bool frameReadable = true;
    };

    // -----------------------------------------------------------------------
    // the entry points, loaded from the loader that happens to be installed
    // -----------------------------------------------------------------------
    namespace VulkanLoader
    {
        inline HMODULE Module()
        {
            static HMODULE module = LoadLibraryW(L"vulkan-1.dll");
            return module;
        }

        inline PFN_vkGetInstanceProcAddr GlobalGetInstanceProcAddr()
        {
            static PFN_vkGetInstanceProcAddr pfn = Module()
                ? (PFN_vkGetInstanceProcAddr)GetProcAddress(Module(), "vkGetInstanceProcAddr")
                : nullptr;

            return pfn;
        }

        inline PFN_vkGetDeviceProcAddr GlobalGetDeviceProcAddr()
        {
            static PFN_vkGetDeviceProcAddr pfn = Module()
                ? (PFN_vkGetDeviceProcAddr)GetProcAddress(Module(), "vkGetDeviceProcAddr")
                : nullptr;

            return pfn;
        }

        // for the instance functions the backend does not need but an
        // application does while it is setting itself up
        template <typename T>
        inline T LoadInstance(const char* name, VkInstance instance = VK_NULL_HANDLE)
        {
            PFN_vkGetInstanceProcAddr pfn = GlobalGetInstanceProcAddr();
            return pfn ? (T)pfn(instance, name) : nullptr;
        }

        inline bool Available()
        {
            return GlobalGetInstanceProcAddr() != nullptr;
        }
    }

    // -----------------------------------------------------------------------
    // reading the frame of the application
    //
    // The drops refract what is on the screen, so the image that is about to be
    // presented is read back into an own image on every frame. That is a transfer
    // from an image of the swap chain, and the swap chain of the application has
    // to allow it:
    //
    //   PCSX2 creates its swap chain with a color attachment and a transfer
    //   destination usage only, and reading such an image is undefined behavior.
    //   A driver is free to ignore that for a while and to stall the queue for
    //   good once it does not, which is what a freeze of the emulator looks like.
    //   Other applications (and the test application of this repository) ask for
    //   the source usage themselves.
    //
    // The usage is therefore widened before the swap chain is created: the hook of
    // the call runs before the description is forwarded to the driver, and the
    // description that is forwarded is the one of the application.
    // -----------------------------------------------------------------------
    inline bool AllowReadingSwapchainImages(VkPhysicalDevice physicalDevice, const VkSwapchainCreateInfoKHR* pCreateInfo)
    {
        if (!pCreateInfo || !physicalDevice)
            return false;

        if (pCreateInfo->imageUsage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
            return true;

        // the surface has to take it, otherwise the creation of the swap chain
        // would fail
        PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR getCapabilities =
            VulkanLoader::LoadInstance<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>("vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

        if (!getCapabilities)
            getCapabilities = (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)GetProcAddress(VulkanLoader::Module(), "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

        if (!getCapabilities)
            return false;

        VkSurfaceCapabilitiesKHR capabilities{};

        if (getCapabilities(physicalDevice, pCreateInfo->surface, &capabilities) != VK_SUCCESS)
            return false;

        if (!(capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT))
            return false;

        const_cast<VkSwapchainCreateInfoKHR*>(pCreateInfo)->imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        return true;
    }

    // -----------------------------------------------------------------------
    // the backend
    // -----------------------------------------------------------------------
    class VulkanBackend : public Backend
    {
    public:
        bool Init(void* pNative) override
        {
            if (!pNative)
                return false;

            if (!VulkanLoader::Available())
                return false;

            const VulkanInitInfo* pInfo = (const VulkanInitInfo*)pNative;

            instance = pInfo->instance;
            physicalDevice = pInfo->physicalDevice;
            device = pInfo->device;
            queue = pInfo->queue;
            queueFamilyIndex = pInfo->queueFamilyIndex;
            frameReadable = pInfo->frameReadable;

            if (!device || !queue)
                return false;

            if (!LoadFunctions())
                return false;

            active = true;
            return true;
        }

        void Shutdown() override
        {
            ReleaseAll();
            device = VK_NULL_HANDLE;
            queue = VK_NULL_HANDLE;
            active = false;
        }

        void Detach() override
        {
            // the device was destroyed, so nothing that came from it can be
            // handed back to the driver any more
            device = VK_NULL_HANDLE;
            queue = VK_NULL_HANDLE;
            active = false;
        }

        void Reset() override
        {
            ReleaseImages();
        }

        // Vulkan hands the device and the queue over for every frame, and the
        // emulator replaces the swap chain when its window changes (a fullscreen
        // switch does it). The queue of the new swap chain is what the drops have
        // to be submitted with; a new device means everything of the backend
        // belongs to the device that is gone and it is built again from scratch.
        bool UpdateNative(void* pNative) override
        {
            if (!pNative)
                return true;

            if (!active)
                return false;

            const VulkanInitInfo* pInfo = (const VulkanInitInfo*)pNative;

            if (pInfo->device != device)
                return false;

            instance = pInfo->instance;
            physicalDevice = pInfo->physicalDevice;
            queueFamilyIndex = pInfo->queueFamilyIndex;
            queue = pInfo->queue;
            frameReadable = pInfo->frameReadable;

            return true;
        }

        bool IsActive() const override
        {
            return active && device != VK_NULL_HANDLE;
        }

        Size GetSize() const override
        {
            if (target.width > 0 && target.height > 0)
                return { (int32_t)target.width, (int32_t)target.height };

            return {};
        }

        void SetMaskTexture(Texture* pMask) override
        {
            pMaskTexture = pMask;
        }

        Texture* CreateTexture(int width, int height, const uint8_t* pixels) override
        {
            if (!active || width <= 0 || height <= 0)
                return nullptr;

            VkImage image = VK_NULL_HANDLE;
            VkDeviceMemory memory = VK_NULL_HANDLE;
            VkImageView view = VK_NULL_HANDLE;
            VkBuffer staging = VK_NULL_HANDLE;
            VkDeviceMemory stagingMemory = VK_NULL_HANDLE;

            if (!CreateImage(width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, image, memory))
                return nullptr;

            if (!CreateImageView(image, VK_FORMAT_R8G8B8A8_UNORM, view))
            {
                vkDestroyImage(device, image, nullptr);
                vkFreeMemory(device, memory, nullptr);
                return nullptr;
            }

            if (pixels)
            {
                const VkDeviceSize size = (VkDeviceSize)width * height * 4;

                if (CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging, stagingMemory))
                {
                    void* pMapped = nullptr;

                    if (vkMapMemory(device, stagingMemory, 0, size, 0, &pMapped) == VK_SUCCESS)
                    {
                        memcpy(pMapped, pixels, (size_t)size);
                        vkUnmapMemory(device, stagingMemory);
                    }

                    VkCommandBuffer commands = BeginOneShot();
                    if (commands)
                    {
                        TransitionImage(commands, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

                        VkBufferImageCopy region{};
                        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                        region.imageSubresource.layerCount = 1;
                        region.imageExtent = { (uint32_t)width, (uint32_t)height, 1 };

                        vkCmdCopyBufferToImage(commands, staging, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

                        TransitionImage(commands, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                        EndOneShot(commands);
                    }

                    vkDestroyBuffer(device, staging, nullptr);
                    vkFreeMemory(device, stagingMemory, nullptr);
                }
            }

            Texture* pTexture = new Texture{};
            pTexture->resource = (void*)image;
            pTexture->extra = (void*)memory;
            pTexture->extra2 = (void*)view;
            pTexture->size = { width, height };
            pTexture->ownsResource = true;
            return pTexture;
        }

        void DestroyTexture(Texture* pTexture) override
        {
            if (!pTexture)
                return;

            // a device that is gone may not be asked to destroy anything
            if (pTexture->ownsResource && device != VK_NULL_HANDLE)
            {
                if (pTexture->extra2)
                    vkDestroyImageView(device, (VkImageView)pTexture->extra2, nullptr);

                if (pTexture->resource)
                    vkDestroyImage(device, (VkImage)pTexture->resource, nullptr);

                if (pTexture->extra)
                    vkFreeMemory(device, (VkDeviceMemory)pTexture->extra, nullptr);
            }

            if (pTexture == pMaskTexture)
                pMaskTexture = nullptr;

            delete pTexture;
        }

        void SetTarget(RenderTarget* pTarget) override
        {
            if (!pTarget)
            {
                target = TargetStateInfo{};
                return;
            }

            target.image = (VkImage)pTarget->resource;
            target.format = pTarget->format ? (VkFormat)pTarget->format : VK_FORMAT_B8G8R8A8_UNORM;
            target.width = (uint32_t)pTarget->size.width;
            target.height = (uint32_t)pTarget->size.height;
            target.state = pTarget->state;
        }

        void SetTargetState(TargetState state) override
        {
            target.state = state;
        }

        void SetCommandQueue(void* pQueue) override
        {
            if (pQueue)
                queue = (VkQueue)pQueue;
        }

        void SetProjection(Projection projection, const Matrix* pWorld, float width, float height) override
        {
            targetWidth = width;
            targetHeight = height;

            if (projection == PROJECTION_WORLD && pWorld)
                memcpy(projectionMatrix.m, pWorld->m, sizeof(projectionMatrix.m));
            else
                projectionMatrix = Matrix::OrthographicOffCenter(0.0f, width, height, 0.0f, 0.0f, 1.0f);

            // The effect works in pixels with the origin in the top left corner
            // and expects what Direct3D does, where the clip space y grows
            // upwards and the viewport flips it. Vulkan flips it the other way
            // round, its clip space points down, so the y is inverted here. This
            // is also what the world space snow needs, its matrix was built on
            // the processor with the same Direct3D convention.
            Matrix flip = Matrix::Identity();
            flip.m[1][1] = -1.0f;

            projectionMatrix = Matrix::Multiply(projectionMatrix, flip);
        }

        void SetSceneUVScale(float offsetX, float scaleX, float offsetY, float scaleY) override
        {
            uvOffsetX = offsetX;
            uvOffsetY = offsetY;
            uvScaleX = scaleX;
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

        void Render(const Vertex* pVertices, int numVertices, PrimitiveType primitive) override
        {
            if (!active || !pVertices || numVertices <= 0 || !target.image || !queue)
                return;

            if (primitive != PRIMITIVE_TRIANGLES && primitive != PRIMITIVE_TRIANGLE_STRIP)
                return;

            if (target.width == 0 || target.height == 0)
                return;

            // The frame is read back into an own image below, which the swap chain
            // of the application has to allow, see AllowReadingSwapchainImages. When
            // it does not, the drops stay out of the frame instead of reading an
            // image that was not created to be read.
            if (!frameReadable)
                return;

            if (!EnsureDeviceObjects())
                return;

            const VkDeviceSize vertexBytes = (VkDeviceSize)numVertices * sizeof(Vertex);

            if (vertexBytes > vertexBufferSize)
            {
                if (!CreateVertexBuffer(vertexBytes))
                    return;
            }

            void* pMapped = nullptr;
            if (vkMapMemory(device, vertexBufferMemory, 0, vertexBytes, 0, &pMapped) != VK_SUCCESS)
                return;

            memcpy(pMapped, pVertices, (size_t)vertexBytes);
            vkUnmapMemory(device, vertexBufferMemory);

            // the constants
            Constants constants{};
            memcpy(constants.projection, projectionMatrix.m, sizeof(constants.projection));
            constants.uvOffset[0] = uvOffsetX;
            constants.uvOffset[1] = uvOffsetY;
            constants.uvScale[0] = uvScaleX;
            constants.uvScale[1] = uvScaleY;
            constants.sceneComplement[0] = sceneComplement ? 1.0f : 0.0f;
            constants.sceneComplement[1] = sceneSampling ? 1.0f : 0.0f;

            void* pConstantMapped = nullptr;
            if (vkMapMemory(device, constantMemory, 0, sizeof(Constants), 0, &pConstantMapped) == VK_SUCCESS)
            {
                memcpy(pConstantMapped, &constants, sizeof(constants));
                vkUnmapMemory(device, constantMemory);
            }

            if (!EnsureSceneImage())
                return;

            if (!EnsureTargetViews())
                return;

            if ((primitive == PRIMITIVE_TRIANGLES) && !EnsureIndexBuffer(numVertices))
                return;

            UpdateDescriptors();

            VkCommandBuffer commands = BeginOneShot();
            if (!commands)
                return;

            const bool presenting = target.state == TARGET_STATE_PRESENT;

            TransitionImage(commands, target.image, presenting ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

            // what the next frame copies into, which is only a change of layout
            // the very first time
            TransitionScene(commands, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            VkImageCopy copy{};
            copy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copy.srcSubresource.layerCount = 1;
            copy.dstSubresource = copy.srcSubresource;
            copy.extent = { target.width, target.height, 1 };
            vkCmdCopyImage(commands, target.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, sceneImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

            // the copy is the scene the drops refract from here on
            TransitionScene(commands, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            TransitionImage(commands, target.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

            VkRenderPassBeginInfo pass{};
            pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            pass.renderPass = renderPass;
            pass.framebuffer = targetFramebuffer;

            VkClearValue clear{};
            pass.clearValueCount = 1;
            pass.pClearValues = &clear;
            pass.renderArea.extent = { target.width, target.height };

            vkCmdBeginRenderPass(commands, &pass, VK_SUBPASS_CONTENTS_INLINE);

            VkViewport viewport{};
            viewport.width = (float)target.width;
            viewport.height = (float)target.height;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(commands, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.extent = { target.width, target.height };
            vkCmdSetScissor(commands, 0, 1, &scissor);

            vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            vkCmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

            const VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(commands, 0, 1, &vertexBuffer, &offset);

            // the effect hands over four vertices per quad, the index buffer
            // turns them into the two triangles of a quad, exactly like the
            // index buffers of the other backends do
            if (primitive == PRIMITIVE_TRIANGLES)
            {
                vkCmdBindIndexBuffer(commands, indexBuffer, 0, VK_INDEX_TYPE_UINT16);
                vkCmdDrawIndexed(commands, (uint32_t)(numVertices / 4) * 6, 1, 0, 0, 0);
            }
            else
            {
                vkCmdDraw(commands, (uint32_t)numVertices, 1, 0, 0);
            }

            vkCmdEndRenderPass(commands);

            // back to a transfer destination, that is where the next frame copies
            // the screen into
            TransitionScene(commands, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            if (presenting)
                TransitionImage(commands, target.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

            EndOneShot(commands);
        }

    private:
        struct Constants
        {
            float projection[16];
            float uvOffset[4];
            float uvScale[4];
            float sceneComplement[4];
        };

        struct TargetStateInfo
        {
            VkImage image = VK_NULL_HANDLE;
            VkFormat format = VK_FORMAT_B8G8R8A8_UNORM;
            uint32_t width = 0;
            uint32_t height = 0;
            TargetState state = TARGET_STATE_PRESENT;
        };

        // one entry per swap chain image, they take turns
        struct TargetViewEntry
        {
            VkImage image = VK_NULL_HANDLE;
            VkFormat format = VK_FORMAT_UNDEFINED;
            VkImageView view = VK_NULL_HANDLE;
            VkFramebuffer framebuffer = VK_NULL_HANDLE;
        };

        // -------------------------------------------------------------------
        // loading
        // -------------------------------------------------------------------
        bool LoadFunctions()
        {
            PFN_vkGetDeviceProcAddr getDeviceProcAddr = (PFN_vkGetDeviceProcAddr)GetProcAddress(VulkanLoader::Module(), "vkGetDeviceProcAddr");

            if (!getDeviceProcAddr)
                return false;

            #define XRD_VK_LOAD(name) name = (PFN_##name)getDeviceProcAddr(device, #name)

            XRD_VK_LOAD(vkDestroyImage);
            XRD_VK_LOAD(vkDestroyImageView);
            XRD_VK_LOAD(vkDestroyBuffer);
            XRD_VK_LOAD(vkDestroyShaderModule);
            XRD_VK_LOAD(vkDestroyRenderPass);
            XRD_VK_LOAD(vkDestroyFramebuffer);
            XRD_VK_LOAD(vkDestroyDescriptorSetLayout);
            XRD_VK_LOAD(vkDestroyDescriptorPool);
            XRD_VK_LOAD(vkDestroyPipeline);
            XRD_VK_LOAD(vkDestroyPipelineLayout);
            XRD_VK_LOAD(vkDestroySampler);
            XRD_VK_LOAD(vkDestroyCommandPool);
            XRD_VK_LOAD(vkDestroyFence);
            XRD_VK_LOAD(vkFreeMemory);
            XRD_VK_LOAD(vkCreateImage);
            XRD_VK_LOAD(vkCreateImageView);
            XRD_VK_LOAD(vkCreateBuffer);
            XRD_VK_LOAD(vkCreateShaderModule);
            XRD_VK_LOAD(vkCreateRenderPass);
            XRD_VK_LOAD(vkCreateFramebuffer);
            XRD_VK_LOAD(vkCreateDescriptorSetLayout);
            XRD_VK_LOAD(vkCreateDescriptorPool);
            XRD_VK_LOAD(vkCreatePipelineLayout);
            XRD_VK_LOAD(vkCreateGraphicsPipelines);
            XRD_VK_LOAD(vkCreateSampler);
            XRD_VK_LOAD(vkCreateCommandPool);
            XRD_VK_LOAD(vkCreateFence);
            XRD_VK_LOAD(vkAllocateDescriptorSets);
            XRD_VK_LOAD(vkAllocateMemory);
            XRD_VK_LOAD(vkMapMemory);
            XRD_VK_LOAD(vkUnmapMemory);
            XRD_VK_LOAD(vkBindImageMemory);
            XRD_VK_LOAD(vkBindBufferMemory);
            XRD_VK_LOAD(vkGetImageMemoryRequirements);
            XRD_VK_LOAD(vkGetBufferMemoryRequirements);
            XRD_VK_LOAD(vkUpdateDescriptorSets);
            XRD_VK_LOAD(vkResetCommandBuffer);
            XRD_VK_LOAD(vkAllocateCommandBuffers);
            XRD_VK_LOAD(vkFreeCommandBuffers);
            XRD_VK_LOAD(vkBeginCommandBuffer);
            XRD_VK_LOAD(vkEndCommandBuffer);
            XRD_VK_LOAD(vkCmdPipelineBarrier);
            XRD_VK_LOAD(vkCmdCopyImage);
            XRD_VK_LOAD(vkCmdCopyBuffer);
            XRD_VK_LOAD(vkCmdCopyBufferToImage);
            XRD_VK_LOAD(vkCmdBeginRenderPass);
            XRD_VK_LOAD(vkCmdEndRenderPass);
            XRD_VK_LOAD(vkCmdBindPipeline);
            XRD_VK_LOAD(vkCmdBindDescriptorSets);
            XRD_VK_LOAD(vkCmdBindVertexBuffers);
            XRD_VK_LOAD(vkCmdBindIndexBuffer);
            XRD_VK_LOAD(vkCmdDrawIndexed);
            XRD_VK_LOAD(vkCmdSetViewport);
            XRD_VK_LOAD(vkCmdSetScissor);
            XRD_VK_LOAD(vkCmdDraw);
            XRD_VK_LOAD(vkQueueSubmit);
            XRD_VK_LOAD(vkQueueWaitIdle);
            XRD_VK_LOAD(vkWaitForFences);
            XRD_VK_LOAD(vkResetFences);
            XRD_VK_LOAD(vkDeviceWaitIdle);
            XRD_VK_LOAD(vkGetDeviceQueue);

            #undef XRD_VK_LOAD

            // the memory types come from the physical device. That is an instance
            // level function: it is taken from the instance when the caller knows
            // it, and straight from the loader otherwise, which works because the
            // loader keeps the instance of a physical device itself.
            if (instance)
                vkGetPhysicalDeviceMemoryProperties = VulkanLoader::LoadInstance<PFN_vkGetPhysicalDeviceMemoryProperties>("vkGetPhysicalDeviceMemoryProperties", instance);

            if (!vkGetPhysicalDeviceMemoryProperties)
                vkGetPhysicalDeviceMemoryProperties = (PFN_vkGetPhysicalDeviceMemoryProperties)GetProcAddress(VulkanLoader::Module(), "vkGetPhysicalDeviceMemoryProperties");

            return vkCreateImage && vkCmdPipelineBarrier && vkQueueSubmit && vkCreateGraphicsPipelines && vkGetPhysicalDeviceMemoryProperties;
        }

        // -------------------------------------------------------------------
        // small helpers
        // -------------------------------------------------------------------
        bool CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& memory)
        {
            VkBufferCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            info.size = size;
            info.usage = usage;
            info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            if (vkCreateBuffer(device, &info, nullptr, &buffer) != VK_SUCCESS)
                return false;

            VkMemoryRequirements requirements{};
            vkGetBufferMemoryRequirements(device, buffer, &requirements);

            VkMemoryAllocateInfo allocate{};
            allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocate.allocationSize = requirements.size;
            allocate.memoryTypeIndex = FindMemoryType(requirements.memoryTypeBits, properties);

            if (vkAllocateMemory(device, &allocate, nullptr, &memory) != VK_SUCCESS)
            {
                vkDestroyBuffer(device, buffer, nullptr);
                buffer = VK_NULL_HANDLE;
                return false;
            }

            vkBindBufferMemory(device, buffer, memory, 0);
            return true;
        }

        bool CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkImage& image, VkDeviceMemory& memory)
        {
            VkImageCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            info.imageType = VK_IMAGE_TYPE_2D;
            info.format = format;
            info.extent = { width, height, 1 };
            info.mipLevels = 1;
            info.arrayLayers = 1;
            info.samples = VK_SAMPLE_COUNT_1_BIT;
            info.tiling = VK_IMAGE_TILING_OPTIMAL;
            info.usage = usage;
            info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            if (vkCreateImage(device, &info, nullptr, &image) != VK_SUCCESS)
                return false;

            VkMemoryRequirements requirements{};
            vkGetImageMemoryRequirements(device, image, &requirements);

            VkMemoryAllocateInfo allocate{};
            allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocate.allocationSize = requirements.size;
            allocate.memoryTypeIndex = FindMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            if (vkAllocateMemory(device, &allocate, nullptr, &memory) != VK_SUCCESS)
            {
                vkDestroyImage(device, image, nullptr);
                image = VK_NULL_HANDLE;
                return false;
            }

            vkBindImageMemory(device, image, memory, 0);
            return true;
        }

        bool CreateImageView(VkImage image, VkFormat format, VkImageView& view)
        {
            VkImageViewCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            info.image = image;
            info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            info.format = format;
            info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            info.subresourceRange.levelCount = 1;
            info.subresourceRange.layerCount = 1;

            return vkCreateImageView(device, &info, nullptr, &view) == VK_SUCCESS;
        }

        uint32_t FindMemoryType(uint32_t bits, VkMemoryPropertyFlags properties)
        {
            VkPhysicalDeviceMemoryProperties memory{};
            vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memory);

            for (uint32_t i = 0; i < memory.memoryTypeCount; i++)
            {
                const bool allowed = (bits & (1u << i)) != 0;
                const bool suitable = (memory.memoryTypes[i].propertyFlags & properties) == properties;

                if (allowed && suitable)
                    return i;
            }

            for (uint32_t i = 0; i < memory.memoryTypeCount; i++)
                if (bits & (1u << i))
                    return i;

            return 0;
        }

        VkCommandBuffer BeginOneShot()
        {
            if (!commandBuffer)
            {
                VkCommandPoolCreateInfo poolInfo{};
                poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
                poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
                poolInfo.queueFamilyIndex = queueFamilyIndex;

                if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
                    return VK_NULL_HANDLE;

                VkCommandBufferAllocateInfo allocate{};
                allocate.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                allocate.commandPool = commandPool;
                allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                allocate.commandBufferCount = 1;

                if (vkAllocateCommandBuffers(device, &allocate, &commandBuffer) != VK_SUCCESS)
                {
                    vkDestroyCommandPool(device, commandPool, nullptr);
                    commandPool = VK_NULL_HANDLE;
                    return VK_NULL_HANDLE;
                }
            }

            vkResetCommandBuffer(commandBuffer, 0);

            VkCommandBufferBeginInfo begin{};
            begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

            if (vkBeginCommandBuffer(commandBuffer, &begin) != VK_SUCCESS)
                return VK_NULL_HANDLE;

            return commandBuffer;
        }

        void EndOneShot(VkCommandBuffer commands)
        {
            if (vkEndCommandBuffer(commands) != VK_SUCCESS)
                return;

            if (!fence)
            {
                VkFenceCreateInfo info{};
                info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

                if (vkCreateFence(device, &info, nullptr, &fence) != VK_SUCCESS)
                    return;
            }

            VkSubmitInfo submit{};
            submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit.commandBufferCount = 1;
            submit.pCommandBuffers = &commands;

            if (vkQueueSubmit(queue, 1, &submit, fence) != VK_SUCCESS)
                return;

            // the effect is drawn once a frame and the backend owns everything it
            // touches, so waiting here is simpler than tracking frames in flight
            vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
            vkResetFences(device, 1, &fence);
        }

        void TransitionScene(VkCommandBuffer commands, VkImageLayout from, VkImageLayout to)
        {
            if (sceneLayout == to)
                return;

            // the tracked layout is the one the image is actually in, unless the
            // image was just created
            TransitionImage(commands, sceneImage, sceneLayout == VK_IMAGE_LAYOUT_UNDEFINED ? VK_IMAGE_LAYOUT_UNDEFINED : from, to);
            sceneLayout = to;
        }

        void TransitionImage(VkCommandBuffer commands, VkImage image, VkImageLayout from, VkImageLayout to)
        {
            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = from;
            barrier.newLayout = to;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.layerCount = 1;

            VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

            switch (from)
            {
            case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
                srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                break;
            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                break;
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                break;
            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                break;
            default:
                break;
            }

            switch (to)
            {
            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                break;
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                break;
            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                break;
            case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
                dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
                break;
            default:
                break;
            }

            vkCmdPipelineBarrier(commands, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        }

        // -------------------------------------------------------------------
        // the objects that depend on the target
        // -------------------------------------------------------------------
        bool EnsureDeviceObjects()
        {
            if (pipeline && pipelineFormat == target.format)
                return renderPass != VK_NULL_HANDLE;

            ReleasePipeline();

            VkAttachmentDescription attachment{};
            attachment.format = target.format;
            attachment.samples = VK_SAMPLE_COUNT_1_BIT;
            attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            VkAttachmentReference reference{};
            reference.attachment = 0;
            reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            VkSubpassDescription subpass{};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &reference;

            VkSubpassDependency dependency{};
            dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            dependency.dstSubpass = 0;
            dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
            dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
            dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            VkRenderPassCreateInfo passInfo{};
            passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            passInfo.attachmentCount = 1;
            passInfo.pAttachments = &attachment;
            passInfo.subpassCount = 1;
            passInfo.pSubpasses = &subpass;
            passInfo.dependencyCount = 1;
            passInfo.pDependencies = &dependency;

            if (vkCreateRenderPass(device, &passInfo, nullptr, &renderPass) != VK_SUCCESS)
                return false;

            VkDescriptorSetLayoutBinding bindings[4]{};

            bindings[0].binding = 0;
            bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            bindings[0].descriptorCount = 1;
            bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

            for (int i = 1; i < 4; i++)
            {
                bindings[i].binding = (uint32_t)i;
                bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                bindings[i].descriptorCount = 1;
                bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            }

            VkDescriptorSetLayoutCreateInfo layoutInfo{};
            layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layoutInfo.bindingCount = 4;
            layoutInfo.pBindings = bindings;

            if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
                return false;

            VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
            pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = 1;
            pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

            if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
                return false;

            VkShaderModule vertexModule = CreateShaderModule(XrdVulkanVertexShaderSpirv, sizeof(XrdVulkanVertexShaderSpirv));
            VkShaderModule fragmentModule = CreateShaderModule(XrdVulkanPixelShaderSpirv, sizeof(XrdVulkanPixelShaderSpirv));

            if (!vertexModule || !fragmentModule)
                return false;

            VkPipelineShaderStageCreateInfo stages[2]{};
            stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
            stages[0].module = vertexModule;
            stages[0].pName = "VSMain";
            stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            stages[1].module = fragmentModule;
            stages[1].pName = "PSMain";

            VkVertexInputBindingDescription binding{};
            binding.binding = 0;
            binding.stride = sizeof(Vertex);
            binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            VkVertexInputAttributeDescription attributes[4]{};
            attributes[0] = { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, x) };
            attributes[1] = { 1, 0, VK_FORMAT_B8G8R8A8_UNORM, offsetof(Vertex, color) };
            attributes[2] = { 2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, u0) };
            attributes[3] = { 3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, u1) };

            VkPipelineVertexInputStateCreateInfo vertexInput{};
            vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertexInput.vertexBindingDescriptionCount = 1;
            vertexInput.pVertexBindingDescriptions = &binding;
            vertexInput.vertexAttributeDescriptionCount = 4;
            vertexInput.pVertexAttributeDescriptions = attributes;

            VkPipelineInputAssemblyStateCreateInfo assembly{};
            assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

            VkPipelineViewportStateCreateInfo viewportState{};
            viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewportState.viewportCount = 1;
            viewportState.scissorCount = 1;

            VkPipelineRasterizationStateCreateInfo raster{};
            raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            raster.polygonMode = VK_POLYGON_MODE_FILL;
            raster.cullMode = VK_CULL_MODE_NONE;
            raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            raster.lineWidth = 1.0f;

            VkPipelineMultisampleStateCreateInfo multisample{};
            multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

            VkPipelineColorBlendAttachmentState blendAttachment{};
            blendAttachment.blendEnable = VK_TRUE;
            blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
            blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
            blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

            VkPipelineColorBlendStateCreateInfo blend{};
            blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            blend.attachmentCount = 1;
            blend.pAttachments = &blendAttachment;

            VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

            VkPipelineDynamicStateCreateInfo dynamic{};
            dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamic.dynamicStateCount = 2;
            dynamic.pDynamicStates = dynamicStates;

            VkGraphicsPipelineCreateInfo pipelineInfo{};
            pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipelineInfo.stageCount = 2;
            pipelineInfo.pStages = stages;
            pipelineInfo.pVertexInputState = &vertexInput;
            pipelineInfo.pInputAssemblyState = &assembly;
            pipelineInfo.pViewportState = &viewportState;
            pipelineInfo.pRasterizationState = &raster;
            pipelineInfo.pMultisampleState = &multisample;
            pipelineInfo.pColorBlendState = &blend;
            pipelineInfo.pDynamicState = &dynamic;
            pipelineInfo.layout = pipelineLayout;
            pipelineInfo.renderPass = renderPass;
            pipelineInfo.subpass = 0;

            const VkResult created = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);

            vkDestroyShaderModule(device, vertexModule, nullptr);
            vkDestroyShaderModule(device, fragmentModule, nullptr);

            if (created != VK_SUCCESS)
                return false;

            pipelineFormat = target.format;

            if (!constantBuffer && !CreateBuffer(256, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, constantBuffer, constantMemory))
                return false;

            if (!sampler && !CreateSampler())
                return false;

            return CreateDescriptorObjects();
        }

        VkShaderModule CreateShaderModule(const uint32_t* pCode, size_t sizeInBytes)
        {
            VkShaderModuleCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            info.codeSize = sizeInBytes;
            info.pCode = pCode;

            VkShaderModule module = VK_NULL_HANDLE;
            vkCreateShaderModule(device, &info, nullptr, &module);
            return module;
        }

        bool CreateSampler()
        {
            VkSamplerCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            info.magFilter = VK_FILTER_LINEAR;
            info.minFilter = VK_FILTER_LINEAR;
            info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            info.maxLod = 0.25f;

            return vkCreateSampler(device, &info, nullptr, &sampler) == VK_SUCCESS;
        }

        bool CreateDescriptorObjects()
        {
            VkDescriptorPoolSize sizes[2]{};
            sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            sizes[0].descriptorCount = 1;
            sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            sizes[1].descriptorCount = 2;

            VkDescriptorPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            poolInfo.maxSets = 1;
            poolInfo.poolSizeCount = 2;
            poolInfo.pPoolSizes = sizes;

            if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
                return false;

            VkDescriptorSetAllocateInfo allocate{};
            allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocate.descriptorPool = descriptorPool;
            allocate.descriptorSetCount = 1;
            allocate.pSetLayouts = &descriptorSetLayout;

            return vkAllocateDescriptorSets(device, &allocate, &descriptorSet) == VK_SUCCESS;
        }

        bool EnsureIndexBuffer(int numVertices)
        {
            const uint32_t quads = (uint32_t)(numVertices / 4) + 1;

            if (indexBuffer && indexQuadCount >= quads)
                return true;

            if (indexBuffer)
            {
                vkDestroyBuffer(device, indexBuffer, nullptr);
                vkFreeMemory(device, indexMemory, nullptr);
                indexBuffer = VK_NULL_HANDLE;
                indexMemory = VK_NULL_HANDLE;
            }

            std::vector<uint16_t> indices((size_t)quads * 6);

            for (uint32_t i = 0; i < quads; i++)
            {
                indices[i * 6 + 0] = (uint16_t)(i * 4 + 0);
                indices[i * 6 + 1] = (uint16_t)(i * 4 + 1);
                indices[i * 6 + 2] = (uint16_t)(i * 4 + 2);
                indices[i * 6 + 3] = (uint16_t)(i * 4 + 0);
                indices[i * 6 + 4] = (uint16_t)(i * 4 + 2);
                indices[i * 6 + 5] = (uint16_t)(i * 4 + 3);
            }

            VkBuffer staging = VK_NULL_HANDLE;
            VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
            const VkDeviceSize size = (VkDeviceSize)indices.size() * sizeof(uint16_t);

            if (!CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging, stagingMemory))
                return false;

            void* pMapped = nullptr;
            if (vkMapMemory(device, stagingMemory, 0, size, 0, &pMapped) == VK_SUCCESS)
            {
                memcpy(pMapped, indices.data(), (size_t)size);
                vkUnmapMemory(device, stagingMemory);
            }

            const bool created = CreateBuffer(size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexMemory);

            if (created)
            {
                VkCommandBuffer commands = BeginOneShot();
                if (commands)
                {
                    VkBufferCopy copyRegion{};
                    copyRegion.size = size;

                    vkCmdCopyBuffer(commands, staging, indexBuffer, 1, &copyRegion);
                    EndOneShot(commands);
                }

                indexQuadCount = quads;
            }

            vkDestroyBuffer(device, staging, nullptr);
            vkFreeMemory(device, stagingMemory, nullptr);

            return created;
        }

        bool CreateVertexBuffer(VkDeviceSize size)
        {
            if (vertexBuffer)
            {
                vkDestroyBuffer(device, vertexBuffer, nullptr);
                vkFreeMemory(device, vertexBufferMemory, nullptr);
                vertexBuffer = VK_NULL_HANDLE;
                vertexBufferMemory = VK_NULL_HANDLE;
            }

            vertexBufferSize = size;

            return CreateBuffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vertexBuffer, vertexBufferMemory);
        }

        bool EnsureSceneImage()
        {
            if (sceneImage && sceneWidth == target.width && sceneHeight == target.height)
                return true;

            ReleaseSceneImage();

            if (!CreateImage(target.width, target.height, target.format,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, sceneImage, sceneImageMemory))
                return false;

            if (!CreateImageView(sceneImage, target.format, sceneView))
                return false;

            sceneLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            sceneWidth = target.width;
            sceneHeight = target.height;
            return true;
        }

        bool EnsureTargetViews()
        {
            for (auto& entry : targetViews)
            {
                if (entry.image == target.image && entry.format == target.format)
                {
                    targetFramebuffer = entry.framebuffer;
                    return true;
                }
            }

            TargetViewEntry entry{};
            entry.image = target.image;
            entry.format = target.format;

            if (!CreateImageView(target.image, target.format, entry.view))
                return false;

            VkFramebufferCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            info.renderPass = renderPass;
            info.attachmentCount = 1;
            info.pAttachments = &entry.view;
            info.width = target.width;
            info.height = target.height;
            info.layers = 1;

            if (vkCreateFramebuffer(device, &info, nullptr, &entry.framebuffer) != VK_SUCCESS)
            {
                vkDestroyImageView(device, entry.view, nullptr);
                return false;
            }

            targetViews.push_back(entry);
            targetFramebuffer = entry.framebuffer;
            return true;
        }

        void ReleaseTargetViews()
        {
            for (auto& entry : targetViews)
            {
                if (entry.framebuffer)
                    vkDestroyFramebuffer(device, entry.framebuffer, nullptr);

                if (entry.view)
                    vkDestroyImageView(device, entry.view, nullptr);
            }

            targetViews.clear();
            targetFramebuffer = VK_NULL_HANDLE;
        }

        void UpdateDescriptors()
        {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = constantBuffer;
            bufferInfo.range = sizeof(Constants);

            VkDescriptorImageInfo sceneInfo{};
            sceneInfo.sampler = sampler;
            sceneInfo.imageView = sceneView;
            sceneInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkDescriptorImageInfo maskInfo{};
            maskInfo.sampler = sampler;
            maskInfo.imageView = (pMaskTexture && pMaskTexture->extra2) ? (VkImageView)pMaskTexture->extra2 : sceneView;
            maskInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkWriteDescriptorSet writes[3]{};

            writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet = descriptorSet;
            writes[0].dstBinding = 0;
            writes[0].descriptorCount = 1;
            writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[0].pBufferInfo = &bufferInfo;

            writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1].dstSet = descriptorSet;
            writes[1].dstBinding = 1;
            writes[1].descriptorCount = 1;
            writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[1].pImageInfo = &sceneInfo;

            writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[2].dstSet = descriptorSet;
            writes[2].dstBinding = 2;
            writes[2].descriptorCount = 1;
            writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[2].pImageInfo = &maskInfo;

            vkUpdateDescriptorSets(device, 3, writes, 0, nullptr);
        }

        // -------------------------------------------------------------------
        // teardown
        // -------------------------------------------------------------------
        void ReleaseSceneImage()
        {
            if (sceneView) { vkDestroyImageView(device, sceneView, nullptr); sceneView = VK_NULL_HANDLE; }
            if (sceneImage) { vkDestroyImage(device, sceneImage, nullptr); sceneImage = VK_NULL_HANDLE; }
            if (sceneImageMemory) { vkFreeMemory(device, sceneImageMemory, nullptr); sceneImageMemory = VK_NULL_HANDLE; }

            sceneWidth = 0;
            sceneHeight = 0;
            sceneLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        }

        void ReleaseImages()
        {
            if (device == VK_NULL_HANDLE)
                return;

            if (vkDeviceWaitIdle)
                vkDeviceWaitIdle(device);

            ReleaseSceneImage();
            ReleaseTargetViews();
        }

        void ReleasePipeline()
        {
            if (device == VK_NULL_HANDLE)
                return;

            if (pipeline) { vkDestroyPipeline(device, pipeline, nullptr); pipeline = VK_NULL_HANDLE; }
            if (renderPass) { vkDestroyRenderPass(device, renderPass, nullptr); renderPass = VK_NULL_HANDLE; }
            ReleaseTargetViews();
            if (descriptorSetLayout) { vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr); descriptorSetLayout = VK_NULL_HANDLE; }
            if (descriptorPool) { vkDestroyDescriptorPool(device, descriptorPool, nullptr); descriptorPool = VK_NULL_HANDLE; }

            descriptorSet = VK_NULL_HANDLE;
            pipelineFormat = VK_FORMAT_UNDEFINED;
        }

        void ReleaseAll()
        {
            if (device == VK_NULL_HANDLE)
                return;

            ReleaseImages();
            ReleasePipeline();

            if (commandBuffer && commandPool) { vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer); commandBuffer = VK_NULL_HANDLE; }
            if (commandPool) { vkDestroyCommandPool(device, commandPool, nullptr); commandPool = VK_NULL_HANDLE; }
            if (fence) { vkDestroyFence(device, fence, nullptr); fence = VK_NULL_HANDLE; }
            if (sampler) { vkDestroySampler(device, sampler, nullptr); sampler = VK_NULL_HANDLE; }
            if (pipelineLayout) { vkDestroyPipelineLayout(device, pipelineLayout, nullptr); pipelineLayout = VK_NULL_HANDLE; }
            if (constantBuffer) { vkDestroyBuffer(device, constantBuffer, nullptr); constantBuffer = VK_NULL_HANDLE; }
            if (constantMemory) { vkFreeMemory(device, constantMemory, nullptr); constantMemory = VK_NULL_HANDLE; }
            if (vertexBuffer) { vkDestroyBuffer(device, vertexBuffer, nullptr); vertexBuffer = VK_NULL_HANDLE; }
            if (vertexBufferMemory) { vkFreeMemory(device, vertexBufferMemory, nullptr); vertexBufferMemory = VK_NULL_HANDLE; }
            if (indexBuffer) { vkDestroyBuffer(device, indexBuffer, nullptr); indexBuffer = VK_NULL_HANDLE; }
            if (indexMemory) { vkFreeMemory(device, indexMemory, nullptr); indexMemory = VK_NULL_HANDLE; }
            indexQuadCount = 0;
        }

        // -------------------------------------------------------------------
        // entry points
        // -------------------------------------------------------------------
        VkDevice device = VK_NULL_HANDLE;
        VkInstance instance = VK_NULL_HANDLE;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkQueue queue = VK_NULL_HANDLE;
        uint32_t queueFamilyIndex = 0;
        bool active = false;

        // whether the images of the swap chain may be read back, see
        // AllowReadingSwapchainImages
        bool frameReadable = true;

        PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties = nullptr;

        PFN_vkDestroyImage vkDestroyImage = nullptr;
        PFN_vkDestroyImageView vkDestroyImageView = nullptr;
        PFN_vkDestroyBuffer vkDestroyBuffer = nullptr;
        PFN_vkDestroyShaderModule vkDestroyShaderModule = nullptr;
        PFN_vkDestroyRenderPass vkDestroyRenderPass = nullptr;
        PFN_vkDestroyFramebuffer vkDestroyFramebuffer = nullptr;
        PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout = nullptr;
        PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool = nullptr;
        PFN_vkDestroyPipeline vkDestroyPipeline = nullptr;
        PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout = nullptr;
        PFN_vkDestroySampler vkDestroySampler = nullptr;
        PFN_vkDestroyCommandPool vkDestroyCommandPool = nullptr;
        PFN_vkDestroyFence vkDestroyFence = nullptr;
        PFN_vkFreeMemory vkFreeMemory = nullptr;
        PFN_vkCreateImage vkCreateImage = nullptr;
        PFN_vkCreateImageView vkCreateImageView = nullptr;
        PFN_vkCreateBuffer vkCreateBuffer = nullptr;
        PFN_vkCreateShaderModule vkCreateShaderModule = nullptr;
        PFN_vkCreateRenderPass vkCreateRenderPass = nullptr;
        PFN_vkCreateFramebuffer vkCreateFramebuffer = nullptr;
        PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout = nullptr;
        PFN_vkCreateDescriptorPool vkCreateDescriptorPool = nullptr;
        PFN_vkCreatePipelineLayout vkCreatePipelineLayout = nullptr;
        PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines = nullptr;
        PFN_vkCreateSampler vkCreateSampler = nullptr;
        PFN_vkCreateCommandPool vkCreateCommandPool = nullptr;
        PFN_vkCreateFence vkCreateFence = nullptr;
        PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets = nullptr;
        PFN_vkAllocateMemory vkAllocateMemory = nullptr;
        PFN_vkMapMemory vkMapMemory = nullptr;
        PFN_vkUnmapMemory vkUnmapMemory = nullptr;
        PFN_vkBindImageMemory vkBindImageMemory = nullptr;
        PFN_vkBindBufferMemory vkBindBufferMemory = nullptr;
        PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements = nullptr;
        PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements = nullptr;
        PFN_vkGetMemoryFdPropertiesKHR vkGetMemoryFdPropertiesKHR = nullptr;
        PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets = nullptr;
        PFN_vkResetCommandBuffer vkResetCommandBuffer = nullptr;
        PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers = nullptr;
        PFN_vkFreeCommandBuffers vkFreeCommandBuffers = nullptr;
        PFN_vkBeginCommandBuffer vkBeginCommandBuffer = nullptr;
        PFN_vkEndCommandBuffer vkEndCommandBuffer = nullptr;
        PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier = nullptr;
        PFN_vkCmdCopyImage vkCmdCopyImage = nullptr;
        PFN_vkCmdCopyBuffer vkCmdCopyBuffer = nullptr;
        PFN_vkCmdCopyBufferToImage vkCmdCopyBufferToImage = nullptr;
        PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass = nullptr;
        PFN_vkCmdEndRenderPass vkCmdEndRenderPass = nullptr;
        PFN_vkCmdBindPipeline vkCmdBindPipeline = nullptr;
        PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets = nullptr;
        PFN_vkCmdBindVertexBuffers vkCmdBindVertexBuffers = nullptr;
        PFN_vkCmdBindIndexBuffer vkCmdBindIndexBuffer = nullptr;
        PFN_vkCmdDrawIndexed vkCmdDrawIndexed = nullptr;
        PFN_vkCmdSetViewport vkCmdSetViewport = nullptr;
        PFN_vkCmdSetScissor vkCmdSetScissor = nullptr;
        PFN_vkCmdDraw vkCmdDraw = nullptr;
        PFN_vkQueueSubmit vkQueueSubmit = nullptr;
        PFN_vkQueueWaitIdle vkQueueWaitIdle = nullptr;
        PFN_vkWaitForFences vkWaitForFences = nullptr;
        PFN_vkResetFences vkResetFences = nullptr;
        PFN_vkDeviceWaitIdle vkDeviceWaitIdle = nullptr;
        PFN_vkGetDeviceQueue vkGetDeviceQueue = nullptr;

        // -------------------------------------------------------------------
        // device objects
        // -------------------------------------------------------------------
        VkCommandPool commandPool = VK_NULL_HANDLE;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        VkBuffer constantBuffer = VK_NULL_HANDLE;
        VkDeviceMemory constantMemory = VK_NULL_HANDLE;
        VkBuffer vertexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
        VkDeviceSize vertexBufferSize = 0;
        VkBuffer indexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory indexMemory = VK_NULL_HANDLE;
        uint32_t indexQuadCount = 0;

        VkRenderPass renderPass = VK_NULL_HANDLE;
        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;
        VkFormat pipelineFormat = VK_FORMAT_UNDEFINED;

        VkImage sceneImage = VK_NULL_HANDLE;
        VkDeviceMemory sceneImageMemory = VK_NULL_HANDLE;
        VkImageView sceneView = VK_NULL_HANDLE;
        VkImageLayout sceneLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        uint32_t sceneWidth = 0;
        uint32_t sceneHeight = 0;

        std::vector<TargetViewEntry> targetViews;
        VkFramebuffer targetFramebuffer = VK_NULL_HANDLE;

        Texture* pMaskTexture = nullptr;
        TargetStateInfo target{};

        Matrix projectionMatrix = Matrix::Identity();
        float targetWidth = 0.0f;
        float targetHeight = 0.0f;
        float uvOffsetX = 0.0f;
        float uvOffsetY = 0.0f;
        float uvScaleX = 1.0f;
        float uvScaleY = 1.0f;
        bool sceneComplement = false;
        bool sceneSampling = true;
    };

    // -----------------------------------------------------------------------
    // The bring up a present hook has to do
    //
    // Vulkan has no implicit state at all, so the information the backend needs
    // is spread over three calls of the application: vkCreateDevice knows the
    // device and the queue family, vkCreateSwapchainKHR knows the format and the
    // size of the frame, and vkQueuePresentKHR knows which image is about to be
    // shown. This class is where the three meet, which is what keeps a hook in a
    // game or an emulator down to forwarding what it sees:
    //
    //   OnCreateDevice   from the vkCreateDevice hook
    //   OnCreateSwapchain from the vkCreateSwapchainKHR hook
    //   Prepare          from the vkQueuePresentKHR hook, returns false when
    //                    there is nothing to draw on
    //   Clear            from the shutdown hook
    // -----------------------------------------------------------------------
    class VulkanPresent
    {
    public:
        void OnCreateDevice(VkPhysicalDevice gpu, const VkDeviceCreateInfo* pCreateInfo, VkDevice* pDevice)
        {
            physicalDevice = gpu;

            // the device does not exist until the call is over, so only the
            // address of the variable is remembered
            pDevicePointer = pDevice;

            if (pCreateInfo && pCreateInfo->queueCreateInfoCount > 0 && pCreateInfo->pQueueCreateInfos)
                queueFamilyIndex = pCreateInfo->pQueueCreateInfos[0].queueFamilyIndex;

            // a device was created, so whatever the backend holds belongs to the
            // previous one and has to go
            device = VK_NULL_HANDLE;
            getSwapchainImages = nullptr;
            deviceChanged = true;

            // the swap chain of the new device is created after this, it reports
            // whether its images may be read (see OnCreateSwapchain)
            frameReadable = true;
        }

        void OnCreateSwapchain(VkDevice /*device*/, const VkSwapchainCreateInfoKHR* pCreateInfo)
        {
            if (!pCreateInfo)
                return;

            format = pCreateInfo->imageFormat;
            extent = pCreateInfo->imageExtent;

            // Every frame is read back out of the image that is presented, so the
            // swap chain has to allow it. This runs before the call is forwarded
            // to the driver, which is why the description can be widened here.
            frameReadable = AllowReadingSwapchainImages(physicalDevice, pCreateInfo);
        }

        bool Prepare(VkQueue queue, const VkPresentInfoKHR* pPresentInfo)
        {
            if (!pPresentInfo || pPresentInfo->swapchainCount == 0 || !pPresentInfo->pSwapchains || !pPresentInfo->pImageIndices)
                return false;

            if (!ResolveDevice())
                return false;

            const VkSwapchainKHR swapchain = pPresentInfo->pSwapchains[0];
            const uint32_t imageIndex = pPresentInfo->pImageIndices[0];

            uint32_t imageCount = 0;
            if (getSwapchainImages(device, swapchain, &imageCount, nullptr) != VK_SUCCESS || imageCount == 0)
                return false;

            std::vector<VkImage> images(imageCount);
            if (getSwapchainImages(device, swapchain, &imageCount, images.data()) != VK_SUCCESS)
                return false;

            if (imageIndex >= imageCount)
                return false;

            if (deviceChanged)
            {
                // the resources of the backend belong to the old device, which
                // the application destroyed before it made this one, so they can
                // only be forgotten
                Detach();
                Shutdown();
                deviceChanged = false;
            }

            VulkanInitInfo initInfo{};
            initInfo.physicalDevice = physicalDevice;
            initInfo.device = device;
            initInfo.queue = queue;
            initInfo.queueFamilyIndex = queueFamilyIndex;
            initInfo.frameReadable = frameReadable;

            Init(RENDERER_VULKAN, &initInfo);

            RenderTarget target{};
            target.resource = (void*)images[imageIndex];
            target.format = (uint32_t)format;
            target.size = { (int32_t)extent.width, (int32_t)extent.height };
            target.state = TARGET_STATE_PRESENT;

            SetTarget(&target);

            return IsActive();
        }

        void Clear()
        {
            pDevicePointer = nullptr;
            physicalDevice = VK_NULL_HANDLE;
            device = VK_NULL_HANDLE;
            getSwapchainImages = nullptr;
            format = VK_FORMAT_UNDEFINED;
            extent = {};
            frameReadable = true;
            deviceChanged = false;
        }

    private:
        bool ResolveDevice()
        {
            if (!pDevicePointer)
                return false;

            if (device != VK_NULL_HANDLE)
                return true;

            device = *pDevicePointer;

            if (!device)
                return false;

            PFN_vkGetDeviceProcAddr getDeviceProcAddr = VulkanLoader::GlobalGetDeviceProcAddr();

            if (!getDeviceProcAddr)
                return false;

            getSwapchainImages = (PFN_vkGetSwapchainImagesKHR)getDeviceProcAddr(device, "vkGetSwapchainImagesKHR");
            return getSwapchainImages != nullptr;
        }

        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkDevice* pDevicePointer = nullptr;
        VkDevice device = VK_NULL_HANDLE;
        uint32_t queueFamilyIndex = 0;
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkExtent2D extent{};
        PFN_vkGetSwapchainImagesKHR getSwapchainImages = nullptr;
        bool frameReadable = true;
        bool deviceChanged = false;
    };

    namespace Detail
    {
        inline Backend* CreateVulkan()
        {
            return new VulkanBackend();
        }

        inline Register registerVulkan(RENDERER_VULKAN, CreateVulkan);
    }
}
