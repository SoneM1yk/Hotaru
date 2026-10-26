#include "driver.h"
#include "vulkan/vulkan_context.h"
#include "vulkan/vulkan_driver.h"

#include <hotaru/result.h>

#include <memory>
#include <utility>

namespace hotaru {

Result<std::unique_ptr<Driver>> Driver::Create(const DriverConfig& config)
{
    switch (config.backend) {
        case RenderBackend::VULKAN: {
            auto context = VulkanContext::Create(config.surface_type);
            if (!context) {
                return std::unexpected(std::move(context.error()));
            }

            std::unique_ptr<Driver> driver =
                std::make_unique<VulkanDriver>(std::move(*context));
            return driver;
        }
        default:
            return Failure(ErrorCode::Unsupported, "Unknown render backend");
    }
}

} // namespace hotaru
