# Third-party dependencies

Vendor source lives under `ThirdParty/` as **pinned Git submodules**, not copied into first-party `Source/`. Dependencies are not automatically enabled merely because their sources are present. See [IMPORT_LOCK.json](IMPORT_LOCK.json) for every newly imported origin, source revision, and category.

## Existing dependencies

- `EASTL`, `EABase`, `EAAssert`: pinned Electronic Arts submodules
- `OpenALSoft`: pinned audio-backend source
- `meshoptimizer`: pinned mesh-processing source
- `FMOD` and `Mono`: separately installed SDK integration records
- `Wwise`: separately installed optional audio SDK integration

## Additional pinned source packages

| Category | Submodule directories |
| --- | --- |
| Compression | `Zstd`, `LZ4`, `xxHash`, `zlib` |
| GPU backend and effects | `VulkanMemoryAllocator`, `D3D12MemoryAllocator`, `VulkanHeaders`, `DirectXHeaders`, `FidelityFX`, `SMAA`, `XeSS`, `Streamline` |
| Shader compilation and SPIR-V | `DirectXShaderCompiler`, `SPIRVTools`, `SPIRVReflect`, `SPIRVCross`, `glslang`, `SPIRVHeaders` |
| Texture processing | `KTXSoftware`, `BasisUniversal`, `Compressonator`, `etcpak`, `CVTT`, `ASTCEncoder` |
| Model import | `ufbx`, `fastgltf` |
| Images | `libpng`, `libjpeg-turbo`, `libwebp`, `tinyexr`, `libavif` |
| Fonts and typography | `FreeType`, `HarfBuzz`, `msdfgen` |
| OS and accessibility | `AccessKit`, `SDL3`, `HIDAPI` |
| Networking | `curl`, `mbedTLS` |
| Media codecs | `libogg`, `libvorbis`, `Opus` |
| Storage | `SQLite` |
| Diagnostics | `libbacktrace` |

## Source acquisition

```sh
git submodule update --init ThirdParty/Zstd ThirdParty/LZ4 ThirdParty/xxHash
# To fetch all pinned third-party source:
git submodule update --init --recursive
```

Large SDKs, shader compilers, and vendor frameworks are intentionally opt-in at CMake integration time. Recursive submodules may fetch additional upstream transitive sources; it does not enable them in SIREngine's default build.

`Steamworks/` documents a **separate proprietary SDK download**; its binaries and headers are not committed.

`fastgltf` is explicitly pinned to **v0.9.1**. `libpng` is sourced from the stable `libpng16` branch snapshot. Each SHA is immutable until a deliberate dependency update.

SIREngine implements its **own OpenGL and Vulkan function loaders**. Neither GLAD nor volk is imported.

Individual vendor licenses, bundled NOTICE files, and redistribution requirements remain authoritative; SIREngine's first-party license does not overwrite them. See [dependency contracts](../Docs/Dependencies.md) and [third-party notices](../THIRD_PARTY_NOTICES.md).
