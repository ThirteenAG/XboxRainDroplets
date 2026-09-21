// ---------------------------------------------------------------------------
// Test application for the Vulkan backend.
//
// Same scene and the same effect calls as the other ones. The application sets
// up what a game sets up (instance, device, queue, swap chain) and draws its
// scene with render passes and rectangle clears, so that there is no second
// graphics pipeline to maintain here. What matters is the three lines in the
// middle of the frame:
//
//     Xrd::SetTarget(...);          the image of the moment, its format and size
//     WaterDrops::Process();
//     WaterDrops::Render();
//
// Nothing links against the Vulkan loader, every entry point is resolved from
// vulkan-1.dll at runtime, the same way the backend does it.
// ---------------------------------------------------------------------------

#define XRD_ENABLE_VULKAN 1
#define WIN32_LEAN_AND_MEAN
#define VK_USE_PLATFORM_WIN32_KHR
#include "xrd/xrd.h"

#include "XrdTest.h"

#include <vulkan/vulkan.h>
#include <vector>

namespace
{
    struct Rect
    {
        float x, y, width, height, r, g, b, a;
    };

    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    uint32_t queueFamilyIndex = 0;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapChain = VK_NULL_HANDLE;
    VkFormat swapChainFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkExtent2D swapChainExtent{};
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainViews;
    std::vector<VkFramebuffer> framebuffers;

    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkSemaphore imageAvailable = VK_NULL_HANDLE;
    VkSemaphore renderFinished = VK_NULL_HANDLE;
    VkFence frameFence = VK_NULL_HANDLE;

    // the entry points, all of them from the loader at runtime
    PFN_vkGetInstanceProcAddr pfnGetInstanceProcAddr = nullptr;
    PFN_vkCreateInstance pfnCreateInstance = nullptr;
    PFN_vkEnumeratePhysicalDevices pfnEnumeratePhysicalDevices = nullptr;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties pfnGetPhysicalDeviceQueueFamilyProperties = nullptr;
    PFN_vkCreateDevice pfnCreateDevice = nullptr;
    PFN_vkGetDeviceQueue pfnGetDeviceQueue = nullptr;
    PFN_vkCreateWin32SurfaceKHR pfnCreateWin32SurfaceKHR = nullptr;
    PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR pfnGetPhysicalDeviceWin32PresentationSupportKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR pfnGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR pfnGetPhysicalDeviceSurfaceFormatsKHR = nullptr;
    PFN_vkCreateSwapchainKHR pfnCreateSwapchainKHR = nullptr;
    PFN_vkGetSwapchainImagesKHR pfnGetSwapchainImagesKHR = nullptr;
    PFN_vkAcquireNextImageKHR pfnAcquireNextImageKHR = nullptr;
    PFN_vkQueuePresentKHR pfnQueuePresentKHR = nullptr;

    PFN_vkCreateImageView pfnCreateImageView = nullptr;
    PFN_vkCreateRenderPass pfnCreateRenderPass = nullptr;
    PFN_vkCreateFramebuffer pfnCreateFramebuffer = nullptr;
    PFN_vkCreateCommandPool pfnCreateCommandPool = nullptr;
    PFN_vkAllocateCommandBuffers pfnAllocateCommandBuffers = nullptr;
    PFN_vkCreateSemaphore pfnCreateSemaphore = nullptr;
    PFN_vkCreateFence pfnCreateFence = nullptr;
    PFN_vkResetCommandBuffer pfnResetCommandBuffer = nullptr;
    PFN_vkBeginCommandBuffer pfnBeginCommandBuffer = nullptr;
    PFN_vkEndCommandBuffer pfnEndCommandBuffer = nullptr;
    PFN_vkCmdPipelineBarrier pfnCmdPipelineBarrier = nullptr;
    PFN_vkCmdBeginRenderPass pfnCmdBeginRenderPass = nullptr;
    PFN_vkCmdEndRenderPass pfnCmdEndRenderPass = nullptr;
    PFN_vkCmdClearAttachments pfnCmdClearAttachments = nullptr;
    PFN_vkQueueSubmit pfnQueueSubmit = nullptr;
    PFN_vkWaitForFences pfnWaitForFences = nullptr;
    PFN_vkResetFences pfnResetFences = nullptr;
    PFN_vkDeviceWaitIdle pfnDeviceWaitIdle = nullptr;

    XrdTest::Window window;
    XrdTest::Camera camera;
    XrdTest::Ui ui;

    float deltaTime = 1.0f / 60.0f;
    int uiSelection = 0;

    template <typename T>
    T LoadInstanceFunction(const char* name)
    {
        return (T)pfnGetInstanceProcAddr(instance, name);
    }

    template <typename T>
    T LoadDeviceFunction(const char* name)
    {
        PFN_vkGetDeviceProcAddr pfn = (PFN_vkGetDeviceProcAddr)pfnGetInstanceProcAddr(instance, "vkGetDeviceProcAddr");
        return pfn ? (T)pfn(device, name) : nullptr;
    }

    bool LoadGlobalFunctions()
    {
        HMODULE module = LoadLibraryW(L"vulkan-1.dll");

        if (!module)
        {
            printf("[vulkan] vulkan-1.dll was not found\n");
            fflush(stdout);
            return false;
        }

        pfnGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)GetProcAddress(module, "vkGetInstanceProcAddr");

        if (!pfnGetInstanceProcAddr)
            return false;

        pfnCreateInstance = (PFN_vkCreateInstance)pfnGetInstanceProcAddr(nullptr, "vkCreateInstance");
        return pfnCreateInstance != nullptr;
    }

    bool InitializeDevice()
    {
        VkApplicationInfo application{};
        application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        application.pApplicationName = "XrdTestVulkan";
        application.apiVersion = VK_API_VERSION_1_0;

        const char* extensions[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };

        VkInstanceCreateInfo instanceInfo{};
        instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceInfo.pApplicationInfo = &application;
        instanceInfo.enabledExtensionCount = 2;
        instanceInfo.ppEnabledExtensionNames = extensions;

        if (pfnCreateInstance(&instanceInfo, nullptr, &instance) != VK_SUCCESS)
        {
            printf("[vulkan] the instance could not be created\n");
            fflush(stdout);
            return false;
        }

        pfnEnumeratePhysicalDevices = LoadInstanceFunction<PFN_vkEnumeratePhysicalDevices>("vkEnumeratePhysicalDevices");
        pfnGetPhysicalDeviceQueueFamilyProperties = LoadInstanceFunction<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties");
        pfnCreateDevice = LoadInstanceFunction<PFN_vkCreateDevice>("vkCreateDevice");
        pfnGetDeviceQueue = LoadInstanceFunction<PFN_vkGetDeviceQueue>("vkGetDeviceQueue");
        pfnCreateWin32SurfaceKHR = LoadInstanceFunction<PFN_vkCreateWin32SurfaceKHR>("vkCreateWin32SurfaceKHR");
        pfnGetPhysicalDeviceWin32PresentationSupportKHR = LoadInstanceFunction<PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR>("vkGetPhysicalDeviceWin32PresentationSupportKHR");
        pfnGetPhysicalDeviceSurfaceCapabilitiesKHR = LoadInstanceFunction<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>("vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
        pfnGetPhysicalDeviceSurfaceFormatsKHR = LoadInstanceFunction<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>("vkGetPhysicalDeviceSurfaceFormatsKHR");
        pfnCreateSwapchainKHR = LoadInstanceFunction<PFN_vkCreateSwapchainKHR>("vkCreateSwapchainKHR");
        pfnGetSwapchainImagesKHR = LoadInstanceFunction<PFN_vkGetSwapchainImagesKHR>("vkGetSwapchainImagesKHR");
        pfnAcquireNextImageKHR = LoadInstanceFunction<PFN_vkAcquireNextImageKHR>("vkAcquireNextImageKHR");
        pfnQueuePresentKHR = LoadInstanceFunction<PFN_vkQueuePresentKHR>("vkQueuePresentKHR");

        uint32_t deviceCount = 0;
        if (pfnEnumeratePhysicalDevices(instance, &deviceCount, nullptr) != VK_SUCCESS || deviceCount == 0)
            return false;

        std::vector<VkPhysicalDevice> devices(deviceCount);
        pfnEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        VkWin32SurfaceCreateInfoKHR surfaceInfo{};
        surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        surfaceInfo.hinstance = GetModuleHandle(nullptr);
        surfaceInfo.hwnd = window.hwnd;

        if (pfnCreateWin32SurfaceKHR(instance, &surfaceInfo, nullptr, &surface) != VK_SUCCESS)
            return false;

        // the first device with a queue family that can both draw and present
        for (VkPhysicalDevice candidate : devices)
        {
            uint32_t familyCount = 0;
            pfnGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, nullptr);

            std::vector<VkQueueFamilyProperties> families(familyCount);
            pfnGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, families.data());

            for (uint32_t i = 0; i < familyCount; i++)
            {
                if (!(families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
                    continue;

                if (!pfnGetPhysicalDeviceWin32PresentationSupportKHR(candidate, i))
                    continue;

                physicalDevice = candidate;
                queueFamilyIndex = i;
                break;
            }

            if (physicalDevice)
                break;
        }

        if (!physicalDevice)
        {
            printf("[vulkan] no device with a queue that can present\n");
            fflush(stdout);
            return false;
        }

        const float priority = 1.0f;

        VkDeviceQueueCreateInfo queueInfo{};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = queueFamilyIndex;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &priority;

        const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

        VkDeviceCreateInfo deviceInfo{};
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.pQueueCreateInfos = &queueInfo;
        deviceInfo.enabledExtensionCount = 1;
        deviceInfo.ppEnabledExtensionNames = deviceExtensions;

        if (pfnCreateDevice(physicalDevice, &deviceInfo, nullptr, &device) != VK_SUCCESS)
            return false;

        pfnGetDeviceQueue(device, queueFamilyIndex, 0, &queue);

        pfnCreateImageView = LoadDeviceFunction<PFN_vkCreateImageView>("vkCreateImageView");
        pfnCreateRenderPass = LoadDeviceFunction<PFN_vkCreateRenderPass>("vkCreateRenderPass");
        pfnCreateFramebuffer = LoadDeviceFunction<PFN_vkCreateFramebuffer>("vkCreateFramebuffer");
        pfnCreateCommandPool = LoadDeviceFunction<PFN_vkCreateCommandPool>("vkCreateCommandPool");
        pfnAllocateCommandBuffers = LoadDeviceFunction<PFN_vkAllocateCommandBuffers>("vkAllocateCommandBuffers");
        pfnCreateSemaphore = LoadDeviceFunction<PFN_vkCreateSemaphore>("vkCreateSemaphore");
        pfnCreateFence = LoadDeviceFunction<PFN_vkCreateFence>("vkCreateFence");
        pfnResetCommandBuffer = LoadDeviceFunction<PFN_vkResetCommandBuffer>("vkResetCommandBuffer");
        pfnBeginCommandBuffer = LoadDeviceFunction<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer");
        pfnEndCommandBuffer = LoadDeviceFunction<PFN_vkEndCommandBuffer>("vkEndCommandBuffer");
        pfnCmdPipelineBarrier = LoadDeviceFunction<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier");
        pfnCmdBeginRenderPass = LoadDeviceFunction<PFN_vkCmdBeginRenderPass>("vkCmdBeginRenderPass");
        pfnCmdEndRenderPass = LoadDeviceFunction<PFN_vkCmdEndRenderPass>("vkCmdEndRenderPass");
        pfnCmdClearAttachments = LoadDeviceFunction<PFN_vkCmdClearAttachments>("vkCmdClearAttachments");
        pfnQueueSubmit = LoadDeviceFunction<PFN_vkQueueSubmit>("vkQueueSubmit");
        pfnWaitForFences = LoadDeviceFunction<PFN_vkWaitForFences>("vkWaitForFences");
        pfnResetFences = LoadDeviceFunction<PFN_vkResetFences>("vkResetFences");
        pfnDeviceWaitIdle = LoadDeviceFunction<PFN_vkDeviceWaitIdle>("vkDeviceWaitIdle");

        return true;
    }

    bool CreateSwapChain()
    {
        VkSurfaceCapabilitiesKHR capabilities{};
        if (pfnGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities) != VK_SUCCESS)
            return false;

        uint32_t formatCount = 0;
        pfnGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);

        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        pfnGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());

        if (formats.empty())
            return false;

        swapChainFormat = formats[0].format;
        const VkColorSpaceKHR colorSpace = formats[0].colorSpace;

        for (const VkSurfaceFormatKHR& format : formats)
        {
            if (format.format == VK_FORMAT_B8G8R8A8_UNORM || format.format == VK_FORMAT_B8G8R8A8_SRGB)
            {
                swapChainFormat = format.format;
                break;
            }
        }

        switch (swapChainFormat)
        {
        case VK_FORMAT_B8G8R8A8_UNORM:
        case VK_FORMAT_B8G8R8A8_SRGB:
        case VK_FORMAT_R8G8B8A8_UNORM:
        case VK_FORMAT_R8G8B8A8_SRGB:
            break;
        default:
            // the shader blends with the alpha of the vertex colour, a format
            // without alpha would ignore it
            swapChainFormat = VK_FORMAT_B8G8R8A8_UNORM;
            break;
        }

        swapChainExtent.width = (uint32_t)window.width;
        swapChainExtent.height = (uint32_t)window.height;

        if (capabilities.currentExtent.width != 0xFFFFFFFF)
            swapChainExtent = capabilities.currentExtent;

        VkSwapchainCreateInfoKHR info{};
        info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        info.surface = surface;
        info.minImageCount = capabilities.minImageCount;
        info.imageFormat = swapChainFormat;
        info.imageColorSpace = colorSpace;
        info.imageExtent = swapChainExtent;
        info.imageArrayLayers = 1;
        info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.preTransform = capabilities.currentTransform;
        info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        info.clipped = VK_TRUE;

        if (pfnCreateSwapchainKHR(device, &info, nullptr, &swapChain) != VK_SUCCESS)
            return false;

        uint32_t imageCount = 0;
        pfnGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);

        swapChainImages.resize(imageCount);
        pfnGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

        swapChainViews.resize(imageCount);
        framebuffers.resize(imageCount);

        for (uint32_t i = 0; i < imageCount; i++)
        {
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = swapChainImages[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = swapChainFormat;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.layerCount = 1;

            if (pfnCreateImageView(device, &viewInfo, nullptr, &swapChainViews[i]) != VK_SUCCESS)
                return false;
        }

        // a pass that leaves what is in the image alone and hands it back in the
        // layout it was given in, the application draws with rectangle clears and
        // then the effect has its turn
        VkAttachmentDescription attachment{};
        attachment.format = swapChainFormat;
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

        VkRenderPassCreateInfo passInfo{};
        passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        passInfo.attachmentCount = 1;
        passInfo.pAttachments = &attachment;
        passInfo.subpassCount = 1;
        passInfo.pSubpasses = &subpass;

        if (pfnCreateRenderPass(device, &passInfo, nullptr, &renderPass) != VK_SUCCESS)
            return false;

        for (uint32_t i = 0; i < imageCount; i++)
        {
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = &swapChainViews[i];
            framebufferInfo.width = swapChainExtent.width;
            framebufferInfo.height = swapChainExtent.height;
            framebufferInfo.layers = 1;

            if (pfnCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffers[i]) != VK_SUCCESS)
                return false;
        }

        return true;
    }

    bool CreateFrameObjects()
    {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndex;

        if (pfnCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
            return false;

        VkCommandBufferAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate.commandPool = commandPool;
        allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate.commandBufferCount = 1;

        if (pfnAllocateCommandBuffers(device, &allocate, &commandBuffer) != VK_SUCCESS)
            return false;

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        if (pfnCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailable) != VK_SUCCESS)
            return false;

        if (pfnCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinished) != VK_SUCCESS)
            return false;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        return pfnCreateFence(device, &fenceInfo, nullptr, &frameFence) == VK_SUCCESS;
    }

    void TransitionImage(VkCommandBuffer commands, VkImage image, VkImageLayout from, VkImageLayout to, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage)
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

        if (from == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        if (to == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        pfnCmdPipelineBarrier(commands, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    // the rectangles of the scene and of the interface, drawn with clears.
    // vkCmdClearAttachments applies every attachment to every rectangle, so a
    // rectangle with its own colour needs its own call.
    void ClearRects(VkCommandBuffer commands, const std::vector<Rect>& rects)
    {
        for (const Rect& rect : rects)
        {
            VkClearRect clearRect{ { (int32_t)rect.x, (int32_t)rect.y, (uint32_t)rect.width, (uint32_t)rect.height }, 0, 1 };

            VkClearAttachment clearValue{};
            clearValue.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            clearValue.colorAttachment = 0;
            clearValue.clearValue.color.float32[0] = rect.r;
            clearValue.clearValue.color.float32[1] = rect.g;
            clearValue.clearValue.color.float32[2] = rect.b;
            clearValue.clearValue.color.float32[3] = rect.a;

            pfnCmdClearAttachments(commands, 1, &clearValue, 1, &clearRect);
        }
    }

    void BuildScene(std::vector<Rect>& rects)
    {
        rects.push_back({ 0.0f, 0.0f, (float)swapChainExtent.width, swapChainExtent.height * 0.62f, 0.35f, 0.45f, 0.65f, 1.0f });
        rects.push_back({ 0.0f, swapChainExtent.height * 0.62f, (float)swapChainExtent.width, swapChainExtent.height * 0.38f, 0.18f, 0.20f, 0.16f, 1.0f });
        rects.push_back({ 0.0f, swapChainExtent.height * 0.62f - 3.0f, (float)swapChainExtent.width, 3.0f, 0.75f, 0.70f, 0.45f, 1.0f });

        for (int i = 0; i < 48; i++)
        {
            const float offset = fmodf((float)i * 137.0f - camera.yaw * 900.0f, (float)swapChainExtent.width + 240.0f);
            const float x = offset - 120.0f;
            const float heightFraction = 0.18f + 0.32f * (float)((i * 37) % 100) / 100.0f;
            const float width = 40.0f + (float)((i * 53) % 60);
            const float height = swapChainExtent.height * 0.62f * heightFraction;

            rects.push_back({ x, swapChainExtent.height * 0.62f - height, width, height,
                0.10f + 0.35f * (float)((i * 17) % 100) / 100.0f, 0.12f, 0.22f + 0.3f * (float)((i * 29) % 100) / 100.0f, 1.0f });
        }

        for (int i = 0; i < 40; i++)
        {
            const float z = fmodf((float)i * 2.5f + camera.z * 6.0f, 100.0f);
            const float y = swapChainExtent.height * 0.62f + swapChainExtent.height * 0.38f * (z / 100.0f) * (z / 100.0f);
            rects.push_back({ 0.0f, y, (float)swapChainExtent.width, 1.5f, 0.30f, 0.34f, 0.30f, 1.0f });
        }
    }

    void BuildUi(std::vector<Rect>& rects)
    {
        for (const XrdTest::Rect& rect : ui.rects)
            rects.push_back({ rect.x, rect.y, rect.width, rect.height, rect.color.r, rect.color.g, rect.color.b, rect.color.a });
    }

    void DrawPass(VkCommandBuffer commands, VkFramebuffer framebuffer, const std::vector<Rect>& rects)
    {
        VkRenderPassBeginInfo pass{};
        pass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        pass.renderPass = renderPass;
        pass.framebuffer = framebuffer;
        pass.renderArea.extent = swapChainExtent;

        pfnCmdBeginRenderPass(commands, &pass, VK_SUBPASS_CONTENTS_INLINE);
        ClearRects(commands, rects);
        pfnCmdEndRenderPass(commands);
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
    XrdTest::Headless::ParseCommandLine("vulkan");

    if (XrdTest::Headless::Active())
        camera.autoYawSpeed = 0.0f;	// the pictures of the check are compared to each other

    if (!window.Create(L"Xbox Rain Droplets - Vulkan", 1280, 720))
        return 1;

    if (!LoadGlobalFunctions() || !InitializeDevice())
        return XrdTest::Headless::DeviceFailed();

    if (!CreateSwapChain() || !CreateFrameObjects())
    {
        printf("[vulkan] the swap chain could not be created\n");
        fflush(stdout);
        return XrdTest::Headless::DeviceFailed();
    }

    Xrd::VulkanInitInfo initInfo{};
    initInfo.instance = instance;
    initInfo.physicalDevice = physicalDevice;
    initInfo.device = device;
    initInfo.queue = queue;
    initInfo.queueFamilyIndex = queueFamilyIndex;

    Xrd::Init(Xrd::RENDERER_VULKAN, &initInfo);
    Xrd::SetCommandQueue(queue);

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

    std::vector<Rect> scene;
    std::vector<Rect> uiRects;

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

        uint32_t imageIndex = 0;
        const VkResult acquired = pfnAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailable, VK_NULL_HANDLE, &imageIndex);

        if (acquired != VK_SUCCESS && acquired != VK_SUBOPTIMAL_KHR)
            break;

        pfnWaitForFences(device, 1, &frameFence, VK_TRUE, UINT64_MAX);
        pfnResetFences(device, 1, &frameFence);

        // The scene is recorded, submitted and waited for before the drops are
        // asked for: the backend submits its own work, and a command buffer that
        // is submitted afterwards would be executed afterwards, which would draw
        // the scene over the drops again.
        pfnResetCommandBuffer(commandBuffer, 0);

        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        pfnBeginCommandBuffer(commandBuffer, &begin);

        TransitionImage(commandBuffer, swapChainImages[imageIndex], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        scene.clear();
        BuildScene(scene);
        DrawPass(commandBuffer, framebuffers[imageIndex], scene);

        pfnEndCommandBuffer(commandBuffer);

        {
            VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

            VkSubmitInfo submit{};
            submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit.waitSemaphoreCount = 1;
            submit.pWaitSemaphores = &imageAvailable;
            submit.pWaitDstStageMask = &waitStage;
            submit.commandBufferCount = 1;
            submit.pCommandBuffers = &commandBuffer;

            if (pfnQueueSubmit(queue, 1, &submit, frameFence) != VK_SUCCESS)
                break;

            pfnWaitForFences(device, 1, &frameFence, VK_TRUE, UINT64_MAX);
            pfnResetFences(device, 1, &frameFence);
        }

        // the drops land on the scene, and the frame is not finished, so the
        // interface drawn after them covers them
        Xrd::RenderTarget target{};
        target.resource = (void*)swapChainImages[imageIndex];
        target.size = { (int32_t)swapChainExtent.width, (int32_t)swapChainExtent.height };
        target.format = (uint32_t)swapChainFormat;
        target.state = Xrd::TARGET_STATE_RENDER_TARGET;
        Xrd::SetTarget(&target);

        XrdTest::Headless::PrepareDrops(frameIndex, window.width, window.height);

        WaterDrops::Process();
        WaterDrops::Render();

        uiRects.clear();
        BuildUi(uiRects);

        pfnResetCommandBuffer(commandBuffer, 0);
        pfnBeginCommandBuffer(commandBuffer, &begin);

        DrawPass(commandBuffer, framebuffers[imageIndex], uiRects);

        TransitionImage(commandBuffer, swapChainImages[imageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

        pfnEndCommandBuffer(commandBuffer);

        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &commandBuffer;
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = &renderFinished;

        if (pfnQueueSubmit(queue, 1, &submit, frameFence) != VK_SUCCESS)
            break;

        VkPresentInfoKHR present{};
        present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = &renderFinished;
        present.swapchainCount = 1;
        present.pSwapchains = &swapChain;
        present.pImageIndices = &imageIndex;

        pfnQueuePresentKHR(queue, &present);

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
            XrdTest::PrintStatus("vulkan", WaterDrops::ms_numDrops, frames, WaterDrops::bEnableSnow, extra);

            wchar_t title[160]{};
            swprintf_s(title, L"Xbox Rain Droplets - Vulkan  |  drops %d  fps %d", WaterDrops::ms_numDrops, frames);
            SetWindowTextW(window.hwnd, title);

            frames = 0;
            lastReport = now;
        }
    }

    if (pfnDeviceWaitIdle)
        pfnDeviceWaitIdle(device);

    WaterDrops::Shutdown();
    Xrd::Shutdown();

    return 0;
}
