#include <hotaru/engine.h>
#include <memory>

#include "backend/vulkan_backend.h"

namespace Hotaru {

struct Engine::Impl
{
    std::unique_ptr<VulkanBackend> backend;
};

Engine::Engine()
    : impl_(std::make_unique<Impl>())
{
    impl_->backend = std::make_unique<VulkanBackend>();
}

Engine::~Engine()
{}

Engine::Engine(Engine&&) noexcept = default;

Engine& Engine::operator=(Engine&&) noexcept = default;

void Engine::Init()
{
    impl_->backend->Init();
}

}
