#include "backend/pipeline.h"
#include "backend/vulkan/vulkan_context.h"
#include "backend/vulkan/vulkan_driver.h"
#include "backend/vulkan/vulkan_pipeline.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <vector>

namespace hotaru {
namespace {

class StubProgram final : public Program
{
public:
    Status SetShader(
        ShaderStage,
        std::span<const uint8_t>) override
    {
        return {};
    }
};

std::vector<uint8_t> ReadFile(const std::filesystem::path& path)
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

TEST(VulkanPipelineTest, RejectsNonVulkanProgram)
{
    auto context = VulkanContext::Create(SurfaceType::NONE);
    ASSERT_TRUE(context.has_value()) << context.error().message;

    VulkanDriver driver(*context);
    StubProgram program;
    PipelineDesc desc;
    desc.program = &program;
    desc.color_format = ColorFormat::BGRA8_UNORM;

    const auto pipeline = driver.CreatePipeline(desc);

    ASSERT_FALSE(pipeline.has_value());
    EXPECT_EQ(pipeline.error().code, ErrorCode::InvalidArgument);
}

TEST(VulkanPipelineTest, RejectsMissingColorFormat)
{
    auto context = VulkanContext::Create(SurfaceType::NONE);
    ASSERT_TRUE(context.has_value()) << context.error().message;

    VulkanDriver driver(*context);
    auto program = driver.CreateProgram();
    ASSERT_TRUE(program.has_value()) << program.error().message;

    PipelineDesc desc;
    desc.program = program->get();

    const auto pipeline = driver.CreatePipeline(desc);

    ASSERT_FALSE(pipeline.has_value());
    EXPECT_EQ(pipeline.error().code, ErrorCode::InvalidArgument);
}

TEST(VulkanPipelineTest, RejectsMissingProgram)
{
    auto context = VulkanContext::Create(SurfaceType::NONE);
    ASSERT_TRUE(context.has_value()) << context.error().message;

    VulkanDriver driver(*context);
    PipelineDesc desc;
    desc.color_format = ColorFormat::BGRA8_UNORM;

    const auto pipeline = driver.CreatePipeline(desc);

    ASSERT_FALSE(pipeline.has_value());
    EXPECT_EQ(pipeline.error().code, ErrorCode::InvalidArgument);
}

#ifdef HOTARU_TEST_SHADER_DIR
TEST(VulkanPipelineTest, CreatesGraphicsPipeline)
{
    auto context = VulkanContext::Create(SurfaceType::NONE);
    ASSERT_TRUE(context.has_value()) << context.error().message;

    VulkanDriver driver(*context);
    auto program = driver.CreateProgram();
    ASSERT_TRUE(program.has_value()) << program.error().message;

    const std::filesystem::path shader_dir(HOTARU_TEST_SHADER_DIR);
    const std::vector<uint8_t> vertex_shader =
        ReadFile(shader_dir / "default.vert.spv");
    const std::vector<uint8_t> fragment_shader =
        ReadFile(shader_dir / "default.frag.spv");
    ASSERT_FALSE(vertex_shader.empty());
    ASSERT_FALSE(fragment_shader.empty());

    Status status = program->get()->SetShader(
        ShaderStage::VERTEX, vertex_shader);
    ASSERT_TRUE(status) << status.error().message;
    status = program->get()->SetShader(
        ShaderStage::FRAGMENT, fragment_shader);
    ASSERT_TRUE(status) << status.error().message;

    PipelineDesc desc;
    desc.program = program->get();
    desc.color_format = ColorFormat::BGRA8_UNORM;

    const auto pipeline = driver.CreatePipeline(desc);

    ASSERT_TRUE(pipeline.has_value()) << pipeline.error().message;
    EXPECT_EQ((*pipeline)->Backend(), RenderBackend::VULKAN);
    EXPECT_EQ((*pipeline)->Kind(), PipelineKind::Graphics);

    const auto* vulkan_pipeline =
        dynamic_cast<const VulkanPipeline*>(pipeline->get());
    ASSERT_NE(vulkan_pipeline, nullptr);
    EXPECT_NE(vulkan_pipeline->NativeHandle(), VK_NULL_HANDLE);
}
#endif

} // namespace
} // namespace hotaru
