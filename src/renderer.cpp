#include "renderer.h"

#include <hotaru/shaders/default_vert.h>
#include <hotaru/shaders/default_frag.h>

#include "view.h"
#include "backend/driver.h"
#include "backend/pipeline.h"

#include <spdlog/spdlog.h>

namespace hotaru {

struct RendererImpl::Impl
{
    Driver* driver = nullptr;
    std::unique_ptr<Pipeline> pipeline;
    std::unique_ptr<CommandList> command_list;
    std::unique_ptr<Program> program;

    Swapchain* swapchain = nullptr;
    Frame* frame = nullptr;
};

RendererImpl::RendererImpl():
    impl_(std::make_unique<Impl>())
{}

RendererImpl::~RendererImpl()
{}

Status RendererImpl::Init(Driver* driver)
{
    impl_->driver = driver;

    impl_->program = TRY(driver->CreateProgram());
    
    TRY_STATUS(impl_->program->
        SetShader(ShaderStage::VERTEX, shaders::DefaultVert()));
    TRY_STATUS(impl_->program->
        SetShader(ShaderStage::FRAGMENT, shaders::DefaultFrag()));

    impl_->command_list = TRY(driver->CreateCommandList());

    return {};
}

bool RendererImpl::BeginFrame(View* view)
{
    if (view == nullptr) {
        return false;
    }
    auto view_impl = static_cast<ViewImpl*>(view);
    impl_->swapchain = view_impl->GetSwapchain();
    if (auto res = impl_->swapchain->AcquireFrame(); res) {
        impl_->frame = res.value();
    }
    else {
        SPDLOG_ERROR("Failed to acquire frame: {}", res.error().message);
        return false;
    }

    if (impl_->pipeline == nullptr) {
        PipelineDesc desc {
            .program = impl_->program.get(),
            .color_format = impl_->frame->Desc().color_format,
        };
        auto pipeline = impl_->driver->CreatePipeline(desc);
        if (!pipeline) {
            SPDLOG_ERROR("Failed to create pipeline: {}",
                         pipeline.error().message);
            impl_->frame = nullptr;
            return false;
        }
        impl_->pipeline = std::move(pipeline.value());
    }

    impl_->command_list->Begin();
    return true;
}

void RendererImpl::Render()
{
    if (impl_->frame == nullptr || impl_->pipeline == nullptr) {
        return;
    }

    auto command_list = impl_->command_list.get();
    RenderPassDesc desc{};
    command_list->BeginRenderPass(impl_->frame, desc);
    command_list->BindPipeline(impl_->pipeline.get());
    command_list->Draw(3);
    command_list->EndRenderPass();
}

void RendererImpl::EndFrame()
{
    if (impl_->frame == nullptr) {
        return;
    }

    auto command_list = impl_->command_list.get();
    command_list->End();

    Status status = impl_->driver->Submit(command_list, *impl_->frame);
    if (!status) {
        SPDLOG_ERROR("Failed to submit frame: {}", status.error().message);
        impl_->frame = nullptr;
        return;
    }

    status = impl_->swapchain->Present(*impl_->frame);
    if (!status) {
        SPDLOG_ERROR("Failed to present frame: {}", status.error().message);
    }
    impl_->frame = nullptr;
}

}
