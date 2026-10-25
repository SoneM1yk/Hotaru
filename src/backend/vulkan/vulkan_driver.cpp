#include "vulkan_driver.h"

#include "backend/swapchain.h"
#include "backend/command_list.h"
#include "backend/vulkan/vulkan_swapchain.h"
#include "hotaru/config.h"
#include "result.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <spdlog/spdlog.h>
#include <volk.h>
#include <vulkan/vulkan_core.h>

namespace hotaru
{
namespace
{

constexpr const char* kValidationLayerName = "VK_LAYER_KHRONOS_validation";
constexpr const char* kWaylandSurfaceExtensionName = "VK_KHR_wayland_surface";
constexpr const char* kWin32SurfaceExtensionName = "VK_KHR_win32_surface";
constexpr uint32_t kTargetApiVersion = VK_API_VERSION_1_2;

bool HasExtension(const std::vector<VkExtensionProperties>& extensions,
                          std::string_view name)
{
    return std::any_of(extensions.begin(), extensions.end(),
                       [name](const VkExtensionProperties& extension) {
                           return std::string_view(extension.extensionName) == name;
                       });
}

bool HasInstanceLayer(const std::vector<VkLayerProperties>& layers,
                      std::string_view name)
{
    return std::any_of(layers.begin(), layers.end(),
                       [name](const VkLayerProperties& layer) {
                           return std::string_view(layer.layerName) == name;
                       });
}

const char* SurfaceTypeName(SurfaceType type)
{
    switch (type) {
        case SurfaceType::WAYLAND:
            return "Wayland";
        case SurfaceType::WIN32:
            return "Win32";
        case SurfaceType::NONE:
        default:
            return "none";
    }
}

const char* DeviceTypeName(VkPhysicalDeviceType type)
{
    switch (type) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            return "discrete GPU";
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            return "integrated GPU";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            return "virtual GPU";
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            return "CPU";
        default:
            return "other";
    }
}

uint32_t DeviceScore(VkPhysicalDeviceType type)
{
    switch (type) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            return 4;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            return 3;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            return 2;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            return 1;
        default:
            return 0;
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* /*user_data*/)
{
    const std::string_view message =
        callback_data != nullptr && callback_data->pMessage != nullptr
        ? callback_data->pMessage
        : "Unknown Vulkan message";

    const char* type_name = "general";
    if ((type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) != 0) {
        type_name = "validation";
    } else if ((type & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) != 0) {
        type_name = "performance";
    }

    switch (severity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            SPDLOG_ERROR("[Vulkan][{}] {}", type_name, message);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            SPDLOG_WARN("[Vulkan][{}] {}", type_name, message);
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            SPDLOG_INFO("[Vulkan][{}] {}", type_name, message);
            break;
        default:
            SPDLOG_DEBUG("[Vulkan][{}] {}", type_name, message);
            break;
    }

    return VK_FALSE;
}

VkDebugUtilsMessengerCreateInfoEXT MakeDebugMessengerCreateInfo()
{
    VkDebugUtilsMessengerCreateInfoEXT create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    create_info.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    create_info.pfnUserCallback = DebugCallback;
    return create_info;
}

} // namespace

struct VulkanDriver::Impl
{
    explicit Impl(const DriverConfig& driver_config)
    {
        surface_type = driver_config.surface_type;
    }

    ~Impl()
    {
        Destroy();
    }

    void Destroy() noexcept
    {
        swapchain.reset();

        if (device != VK_NULL_HANDLE) {
            vkDestroyDevice(device, nullptr);
            device = VK_NULL_HANDLE;
        }

        queues.clear();
        graphics_queue = VK_NULL_HANDLE;
        graphics_queue_family = VK_QUEUE_FAMILY_IGNORED;

        if (debug_messenger != VK_NULL_HANDLE && instance != VK_NULL_HANDLE) {
            const auto destroy_debug_messenger =
                reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                    vkGetInstanceProcAddr(instance,
                                          "vkDestroyDebugUtilsMessengerEXT"));
            if (destroy_debug_messenger != nullptr) {
                destroy_debug_messenger(instance, debug_messenger, nullptr);
            }
            debug_messenger = VK_NULL_HANDLE;
        }

        if (instance != VK_NULL_HANDLE) {
            vkDestroyInstance(instance, nullptr);
            instance = VK_NULL_HANDLE;
        }

        physical_device = VK_NULL_HANDLE;
        validation_enabled = false;
        debug_utils_enabled = false;
    }

    struct QueueInfo
    {
        VkQueue queue = VK_NULL_HANDLE;
        uint32_t family_index = VK_QUEUE_FAMILY_IGNORED;
        VkQueueFlags flags = 0;
    };

    SurfaceType surface_type;
    std::vector<std::string> instance_extensions;
    std::vector<std::string> device_extensions;

    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphics_queue = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
    uint32_t graphics_queue_family = VK_QUEUE_FAMILY_IGNORED;
    uint32_t present_queue_family = VK_QUEUE_FAMILY_IGNORED;
    uint32_t api_version = VK_API_VERSION_1_0;
    std::vector<QueueInfo> queues;
    bool validation_enabled = false;
    bool debug_utils_enabled = false;

    std::unique_ptr<VulkanSwapchain> swapchain;
};

VulkanDriver::VulkanDriver(const DriverConfig& config)
    : impl_(std::make_unique<Impl>(config))
{}

VulkanDriver::~VulkanDriver() = default;

Result VulkanDriver::Init()
{
    if (volkInitialize() != VK_SUCCESS) {
        return Result::Failure(ResultType::InitializationFailed, 
            "Volk Loader Failed!");
    }

    const Result result = CreateInstance()
        .AndThen([this] { volkLoadInstance(impl_->instance); return Result::Ok(); })
        .AndThen([this] { return SetupDebugMessenger(); })
        .AndThen([this] { return PickPhysicalDevice(); })
        .AndThen([this] { return CreateLogicalDevice(); })
        .AndThen([this] { volkLoadDevice(impl_->device); return Result::Ok(); });
    if (!result) {
        impl_->Destroy();
        return result;
    }

    SPDLOG_INFO("Vulkan backend initialized (surface: {})",
                SurfaceTypeName(impl_->surface_type));
    return Result::Ok();
}

Result VulkanDriver::CreateInstance()
{
    // Extension
    uint32_t extension_count = 0;
    VkResult vk_result = vkEnumerateInstanceExtensionProperties(
        nullptr, &extension_count, nullptr);
    if (vk_result != VK_SUCCESS) {
        return Result::Failure(
            ResultType::InitializationFailed,
            "Failed to enumerate Vulkan instance extensions");
    }

    std::vector<VkExtensionProperties> available_extensions(extension_count);
    if (extension_count > 0) {
        vk_result = vkEnumerateInstanceExtensionProperties(
            nullptr, &extension_count, available_extensions.data());
        if (vk_result != VK_SUCCESS) {
            return Result::Failure(
                ResultType::InitializationFailed,
                "Failed to query Vulkan instance extensions");
        }
    }

    std::vector<const char*> enabled_extensions;
    enabled_extensions.reserve(impl_->instance_extensions.size() + 2);


    auto ResolveInstanceExtensions = [this](
        const std::vector<VkExtensionProperties>& available,
        std::vector<const char*>& extensions)
    {
        if (impl_->surface_type != SurfaceType::NONE) {
            extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
        }

        switch (impl_->surface_type) {
            case SurfaceType::NONE:
                break;
            case SurfaceType::WAYLAND:
                extensions.push_back(kWaylandSurfaceExtensionName);
                break;
            case SurfaceType::WIN32:
                extensions.push_back(kWin32SurfaceExtensionName);
                break;
            default:
                return Result::Failure(ResultType::InvalidArgument,
                                    "Unsupported surface type");
        }

        for (const std::string& extension : impl_->instance_extensions) {
            extensions.push_back(extension.c_str());
        }

        return Result::Ok();
    };

    if (Result result = ResolveInstanceExtensions(
            available_extensions, enabled_extensions);
        !result) {
        return result;
    }

#ifndef NDEBUG
    // Validation Layer
    uint32_t layer_count = 0;
    vk_result = vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    if (vk_result != VK_SUCCESS) {
        return Result::Failure(
            ResultType::InitializationFailed,
            "Failed to enumerate Vulkan instance layers");
    }

    std::vector<VkLayerProperties> available_layers(layer_count);
    if (layer_count > 0) {
        vk_result = vkEnumerateInstanceLayerProperties(
            &layer_count, available_layers.data());
        if (vk_result != VK_SUCCESS) {
            return Result::Failure(
                ResultType::InitializationFailed,
                "Failed to query Vulkan instance layers");
        }
    }

    impl_->validation_enabled =
        HasInstanceLayer(available_layers, kValidationLayerName);
    if (!impl_->validation_enabled) {
        SPDLOG_WARN("Vulkan validation layer is unavailable; continuing "
                    "without validation");
    }
#endif

    // Debug Utils
    const bool debug_utils_available = HasExtension(
        available_extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    impl_->debug_utils_enabled =
        impl_->validation_enabled && debug_utils_available;
    if (impl_->debug_utils_enabled) {
        enabled_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    } else if (impl_->validation_enabled) {
        SPDLOG_WARN("Vulkan debug utils extension is unavailable; validation "
                    "messages will only be written to stderr");
    }

    // API Version
    uint32_t api_version = VK_API_VERSION_1_0;
    const auto enumerate_version =
        reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
            vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion"));
    if (enumerate_version != nullptr) {
        uint32_t loader_version = VK_API_VERSION_1_0;
        if (enumerate_version(&loader_version) == VK_SUCCESS) {
            api_version = std::min(loader_version, kTargetApiVersion);
        }
    }

    VkApplicationInfo application_info{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Hotaru",
        .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
        .pEngineName = "Hotaru Engine",
        .engineVersion = VK_MAKE_VERSION(0, 1, 0),
        .apiVersion = api_version,
    };

    VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
    VkInstanceCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &application_info,
        .enabledExtensionCount =
            static_cast<uint32_t>(enabled_extensions.size()),
        .ppEnabledExtensionNames = enabled_extensions.data()
    };

    if (impl_->validation_enabled) {
        create_info.enabledLayerCount = 1;
        create_info.ppEnabledLayerNames = &kValidationLayerName;
    }

    if (impl_->debug_utils_enabled) {
        debug_create_info = MakeDebugMessengerCreateInfo();
        create_info.pNext = &debug_create_info;
    }

    vk_result = vkCreateInstance(&create_info, nullptr, &impl_->instance);
    if (vk_result != VK_SUCCESS) {
        impl_->instance = VK_NULL_HANDLE;
        return Result::Failure(
            ResultType::InitializationFailed,
            "vkCreateInstance failed with VkResult " +
                std::to_string(static_cast<int>(vk_result)));
    }

    impl_->api_version = api_version;

    return Result::Ok();
}

Result VulkanDriver::SetupDebugMessenger()
{
    if (!impl_->debug_utils_enabled) {
        return Result::Ok();
    }

    const auto create_debug_messenger =
        reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(impl_->instance,
                                  "vkCreateDebugUtilsMessengerEXT"));
    if (create_debug_messenger == nullptr) {
        SPDLOG_WARN("vkCreateDebugUtilsMessengerEXT is unavailable");
        impl_->debug_utils_enabled = false;
        return Result::Ok();
    }

    const VkDebugUtilsMessengerCreateInfoEXT create_info =
        MakeDebugMessengerCreateInfo();
    const VkResult vk_result = create_debug_messenger(
        impl_->instance, &create_info, nullptr, &impl_->debug_messenger);
    if (vk_result != VK_SUCCESS) {
        impl_->debug_messenger = VK_NULL_HANDLE;
        SPDLOG_WARN("Failed to create Vulkan debug messenger (VkResult {})",
                    static_cast<int>(vk_result));
    }

    return Result::Ok();
}

Result VulkanDriver::PickPhysicalDevice()
{
    uint32_t device_count = 0;
    VkResult vk_result =
        vkEnumeratePhysicalDevices(impl_->instance, &device_count, nullptr);
    if (vk_result != VK_SUCCESS) {
        return Result::Failure(
            ResultType::InitializationFailed,
            "Failed to enumerate Vulkan physical devices");
    }

    if (device_count == 0) {
        return Result::Failure(ResultType::Unsupported,
                               "No Vulkan physical device is available");
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    vk_result = vkEnumeratePhysicalDevices(
        impl_->instance, &device_count, devices.data());
    if (vk_result != VK_SUCCESS) {
        return Result::Failure(
            ResultType::InitializationFailed,
            "Failed to query Vulkan physical devices");
    }

    uint32_t best_score = 0;
    for (const VkPhysicalDevice device : devices) {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(device, &properties);

        if (properties.apiVersion < impl_->api_version) {
            continue;
        }

        uint32_t extension_count = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr,
                                             &extension_count, nullptr);
        std::vector<VkExtensionProperties> extensions(extension_count);
        if (extension_count > 0) {
            vkEnumerateDeviceExtensionProperties(
                device, nullptr, &extension_count, extensions.data());
        }

        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(
            device, &queue_family_count, nullptr);
        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        if (queue_family_count > 0) {
            vkGetPhysicalDeviceQueueFamilyProperties(
                device, &queue_family_count, queue_families.data());
        }

        uint32_t graphics_family = VK_QUEUE_FAMILY_IGNORED;
        for (uint32_t index = 0; index < queue_family_count; ++index) {
            if ((queue_families[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 &&
                queue_families[index].queueCount > 0) {
                graphics_family = index;
                break;
            }
        }

        if (graphics_family == VK_QUEUE_FAMILY_IGNORED) {
            continue;
        }

        bool extensions_supported = true;
        for (const std::string& extension : impl_->device_extensions) {
            if (!HasExtension(extensions, extension)) {
                extensions_supported = false;
                break;
            }
        }
        if (extensions_supported &&
            impl_->surface_type != SurfaceType::NONE &&
            !HasExtension(extensions,
                                VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
            extensions_supported = false;
        }
        if (!extensions_supported) {
            continue;
        }

        const uint32_t score = DeviceScore(properties.deviceType);
        SPDLOG_DEBUG("Vulkan device: {} ({}), graphics queue family {}",
                     properties.deviceName,
                     DeviceTypeName(properties.deviceType),
                     graphics_family);

        if (impl_->physical_device == VK_NULL_HANDLE || score > best_score) {
            impl_->physical_device = device;
            impl_->graphics_queue_family = graphics_family;
            best_score = score;
        }
    }

    if (impl_->physical_device == VK_NULL_HANDLE) {
        return Result::Failure(
            ResultType::Unsupported,
            "No Vulkan physical device satisfies the requested features");
    }

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(impl_->physical_device, &properties);
    SPDLOG_INFO("Using Vulkan device: {} ({}), graphics queue family {}",
                properties.deviceName,
                DeviceTypeName(properties.deviceType),
                impl_->graphics_queue_family);

    return Result::Ok();
}

Result VulkanDriver::CreateLogicalDevice()
{
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(
        impl_->physical_device, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    if (queue_family_count > 0) {
        vkGetPhysicalDeviceQueueFamilyProperties(
            impl_->physical_device, &queue_family_count, queue_families.data());
    }

    std::vector<float> queue_priorities(queue_family_count, 1.0f);
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    queue_create_infos.reserve(queue_family_count);
    for (uint32_t index = 0; index < queue_family_count; ++index) {
        if (queue_families[index].queueCount == 0) {
            continue;
        }

        VkDeviceQueueCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        create_info.queueFamilyIndex = index;
        create_info.queueCount = 1;
        create_info.pQueuePriorities = &queue_priorities[index];
        queue_create_infos.push_back(create_info);
    }

    // Device Extensions
    std::vector<const char*> enabled_extensions;
    enabled_extensions.reserve(impl_->device_extensions.size() + 1);
    uint32_t available_extension_count = 0;
    vkEnumerateDeviceExtensionProperties(impl_->physical_device, nullptr,
                                         &available_extension_count, nullptr);
    std::vector<VkExtensionProperties> available_extensions(
        available_extension_count);
    if (available_extension_count > 0) {
        vkEnumerateDeviceExtensionProperties(
            impl_->physical_device, nullptr, &available_extension_count,
            available_extensions.data());
    }

    auto ResolveDeviceExtensions = [this](
        const std::vector<VkExtensionProperties>& available,
        std::vector<const char*>& extensions)
    {
        if (impl_->surface_type != SurfaceType::NONE) {
            extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        }
        
        return Result::Ok();
    };
    if (Result res = ResolveDeviceExtensions(
            available_extensions, enabled_extensions); !res) {
        return res;
    }

    // Create Device
    VkPhysicalDeviceFeatures enabled_features{};
    VkDeviceCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount =
            static_cast<uint32_t>(queue_create_infos.size()),
        .pQueueCreateInfos = queue_create_infos.data(),
        .enabledExtensionCount =
            static_cast<uint32_t>(enabled_extensions.size()),
        .ppEnabledExtensionNames = enabled_extensions.data(),
        .pEnabledFeatures = &enabled_features
    };

    const VkResult vk_result =
        vkCreateDevice(impl_->physical_device, &create_info, nullptr,
                       &impl_->device);
    if (vk_result != VK_SUCCESS) {
        impl_->device = VK_NULL_HANDLE;
        return Result::Failure(
            ResultType::InitializationFailed,
            "vkCreateDevice failed with VkResult " +
                std::to_string(static_cast<int>(vk_result)));
    }

    // Get Device Queue
    impl_->queues.clear();
    impl_->queues.reserve(queue_create_infos.size());
    for (const VkDeviceQueueCreateInfo& queue_create_info : queue_create_infos) {
        Impl::QueueInfo info;
        info.family_index = queue_create_info.queueFamilyIndex;
        info.flags = queue_families[info.family_index].queueFlags;
        vkGetDeviceQueue(impl_->device, info.family_index, 0, &info.queue);
        impl_->queues.push_back(info);

        if (info.family_index == impl_->graphics_queue_family) {
            impl_->graphics_queue = info.queue;
        }
    }

    if (impl_->graphics_queue == VK_NULL_HANDLE) {
        return Result::Failure(
            ResultType::InitializationFailed,
            "Failed to retrieve the Vulkan graphics queue");
    }

    return Result::Ok();
}

CommandList* VulkanDriver::CreateCommandList()
{
    return nullptr;
}

Swapchain* VulkanDriver::CreateSwapchain(const NativeSurfaceDesc& desc) 
{
    impl_->swapchain = 
        std::make_unique<VulkanSwapchain>(
            desc, impl_->instance, impl_->device);
    
    // Surface Format 
    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        impl_->physical_device, impl_->swapchain->Surface(), 
        &format_count, nullptr);
    
    std::vector<VkSurfaceFormatKHR> available_formats;
    if (format_count != 0) {
        available_formats.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            impl_->physical_device, impl_->swapchain->Surface(), 
            &format_count, available_formats.data());        
    }

    auto choose_surface_format = 
        [](const std::vector<VkSurfaceFormatKHR> &available_formats) {
        if (available_formats.size() == 1 && 
            available_formats[0].format == VK_FORMAT_UNDEFINED) {
            return VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
        }

        for (const auto& available_format : available_formats) {
            if (available_format.format == VK_FORMAT_B8G8R8A8_UNORM && 
                available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR){
                return available_format;
            }
        }

        return available_formats[0];
    };


    // Present Mode
    uint32_t present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        impl_->physical_device, impl_->swapchain->Surface(),
        &present_mode_count, nullptr);
    
    std::vector<VkPresentModeKHR> available_present_modes;
    if (present_mode_count != 0) {
        available_present_modes.resize(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            impl_->physical_device, impl_->swapchain->Surface(),
            &present_mode_count, available_present_modes.data());        
    }
 
    auto choose_present_mode = 
        [](const std::vector<VkPresentModeKHR> available_present_modes) {
        VkPresentModeKHR best_mode = VK_PRESENT_MODE_FIFO_KHR;

        for (const auto& available_present_mode : available_present_modes) {
            if (available_present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                return available_present_mode;
            } else if (available_present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                best_mode = available_present_mode;
            }
        }

        return best_mode;
    };
    
    // Surface Capabilities
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        impl_->physical_device, impl_->swapchain->Surface(), &capabilities);

    // Queue
    VkBool32 present_support = VK_FALSE;
    for (const auto& queue_info : impl_->queues) {
        if (vkGetPhysicalDeviceSurfaceSupportKHR(
            impl_->physical_device, queue_info.family_index, 
            impl_->swapchain->Surface(), &present_support); 
            present_support == VK_TRUE) {
            
            impl_->present_queue_family = queue_info.family_index;
            break;        
        }
    }

    if (!impl_->swapchain->Init(
            capabilities,
            choose_surface_format(available_formats),
            choose_present_mode(available_present_modes))) {
        impl_->swapchain.reset();
        return nullptr;
    }

    return impl_->swapchain.get();
}


} // namespace hotaru
