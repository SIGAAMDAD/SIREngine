# Third-party licenses and source provenance

SIREngine is Godot-derived. Code ported from original engines, framework libraries and terrain implementations must retain applicable authorship, license notices and exact source provenance.

Pinned source dependencies are managed as Git submodules under `ThirdParty/`:

- Electronic Arts EASTL, EABase and EAAssert — retain license files from their pinned repositories.
- [OpenAL Soft](https://github.com/kcat/openal-soft) — an OpenAL implementation with library and bundled-component licenses; preserve its `COPYING` and related notices.
- [meshoptimizer](https://github.com/zeux/meshoptimizer) — MIT-licensed mesh processing and optimization library.

FMOD Studio API and Wwise are separately licensed SDKs, and their proprietary files are not committed. Mono is resolved from a separately installed embedding SDK and retains its runtime/distribution licensing obligations. System-installed OpenAL implementations retain their own licenses.

The root [LICENSE](LICENSE) is the Mozilla Public License 2.0 for SIREngine-owned source files, with preserved licenses for external and imported source. A license audit is required before third-party code is imported or runtime libraries are packaged.
