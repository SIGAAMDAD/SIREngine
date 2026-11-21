# Latest policy versus current main checkout

Checked on 2026-11-20 against connected `SIGAAMDAD/SIREngine` **main**:

| Contract | Current checked-in main | Latest user-established target | Agent behavior |
| --- | --- | --- | --- |
| C++ standard | Root `CMakeLists.txt` and FASTBuild presets select C++17 | C++23 primary; C++17 fallback when unsupported | Compile against actual target mode; apply C++23 only in C++23-enabled targets; change build config only in an authorized migration |
| Formatting | `.clang-format` says `Standard: c++17`, 140 columns, tabs and custom braces | Keep formatting conventions and update language policy in a separate authorized pass | Honor actual `.clang-format` now, report policy gap |
| Header guards | `Docs/Standards/cpp.md` describes `__SIRENGINE_[HEADER_NAME]_H__` | Latest requested `__SIR_[CLASS_NAME]_H__` (strip SIR from class name) | Use latest convention for newly owned headers; don't bulk-rewrite preexisting guards during unrelated tasks |
| Owned structs/enums | `sir[Name]_t` typedef forms in docs | Retain `sir[Name]_t`, no alias duplicates | Follow newest policy in new source; preserve ABI for existing types |
| Allocations | Repository memory contract with source-owned allocator | Overridden `new/delete` routed through engine allocator, no bypass | Inspect allocator routing; do not invent a blanket `new/delete` prohibition |
| FASTBuild | `dev` normal, `tooling` Ninja compile database | FASTBuild remains development default | Keep original presets; no generated project overlay |

`Docs/Standards/cpp.md` currently describes C++17 and legacy header guard guidance. It has not been edited here. A future implementation task should reconcile these docs and actual CMake/toolchain files deliberately, with compile tests.

Sources inspected:

- https://github.com/SIGAAMDAD/SIREngine/blob/main/CMakeLists.txt
- https://github.com/SIGAAMDAD/SIREngine/blob/main/CMakePresets.json
- https://github.com/SIGAAMDAD/SIREngine/blob/main/.clang-format
- https://github.com/SIGAAMDAD/SIREngine/blob/main/Docs/Standards/cpp.md
- https://github.com/SIGAAMDAD/SIREngine/blob/main/Docs/README.md

This file records a time-specific comparison and should be rechecked against the actual local branch before work.
