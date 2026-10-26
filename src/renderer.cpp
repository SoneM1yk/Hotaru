#include "renderer.h"

namespace hotaru {

struct RendererImpl::Impl
{

};

RendererImpl::RendererImpl():
    impl_(std::make_unique<Impl>())
{}

RendererImpl::~RendererImpl()
{}

bool RendererImpl::BeginFrame(View* view)
{
    
}

void RendererImpl::Render()
{

}

void RendererImpl::EndFrame()
{
    
}

}