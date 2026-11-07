include("${CMAKE_CURRENT_LIST_DIR}/../../../Build/CMake/SIRBuildTreeGuard.cmake")
# Native OS services belong to the Core library and share its lifecycle.
set(SIR_CORE_PLATFORM_COMMON
    SIRPlatformUtils.cpp
    Window/SIRWindowSystem.cpp
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
        Window/Windows/SIRInputWindowWindows.cpp
        Window/Windows/SIRWindowDriverWin32.cpp
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
    option(SIR_WITH_X11_INPUT "Build native X11 input backend when X11 is available" ON)
    if(SIR_WITH_X11_INPUT)
        find_package(X11 QUIET)
        if(X11_FOUND)
            list(APPEND SIR_CORE_PLATFORM_OS Input/Linux/SIRLinuxX11Backend.cpp Window/Linux/SIRInputWindowX11.cpp Window/Linux/SIRWindowDriverX11.cpp Input/Linux/SIRX11InputBridge.cpp)
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
