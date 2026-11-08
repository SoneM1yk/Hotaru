#include "view.h"

#include "backend/swapchain.h"

#include <hotaru/result.h>

#include <memory>
#include <utility>

namespace hotaru {

ViewImpl::ViewImpl(std::unique_ptr<Swapchain> swapchain)
    : swapchain_(std::move(swapchain))
{}

ViewImpl::~ViewImpl() = default;

Camera* ViewImpl::GetCamera() const 
{
    return nullptr;
}

Swapchain* ViewImpl::GetSwapchain() 
{
    return swapchain_.get();
}

Status ViewImpl::Resize(const Extent& ext)
{
    return swapchain_->Resize(ext);
}

} // namespace hotaru
