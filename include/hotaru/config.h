#pragma once

#include <cstdint>
#include <variant>

namespace hotaru {

enum class RenderBackend
{
    VULKAN,
};

enum class SurfaceType
{
    NONE,
    WAYLAND,
    WIN32,
};

struct Extent 
{
    uint32_t width;
    uint32_t height;
};

struct WaylandSurfaceDesc
{
    Extent extent;
    void* surface;
    void* display;
};

using NativeSurfaceDesc = std::variant<
    WaylandSurfaceDesc>;

struct AppConfig 
{
    RenderBackend backend = RenderBackend::VULKAN;
    SurfaceType surface_type = SurfaceType::NONE;
};

} // namespace hotaru
