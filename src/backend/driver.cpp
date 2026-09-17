#include "driver.h"
#include "vulkan/vulkan_driver.h"

#include <memory>
#include <spdlog/spdlog.h>

namespace hotaru {

std::unique_ptr<Driver> Driver::Create(const DriverConfig& config)
{
    switch (config.backend) {
        case RenderBackend::VULKAN:
        {
            auto vulkan_driver = std::make_unique<VulkanDriver>(config);
            Result result = vulkan_driver->Init();
            if (!result) {
                SPDLOG_ERROR("Failed to initialize Vulkan backend: {}",
                             result.Message());
                return nullptr;
            }
            return vulkan_driver;
        }
        default:
            SPDLOG_ERROR("Unknown render backend: {}",
                         static_cast<int>(config.backend));
            return nullptr;
    }
}

} // namespace hotaru
