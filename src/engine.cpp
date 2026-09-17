#include "backend/driver.h"
#include <hotaru/engine.h>
#include <memory>
#include <spdlog/spdlog.h>

namespace hotaru {

struct Engine::Impl
{
    std::unique_ptr<Driver> driver;
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

} // namespace hotaru
