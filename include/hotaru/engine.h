#pragma once

#include <hotaru/config.h>
#include <memory>

namespace hotaru{

class Scene;
class View;
class Renderer;

class Engine
{
public:
    ~Engine();
    Engine(Engine&&) noexcept = default;
    Engine& operator=(Engine&&) noexcept = default;
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    static std::unique_ptr<Engine> Create(const AppConfig& config);

    Scene* CreateScene();
    View* CreateView(const NativeSurfaceDesc& desc);
    Renderer* CreateRenderer();

private:
    Engine();

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}
