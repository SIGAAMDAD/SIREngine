# 09 — Platform and API support matrix

**Status:** release policy and target plan.

| Platform | Foundation C++ CI | Godot editor nightly | SIR runtime release gate | Target graphics |
| --- | --- | --- | --- | --- |
| Windows x86-64 | Required | Required | Required after integration | D3D12, Vulkan 1.4+, OpenGL 4.6 |
| Linux x86-64 | Required | Required | Required after integration | Vulkan 1.4+, OpenGL 4.6 |
| macOS | Manual preview only | Deferred | Deferred | Native strategy to be decided after other OS targets |

The foundation smoke test validates C++17, EASTL and the CMake entrypoint; it is not a claim that SIR resource and renderer integration works. Nightly Godot editor builds test the pinned fork and catch upstream/regression failures. The future SIR runtime release gate also requires actual graphics-driver startup and representative scene loading.

## OS boundary

All OS-specific APIs belong under `src/platform/` or an approved backend. The common engine must not expose `windows.h`, Vulkan headers, Objective-C or device SDKs in general gameplay-facing interfaces. Keep byte order, page size, alignment, file-system case sensitivity, dynamic library loading and system allocation behavior explicit.

## Cross-platform acceptance

CI must build the same source commit with pinned dependencies and record toolchain versions. Run at least an editor headless import test, package validation, memory-domain leak audit and a graphics smoke test on real supported hardware before marking a target supported. Compiler success and native graphics initialization are distinct gates.

macOS is intentionally gated after Windows and Linux; a manual CMake preset does not imply completed Apple graphics/audio support.
