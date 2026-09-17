#pragma once

#include <memory>

namespace Hotaru{

class Engine
{
public:
    Engine();
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) noexcept;
    Engine& operator=(Engine&&) noexcept;

    void Init();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}
