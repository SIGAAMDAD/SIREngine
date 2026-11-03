# Third-party licenses and source provenance

SIREngine is Godot-derived. Code ported from original engines, framework libraries and terrain implementations must retain applicable authorship, license notices and exact source provenance.

Pinned source dependencies are managed as Git submodules under `ThirdParty/`:

- Electronic Arts EASTL, EABase and EAAssert — retain license files from their pinned repositories.
- [OpenAL Soft](https://github.com/kcat/openal-soft) — an OpenAL implementation with library and bundled-component licenses; preserve its `COPYING` and related notices.
- [meshoptimizer](https://github.com/zeux/meshoptimizer) — MIT-licensed mesh processing and optimization library.
- [GoogleTest](https://github.com/google/googletest) — BSD-3-Clause licensed C++ unit testing framework.
- [Google Benchmark](https://github.com/google/benchmark) — Apache-2.0 licensed native CPU benchmarking framework.

FMOD Studio API and Wwise are separately licensed SDKs, and their proprietary files are not committed. Mono is resolved from a separately installed embedding SDK and retains its runtime/distribution licensing obligations. System-installed OpenAL implementations retain their own licenses.

The root [LICENSE](LICENSE) is the Mozilla Public License 2.0 for SIREngine-owned source files, with preserved licenses for external and imported source. A license audit is required before third-party code is imported or runtime libraries are packaged.

## Additional source-only dependencies

The newly registered open-source and public SDK source repositories are enumerated in [`ThirdParty/IMPORT_LOCK.json`](ThirdParty/IMPORT_LOCK.json). They retain their upstream license terms, notices, and authored source trees inside the individual submodules. In particular, Zstandard permits a BSD license choice distinct from its GPL alternative, and no GPL licensing is applied to SIREngine by this import.

Public repository availability does not grant unrestricted redistribution of every bundled binary/SDK. Review the vendor licenses for XeSS, Streamline, FidelityFX and other platform/tooling SDKs before packaging them in an engine release. Steamworks is a separate licensed SDK and has not been committed.
