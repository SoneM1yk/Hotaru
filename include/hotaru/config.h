#pragma once

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

struct WaylandSurfaceDesc
{
    void* surface;
    void* display;
};

union NativeSurfaceDesc
{
    WaylandSurfaceDesc wayland;
};

struct AppConfig 
{
    RenderBackend backend = RenderBackend::VULKAN;
    SurfaceType surface_type = SurfaceType::NONE;
};

} // namespace hotaru
