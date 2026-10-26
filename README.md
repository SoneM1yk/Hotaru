<div align="center">
  <img src="assets/logo.jpg" alt="Hotaru - A real-time rendering engine" width="600">
</div>

<p align="center">
  A lightweight, cross-platform real-time rendering engine written in modern C++.
</p>

<p align="center">
  <img alt="C++23" src="https://img.shields.io/badge/C%2B%2B-23-blue">
  <img alt="Vulkan" src="https://img.shields.io/badge/API-Vulkan-9b4dca">
  <img alt="CMake" src="https://img.shields.io/badge/build-CMake-064f8c">
  <img alt="Status" src="https://img.shields.io/badge/status-work%20in%20progress-f4a261">
</p>

## Overview

**Hotaru** is a real-time rendering engine built around a small, backend-oriented
architecture. It is designed to keep the public engine API independent from the
underlying graphics API, starting with a Vulkan backend.

The project is currently under active development. The Vulkan device setup is in
place; surface creation, swapchain management, and the first render pass are the
next milestones.

## Features

- Modern C++23 codebase with a clear public/private header split
- Pluggable render backend through the `Driver` interface
- Vulkan instance, validation layer, and debug messenger setup
- Physical-device scoring and graphics queue selection
- Explicit `Result`-based initialization and clean resource teardown
- CMake build with optional AddressSanitizer support

## Current Status

| Area | Status |
| --- | --- |
| Engine API | Early skeleton |
| Vulkan initialization | Implemented |
| Surface and swapchain | Planned |
| Renderer and frame loop | Planned |
| Scene, view, and camera | Planned |

## Requirements

- A C++23 compiler
- CMake 3.21 or newer
- Vulkan SDK or Vulkan development headers
- `spdlog`
- `EnTT`
- GLFW, GTest, and GoogleTest for the test suite

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

To build without tests:

```bash
cmake -S . -B build -DBUILD_TEST=OFF
```

To enable AddressSanitizer:

```bash
cmake -S . -B build -DENABLE_ASAN=ON
```

## Usage

```cpp
#include <hotaru/config.h>
#include <hotaru/engine.h>

hotaru::AppConfig config;
config.backend = hotaru::RenderBackend::VULKAN;

std::unique_ptr<hotaru::Engine> engine = hotaru::Engine::Create(config);
if (!engine) {
    return 1;
}
```

Window and surface integration are not part of the public API yet.

## Project Layout

```text
include/hotaru/       Public engine headers
src/                  Engine and backend implementation
src/backend/          Backend abstraction and device drivers
src/backend/vulkan/   Vulkan implementation
test/                 Tests and rendering smoke tests
assets/               Branding and documentation assets
```

## Contributing

Hotaru is evolving quickly, so APIs may change while the rendering pipeline is
being built. Bug reports, design discussions, and focused pull requests are
welcome.

## License

No license has been added yet.
