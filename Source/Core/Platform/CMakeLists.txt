include("${CMAKE_CURRENT_LIST_DIR}/../../../Build/CMake/SIRBuildTreeGuard.cmake")
# Native OS services belong to the Core library and share its lifecycle.
set(SIR_CORE_PLATFORM_COMMON
    SIRPlatformUtils.cpp
    SIRPlatformNativeWindow.cpp
    Window/SIRWindowSystem.cpp
    Window/SIRNativeWindowHost.cpp
    Window/Headless/SIRWindowDriverHeadless.cpp
    SIRVirtualMemory.cpp
    SIRCPUInfo.cpp
    SIRThread.cpp
    SIRSync.cpp
    SIRAtomic.cpp
    SIRPlatformRootDirectory.cpp
    SIRPlatformAtomicFile.cpp
    Input/SIRInputActions.cpp
    Input/SIRInputReplay.cpp
    Input/SIRInputHistory.cpp
    Input/SIRInputBackend.cpp
    Input/SIRInputText.cpp
    Input/SIRInputUIDispatch.cpp
    Input/SIRInputHaptics.cpp
    Input/SIRInputDeviceRegistry.cpp
    Input/SIRInputControllerProfiles.cpp
    Input/SIRInputFrameState.cpp
    Input/SIRInputService.cpp
)

if(WIN32)
    set(SIR_CORE_PLATFORM_OS
        Windows/SIRPlatformFileWindows.cpp
        Windows/SIRPlatformContextWindows.cpp
        Windows/SIRPlatformRootDirectoryWindows.cpp
        Windows/SIRPlatformFileMappingWindows.cpp
        Windows/SIRPlatformPipeWindows.cpp
        Windows/SIRSemaphoreWindows.cpp
        Windows/SIRVirtualMemoryWindows.cpp
        Windows/SIRPlatformMemoryWindows.cpp
        Windows/SIRCPUInfoWindows.cpp
        Windows/SIRSystemInfoWindows.cpp
        Windows/SIRDynamicLibraryWindows.cpp
        Windows/SIRPlatformTimeWindows.cpp
        Windows/SIRPlatformProcessWindows.cpp
        Window/Windows/SIRWindowDriverWin32.cpp
        Window/Windows/SIRWGLBootstrapWindow.cpp
        Input/Windows/SIRWin32InputBridge.cpp
        Input/Windows/SIRWindowsRawBackend.cpp
        Input/Windows/SIRWindowsRawDecode.cpp
        Input/Windows/SIRWindowsXInputBackend.cpp
        Windows/SIRChildProcessWindows.cpp
        Windows/SIRFileLockWindows.cpp
        Windows/SIRPlatformFileSystemWindows.cpp
        Windows/SIRPlatformThreadControlWindows.cpp
    )
    # GameInput is first-party Windows controller I/O. The runtime is loaded
    # from the OS, while its v1 SDK header is an explicitly pinned build input.
    option(SIR_WITH_GAMEINPUT "Enable native Windows GameInput controller history" ON)
    option(SIR_REQUIRE_GAMEINPUT_SDK "Require GameInput SDK during configure (CI acceptance)" OFF)
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
                Input/Windows/SIRWindowsGameInputBackend.cpp
                Input/Windows/SIRWindowsGameInputCodec.cpp)
            target_include_directories(SIRCore PRIVATE "${SIR_GAMEINPUT_INCLUDE_DIR}")
            target_compile_definitions(SIRCore PUBLIC SIR_INPUT_GAMEINPUT_AVAILABLE=1)
        elseif(SIR_REQUIRE_GAMEINPUT_SDK)
            message(FATAL_ERROR "GameInput v1 SDK required but the selected GameInput.h is missing or incompatible")
        elseif(SIR_GAMEINPUT_INCLUDE_DIR)
            message(STATUS "GameInput.h is not the v1 SDK; native GameInput disabled (XInput fallback remains available)")
        endif()
    elseif(SIR_REQUIRE_GAMEINPUT_SDK)
        message(FATAL_ERROR "SIR_REQUIRE_GAMEINPUT_SDK requires SIR_WITH_GAMEINPUT=ON")
    endif()
    target_link_libraries(SIRCore PRIVATE psapi user32 imm32)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(SIR_CORE_PLATFORM_OS
        Linux/SIRPlatformFileLinux.cpp
        Linux/SIRPlatformContextLinux.cpp
        Linux/SIRPlatformRootDirectoryLinux.cpp
        Linux/SIRPlatformFileMappingLinux.cpp
        Linux/SIRPlatformPipeLinux.cpp
        Linux/SIRSemaphoreLinux.cpp
        Linux/SIRVirtualMemoryLinux.cpp
        Linux/SIRPlatformMemoryLinux.cpp
        Linux/SIRCPUInfoLinux.cpp
        Linux/SIRSystemInfoLinux.cpp
        Linux/SIRDynamicLibraryLinux.cpp
        Linux/SIRPlatformTimeLinux.cpp
        Linux/SIRPlatformProcessLinux.cpp
        Input/Linux/SIRLinuxEvdevBackend.cpp
        Linux/SIRChildProcessLinux.cpp
        Linux/SIRFileLockLinux.cpp
        Linux/SIRPlatformFileSystemLinux.cpp
        Linux/SIRPlatformThreadControlLinux.cpp
    )
    option(SIR_WITH_WAYLAND_WINDOW "Build native Linux Wayland xdg-shell window driver" ON)
    option(SIR_WITH_WAYLAND_INPUT "Build first-party Wayland seat input bridge" ON)
    option(SIR_REQUIRE_WAYLAND_INPUT "Require native Wayland keyboard/pointer input at configure" OFF)
    option(SIR_REQUIRE_WAYLAND_WINDOW "Fail configure when the native Wayland driver cannot be built" OFF)
    if(SIR_WITH_WAYLAND_WINDOW)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(SIR_WAYLAND_CLIENT QUIET IMPORTED_TARGET wayland-client)
            pkg_get_variable(SIR_WAYLAND_PROTOCOL_DATA wayland-protocols pkgdatadir)
        endif()
        find_program(SIR_WAYLAND_SCANNER NAMES wayland-scanner)
        set(SIR_XDG_SHELL_PROTOCOL "${SIR_WAYLAND_PROTOCOL_DATA}/stable/xdg-shell/xdg-shell.xml")
        if(SIR_WAYLAND_CLIENT_FOUND AND SIR_WAYLAND_SCANNER AND EXISTS "${SIR_XDG_SHELL_PROTOCOL}")
            set(SIR_WAYLAND_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/WaylandGenerated")
            set(SIR_WAYLAND_PROTOCOL_H "${SIR_WAYLAND_GENERATED}/xdg-shell-client-protocol.h")
            set(SIR_WAYLAND_PROTOCOL_C "${SIR_WAYLAND_GENERATED}/xdg-shell-protocol.c")
            file(MAKE_DIRECTORY "${SIR_WAYLAND_GENERATED}")
            add_custom_command(
                OUTPUT "${SIR_WAYLAND_PROTOCOL_H}" "${SIR_WAYLAND_PROTOCOL_C}"
                COMMAND "${SIR_WAYLAND_SCANNER}" client-header "${SIR_XDG_SHELL_PROTOCOL}" "${SIR_WAYLAND_PROTOCOL_H}"
                COMMAND "${SIR_WAYLAND_SCANNER}" private-code "${SIR_XDG_SHELL_PROTOCOL}" "${SIR_WAYLAND_PROTOCOL_C}"
                DEPENDS "${SIR_XDG_SHELL_PROTOCOL}"
                VERBATIM)
            add_custom_target(SIRWindowWaylandProtocol DEPENDS "${SIR_WAYLAND_PROTOCOL_H}" "${SIR_WAYLAND_PROTOCOL_C}")
            add_dependencies(SIRCore SIRWindowWaylandProtocol)
            list(APPEND SIR_CORE_PLATFORM_OS
                Window/Linux/SIRWindowDriverWayland.cpp
                "${SIR_WAYLAND_PROTOCOL_C}")
            target_include_directories(SIRCore PRIVATE "${SIR_WAYLAND_GENERATED}")
            target_link_libraries(SIRCore PRIVATE PkgConfig::SIR_WAYLAND_CLIENT)
            target_compile_definitions(SIRCore PUBLIC SIR_WINDOW_WAYLAND_AVAILABLE=1)
            if(SIR_WITH_WAYLAND_INPUT)
                pkg_check_modules(SIR_WAYLAND_XKB QUIET IMPORTED_TARGET xkbcommon)
                if(SIR_WAYLAND_XKB_FOUND)
                    list(APPEND SIR_CORE_PLATFORM_OS Input/Linux/SIRWaylandInputBridge.cpp)
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
    option(SIR_WITH_X11_INPUT "Build native X11 input backend when X11 is available" ON)
    if(SIR_WITH_X11_INPUT)
        find_package(X11 QUIET)
        if(X11_FOUND)
            list(APPEND SIR_CORE_PLATFORM_OS Input/Linux/SIRLinuxX11Backend.cpp Window/Linux/SIRWindowDriverX11.cpp Input/Linux/SIRX11InputBridge.cpp)
            target_compile_definitions(SIRCore PUBLIC SIR_WINDOW_X11_AVAILABLE=1)
            target_link_libraries(SIRCore PRIVATE X11::X11)
            target_compile_definitions(SIRCore PUBLIC SIR_INPUT_X11_AVAILABLE=1)
            if(X11_Xi_FOUND AND TARGET X11::Xi)
                target_link_libraries(SIRCore PRIVATE X11::Xi)
                target_compile_definitions(SIRCore PUBLIC SIR_INPUT_XINPUT2_AVAILABLE=1)
            endif()
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
