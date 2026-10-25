#include "backend/driver.h"
#include "backend/swapchain.h"
#include "view.h"

#include <hotaru/config.h>
#include <hotaru/view.h>
#include <hotaru/engine.h>

#include <spdlog/spdlog.h>

#include <memory>

namespace hotaru {

struct Engine::Impl
{
    std::unique_ptr<Driver> driver;
    std::vector<std::unique_ptr<ViewImpl>> views;
};

Engine::Engine()
    : impl_(std::make_unique<Impl>())
{}

Engine::~Engine()
{}

std::unique_ptr<Engine> Engine::Create(const AppConfig& config)
{
    auto engine = std::unique_ptr<Engine>(new Engine);
    auto& impl = engine->impl_;

    impl->driver = Driver::Create(DriverConfig{
        .backend = config.backend,
        .surface_type = config.surface_type,
    });
    if (impl->driver == nullptr) {
        SPDLOG_ERROR("Failed to create render driver");
        return nullptr;
    }

    return engine;
}

View* Engine::CreateView(const NativeSurfaceDesc& desc) 
{
    Swapchain* swapchain = impl_->driver->CreateSwapchain(desc);
    if (swapchain == nullptr) {
        return nullptr;
    }
    impl_->views.push_back(std::make_unique<ViewImpl>(swapchain));

    return impl_->views.back().get();
}

} // namespace hotaru
