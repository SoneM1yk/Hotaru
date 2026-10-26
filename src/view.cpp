#include "view.h"

#include "backend/swapchain.h"

#include <hotaru/result.h>

#include <memory>
#include <utility>

namespace hotaru {

struct ViewImpl::Impl
{
    explicit Impl(std::unique_ptr<Swapchain> swapchain)
        : swapchain(std::move(swapchain))
    {}

    std::unique_ptr<Swapchain> swapchain;
};

ViewImpl::ViewImpl(std::unique_ptr<Swapchain> swapchain)
    : impl_(std::make_unique<Impl>(std::move(swapchain)))
{}

ViewImpl::~ViewImpl() = default;

Camera* ViewImpl::GetCamera() const 
{
    return nullptr;
}

Status ViewImpl::Resize(const Extent& ext)
{
    return impl_->swapchain->Resize(ext);
}

} // namespace hotaru
