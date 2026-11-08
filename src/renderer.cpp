#include "renderer.h"

#include <hotaru/shaders/default_vert.h>
#include <hotaru/shaders/default_frag.h>

#include "view.h"
#include "backend/command_list.h"
#include "backend/frame.h"
#include "backend/driver.h"
#include "backend/pipeline.h"
#include "backend/program.h"
#include "backend/swapchain.h"

#include <spdlog/spdlog.h>

#include <optional>

namespace hotaru {

RendererImpl::RendererImpl() = default;

RendererImpl::~RendererImpl()
{}

Status RendererImpl::Init(Driver* driver)
{
    driver_ = driver;

    program_ = TRY(driver->CreateProgram());
    
    TRY_STATUS(program_->
        SetShader(ShaderStage::VERTEX, shaders::DefaultVert()));
    TRY_STATUS(program_->
        SetShader(ShaderStage::FRAGMENT, shaders::DefaultFrag()));

    command_list_ = TRY(driver->CreateCommandList());

    return {};
}

bool RendererImpl::BeginFrame(View* view)
{
    if (view == nullptr) {
        return false;
    }
    auto view_impl = static_cast<ViewImpl*>(view);
    swapchain_ = view_impl->GetSwapchain();
    if (auto res = swapchain_->AcquireFrame(); res) {
        frame_ = std::move(res.value());
    }
    else {
        SPDLOG_ERROR("Failed to acquire frame: {}", res.error().message);
        return false;
    }

    if (pipeline_ == nullptr) {
        PipelineDesc desc {
            .program = program_.get(),
            .color_format = frame_->desc.color_format,
        };
        auto pipeline = driver_->CreatePipeline(desc);
        if (!pipeline) {
            SPDLOG_ERROR("Failed to create pipeline: {}",
                         pipeline.error().message);
            frame_.reset();
            return false;
        }
        pipeline_ = std::move(pipeline.value());
    }

    command_list_->Begin();
    return true;
}

void RendererImpl::Render()
{
    if (!frame_.has_value() || pipeline_ == nullptr) {
        return;
    }

    auto command_list = command_list_.get();
    RenderPassDesc desc{};
    command_list->BeginRenderPass(*frame_, desc);
    command_list->BindPipeline(pipeline_.get());
    command_list->Draw(3);
    command_list->EndRenderPass();
}

void RendererImpl::EndFrame()
{
    if (!frame_.has_value()) {
        return;
    }

    auto command_list = command_list_.get();
    command_list->End();

    Status status = driver_->Submit(command_list, *frame_);
    if (!status) {
        SPDLOG_ERROR("Failed to submit frame: {}", status.error().message);
        frame_.reset();
        return;
    }

    status = swapchain_->Present(*frame_);
    if (!status) {
        SPDLOG_ERROR("Failed to present frame: {}", status.error().message);
    }
    frame_.reset();
}

}
