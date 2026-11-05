#include "renderer.h"

#include "backend/command_list.h"
#include "backend/driver.h"
#include "backend/pipeline.h"
#include "backend/program.h"
#include "backend/swapchain.h"
#include "view.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <fstream>
#include <string_view>
#include <vector>

namespace hotaru {

namespace {

std::vector<uint8_t> ReadShaderFile(const char* path)
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

} // namespace

struct RendererImpl::Impl
{
    Driver* driver = nullptr;
    // Declaration order is destruction order in reverse. Keep the command
    // list alive until the pipeline and program have been released.
    std::unique_ptr<CommandList> command_list;
    std::unique_ptr<Pipeline> pipeline;
    std::unique_ptr<Program> program;
    Swapchain* swapchain = nullptr;
    SwapchainFrame frame{};
    bool frame_active = false;
};

RendererImpl::RendererImpl(Driver* driver, ColorFormat /*color_format*/)
    : impl_(std::make_unique<Impl>())
{
    impl_->driver = driver;
    if (impl_->driver == nullptr) {
        return;
    }

    auto command_list = impl_->driver->CreateCommandList();
    if (!command_list) {
        SPDLOG_ERROR("Failed to create the render command list: {}",
                     command_list.error().message);
        return;
    }
    impl_->command_list = std::move(*command_list);
}

RendererImpl::~RendererImpl() = default;

bool RendererImpl::EnsurePipeline(const ColorFormat color_format)
{
    if (impl_->pipeline != nullptr) {
        return true;
    }
    if (impl_->driver == nullptr) {
        return false;
    }

    auto program = impl_->driver->CreateProgram();
    if (!program) {
        SPDLOG_ERROR("Failed to create the render program: {}",
                     program.error().message);
        return false;
    }

    const std::vector<uint8_t> vertex_shader =
        ReadShaderFile(HOTARU_DEFAULT_VERTEX_SHADER);
    const std::vector<uint8_t> fragment_shader =
        ReadShaderFile(HOTARU_DEFAULT_FRAGMENT_SHADER);
    if (vertex_shader.empty() || fragment_shader.empty()) {
        SPDLOG_ERROR(
            "Failed to load SPIR-V shaders ({} and {})",
            HOTARU_DEFAULT_VERTEX_SHADER,
            HOTARU_DEFAULT_FRAGMENT_SHADER);
        return false;
    }

    Status status = program->get()->SetShader(
        ShaderStage::VERTEX, vertex_shader);
    if (!status) {
        SPDLOG_ERROR("Failed to set the vertex shader: {}",
                     status.error().message);
        return false;
    }
    status = program->get()->SetShader(
        ShaderStage::FRAGMENT, fragment_shader);
    if (!status) {
        SPDLOG_ERROR("Failed to set the fragment shader: {}",
                     status.error().message);
        return false;
    }

    PipelineDesc desc{
        .kind = PipelineKind::Graphics,
        .program = program->get(),
        .color_format = color_format,
    };
    auto pipeline = impl_->driver->CreatePipeline(desc);
    if (!pipeline) {
        SPDLOG_ERROR("Failed to create the render pipeline: {}",
                     pipeline.error().message);
        return false;
    }

    impl_->program = std::move(*program);
    impl_->pipeline = std::move(*pipeline);
    return true;
}

bool RendererImpl::BeginFrame(View* view)
{
    if (view == nullptr || impl_->command_list == nullptr ||
        impl_->frame_active) {
        return false;
    }

    auto* view_impl = dynamic_cast<ViewImpl*>(view);
    if (view_impl == nullptr || view_impl->GetSwapchain() == nullptr) {
        return false;
    }
    impl_->swapchain = view_impl->GetSwapchain();

    if (!EnsurePipeline(impl_->swapchain->GetColorFormat())) {
        return false;
    }

    auto frame = impl_->swapchain->AcquireNextImage();
    if (!frame) {
        if (frame.error().code != ErrorCode::OutOfDate) {
            SPDLOG_ERROR("Failed to acquire a swapchain image: {}",
                         frame.error().message);
        }
        return false;
    }
    impl_->frame = *frame;

    Status status = impl_->command_list->Begin();
    if (!status) {
        SPDLOG_ERROR("Failed to begin the command list: {}",
                     status.error().message);
        impl_->swapchain->WaitIdle();
        impl_->swapchain->AbandonFrame();
        return false;
    }
    status = impl_->command_list->BeginRendering(impl_->frame.render_target);
    if (!status) {
        SPDLOG_ERROR("Failed to begin rendering: {}",
                     status.error().message);
        impl_->command_list->End();
        impl_->swapchain->WaitIdle();
        impl_->swapchain->AbandonFrame();
        return false;
    }

    impl_->frame_active = true;
    return true;
}

void RendererImpl::Render()
{
    if (!impl_->frame_active || impl_->pipeline == nullptr) {
        return;
    }

    Status status = impl_->command_list->BindPipeline(
        impl_->pipeline.get());
    if (!status) {
        SPDLOG_ERROR("Failed to bind the pipeline: {}",
                     status.error().message);
        return;
    }

    status = impl_->command_list->Draw(3);
    if (!status) {
        SPDLOG_ERROR("Failed to draw the triangle: {}",
                     status.error().message);
    }
}

void RendererImpl::EndFrame()
{
    if (!impl_->frame_active) {
        return;
    }
    impl_->frame_active = false;

    Status status = impl_->command_list->EndRendering();
    if (!status) {
        SPDLOG_ERROR("Failed to end rendering: {}",
                     status.error().message);
        return;
    }
    status = impl_->command_list->End();
    if (!status) {
        SPDLOG_ERROR("Failed to end the command list: {}",
                     status.error().message);
        return;
    }
    status = impl_->swapchain->Submit(*impl_->command_list);
    if (!status) {
        SPDLOG_ERROR("Failed to submit the frame: {}",
                     status.error().message);
        return;
    }
    status = impl_->swapchain->Present();
    if (!status && status.error().code != ErrorCode::OutOfDate) {
        SPDLOG_ERROR("Failed to present the frame: {}",
                     status.error().message);
    }
}

}
