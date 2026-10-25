#include "view.h"

#include "backend/swapchain.h"

namespace hotaru {

struct ViewImpl::Impl
{
    Swapchain* swapchain;
};

ViewImpl::ViewImpl(Swapchain* swapchain) :
    impl_(std::make_unique<Impl>()) 
{
    impl_->swapchain = swapchain;
}

ViewImpl::~ViewImpl()
{}

Camera* ViewImpl::GetCamera() const 
{
    return nullptr;
}

void ViewImpl::Resize(const Extent& ext)
{

}

}