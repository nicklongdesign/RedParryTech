#include "prelude.h"
#ifdef Sys_Win32_
    #include <windows.h>
    #define VK_USE_PLATFORM_WIN32_KHR 1
#endif

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#ifdef Sys_Win32_
    #include <vulkan/vulkan_win32.h>
#endif
#include <vulkan/vk_enum_string_helper.h>

#include <string.h>

#include <cassert>
#include <cstring>
#include <bit>
#include "rpt_allocators.h"
#include "rpt_slang.h"
#include "rpt_integral_map.h"
#include "rpt_file.h"
#include "rpt_vulkan_ext.h"
// #include "rpt_math.h"


#define PROJECT_NAME "rpt"

#define RPTErr_ SDL_LogError
#define RPTInfo_ SDL_LogInfo

void* rpt_vk_alloc(void* p_userData, usize size, usize alignment, VkSystemAllocationScope scope) {
    Assert_(size % DEFAULT_ALIGNMENT == 0);
    usize trueAlignment = min(alignment, DEFAULT_ALIGNMENT);
    TIntegralMap<uintptr_t, usize>* p_allocationSizeMap = static_cast<TIntegralMap<uintptr_t, usize>*>(p_userData);
    void* result = SDL_aligned_alloc(trueAlignment, size);
    Assert_(result != nullptr);
    const uintptr_t resultAddr = Recast_<uintptr_t>(result);
    SDL_LogInfo(0, "Adding %d to allocations", resultAddr);
    p_allocationSizeMap->force_add(Recast_<uintptr_t>(result), size);
    return result;
}

void rpt_vk_free(void* p_userData, void* p_original) {
    if (p_original == nullptr) return;
    TIntegralMap<uintptr_t, usize>* p_allocationSizeMap = static_cast<TIntegralMap<uintptr_t, usize>*>(p_userData);
    const uintptr_t originalAddr = Recast_<uintptr_t>(p_original);
    SDL_LogInfo(0, "Removing %d from allocations", originalAddr);
    const bool removeResult = p_allocationSizeMap->try_remove(originalAddr);
    Assert_(removeResult);
    SDL_aligned_free(p_original);
    p_original = nullptr;
}

void* rpt_vk_realloc(void* p_userData, void* p_original, usize size, usize alignment, VkSystemAllocationScope scope) {
    Assert_(size % DEFAULT_ALIGNMENT == 0);
    usize trueAlignment = min(alignment, DEFAULT_ALIGNMENT);
    if (p_original == nullptr) return rpt_vk_alloc(p_userData, size, trueAlignment, scope);
    if (size == 0) {
        rpt_vk_free(p_userData, p_original);
        return nullptr;
    }

    void* p_result = SDL_aligned_alloc(trueAlignment, size);
    if (p_result == nullptr) return nullptr;

    const uintptr_t originalAddr = Recast_<uintptr_t>(p_original);
    const uintptr_t resultAddr = Recast_<uintptr_t>(p_result);

    SDL_LogInfo(0, "Reallocating %d to %d", originalAddr, resultAddr);

    TIntegralMap<uintptr_t, usize>* p_allocationSizeMap = static_cast<TIntegralMap<uintptr_t, usize>*>(p_userData);
    const usize* p_originalSize = p_allocationSizeMap->get(originalAddr);
    Assert_(p_originalSize != nullptr);
    const usize originalSize = *p_originalSize;
    const usize copySize = min(size, originalSize);
    memcpy(p_result, p_original, copySize);
    p_allocationSizeMap->force_add(resultAddr, size);
    const bool removeResult = p_allocationSizeMap->try_remove(originalAddr);
    Assert_(removeResult);
    SDL_aligned_free(p_original);
    p_original = nullptr;
    return p_result;
}

TIntegralMap<uintptr_t, usize> allocationSizeMap;

const VkAllocationCallbacks g_vkAllocator = {
    .pUserData {&allocationSizeMap},
    .pfnAllocation {rpt_vk_alloc},
    .pfnReallocation {rpt_vk_realloc},
    .pfnFree {rpt_vk_free},
    .pfnInternalAllocation {nullptr},
    .pfnInternalFree {nullptr},
};

#define PTR_VK_ALLOATOR &g_vkAllocator
// #define PTR_VK_ALLOATOR nullptr

static const cstring vk_result_str(VkResult result) {
    return string_VkResult(result);
}

static bool vk_check(VkResult result) {
    if (result == VK_SUCCESS) return true;

    return false;
}

struct SSwapchainDetails {
    SSwapchainDetails() = delete;
    SSwapchainDetails(VkPhysicalDevice device, VkSurfaceKHR surface) noexcept;

    VkSurfaceCapabilitiesKHR m_capabilities;
    TDynArray<VkSurfaceFormatKHR> m_formats;
    TDynArray<VkPresentModeKHR> m_presentModes;
};

SSwapchainDetails::SSwapchainDetails(VkPhysicalDevice device, VkSurfaceKHR surface) noexcept {
    assert(vk_check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &m_capabilities)));

    u32 formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    assert(formatCount);
    m_formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, m_formats.data());

    u32 presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    assert(presentModeCount);
    m_presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, m_presentModes.data());
}

struct SSwapchain {
    SSwapchain() {}
    SSwapchain(VkDevice device, VkSurfaceKHR surface, const SSwapchainDetails& details) noexcept;
    void destroy_vk_resources(VkDevice device);

    VkSwapchainKHR m_handle;
    TDynArray<VkImage> m_images;
    TDynArray<VkImageView> m_imageViews;
    VkSurfaceFormatKHR m_format;
    VkPresentModeKHR m_presentMode;
    VkExtent2D m_extent;
    VkSurfaceCapabilitiesKHR m_capabilities;
    u32 m_imageCount;
};

SSwapchain::SSwapchain(VkDevice device, VkSurfaceKHR surface, const SSwapchainDetails& details) noexcept {
    VkSurfaceFormatKHR selectedFormat;
    bool isSurfaceFormatFound = false;
    for (const VkSurfaceFormatKHR& formatInfo : details.m_formats) {
        const bool isImageFormatMatch = formatInfo.format == VK_FORMAT_R8G8B8A8_SRGB;
        const bool isColorSpaceMatch = formatInfo.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        if (isImageFormatMatch && isColorSpaceMatch) {
            selectedFormat = formatInfo;
            isSurfaceFormatFound = true;
        }
    }

    if (!isSurfaceFormatFound) {
        selectedFormat = details.m_formats[0];
        SDL_LogWarn(0, "Couldn't find surface format with RGB8 and SRGB colorspace. Choosing %d | %d instead", selectedFormat.format, selectedFormat.colorSpace);
    }

    VkPresentModeKHR selectedPresentMode;
    bool isPresentModeFound = false;
    for (const VkPresentModeKHR& presentMode : details.m_presentModes) {
        if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            selectedPresentMode = presentMode;
            isPresentModeFound = true;
        }
    }

    if (!isPresentModeFound) {
        selectedPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    }

    m_format = selectedFormat;
    m_presentMode = selectedPresentMode;
    m_extent = details.m_capabilities.currentExtent;
    m_imageCount = min(details.m_capabilities.minImageCount + 1, details.m_capabilities.maxImageCount);
    m_capabilities = details.m_capabilities;

    VkSwapchainCreateInfoKHR swapchainCreateInfo = {
        .sType {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR},
        .pNext {nullptr},
        .flags {0},
        .surface {surface},
        .minImageCount {m_imageCount},
        .imageFormat {m_format.format},
        .imageColorSpace {m_format.colorSpace},
        .imageExtent {m_extent},
        .imageArrayLayers {1},
        .imageUsage {VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT},
        .imageSharingMode {VK_SHARING_MODE_EXCLUSIVE},
        .queueFamilyIndexCount {0},
        .pQueueFamilyIndices {nullptr},
        .preTransform {m_capabilities.currentTransform},
        .compositeAlpha {VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR},
        .presentMode {m_presentMode},
        .clipped {VK_TRUE},
        .oldSwapchain {VkSwapchainKHR{}}
    };

    assert(vk_check(vkCreateSwapchainKHR(device, &swapchainCreateInfo, PTR_VK_ALLOATOR, &m_handle)));
    vkGetSwapchainImagesKHR(device, m_handle, &m_imageCount, nullptr);
    m_images.resize(m_imageCount);
    vkGetSwapchainImagesKHR(device, m_handle, &m_imageCount, m_images.data());

    m_imageViews.resize(m_imageCount);
    for (usize i = 0; i < m_imageCount; ++i) {
        VkImageViewCreateInfo imageViewCreateInfo = {
            .sType {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO},
            .pNext {nullptr},
            .flags {0},
            .image {m_images[i]},
            .viewType {VK_IMAGE_VIEW_TYPE_2D},
            .format {m_format.format},
            .components {VkComponentMapping {
                .r {VK_COMPONENT_SWIZZLE_IDENTITY},
                .g {VK_COMPONENT_SWIZZLE_IDENTITY},
                .b {VK_COMPONENT_SWIZZLE_IDENTITY},
                .a {VK_COMPONENT_SWIZZLE_IDENTITY}
            }},
            .subresourceRange {VkImageSubresourceRange {
                .aspectMask {VK_IMAGE_ASPECT_COLOR_BIT},
                .baseMipLevel {0},
                .levelCount {1},
                .baseArrayLayer {0},
                .layerCount {1}
            }}
        };

        assert(
            vk_check(
                vkCreateImageView(device, &imageViewCreateInfo, PTR_VK_ALLOATOR, &m_imageViews[i])));
    }
}

void SSwapchain::destroy_vk_resources(VkDevice device) {
    for (const VkImageView& imageView : m_imageViews) {
        vkDestroyImageView(device, imageView, PTR_VK_ALLOATOR);
    }

    vkDestroySwapchainKHR(device, m_handle, PTR_VK_ALLOATOR);
}


class IRenderHardwareInterface {};

class CRHI_Vulkan : public IRenderHardwareInterface {
public:
    CRHI_Vulkan(SDL_Window *window);
    ~CRHI_Vulkan();

private:
    static constexpr const u32 FRAMES_IN_FLIGHT = 2;
    static constexpr const usize TEXTURE_SHADER_INDEX = 0;
    static constexpr const usize FRAGMENT_SHADER_INDEX = 1;

    struct SQueue {
        VkQueue m_handle;
        u32 m_familyIndex;
        u32 m_queueIndex;
    };


    SDL_Window* mp_window;

    // Vulkan Handles
    VkInstance m_instance = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    TIntegralMap<VkQueueFlags, SQueue> m_queues;
    SQueue m_presentQueue;
    SSwapchain m_swapchain;
    VkShaderEXT m_shaders[2];
    TIntegralMap<u32, VkCommandPool> m_commandPools;
    VkCommandBuffer m_graphicsCommandBuffers[FRAMES_IN_FLIGHT];

    // Shader stuff
    // CSlangCompiler m_slang;
    // This is slow and unwieldy, this should probably be moved to a build pipeline
    // When it's time to have that sort of thing

    // This is a bunch of data that probably shouldn't be in an RHI for this toy it'll live here for now
    // This should all be dispersed across asset structures and some kind of pipeline system
    VkDescriptorSetLayout m_textureSampleDescriptorLayout;
    VkDescriptorPool m_descriptorPool;

private:
#ifdef Debug_
    static constexpr u32 m_validationLayersCount = 3;
    static constexpr const cstring m_validationLayers[m_validationLayersCount] = {
        "VK_LAYER_KHRONOS_validation",
        "VK_LAYER_KHRONOS_synchronization2",
        "VK_LAYER_KHRONOS_shader_object",
    };
#endif

    static constexpr u32 m_extensionsCount = 4;
    static constexpr const cstring m_deviceExtensions[m_extensionsCount] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        VK_EXT_SHADER_OBJECT_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
    };

    static bool check_device_extension_support(VkPhysicalDevice physicalDevice);
};

CRHI_Vulkan::CRHI_Vulkan(SDL_Window* window) 
    : mp_window(window)
{
    // Start initializing the slang compiler on a thread, cause that's a doozy
    // SDL_Thread* slangInitalizerThread = SDL_CreateThread(CSlangCompiler::thread_initialize, "Slang Initializer", &m_slang);
    // assert(slangInitalizerThread != nullptr);

    VkApplicationInfo app_info = {
        .sType {VK_STRUCTURE_TYPE_APPLICATION_INFO},
        .pNext {nullptr},
        .pApplicationName {"game"},
        .applicationVersion {VK_MAKE_VERSION(0, 0, 1)},
        .pEngineName {"RPT"},
        .engineVersion {VK_MAKE_VERSION(0, 0, 1)},
        .apiVersion {VK_API_VERSION_1_3}
    };
    { // create_instance
        u32 sdl_extension_count = 0;
        cstring_const const* extensions = SDL_Vulkan_GetInstanceExtensions(&sdl_extension_count);
        RPTInfo_(0, "SDL Required Extensions:\n");
        for(usize i = 0; i < sdl_extension_count; ++i) {
            RPTInfo_(0, "\t%s\n", extensions[i]);
        }

        #ifdef Debug_
            u32 available_layer_count = 0;
            vkEnumerateInstanceLayerProperties(&available_layer_count, nullptr);
            VkLayerProperties* layer_properties = static_cast<VkLayerProperties*>(SDL_malloc(sizeof(VkLayerProperties) * available_layer_count));
            vkEnumerateInstanceLayerProperties(&available_layer_count, layer_properties);
            RPTInfo_(0, "Available Layers:\n");
            for (usize j = 0; j < available_layer_count; ++j) {
                const cstring available_layer = layer_properties[j].layerName;
                RPTInfo_(0,"\t%s\n", available_layer);
            }
            for (usize i = 0; i < m_validationLayersCount; ++i) {
                bool is_match_found = false;
                for (usize j = 0; j < available_layer_count; ++j) {
                    const cstring available_layer = layer_properties[j].layerName;
                    is_match_found = SDL_strcmp(m_validationLayers[i], available_layer) == 0;
                    if (is_match_found) {
                        j = available_layer_count;
                        continue;
                    }
                }

                if (!is_match_found) {
                    RPTErr_(0, "Validation Layer %s is unavailable\n", m_validationLayers[i]);
                }
            }
        #endif
        VkInstanceCreateInfo instance_info = {
            .sType {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO},
            .pNext {nullptr},
            .flags {0},
            .pApplicationInfo {&app_info},
        #ifdef Debug_
            .enabledLayerCount {m_validationLayersCount},
            .ppEnabledLayerNames {m_validationLayers},
        #else
            .enabledLayerCount {0},
            .ppEnabledLayerNames {nullptr},
        #endif
            .enabledExtensionCount {sdl_extension_count},
            .ppEnabledExtensionNames {extensions},
        };

        if (!vk_check(vkCreateInstance(&instance_info, PTR_VK_ALLOATOR, &m_instance))) {
            SDL_free(layer_properties);
            assert(false);
        }
        SDL_free(layer_properties);
    }

    { //create_surface
    #ifdef Sys_Win32_
        struct SWindowsPointers {
            void* hwnd;
            void* hinstance;
        };

        // void get_windows_pointers_cb(void* userData, SDL_PropertiesID props, const CStr name) {
        const auto get_windows_pointers_cb = [] (void* userData, SDL_PropertiesID props, const cstring name) {
            SWindowsPointers* ptrs = static_cast<SWindowsPointers*>(userData);
            SDL_PropertyType propType = SDL_GetPropertyType(props, name);

            if (propType != SDL_PROPERTY_TYPE_POINTER) return;

            if (SDL_strcmp(name, SDL_PROP_WINDOW_WIN32_HWND_POINTER) == 0) {
                ptrs->hwnd = SDL_GetPointerProperty(props, name, nullptr);
            }

            if (SDL_strcmp(name, SDL_PROP_WINDOW_WIN32_INSTANCE_POINTER) == 0) {
                ptrs->hinstance = SDL_GetPointerProperty(props, name, nullptr);
            }
        };

        SWindowsPointers winPtrs;
        SDL_PropertiesID windowProperties = SDL_GetWindowProperties(window);
        SDL_EnumerateProperties(windowProperties, get_windows_pointers_cb, &winPtrs);

        VkWin32SurfaceCreateInfoKHR surface_create_info = {
            .sType {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR},
            .pNext {nullptr},
            .flags {0},
            .hinstance {static_cast<HINSTANCE>(winPtrs.hinstance)},
            .hwnd {static_cast<HWND>(winPtrs.hwnd)}
        };
        assert(vk_check(vkCreateWin32SurfaceKHR(m_instance, &surface_create_info, PTR_VK_ALLOATOR, &m_surface)));
    #endif 
    }

    {//Select Physical Device
        u32 deviceCount = 0;
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            RPTErr_(0, "No physical devices");
            assert(false);
        }

        TDynArray<VkPhysicalDevice> possibleDevices(deviceCount);
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, possibleDevices.data());

        const auto get_suitibility_score = [](VkPhysicalDevice device, VkSurfaceKHR surface) -> usize {
            VkPhysicalDeviceProperties2 props = {
                .sType {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2},
                .pNext {nullptr},
                .properties {},
            };

            VkPhysicalDeviceFeatures2 features = {
                .sType {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2},
                .pNext {nullptr},
                .features {}
            };

            vkGetPhysicalDeviceProperties2(device, &props);
            vkGetPhysicalDeviceFeatures2(device, &features);
            
            if (!features.features.geometryShader) {
                return 0;
            }

            bool deviceSupportsRequiredExtensions = false;
            {
                u32 extensionCount = 0;
                vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
                TDynArray<VkExtensionProperties> deviceExtensions(extensionCount);
                vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, deviceExtensions.data());


                for (usize i = 0; i < m_extensionsCount; ++i) {
                    bool extensionFound = false;
                    for (const VkExtensionProperties& extension : deviceExtensions) {
                        const char* name = extension.extensionName;
                        if (SDL_strcmp(m_deviceExtensions[i], name) == 0) {
                            extensionFound = true;
                            break;
                        }
                    }

                    if (!extensionFound) {
                        deviceSupportsRequiredExtensions = false;
                        break;
                    }
                    else if (i == m_extensionsCount - 1) {
                        deviceSupportsRequiredExtensions = true;
                    }
                }
            }

            if(!deviceSupportsRequiredExtensions) {
                return 0;
            }

            SSwapchainDetails swapchainDetails(device, surface);
            if (swapchainDetails.m_formats.size() == 0 || swapchainDetails.m_presentModes.size() == 0) {
                return 0;
            }

            usize score {0};
            if (props.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                score += 100000;
            }

            score += props.properties.limits.maxImageDimension2D;

            return score;
        };


        usize maxScore = 0;
        VkPhysicalDevice bestCandidateDevice {};
        for (const VkPhysicalDevice& device : possibleDevices) {
            const usize deviceScore = get_suitibility_score(device, m_surface);
            if (deviceScore > maxScore) {
                maxScore = deviceScore;
                bestCandidateDevice = device;
            }
        }

        assert(maxScore);
        m_physicalDevice = bestCandidateDevice;
    }


    TDynArray<VkQueueFlags> queueCreateInfos(0);
    u32 graphicsFamilyIndex = 0;
    u32 graphicsQueueIndex = 0;
    u32 presentFamilyIndex = 0;
    u32 presentQueueIndex = 0;
    u32 transferFamilyIndex = 0;
    u32 transferQueueIndex = 0;;
    { // Assign Queues
        u32 queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties2(m_physicalDevice, &queueFamilyCount, nullptr);
        VkQueueFamilyProperties2 defaultProperties = {
            .sType {VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2},
            .pNext {nullptr},
            .queueFamilyProperties {}

        };
        TDynArray<VkQueueFamilyProperties2> availableQueues(queueFamilyCount, defaultProperties);
        vkGetPhysicalDeviceQueueFamilyProperties2(m_physicalDevice, &queueFamilyCount, availableQueues.data());

        // Find the graphics-present family first
        for (u32 i = 0; i < availableQueues.size(); ++i) {
            const VkQueueFlags queueFlags = availableQueues[i].queueFamilyProperties.queueFlags;

            if (queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                graphicsFamilyIndex = i;
                graphicsQueueIndex = 0;

                u32 present_queue_found = 0;
                assert(vk_check(vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, i, m_surface, &present_queue_found)));
                assert(present_queue_found);

                presentFamilyIndex = i;
                presentQueueIndex = 1;
            }
        }

        // Starting over to make sure we cover our bases, we're not assuming any order, but we are priortizing graphics
        for (u32 i = 0; i < availableQueues.size(); ++i) {
            if (i == graphicsFamilyIndex || i == presentFamilyIndex) continue;
            const VkQueueFlags queueFlags = availableQueues[i].queueFamilyProperties.queueFlags;
            if (queueFlags & VK_QUEUE_TRANSFER_BIT) {
                transferFamilyIndex = i;
                transferQueueIndex = 0;
            }
        }
    }

    { // Create Device
        constexpr u32 graphicsPresentQueueCount = 2;
        f32 graphicsPresentQueuePriorities[graphicsPresentQueueCount] = { 1.0f, 1.0f};
        const VkDeviceQueueCreateInfo graphicsPresentQueueCreateInfo = {
            .sType {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO},
            .pNext {nullptr},
            .queueFamilyIndex {graphicsFamilyIndex},
            .queueCount {graphicsPresentQueueCount},
            .pQueuePriorities {graphicsPresentQueuePriorities}
        };

        f32 transferQueuePriority = 1.0f;
        const VkDeviceQueueCreateInfo transferQueueCreateInfo = {
            .sType {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO},
            .pNext {nullptr},
            .queueFamilyIndex {transferFamilyIndex},
            .queueCount {1},
            .pQueuePriorities {&transferQueuePriority}
        };

        VkDeviceQueueCreateInfo queueCreateInfos[2] = {graphicsPresentQueueCreateInfo, transferQueueCreateInfo};

        VkPhysicalDeviceFeatures2 deviceFeatures = {
            .sType {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2}
        };
        vkGetPhysicalDeviceFeatures2(m_physicalDevice, &deviceFeatures);

        VkPhysicalDeviceShaderObjectFeaturesEXT shaderObjectInfo = {
            .sType {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT},
            .pNext {nullptr},
            .shaderObject {VK_TRUE}
        };

        const VkPhysicalDeviceVulkan13Features vk13Features = {
            .sType {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES},
            .pNext {&shaderObjectInfo},
            .synchronization2 {true},
            .dynamicRendering {true}
        };

        const VkDeviceCreateInfo deviceCreateInfo = {
            .sType {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO},
            .pNext {&vk13Features},
            .flags {0},
            .queueCreateInfoCount {2},
            .pQueueCreateInfos {queueCreateInfos},
            .enabledLayerCount {0},
            .ppEnabledLayerNames {nullptr},
            .enabledExtensionCount {m_extensionsCount},
            .ppEnabledExtensionNames {m_deviceExtensions},
            .pEnabledFeatures {&deviceFeatures.features},
        };

        Assert_(vk_check(vkCreateDevice(m_physicalDevice, &deviceCreateInfo, PTR_VK_ALLOATOR, &m_device)));

        DefineExtFn_(m_device, vkCreateShadersEXT);
        DefineExtFn_(m_device, vkDestroyShaderEXT);
        DefineExtFn_(m_device, vkGetShaderBinaryDataEXT);
        DefineExtFn_(m_device, vkCmdBindShadersEXT);
        DefineExtFn_(m_device, vkCmdSetDepthClampRangeEXT);
    }

    { //Retrieve Queue handles and store indices for future use
        SQueue graphicsQueue = {
            .m_familyIndex {graphicsFamilyIndex},
            .m_queueIndex {graphicsQueueIndex},
        };
        vkGetDeviceQueue(m_device, graphicsQueue.m_familyIndex, graphicsQueue.m_queueIndex, &graphicsQueue.m_handle);
        m_queues.force_add(VK_QUEUE_GRAPHICS_BIT, graphicsQueue);

        m_presentQueue = {
            .m_familyIndex {presentFamilyIndex},
            .m_queueIndex {presentQueueIndex},
        };
        vkGetDeviceQueue(m_device, m_presentQueue.m_familyIndex, m_presentQueue.m_queueIndex, &m_presentQueue.m_handle);

        SQueue transferQueue = {
            .m_familyIndex {transferFamilyIndex},
            .m_queueIndex {transferQueueIndex},
        };
        vkGetDeviceQueue(m_device, transferQueue.m_familyIndex, transferQueue.m_queueIndex, &transferQueue.m_handle);
        m_queues.force_add(VK_QUEUE_TRANSFER_BIT, transferQueue);
    }

    SSwapchainDetails details(m_physicalDevice, m_surface);
    m_swapchain = SSwapchain(m_device, m_surface, details);

    { // Create Descriptor set layout
        const VkDescriptorSetLayoutBinding textureLayoutBinding = {
            .binding {0},
            .descriptorType {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
            .descriptorCount {1},
            .stageFlags {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
            .pImmutableSamplers {nullptr}
        };

        const VkDescriptorSetLayoutCreateInfo textureLayoutCreateInfo = {
            .sType {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO},
            .pNext {nullptr},
            .flags {},
            .bindingCount {1},
            .pBindings {&textureLayoutBinding}
        };

        Assert_(
            vk_check(
                vkCreateDescriptorSetLayout(m_device, &textureLayoutCreateInfo, PTR_VK_ALLOATOR, &m_textureSampleDescriptorLayout)));
    }

    { // Create Descriptor Pool
        const VkDescriptorPoolSize descriptorPoolSize = {
            .type {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
            .descriptorCount {1}
        };

        const VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
            .sType {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO},
            .flags {VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT},
            .maxSets {1},
            .poolSizeCount {1},
            .pPoolSizes {&descriptorPoolSize}
        };
        Assert_(
            vk_check(
                vkCreateDescriptorPool(m_device, &descriptorPoolCreateInfo, PTR_VK_ALLOATOR, &m_descriptorPool)));
    }

    {
        CFileLoader vertCodeLoader = CFileLoader();
        CFileLoader fragCodeLoader = CFileLoader();
        Assert_(vertCodeLoader.open("assets/shaders/build/texture.vert.spv", "rb"));
        Assert_(fragCodeLoader.open("assets/shaders/build/texture.frag.spv", "rb"));
        usize vertCodeBytes;
        usize fragCodeBytes;

        const cstring p_vertCode = vertCodeLoader.read_file(&vertCodeBytes);
        Assert_(p_vertCode != nullptr);

        const cstring p_fragCode = fragCodeLoader.read_file(&fragCodeBytes);
        Assert_(p_fragCode != nullptr);

        const VkPushConstantRange textureConstantRange = {
            .stageFlags {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
            .offset {0},
            .size {sizeof(f32) * 16 * 3}
        };

        const VkShaderCreateInfoEXT vertexCreateInfo = {
            .sType {VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT},
            .pNext {nullptr},
            .flags {0},
            .stage {VK_SHADER_STAGE_VERTEX_BIT},
            .nextStage = {VK_SHADER_STAGE_FRAGMENT_BIT},
            .codeType {VK_SHADER_CODE_TYPE_SPIRV_EXT},
            .codeSize {static_cast<u32>(vertCodeBytes)},
            .pCode {p_vertCode},
            .pName {"main"},
            .setLayoutCount {1},
            .pSetLayouts {&m_textureSampleDescriptorLayout},
            .pushConstantRangeCount {1},
            .pPushConstantRanges {&textureConstantRange},
            .pSpecializationInfo {nullptr}
        };

        const VkShaderCreateInfoEXT fragmentCreateInfo = {
            .sType {VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT},
            .pNext {nullptr},
            .flags {0},
            .stage {VK_SHADER_STAGE_FRAGMENT_BIT},
            .nextStage = {0},
            .codeType {VK_SHADER_CODE_TYPE_SPIRV_EXT},
            .codeSize {static_cast<u32>(fragCodeBytes)},
            .pCode {p_fragCode},
            .pName {"main"},
            .setLayoutCount {1},
            .pSetLayouts {&m_textureSampleDescriptorLayout},
            .pushConstantRangeCount {1},
            .pPushConstantRanges {&textureConstantRange},
            .pSpecializationInfo {nullptr}
        };

        VkShaderCreateInfoEXT createInfos[] = {
            vertexCreateInfo,
            fragmentCreateInfo
        };

        Assert_(
            vk_check(
                vkCreateShadersEXT(
                    m_device,
                    2,
                    createInfos,
                    PTR_VK_ALLOATOR,
                    m_shaders)));
    }

    {
        for (usize i = 0; i < m_queues.length(); ++i) {
            const SQueue* p_queue = m_queues.at_index(i);
            const u32 familyIndex = p_queue->m_familyIndex;
            if (m_commandPools.contains(familyIndex)) continue;

            m_commandPools.force_add(familyIndex, 0);
            VkCommandPool* p_currentPool = m_commandPools.get_mut(familyIndex);
            Assert_(p_currentPool != nullptr);

            const VkCommandPoolCreateInfo createInfo = {
                .sType {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO},
                .pNext {nullptr},
                .flags {VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT},
                .queueFamilyIndex {familyIndex}
            };

            Assert_(
                vk_check(
                    vkCreateCommandPool(m_device, &createInfo, PTR_VK_ALLOATOR, p_currentPool)));
        }

        const SQueue* graphicsQueue = m_queues.get(VK_QUEUE_GRAPHICS_BIT);
        Assert_(graphicsQueue != nullptr);
        u32 graphicsFamilyIndex = graphicsQueue->m_familyIndex;
        Assert_(m_commandPools.contains(graphicsFamilyIndex));
        const VkCommandBufferAllocateInfo bufferAllocateInfo = {
            .sType {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO},
            .pNext {nullptr},
            .commandPool {*m_commandPools.get(graphicsFamilyIndex)},
            .level {VK_COMMAND_BUFFER_LEVEL_PRIMARY},
            .commandBufferCount {FRAMES_IN_FLIGHT}
        };

        Assert_(
            vk_check(
                vkAllocateCommandBuffers(m_device, &bufferAllocateInfo, m_graphicsCommandBuffers)));
    }
}

CRHI_Vulkan::~CRHI_Vulkan() {
    vkFreeCommandBuffers(m_device, *m_commandPools.get(VK_QUEUE_GRAPHICS_BIT), FRAMES_IN_FLIGHT, m_graphicsCommandBuffers);

    for (usize i = 0; i < m_commandPools.length(); ++i) {
        vkDestroyCommandPool(m_device, *m_commandPools.at_index_mut(i), PTR_VK_ALLOATOR);
    }

    for (usize i = 0; i < 2; ++i) {
        vkDestroyShaderEXT(m_device, m_shaders[i], PTR_VK_ALLOATOR);
    }
    vkDestroyDescriptorPool(m_device, m_descriptorPool, PTR_VK_ALLOATOR);
    vkDestroyDescriptorSetLayout(m_device, m_textureSampleDescriptorLayout, PTR_VK_ALLOATOR);
    m_swapchain.destroy_vk_resources(m_device);
    vkDestroyDevice(m_device, PTR_VK_ALLOATOR);
    vkDestroySurfaceKHR(m_instance, m_surface, PTR_VK_ALLOATOR);
    vkDestroyInstance(m_instance, PTR_VK_ALLOATOR);
}

int main(int argc, char **argv) {
    #ifdef Debug_
    SDL_LogInfo(0, "Debug build");
    #endif
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
    SDL_InitFlags init_flags = 
        SDL_INIT_AUDIO | 
        SDL_INIT_VIDEO |
        SDL_INIT_GAMEPAD;

    const bool is_init_successful = SDL_Init(init_flags);
    if (!is_init_successful){
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not init SDL: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("rpt", 1280, 720, SDL_WINDOW_VULKAN);
    if (window == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Could not create window: %s\n", SDL_GetError());
        return 1;
    }

    CRHI_Vulkan vulkan_rhi = CRHI_Vulkan(window);

    bool is_game_active = true;
    while(is_game_active) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_KEY_DOWN:
                switch(event.key.key) {
                case SDLK_ESCAPE:
                    is_game_active = false;
                default:
                    break;
                }
                break;
            default:
                break;
            }
        }
    }

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
