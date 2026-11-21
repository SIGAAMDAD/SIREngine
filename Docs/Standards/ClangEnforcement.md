# SIREngine C++ Standards — Level 3 Enforcement

## Authority and scope

SIREngine first-party `Source/`, `Tests/`, and `Tools/` sources use the root
`.clang-format`, `.clang-tidy`, `.clangd`, and `Tools/Standards/sir_clang_policy.py`.
Generated code and vendored dependencies retain their own policies. Intentional
negative fixtures in `Tests/Standards/` are not production inputs.

The primary language is C++23. C++17 exists for environments where C++23 is
not supported. Compatible features must retain their defined behavior in that
fallback; C++17 does not replace the primary engine language.

## Locally run the gates

```sh
cmake --preset standards-clang23
cmake --build --preset standards-clang23 --target sir-format-check
cmake --build --preset standards-clang23 --target sir-tidy-check
cmake --build --preset standards-clang23 --target sir-policy-check
cmake --build --preset standards-clang23 --target sir-policy-regression-check
cmake --build --preset standards-clang23 --target sir-header-check
cmake --build --preset standards-clang23 --target sir-standards-check
cmake --build --preset standards-clang23 --target sir-compatibility-check
```

The CMake gates use the generated compilation database for the configured
preset, including `out/<hostSystemName>/standards-clang23/compile_commands.json`. A
missing/empty compilation database or tool is a failure, not a skipped check.
The runner requires the pinned LLVM 23.1.5 tool versions.

The format check covers all first-party C/C++ source and headers on disk. Tidy
and AST checks analyze all first-party translation units listed in the authentic
compilation database. Headers parsed by those units receive AST analysis; the
AST policy runner separately examines relevant first-party header rules. The
header-independence target compiles headers in configured subsystems using
corresponding authentic compilation flags; other platform/optional headers are
reported as unqualified in this configuration.
Translation units excluded from a build configuration cannot be claimed as
validated by that configuration; qualify other targets/configurations separately.

For clangd indexing:

```sh
python3 Tools/Standards/run_checks.py --repo . \
  --compdb out/Linux/tooling/compile_commands.json --mode publish-compdb
```

## Optional local pre-commit hook

```sh
git config core.hooksPath .githooks
```

On staged first-party C++ changes, the hook verifies staged formatting, then
runs full policy and tidy checking against the local compilation database.
It refuses to silently pass when that database is missing. Set
`SIR_COMPILE_COMMANDS=/absolute/path/compile_commands.json` to select another
valid database. This local hook is an additional check, not a replacement for
CI. Changes made after the database was generated require reconfiguration.

## Continuous integration

`.github/workflows/ci.yml` requires the reusable
`.github/workflows/standards.yml` job, including:

- LLVM 23.1.5 format/tidy/AST policy and positive/negative regression fixtures;
- a C++23 Clang/Ninja configure, native build, and CTest;
- a separate C++17 fallback configure, native build, and CTest;
- existing GCC and MSVC CI jobs remain part of `sir/required`.

An incomplete standards check, missing tool, failed native test, or partially
analyzed compilation database does **not** count as a passing qualification.
The required status job explicitly consumes the standards workflow result.

## Limits of static source analysis

The semantic checker is a standalone Clang JSON AST analyzer, not a compiled
clang-tidy plugin. The CI wiring qualifies the rules it currently implements;
it cannot prove actual overridden `operator new` routing, smart-pointer
ownership, cross-module ABI identity, runtime thread safety, or performance.
Those contracts require separate implementation and native tests. Legacy code
with violations remains a failing baseline until repaired; this package does
not rewrite source files or suppress existing violations.

## Windows and Linux toolchain integration

Both systems require LLVM/Clang 23.1.5, Python 3, CMake 4.4.5+, and Ninja
for the standards qualification presets. Windows builds must be launched
from an initialized Visual Studio developer environment when using clang-cl,
because MSVC/Windows SDK search paths are part of the authentic compilation
command. The Windows CI job installs the attested official LLVM MSI, uses
clang-cl with Ninja, and verifies the C++23 and C++17 builds separately.

The `.clangd` configuration is shared. It does **not** force a compiler name,
`-std=c++23`, or Linux include paths. It uses a repo-root copy of the platform's
real `compile_commands.json`, which preserves `clang-cl`/MSVC or Clang/GCC
options. To publish a native development database:

**Linux (shell):**

```sh
cmake --preset tooling
cmake --build --preset tooling --target sir-clangd-db
```

**Windows (Visual Studio Developer PowerShell):**

```powershell
cmake --preset tooling -DCMAKE_CXX_COMPILER=clang-cl
cmake --build --preset tooling --target sir-clangd-db
```

For explicit fallback work, use `tooling-cxx17` and publish its database with
`python Tools/Standards/run_checks.py --repo . --compdb out/Windows/tooling-cxx17/compile_commands.json --mode publish-compdb`
(adapt `Windows` to `Linux` on Linux). Restart clangd after switching modes.
The root database is generated and should not be committed.

Optional local Git hooks can be installed on both operating systems with
`git config core.hooksPath .githooks`. Git for Windows provides a POSIX shell
for hooks; the shim delegates all path and Git logic to Python. Staged C++ is
formatted using stdin and its actual staged bytes, so unstaged modifications
cannot hide violations. Full policy and clang-tidy gates use the platform's
CMake compilation database. The alternate database path can be selected using
`SIR_COMPILE_COMMANDS`.

clangd has background and standard-library indexing, complete diagnostic
reporting, shared clang-tidy checks with fast-filtering, unused/missing include
analysis, fully-qualified `std`/`eastl` namespace suggestions, predictable
completion/include insertion, type/parameter inlay hints, and expanded hover
information. Generated and third-party trees are excluded from first-party
editor diagnostics and background indexing. Compilation database publication
remains mandatory for correct platform-specific parsing.
