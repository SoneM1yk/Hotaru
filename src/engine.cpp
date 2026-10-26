#include "backend/driver.h"
#include "backend/swapchain.h"
#include "view.h"

#include <hotaru/config.h>
#include <hotaru/engine.h>
#include <hotaru/view.h>

#include <spdlog/spdlog.h>

#include <memory>
#include <utility>

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

    auto driver = Driver::Create(DriverConfig{
        .backend = config.backend,
        .surface_type = config.surface_type,
    });
    if (!driver) {
        SPDLOG_ERROR("Failed to create render driver: {}",
                     driver.error().message);
        return nullptr;
    }
    impl->driver = std::move(*driver);

    return engine;
}

View* Engine::CreateView(const NativeSurfaceDesc& desc)
{
    auto swapchain = impl_->driver->CreateSwapchain(desc);
    if (!swapchain) {
        SPDLOG_ERROR("Failed to create swapchain: {}",
                     swapchain.error().message);
        return nullptr;
    }
    impl_->views.push_back(
        std::make_unique<ViewImpl>(std::move(*swapchain)));

    return impl_->views.back().get();
}

} // namespace hotaru
