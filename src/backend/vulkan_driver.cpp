#include "vulkan_driver.h"
#include "swap_chain.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_wayland.h>

#include <spdlog/spdlog.h>

namespace Hotaru
{

#pragma region VulkanSwapChain

class VulkanSwapChain : public SwapChain
{
public:
    VulkanSwapChain(VkInstance instance): 
        instance_(instance)
    {}

    void Init(void* native_handle, void* native_display) 
    {
        VkWaylandSurfaceCreateInfoKHR create_info {
            .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
            .display = (wl_display*)native_display,
            .surface = (wl_surface*)native_handle,
        };

        if (VkResult res = vkCreateWaylandSurfaceKHR(instance_, &create_info, nullptr, &surface_);
            res != VK_SUCCESS) {
            SPDLOG_ERROR("Create Wayland Surface Failed: {}!", res);
        }
    }

    void QuerySwapChainSupport(VkPhysicalDevice device) 
    {
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &capabilities_);

        // Format 
        uint32_t format_count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &format_count, nullptr);
        formats_.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &format_count, formats_.data());

        

    }

    virtual ~VulkanSwapChain()
    {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
    }

private:
    VkInstance instance_;
    VkSurfaceKHR surface_;
    VkSurfaceCapabilitiesKHR capabilities_;
    std::vector<VkSurfaceFormatKHR> formats_;
    std::vector<VkPresentModeKHR> presentModes_; 
};

#pragma region end


struct VulkanDriver::Impl
{
    std::vector<const char*> instExtensions;
    std::vector<const char*> deviceExtensions;
    std::vector<const char*> layers;

    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    uint32_t queueFamilyIndex = UINT32_MAX;

    VkDevice device;
    VkQueue graphicsQueue;

    std::unique_ptr<VulkanSwapChain> swapChain;
};

VulkanDriver::VulkanDriver(std::vector<const char*> extensions) :
    impl_(std::make_unique<Impl>())
{
    impl_->instExtensions = std::move(extensions);
    impl_->deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    impl_->layers.push_back("VK_LAYER_KHRONOS_validation");
}

VulkanDriver::~VulkanDriver()
{
    impl_->swapChain.reset();
    vkDestroyDevice(impl_->device, nullptr);
    vkDestroyInstance(impl_->instance, nullptr);
}

void VulkanDriver::Init()
{
    CreateInstance();
    PickPhysicalDevice();
    CreateLogicalDevice();
}

void VulkanDriver::CreateInstance()
{
    // Available Extensions
    uint32_t extension_count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
    std::vector<VkExtensionProperties> extensions{extension_count};
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extensions.data());
    for (auto it = impl_->instExtensions.begin(); it != impl_->instExtensions.end();) {
        const char* ext = *it;
        bool found = std::any_of(extensions.begin(), extensions.end(), 
            [ext](const VkExtensionProperties& available) {
                return std::strcmp(ext, available.extensionName) == 0;
            }
        );

        if (!found) {
            SPDLOG_ERROR("Extension not found: {} !", ext);
            return;
        }
    }

    // Available Layers
    uint32_t layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    std::vector<VkLayerProperties> available_layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());
    for (auto it = impl_->layers.begin(); it != impl_->layers.end();) {
        const char* layer = *it;
        bool found = std::any_of(available_layers.begin(), available_layers.end(), 
            [layer](const VkLayerProperties& available) {
                return std::strcmp(layer, available.layerName) == 0;
            }
        );
        if (!found) {
            SPDLOG_ERROR("Layer not found: {} !", layer);
            return;
        } 
    }
 
    // TODO: Check available before use 
    // Create Instance
    VkApplicationInfo app_info{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "HotaruRender",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "HotaruEngine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3,
    };

    VkInstanceCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,

        // Enable Layers
        .enabledLayerCount = (uint32_t)impl_->layers.size(),
        .ppEnabledLayerNames = impl_->layers.data(),
        
        .enabledExtensionCount = (uint32_t)impl_->instExtensions.size(),
        .ppEnabledExtensionNames = impl_->instExtensions.data()
    };

    VkResult res = vkCreateInstance(&create_info, nullptr, &impl_->instance);
    if (res != VK_SUCCESS) {
        SPDLOG_ERROR("Create Instance Failed: {}!", res);
        return;
    }
}

void VulkanDriver::PickPhysicalDevice()
{
    // Choose Physical Device
    impl_->physicalDevice = VK_NULL_HANDLE;
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(impl_->instance, &device_count, nullptr);
    if (device_count == 0) {
        SPDLOG_ERROR("No Physical Device! ");
        return;
    }
    std::vector<VkPhysicalDevice> devices{device_count};
    vkEnumeratePhysicalDevices(impl_->instance, &device_count, devices.data());

    impl_->physicalDevice = devices.front();

    // Choose Queue Family
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(impl_->physicalDevice, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(impl_->physicalDevice, &queue_family_count, queue_families.data());
    for (uint32_t index = 0; index < queue_family_count; index++) {
        if (queue_families[index].queueCount > 0 &&
            queue_families[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            impl_->queueFamilyIndex = index;
            break;
        }
    }

}

void VulkanDriver::CreateLogicalDevice()
{
    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = impl_->queueFamilyIndex,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority
    };

    VkPhysicalDeviceFeatures physical_device_features = {};

    
    VkDeviceCreateInfo device_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_create_info,

        .enabledExtensionCount = (uint32_t)impl_->deviceExtensions.size(),
        .ppEnabledExtensionNames = impl_->deviceExtensions.data(),

        .pEnabledFeatures = &physical_device_features,
    };
    if (vkCreateDevice(impl_->physicalDevice, &device_create_info, nullptr, &impl_->device) != VK_SUCCESS) {
        SPDLOG_ERROR("Create Device Failed!");
        return;
    }

    vkGetDeviceQueue(impl_->device, 0, 0, &impl_->graphicsQueue);
}

SwapChain* VulkanDriver::CreateSwapChain(void* native_handle, void* native_display)
{
    impl_->swapChain = std::make_unique<VulkanSwapChain>(impl_->instance);
    impl_->swapChain->Init(native_handle, native_display);
    return impl_->swapChain.get();
}


}