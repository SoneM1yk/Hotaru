#include <hotaru/engine.h>
#include <memory>

#include "backend/driver.h"
#include "backend/platforms/platform.h"
#include "backend/swap_chain.h"

namespace Hotaru {

struct Engine::Impl
{
    std::unique_ptr<Platform> platform;
    Driver* driver;
    SwapChain* swapChain;
};

Engine::Engine()
    : impl_(std::make_unique<Impl>())
{
    impl_->platform = Platform::Create(PlatformConfig{});
}

Engine::~Engine()
{}

Engine::Engine(Engine&&) noexcept = default;

Engine& Engine::operator=(Engine&&) noexcept = default;

void Engine::Init()
{
    impl_->driver = impl_->platform->CreateDriver();
}

}
