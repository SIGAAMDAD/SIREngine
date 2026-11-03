# 09 — Platform and API support matrix

**Status:** release policy and target plan.

| Platform | Foundation C++ CI | Native Release nightly | SIR runtime release gate | Target graphics |
| --- | --- | --- | --- | --- |
| Windows x86-64 | Required | Required | Required after integration | D3D12, Vulkan 1.4+, OpenGL 4.6 |
| Linux x86-64 | Required | Required | Required after integration | Vulkan 1.4+, OpenGL 4.6 |
| macOS | Manual preview only | Deferred | Deferred | Native strategy to be decided after other OS targets |

The foundation C++17 Core/Launcher tests verify compilation, linking and startup contracts, not a complete editor or renderer. Native Release builds and packaging run at night for Windows and Linux. Editor and native graphics acceptance will be added when those systems have been ported and can be exercised on their target platforms.

## OS boundary

All OS-specific APIs belong under `Engine/Source/Platform/` or an approved backend. The common engine must not expose `windows.h`, Vulkan headers, Objective-C or device SDKs in general gameplay-facing interfaces. Keep byte order, page size, alignment, file-system case sensitivity, dynamic library loading and system allocation behavior explicit.

## Cross-platform acceptance

CI must build the same source commit with pinned dependencies and record toolchain versions. Run at least an editor headless import test, package validation, memory-domain leak audit and a graphics smoke test on real supported hardware before marking a target supported. Compiler success and native graphics initialization are distinct gates.

macOS is intentionally gated after Windows and Linux; a manual CMake preset does not imply completed Apple graphics/audio support.
