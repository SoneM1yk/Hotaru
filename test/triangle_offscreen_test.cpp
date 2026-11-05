#include "backend/command_list.h"
#include "backend/pipeline.h"
#include "backend/program.h"
#include "backend/vulkan/vulkan_command_list.h"
#include "backend/vulkan/vulkan_context.h"
#include "backend/vulkan/vulkan_driver.h"
#include "backend/vulkan/vulkan_render_target.h"

#include <hotaru/config.h>
#include <hotaru/result.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

#include <volk.h>

namespace hotaru {
namespace {

constexpr uint32_t kWidth = 64;
constexpr uint32_t kHeight = 64;

std::vector<uint8_t> ReadShader(const char* path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return {};
    }
    const std::streamsize size = file.tellg();
    if (size <= 0) {
        return {};
    }
    std::vector<uint8_t> data(static_cast<size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(data.data()), size);
    return file ? data : std::vector<uint8_t>{};
}

uint32_t FindMemoryType(
    const VkPhysicalDevice physical_device,
    const uint32_t type_bits,
    const VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memory_properties{};
    vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);
    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
        if ((type_bits & (1u << i)) != 0 &&
            (memory_properties.memoryTypes[i].propertyFlags & properties) ==
                properties) {
            return i;
        }
    }
    return UINT32_MAX;
}

TEST(TriangleOffscreenTest, RendersRedTriangle)
{
#ifdef HOTARU_TEST_SHADER_DIR
    auto context = VulkanContext::Create(SurfaceType::NONE);
    ASSERT_TRUE(context.has_value()) << context.error().message;
    const VkDevice device = (*context)->Device();
    VulkanDriver driver(*context);

    auto program = driver.CreateProgram();
    ASSERT_TRUE(program.has_value()) << program.error().message;
    const auto vertex_spv =
        ReadShader(HOTARU_TEST_SHADER_DIR "/default.vert.spv");
    const auto fragment_spv =
        ReadShader(HOTARU_TEST_SHADER_DIR "/default.frag.spv");
    ASSERT_FALSE(vertex_spv.empty());
    ASSERT_FALSE(fragment_spv.empty());
    ASSERT_TRUE(program->get()->SetShader(ShaderStage::VERTEX, vertex_spv));
    ASSERT_TRUE(program->get()->SetShader(ShaderStage::FRAGMENT, fragment_spv));

    PipelineDesc desc;
    desc.program = program->get();
    desc.color_format = ColorFormat::RGBA8_UNORM;
    auto pipeline = driver.CreatePipeline(desc);
    ASSERT_TRUE(pipeline.has_value()) << pipeline.error().message;

    VkImageCreateInfo image_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = {kWidth, kHeight, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                 VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VkImage image = VK_NULL_HANDLE;
    ASSERT_EQ(vkCreateImage(device, &image_info, nullptr, &image), VK_SUCCESS);

    VkMemoryRequirements image_requirements{};
    vkGetImageMemoryRequirements(device, image, &image_requirements);
    const uint32_t image_memory_type = FindMemoryType(
        (*context)->PhysicalDevice(), image_requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    ASSERT_NE(image_memory_type, UINT32_MAX);

    VkMemoryAllocateInfo image_allocation{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = image_requirements.size,
        .memoryTypeIndex = image_memory_type,
    };
    VkDeviceMemory image_memory = VK_NULL_HANDLE;
    ASSERT_EQ(vkAllocateMemory(
        device, &image_allocation, nullptr, &image_memory), VK_SUCCESS);
    ASSERT_EQ(vkBindImageMemory(device, image, image_memory, 0), VK_SUCCESS);

    VkImageViewCreateInfo view_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .levelCount = 1,
            .layerCount = 1,
        },
    };
    VkImageView image_view = VK_NULL_HANDLE;
    ASSERT_EQ(vkCreateImageView(device, &view_info, nullptr, &image_view),
              VK_SUCCESS);

    const VkDeviceSize readback_size = kWidth * kHeight * 4;
    VkBufferCreateInfo buffer_info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = readback_size,
        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    VkBuffer readback_buffer = VK_NULL_HANDLE;
    ASSERT_EQ(vkCreateBuffer(device, &buffer_info, nullptr, &readback_buffer),
              VK_SUCCESS);

    VkMemoryRequirements buffer_requirements{};
    vkGetBufferMemoryRequirements(device, readback_buffer, &buffer_requirements);
    const uint32_t buffer_memory_type = FindMemoryType(
        (*context)->PhysicalDevice(), buffer_requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    ASSERT_NE(buffer_memory_type, UINT32_MAX);

    VkMemoryAllocateInfo buffer_allocation{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = buffer_requirements.size,
        .memoryTypeIndex = buffer_memory_type,
    };
    VkDeviceMemory readback_memory = VK_NULL_HANDLE;
    ASSERT_EQ(vkAllocateMemory(
        device, &buffer_allocation, nullptr, &readback_memory), VK_SUCCESS);
    ASSERT_EQ(vkBindBufferMemory(
        device, readback_buffer, readback_memory, 0), VK_SUCCESS);

    VulkanRenderTarget target{
        .image = image,
        .image_view = image_view,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .initial_layout = VK_IMAGE_LAYOUT_UNDEFINED,
        .final_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .extent = {kWidth, kHeight},
    };

    auto command_list = driver.CreateCommandList();
    ASSERT_TRUE(command_list.has_value()) << command_list.error().message;
    ASSERT_TRUE((*command_list)->Begin());
    ASSERT_TRUE((*command_list)->BeginRendering(RenderTarget{
        .extent = {kWidth, kHeight},
        .color_format = ColorFormat::RGBA8_UNORM,
        .native_handle = &target,
    }));
    ASSERT_TRUE((*command_list)->BindPipeline(pipeline->get()));
    ASSERT_TRUE((*command_list)->Draw(3));
    ASSERT_TRUE((*command_list)->EndRendering());
    ASSERT_TRUE((*command_list)->End());

    auto* vk_command_list =
        dynamic_cast<VulkanCommandList*>(command_list->get());
    ASSERT_NE(vk_command_list, nullptr);

    VkQueue queue = VK_NULL_HANDLE;
    for (const VulkanContext::QueueInfo& info : (*context)->Queues()) {
        if (info.family_index == (*context)->GraphicsQueueFamily()) {
            queue = info.queue;
            break;
        }
    }
    ASSERT_NE(queue, VK_NULL_HANDLE);

    VkCommandBuffer render_command_buffer =
        vk_command_list->NativeHandle();
    VkFenceCreateInfo fence_info{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    };
    VkFence readback_fence = VK_NULL_HANDLE;
    ASSERT_EQ(vkCreateFence(device, &fence_info, nullptr, &readback_fence),
              VK_SUCCESS);

    VkCommandPoolCreateInfo copy_pool_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = (*context)->GraphicsQueueFamily(),
    };
    VkCommandPool copy_pool = VK_NULL_HANDLE;
    ASSERT_EQ(vkCreateCommandPool(device, &copy_pool_info, nullptr, &copy_pool),
              VK_SUCCESS);
    VkCommandBufferAllocateInfo copy_alloc{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = copy_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer copy_command_buffer = VK_NULL_HANDLE;
    ASSERT_EQ(vkAllocateCommandBuffers(device, &copy_alloc, &copy_command_buffer),
              VK_SUCCESS);

    VkCommandBufferBeginInfo copy_begin{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    ASSERT_EQ(vkBeginCommandBuffer(copy_command_buffer, &copy_begin),
              VK_SUCCESS);

    VkImageMemoryBarrier to_transfer{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .levelCount = 1,
            .layerCount = 1,
        },
    };
    vkCmdPipelineBarrier(
        copy_command_buffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
        &to_transfer);

    VkBufferImageCopy copy_region{
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .layerCount = 1,
        },
        .imageExtent = {kWidth, kHeight, 1},
    };
    vkCmdCopyImageToBuffer(
        copy_command_buffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        readback_buffer, 1, &copy_region);
    ASSERT_EQ(vkEndCommandBuffer(copy_command_buffer), VK_SUCCESS);

    VkSubmitInfo copy_submit{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &copy_command_buffer,
    };
    const VkCommandBuffer submitted_buffers[] = {
        render_command_buffer, copy_command_buffer};
    VkSubmitInfo combined_submit{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 2,
        .pCommandBuffers = submitted_buffers,
    };
    ASSERT_EQ(vkQueueSubmit(queue, 1, &combined_submit, readback_fence),
              VK_SUCCESS);
    ASSERT_EQ(vkWaitForFences(
        device, 1, &readback_fence, VK_TRUE, UINT64_MAX), VK_SUCCESS);

    void* mapped = nullptr;
    ASSERT_EQ(vkMapMemory(device, readback_memory, 0, readback_size, 0, &mapped),
              VK_SUCCESS);
    std::vector<uint8_t> pixels(readback_size);
    std::memcpy(pixels.data(), mapped, static_cast<size_t>(readback_size));
    vkUnmapMemory(device, readback_memory);

    const size_t center = ((kHeight / 2) * kWidth + (kWidth / 2)) * 4;
    const size_t corner = 0;
    EXPECT_GT(pixels[corner + 0], 5) << "corner should contain clear color";
    EXPECT_GT(pixels[corner + 3], 200) << "corner should be opaque";
    EXPECT_GT(pixels[center + 0], 200) << "center pixel should be red";
    EXPECT_LT(pixels[center + 1], 40) << "center pixel should be red";
    EXPECT_LT(pixels[center + 2], 40) << "center pixel should be red";

    vkDestroyFence(device, readback_fence, nullptr);
    vkDestroyCommandPool(device, copy_pool, nullptr);
    vkDestroyBuffer(device, readback_buffer, nullptr);
    vkFreeMemory(device, readback_memory, nullptr);
    vkDestroyImageView(device, image_view, nullptr);
    vkDestroyImage(device, image, nullptr);
    vkFreeMemory(device, image_memory, nullptr);
#else
    GTEST_SKIP() << "SPIR-V shaders are not available";
#endif
}

} // namespace
} // namespace hotaru
