#include "vulkan_pipeline.h"

namespace hotaru {

struct VulkanPipeline::Impl
{

};

VulkanPipeline::VulkanPipeline():
    impl_(std::make_unique<Impl>())
{

}

VulkanPipeline::~VulkanPipeline()
{}



}