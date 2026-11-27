# ===========================================================================
# SIREngine MPLv2 Source Code
# Copyright (C) 2023-2027 Noah Van Til
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# This software is provided "as is", without warranty of any kind,
# express or implied, including but not limited to the warranties
# of merchantability, fitness for a particular purpose and noninfringement.
# ===========================================================================

include("${CMAKE_CURRENT_LIST_DIR}/../../Build/CMake/SIRBuildTreeGuard.cmake")
add_library(SIRCore STATIC SIRApplication.cpp SIRApplicationScheduling.cpp Systems/SIREngineSystem.cpp
    Diagnostics/Assertions/SIRAssert.cpp
    Diagnostics/SIREmergencyDiagnostics.cpp
    Diagnostics/Logging/SIRLoggerSystem.cpp
    Console/SIRConsoleLogSink.cpp
    Hardware/SIRHardwareCatalog.cpp
    Input/SIRInputBindingMap.cpp)
add_library(SIR::Core ALIAS SIRCore)
target_include_directories(SIRCore PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRCore PUBLIC cxx_std_17)
set(SIR_DIAGNOSTIC_ASSERT_ENABLED "1" CACHE STRING "Enable active diagnostic assertions for SIREngine targets")
set_property(CACHE SIR_DIAGNOSTIC_ASSERT_ENABLED PROPERTY STRINGS 0 1)
target_compile_definitions(SIRCore PUBLIC SIR_DIAGNOSTIC_ASSERT_ENABLED=${SIR_DIAGNOSTIC_ASSERT_ENABLED})

find_package(BZip2 REQUIRED)
find_package(ZLIB REQUIRED)
find_path(SIR_LZ4_INCLUDE_DIR NAMES lz4.h REQUIRED)
find_library(SIR_LZ4_LIBRARY NAMES lz4 REQUIRED)
find_path(SIR_ZSTD_INCLUDE_DIR NAMES zstd.h REQUIRED)
find_library(SIR_ZSTD_LIBRARY NAMES zstd REQUIRED)

add_library(SIR::LZ4 UNKNOWN IMPORTED)
set_target_properties(SIR::LZ4 PROPERTIES
    IMPORTED_LOCATION "${SIR_LZ4_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${SIR_LZ4_INCLUDE_DIR}")
add_library(SIR::Zstd UNKNOWN IMPORTED)
set_target_properties(SIR::Zstd PROPERTIES
    IMPORTED_LOCATION "${SIR_ZSTD_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${SIR_ZSTD_INCLUDE_DIR}")
target_link_libraries(SIRCore PRIVATE BZip2::BZip2 ZLIB::ZLIB SIR::LZ4 SIR::Zstd)
target_sources(SIRCore PRIVATE
    Compression/SIRBZip2Codec.cpp
    Compression/SIRLZ4Codec.cpp
    Compression/SIRZlibCodec.cpp
    Compression/SIRZstdCodec.cpp)
target_sources(SIRCore PRIVATE
    IO/SIRIOBufferPool.cpp
    IO/SIRIORequestPlanner.cpp
    IO/Backends/SIRJobIOService.cpp)
if(MSVC)
    target_compile_options(SIRCore PRIVATE /W4)
else()
    target_compile_options(SIRCore PRIVATE -Wall -Wextra -Wpedantic)
endif()

block(SCOPE_FOR VARIABLES)
# -- Core/Input --
# Logical input services belong to Core independent of the native OS providers.
target_sources(SIRCore PRIVATE
    ${PROJECT_SOURCE_DIR}/Source/Core/Input/SIRInputActions.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Input/SIRInputBackend.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Input/SIRInputControllerProfiles.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Input/SIRInputDeviceRegistry.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Input/SIRInputFrameState.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Input/SIRInputHaptics.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Input/SIRInputHistory.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Input/SIRInputReplay.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Input/SIRInputService.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Input/SIRInputText.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Input/SIRInputUIDispatch.cpp
)
endblock()
block(SCOPE_FOR VARIABLES)
# -- Core/Platform --
# Native OS services belong to the Core library and share its lifecycle.
set(SIR_CORE_PLATFORM_COMMON
    ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Shared/Window/SIRPlatformNativeWindow.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Shared/Window/SIRNativeWindowLease.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Shared/Window/SIRWindowSystem.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Shared/Window/SIRNativeWindowHost.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Headless/SIRWindowDriverHeadless.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Shared/Memory/SIRVirtualMemory.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Shared/Diagnostics/SIRCPUInfo.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Linux/Storage/SIRPlatformStorageQueryLinux.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Shared/Threading/SIRSync.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Shared/Threading/SIRSyncAdvanced.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Shared/File/SIRPlatformRootDirectory.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Shared/File/SIRPlatformAtomicFile.cpp
)

if(WIN32)
    set(SIR_CORE_PLATFORM_OS
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Windows/Window/SIRNativeWindowHostWindows.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Windows/Window/SIRNativeInputHostWindows.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Windows/Threading/SIRThreadWindows.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Windows/Threading/SIRSyncWindows.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Windows/File/SIRPlatformFileWindows.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Windows/Diagnostics/SIRPlatformContextWindows.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Windows/Diagnostics/SIRPlatformDebugWindows.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Windows/Logging/SIRPlatformLogFileSink.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Windows/Logging/SIRPlatformLogConsoleSink.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Windows/Storage/SIRPlatformStorageQueryWindows.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Windows/File/SIRPlatformRootDirectoryWindows.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Windows/File/SIRPlatformFileMappingWindows.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Windows/Process/SIRPlatformPipeWindows.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Windows/Threading/SIRSemaphoreWindows.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Windows/Memory/SIRVirtualMemoryWindows.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Windows/Memory/SIRPlatformMemoryWindows.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Windows/Diagnostics/SIRCPUInfoWindows.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Windows/Diagnostics/SIRSystemInfoWindows.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Windows/Library/SIRDynamicLibraryWindows.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Windows/Diagnostics/SIRPlatformTimeWindows.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Windows/Threading/SIRThreadSchedulingWindows.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Windows/Window/SIRWindowDriverWin32.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Windows/Window/SIRWGLBootstrapWindow.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Windows/Input/SIRWin32InputBridge.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Windows/Input/SIRWindowsRawBackend.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Windows/Input/SIRWindowsRawDecode.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Windows/Input/SIRWindowsXInputBackend.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Windows/Process/SIRChildProcessWindows.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Windows/File/SIRFileLockWindows.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Windows/File/SIRPlatformFileSystemWindows.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Windows/Threading/SIRPlatformThreadControlWindows.cpp
    )
    # GameInput is first-party Windows controller I/O. The runtime is loaded
    # from the OS, while its v1 SDK header is an explicitly pinned build input.
    if(SIR_WITH_GAMEINPUT)
        find_path(SIR_GAMEINPUT_INCLUDE_DIR NAMES GameInput.h HINTS "$ENV{SIR_GAMEINPUT_SDK_INCLUDE}")
        if(SIR_GAMEINPUT_INCLUDE_DIR)
            # Some Windows SDKs expose a different, incompatible GameInput.h.
            # Compile the actual v1 symbols before enabling the native backend.
            include(CheckCXXSourceCompiles)
            include(CMakePushCheckState)
            cmake_push_check_state(RESET)
            set(CMAKE_REQUIRED_INCLUDES "${SIR_GAMEINPUT_INCLUDE_DIR}")
            unset(SIR_GAMEINPUT_SDK_IS_V1 CACHE)
            check_cxx_source_compiles([=[
                #include <Windows.h>
                #include <GameInput.h>
                #if !defined(GAMEINPUT_API_VERSION) || GAMEINPUT_API_VERSION != 1
                    #error SIREngine requires the GameInput v1 SDK
                #endif
                static_assert(GameInput::v1::GameInputGamepadA == 0x00000004);
                int main() { return 0; }
            ]=] SIR_GAMEINPUT_SDK_IS_V1)
            cmake_pop_check_state()
        endif()
        if(SIR_GAMEINPUT_INCLUDE_DIR AND SIR_GAMEINPUT_SDK_IS_V1)
            list(APPEND SIR_CORE_PLATFORM_OS
                ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Windows/Input/SIRWindowsGameInputBackend.cpp
                ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Windows/Input/SIRWindowsGameInputCodec.cpp)
            target_include_directories(SIRCore PRIVATE "${SIR_GAMEINPUT_INCLUDE_DIR}")
            target_compile_definitions(SIRCore PUBLIC SIR_INPUT_GAMEINPUT_AVAILABLE=1)
        elseif(SIR_REQUIRE_GAMEINPUT_SDK)
            message(FATAL_ERROR "GameInput v1 SDK required but the selected ${PROJECT_SOURCE_DIR}/Source/Core/Platform/GameInput.h is missing or incompatible")
        elseif(SIR_GAMEINPUT_INCLUDE_DIR)
            message(STATUS "${PROJECT_SOURCE_DIR}/Source/Core/Platform/GameInput.h is not the v1 SDK; native GameInput disabled (XInput fallback remains available)")
        endif()
    elseif(SIR_REQUIRE_GAMEINPUT_SDK)
        message(FATAL_ERROR "SIR_REQUIRE_GAMEINPUT_SDK requires SIR_WITH_GAMEINPUT=ON")
    endif()
    target_link_libraries(SIRCore PRIVATE psapi user32 imm32)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(SIR_CORE_PLATFORM_OS
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Linux/Window/SIRNativeWindowHostLinux.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Linux/Window/SIRNativeInputHostLinux.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Linux/Threading/SIRThreadLinux.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Linux/Threading/SIRSyncLinux.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Linux/File/SIRPlatformFileLinux.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Linux/Diagnostics/SIRPlatformContextLinux.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Linux/Diagnostics/SIRPlatformDebugLinux.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Linux/Logging/SIRPlatformLogFileSink.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Linux/Logging/SIRPlatformLogConsoleSink.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Linux/File/SIRPlatformRootDirectoryLinux.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Linux/File/SIRPlatformFileMappingLinux.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Linux/Process/SIRPlatformPipeLinux.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Linux/Threading/SIRSemaphoreLinux.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Linux/Memory/SIRVirtualMemoryLinux.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Linux/Memory/SIRPlatformMemoryLinux.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Linux/Diagnostics/SIRCPUInfoLinux.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Linux/Diagnostics/SIRSystemInfoLinux.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Linux/Library/SIRDynamicLibraryLinux.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Linux/Diagnostics/SIRPlatformTimeLinux.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Linux/Threading/SIRThreadSchedulingLinux.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Linux/Input/SIRLinuxEvdevBackend.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Linux/Window/SIRNativeInputEvdevGamepadDriver.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Linux/Process/SIRChildProcessLinux.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Linux/File/SIRFileLockLinux.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Linux/File/SIRPlatformFileSystemLinux.cpp
        ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Linux/Threading/SIRPlatformThreadControlLinux.cpp
    )
    if(SIR_WITH_WAYLAND_WINDOW)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(SIR_WAYLAND_CLIENT QUIET IMPORTED_TARGET wayland-client)
            pkg_get_variable(SIR_WAYLAND_PROTOCOL_DATA wayland-protocols pkgdatadir)
        endif()
        find_program(SIR_WAYLAND_SCANNER NAMES wayland-scanner)
        set(SIR_XDG_SHELL_PROTOCOL "${SIR_WAYLAND_PROTOCOL_DATA}/stable/xdg-shell/xdg-shell.xml")
        set(SIR_POINTER_CONSTRAINTS_PROTOCOL "${SIR_WAYLAND_PROTOCOL_DATA}/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml")
        set(SIR_RELATIVE_POINTER_PROTOCOL "${SIR_WAYLAND_PROTOCOL_DATA}/unstable/relative-pointer/relative-pointer-unstable-v1.xml")
        set(SIR_FRACTIONAL_SCALE_PROTOCOL "${SIR_WAYLAND_PROTOCOL_DATA}/staging/fractional-scale/fractional-scale-v1.xml")
        set(SIR_VIEWPORTER_PROTOCOL "${SIR_WAYLAND_PROTOCOL_DATA}/stable/viewporter/viewporter.xml")
        if(SIR_WAYLAND_CLIENT_FOUND AND SIR_WAYLAND_SCANNER AND EXISTS "${SIR_XDG_SHELL_PROTOCOL}")
            set(SIR_WAYLAND_GENERATED "${PROJECT_BINARY_DIR}/Source/Core/Platform/WaylandGenerated")
            set(SIR_WAYLAND_PROTOCOL_H "${SIR_WAYLAND_GENERATED}/xdg-shell-client-protocol.h")
            set(SIR_WAYLAND_PROTOCOL_C "${SIR_WAYLAND_GENERATED}/xdg-shell-protocol.c")
            file(MAKE_DIRECTORY "${SIR_WAYLAND_GENERATED}")
            add_custom_command(
                OUTPUT "${SIR_WAYLAND_PROTOCOL_H}" "${SIR_WAYLAND_PROTOCOL_C}"
                COMMAND "${SIR_WAYLAND_SCANNER}" client-header "${SIR_XDG_SHELL_PROTOCOL}" "${SIR_WAYLAND_PROTOCOL_H}"
                COMMAND "${SIR_WAYLAND_SCANNER}" private-code "${SIR_XDG_SHELL_PROTOCOL}" "${SIR_WAYLAND_PROTOCOL_C}"
                DEPENDS "${SIR_XDG_SHELL_PROTOCOL}"
                VERBATIM)
            set(SIR_WAYLAND_POINTER_GENERATED)
            if(EXISTS "${SIR_POINTER_CONSTRAINTS_PROTOCOL}" AND EXISTS "${SIR_RELATIVE_POINTER_PROTOCOL}")
                set(SIR_POINTER_CONSTRAINTS_H "${SIR_WAYLAND_GENERATED}/pointer-constraints-unstable-v1-client-protocol.h")
                set(SIR_POINTER_CONSTRAINTS_C "${SIR_WAYLAND_GENERATED}/pointer-constraints-unstable-v1-protocol.c")
                set(SIR_RELATIVE_POINTER_H "${SIR_WAYLAND_GENERATED}/relative-pointer-unstable-v1-client-protocol.h")
                set(SIR_RELATIVE_POINTER_C "${SIR_WAYLAND_GENERATED}/relative-pointer-unstable-v1-protocol.c")
                add_custom_command(
                    OUTPUT "${SIR_POINTER_CONSTRAINTS_H}" "${SIR_POINTER_CONSTRAINTS_C}"
                           "${SIR_RELATIVE_POINTER_H}" "${SIR_RELATIVE_POINTER_C}"
                    COMMAND "${SIR_WAYLAND_SCANNER}" client-header "${SIR_POINTER_CONSTRAINTS_PROTOCOL}" "${SIR_POINTER_CONSTRAINTS_H}"
                    COMMAND "${SIR_WAYLAND_SCANNER}" private-code "${SIR_POINTER_CONSTRAINTS_PROTOCOL}" "${SIR_POINTER_CONSTRAINTS_C}"
                    COMMAND "${SIR_WAYLAND_SCANNER}" client-header "${SIR_RELATIVE_POINTER_PROTOCOL}" "${SIR_RELATIVE_POINTER_H}"
                    COMMAND "${SIR_WAYLAND_SCANNER}" private-code "${SIR_RELATIVE_POINTER_PROTOCOL}" "${SIR_RELATIVE_POINTER_C}"
                    DEPENDS "${SIR_POINTER_CONSTRAINTS_PROTOCOL}" "${SIR_RELATIVE_POINTER_PROTOCOL}"
                    VERBATIM)
                set(SIR_WAYLAND_POINTER_GENERATED "${SIR_POINTER_CONSTRAINTS_H}" "${SIR_POINTER_CONSTRAINTS_C}"
                    "${SIR_RELATIVE_POINTER_H}" "${SIR_RELATIVE_POINTER_C}")
                target_compile_definitions(SIRCore PRIVATE SIR_WAYLAND_POINTER_PROTOCOLS_AVAILABLE=1)
            endif()
            set(SIR_WAYLAND_FRACTIONAL_GENERATED)
            if(EXISTS "${SIR_FRACTIONAL_SCALE_PROTOCOL}" AND EXISTS "${SIR_VIEWPORTER_PROTOCOL}")
                set(SIR_FRACTIONAL_H "${SIR_WAYLAND_GENERATED}/fractional-scale-v1-client-protocol.h")
                set(SIR_FRACTIONAL_C "${SIR_WAYLAND_GENERATED}/fractional-scale-v1-protocol.c")
                set(SIR_VIEWPORTER_H "${SIR_WAYLAND_GENERATED}/viewporter-client-protocol.h")
                set(SIR_VIEWPORTER_C "${SIR_WAYLAND_GENERATED}/viewporter-protocol.c")
                add_custom_command(
                    OUTPUT "${SIR_FRACTIONAL_H}" "${SIR_FRACTIONAL_C}" "${SIR_VIEWPORTER_H}" "${SIR_VIEWPORTER_C}"
                    COMMAND "${SIR_WAYLAND_SCANNER}" client-header "${SIR_FRACTIONAL_SCALE_PROTOCOL}" "${SIR_FRACTIONAL_H}"
                    COMMAND "${SIR_WAYLAND_SCANNER}" private-code "${SIR_FRACTIONAL_SCALE_PROTOCOL}" "${SIR_FRACTIONAL_C}"
                    COMMAND "${SIR_WAYLAND_SCANNER}" client-header "${SIR_VIEWPORTER_PROTOCOL}" "${SIR_VIEWPORTER_H}"
                    COMMAND "${SIR_WAYLAND_SCANNER}" private-code "${SIR_VIEWPORTER_PROTOCOL}" "${SIR_VIEWPORTER_C}"
                    DEPENDS "${SIR_FRACTIONAL_SCALE_PROTOCOL}" "${SIR_VIEWPORTER_PROTOCOL}"
                    VERBATIM)
                set(SIR_WAYLAND_FRACTIONAL_GENERATED "${SIR_FRACTIONAL_H}" "${SIR_FRACTIONAL_C}"
                    "${SIR_VIEWPORTER_H}" "${SIR_VIEWPORTER_C}")
                target_compile_definitions(SIRCore PRIVATE SIR_WAYLAND_FRACTIONAL_SCALE_AVAILABLE=1)
            endif()
            add_custom_target(SIRWindowWaylandProtocol DEPENDS "${SIR_WAYLAND_PROTOCOL_H}" "${SIR_WAYLAND_PROTOCOL_C}"
                ${SIR_WAYLAND_POINTER_GENERATED} ${SIR_WAYLAND_FRACTIONAL_GENERATED})
            add_dependencies(SIRCore SIRWindowWaylandProtocol)
            list(APPEND SIR_CORE_PLATFORM_OS
                ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Linux/Window/SIRWindowDriverWayland.cpp
                "${SIR_WAYLAND_PROTOCOL_C}"
                ${SIR_WAYLAND_POINTER_GENERATED}
                ${SIR_WAYLAND_FRACTIONAL_GENERATED})
            target_include_directories(SIRCore PRIVATE "${SIR_WAYLAND_GENERATED}")
            target_link_libraries(SIRCore PRIVATE PkgConfig::SIR_WAYLAND_CLIENT)
            target_compile_definitions(SIRCore PUBLIC SIR_WINDOW_WAYLAND_AVAILABLE=1)
            if(SIR_WITH_WAYLAND_INPUT)
                pkg_check_modules(SIR_WAYLAND_XKB QUIET IMPORTED_TARGET xkbcommon)
                if(SIR_WAYLAND_XKB_FOUND)
                    list(APPEND SIR_CORE_PLATFORM_OS ${PROJECT_SOURCE_DIR}/Source/Core/Platform/Linux/Input/SIRWaylandInputBridge.cpp)
                    target_link_libraries(SIRCore PRIVATE PkgConfig::SIR_WAYLAND_XKB)
                    target_compile_definitions(SIRCore PUBLIC SIR_INPUT_WAYLAND_AVAILABLE=1)
                    target_compile_definitions(SIRCore PRIVATE SIR_WAYLAND_XKB_AVAILABLE=1)
                elseif(SIR_REQUIRE_WAYLAND_INPUT)
                    message(FATAL_ERROR "Native Wayland input requires xkbcommon and Wayland client development packages")
                else()
                    message(STATUS "Wayland seat input unavailable: missing xkbcommon development package")
                endif()
            elseif(SIR_REQUIRE_WAYLAND_INPUT)
                message(FATAL_ERROR "SIR_REQUIRE_WAYLAND_INPUT requires SIR_WITH_WAYLAND_INPUT=ON")
            endif()
        elseif(SIR_REQUIRE_WAYLAND_WINDOW OR SIR_REQUIRE_WAYLAND_INPUT)
            message(FATAL_ERROR "Native Wayland input/window requires wayland-client, wayland-scanner and wayland-protocols xdg-shell")
        else()
            message(STATUS "Native Wayland window backend unavailable; X11/headless remain separately selectable")
        endif()
    elseif(SIR_REQUIRE_WAYLAND_WINDOW)
        message(FATAL_ERROR "SIR_REQUIRE_WAYLAND_WINDOW requires SIR_WITH_WAYLAND_WINDOW=ON")
    elseif(SIR_REQUIRE_WAYLAND_INPUT)
        message(FATAL_ERROR "SIR_REQUIRE_WAYLAND_INPUT requires SIR_WITH_WAYLAND_WINDOW=ON")
    endif()
    if(SIR_WITH_X11_INPUT AND NOT SIR_WITH_X11_WINDOW)
        message(FATAL_ERROR "SIR_WITH_X11_INPUT requires SIR_WITH_X11_WINDOW=ON")
    endif()
    if(SIR_WITH_X11_WINDOW OR SIR_WITH_X11_INPUT)
        find_package(X11 QUIET)
        if(X11_FOUND)
            if(SIR_WITH_X11_WINDOW)
                list(APPEND SIR_CORE_PLATFORM_OS
                    "${PROJECT_SOURCE_DIR}/Source/Core/Platform/Linux/SIRNativeWindowX11.cpp"
                    "${PROJECT_SOURCE_DIR}/Source/Core/Platform/Linux/Window/SIRWindowDriverX11.cpp")
                target_compile_definitions(SIRCore PUBLIC SIR_WINDOW_X11_AVAILABLE=1)
                if(X11_Xrandr_FOUND AND TARGET X11::Xrandr)
                    target_link_libraries(SIRCore PRIVATE X11::Xrandr)
                    target_compile_definitions(SIRCore PRIVATE SIR_WINDOW_XRANDR_AVAILABLE=1)
                endif()
            endif()
            if(SIR_WITH_X11_INPUT)
                list(APPEND SIR_CORE_PLATFORM_OS
                    "${PROJECT_SOURCE_DIR}/Source/Core/Platform/Linux/Window/SIRNativeInputDriverX11.cpp"
                    "${PROJECT_SOURCE_DIR}/Source/Core/Platform/Linux/Input/SIRLinuxX11Backend.cpp"
                    "${PROJECT_SOURCE_DIR}/Source/Core/Platform/Linux/Input/SIRX11InputBridge.cpp")
                target_compile_definitions(SIRCore PUBLIC SIR_INPUT_X11_AVAILABLE=1)
                if(X11_Xi_FOUND AND TARGET X11::Xi)
                    target_link_libraries(SIRCore PRIVATE X11::Xi)
                    target_compile_definitions(SIRCore PUBLIC SIR_INPUT_XINPUT2_AVAILABLE=1)
                endif()
            endif()
            target_link_libraries(SIRCore PRIVATE X11::X11)
        endif()
    endif()
else()
    message(FATAL_ERROR "SIRCore platform services currently support Windows and Linux")
endif()

target_sources(SIRCore PRIVATE ${SIR_CORE_PLATFORM_COMMON} ${SIR_CORE_PLATFORM_OS})
if(CMAKE_DL_LIBS)
    target_link_libraries(SIRCore PRIVATE "${CMAKE_DL_LIBS}")
endif()
find_package(Threads REQUIRED)
target_link_libraries(SIRCore PRIVATE Threads::Threads)
endblock()
target_sources(SIRCore PRIVATE Memory/SIRMemoryManager.cpp Objects/SIRObjectRegistry.cpp)
block(SCOPE_FOR VARIABLES)
# -- Core/Memory --
# Framework allocator implementations are subordinate to the SIR memory page authority.
# The existing SIRMemoryManager.cpp remains owned by Source/Core/CMakeLists.txt.
target_sources(SIRCore PRIVATE
    ${PROJECT_SOURCE_DIR}/Source/Core/Memory/Allocators/SIRAllocationRouter.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Memory/SIRBitmapAllocator.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Memory/Allocators/SIRBuddyAllocator.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Memory/SIRCacheAllocator.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Memory/SIRCompactBucketAllocator.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Memory/SIRFrameAllocator.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Memory/SIRFreeListAllocator.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Memory/SIRGeneralHeap.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Memory/SIRHandleHeap.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Memory/SIRHighLowAllocator.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Memory/SIRHeapBlock.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Memory/SIRJobScratchAllocator.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Memory/SIRLinearAllocator.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Memory/SIRMediumPageAllocator.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Memory/SIRMemoryColdPageStager.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Memory/SIRMemoryLayoutPlanner.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Memory/SIRMemoryPressureCoordinator.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Memory/SIRPageRunAllocator.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Memory/SIRPoolAllocator.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Memory/SIRReclaimableSizeClassAllocator.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Memory/SIRRingAllocator.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Memory/SIRSharedMemoryPool.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Memory/SIRSizeClassAllocator.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Memory/SIRSlabAllocator.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Memory/SIRTLSFAllocator.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Memory/SIRThreadLocalAllocator.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Memory/SIRVirtualStackAllocator.cpp
)
endblock()
block(SCOPE_FOR VARIABLES)
# -- Core/Events --
target_sources(SIRCore PRIVATE
    ${PROJECT_SOURCE_DIR}/Source/Core/Events/SIREventRegistry.cpp
)
endblock()
block(SCOPE_FOR VARIABLES)
# -- Core/Config --
target_sources(SIRCore PRIVATE
    ${PROJECT_SOURCE_DIR}/Source/Core/Config/SIRConfigBudget.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Config/Formats/Delimited/SIRDelimited.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Config/Formats/INI/SIRIni.cpp
    ${PROJECT_SOURCE_DIR}/Source/Core/Config/SIRConfigAtomicFile.cpp)
endblock()
block(SCOPE_FOR VARIABLES)
# -- Core/Strings --
target_sources(SIRCore PRIVATE ${PROJECT_SOURCE_DIR}/Source/Core/Strings/SIRStringPool.cpp)
endblock()
block(SCOPE_FOR VARIABLES)
# -- Core/Console --
target_sources(SIRCore PRIVATE ${PROJECT_SOURCE_DIR}/Source/Core/Console/SIRConsoleLogSink.cpp ${PROJECT_SOURCE_DIR}/Source/Core/Console/SIRConsoleSystem.cpp)
endblock()
block(SCOPE_FOR VARIABLES)
# -- Core/Jobs --
# Core-owned, bounded CPU task execution foundation.
target_sources(SIRCore PRIVATE ${PROJECT_SOURCE_DIR}/Source/Core/Jobs/SIRTaskScheduler.cpp ${PROJECT_SOURCE_DIR}/Source/Core/Jobs/SIRTaskGraph.cpp ${PROJECT_SOURCE_DIR}/Source/Core/Jobs/SIRTaskFenceTracker.cpp)
endblock()
block(SCOPE_FOR VARIABLES)
# -- Core/VFS --
target_sources(SIRCore PRIVATE ${PROJECT_SOURCE_DIR}/Source/Core/VFS/SIRVFSPath.cpp ${PROJECT_SOURCE_DIR}/Source/Core/VFS/SIRVFSMountTable.cpp ${PROJECT_SOURCE_DIR}/Source/Core/VFS/SIRVFSOverlayResolver.cpp ${PROJECT_SOURCE_DIR}/Source/Core/VFS/SIRVFSFileHandleTable.cpp ${PROJECT_SOURCE_DIR}/Source/Core/VFS/SIRVFSDirectorySource.cpp)
endblock()
sir_configure_owned_target(SIRCore)

# Versioned scheduling -- retains the canonical application and window pump.
target_sources(SIRCore PRIVATE Scheduling/SIRSimulationSchedule.cpp Scheduling/SIRTimelineCoordinator.cpp)
