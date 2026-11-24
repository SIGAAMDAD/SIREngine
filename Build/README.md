# Building and editing

Use CMake 4.4.5 or newer and Ninja with GCC, Clang, or Visual Studio 2022
Build Tools (Desktop development with C++ and a Windows SDK).

Linux GCC:

```sh
cmake --preset linux-gcc
cmake --build --preset linux-gcc
ctest --preset linux-gcc
```

Use `linux-clang` in all three commands for Clang. These presets use separate
build directories and export their actual compile commands. The Clang toolchain
uses the GCC runtime reported by `g++`; override `SIR_CLANG_GCC_INSTALL_DIR` when
using another GCC installation. Optional SDKs remain opt-in.

On Windows, run the same commands with `windows-msvc` from an x64 Developer
PowerShell. `windows-clang-cl` requires the Visual Studio LLVM tools as well.
Both use Ninja. Alternatively configure `vs2022` and build/test with `vs2022`
(Debug) or `vs2022-release` (Release). Visual Studio generators do not export
compile_commands.json; use a Ninja preset for clangd.

In VS Code, install the recommended CMake Tools, C/C++, and clangd extensions.
Run **CMake: Select Configure Preset**, select the host/compiler preset, then
configure and select the corresponding build preset. CMake Tools activates the
Visual Studio developer environment on Windows. Ctrl+Shift+B builds; the test
task runs CTest against the selected build directory and configuration. Select
a CMake launch target and the Linux/GDB or Windows/MSVC debug configuration.
GDB must be installed for Linux debugging.

clangd uses the selected CMake build directory. After switching compilers,
run **clangd: Restart language server**. Workspace analysis settings support
clangd 18 and newer. The Linux query-driver allowlist covers /usr/bin GCC and
Clang; add an explicit trusted compiler path when using another installation.
For Visual Studio generator IntelliSense, disable clangd and set
`C_Cpp.intelliSenseEngine` to `default`; CMake Tools supplies target settings.

The pinned LLVM 23 standards/coverage presets are separate from normal builds.
Successful configuration does not imply the current source tree compiles;
compiler errors and unbuilt test executables must be resolved before CTest can
run the complete suite. MSVC configurations require validation on Windows.
