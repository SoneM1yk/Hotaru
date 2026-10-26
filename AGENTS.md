# Repository Guidelines

Hotaru is a C++23 real-time rendering engine with a Vulkan backend. Keep the public engine API independent of any specific graphics backend.

## Project Structure & Module Organization

- `include/hotaru/`: Public API headers; include these as `<hotaru/...>`.
- `src/`: Engine implementation and private headers. `src/backend/` defines backend interfaces, while `src/backend/vulkan/` contains the Vulkan driver, swapchain, pipeline, and program.
- `test/`: GoogleTest suites and rendering smoke tests.
- `shader/`: GLSL shaders. `assets/`: Branding and documentation assets.
- `build/`: Generated CMake output; it is ignored and should not be committed.

## Build, Test, and Development Commands

Install a C++23 compiler, CMake 3.21+, Vulkan development files, `spdlog`, `EnTT`, `volk`, `Eigen3`, GLFW, and GTest before configuring.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

- `-DBUILD_TEST=OFF` skips tests; `-DENABLE_ASAN=ON` enables AddressSanitizer.
- Reconfigure after adding source files because `src/CMakeLists.txt` globs them.
- No standalone application target exists yet; use the test binaries to exercise the engine.

## Coding Style & Naming Conventions

- Use C++23, four-space indentation, Allman braces, and `#pragma once` in headers.
- Use `PascalCase` for types and functions (`VulkanDriver`, `CreateView`), `snake_case` for variables and parameters (`surface_type`), and a trailing underscore for private members (`impl_`).
- Put code in namespace `hotaru`. Use quoted includes for private headers and angle-bracket includes for public headers.
- No formatter or linter is configured; match surrounding code and keep diffs focused.

## Testing Guidelines

Tests use GoogleTest with CTest discovery. Name files `*_test.cpp`, suites `PascalCaseTest`, and cases as descriptive `PascalCase` names. Run one suite with:

```bash
ctest --test-dir build -R VulkanInitTest --output-on-failure
```

No coverage threshold is configured. Vulkan and swapchain tests require a working Vulkan loader; window-related tests also need a compatible Wayland environment.

## Commit & Pull Request Guidelines

History follows `<type>: <summary>` Conventional Commit messages, such as `feat: Add swapchain` and `chore: add logo and README`. Use focused types such as `feat`, `fix`, `test`, `docs`, or `chore`; avoid submitting bare `wip` commits.

No PR template or CI is configured. PRs should summarize the change, link relevant issues, list verification commands, and note hardware or driver requirements. Include screenshots or logs for rendering changes and call out public API changes.
