# Core-owned platform and input services.
set(SIR_CORE_PLATFORM_COMMON
    SIRPlatformUtils.cpp
    SIRVirtualMemory.cpp
    SIRCPUInfo.cpp
    SIRThread.cpp
    SIRSync.cpp
    SIRAtomic.cpp
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
        Input/Windows/SIRInputWindowWindows.cpp
        Input/Windows/SIRWindowsRawBackend.cpp
        Input/Windows/SIRWindowsRawDecode.cpp
        Input/Windows/SIRWindowsXInputBackend.cpp
        Windows/SIRChildProcessWindows.cpp
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
            list(APPEND SIR_CORE_PLATFORM_OS
                Input/Windows/SIRWindowsGameInputBackend.cpp
                Input/Windows/SIRWindowsGameInputCodec.cpp)
            target_include_directories(SIRCore PRIVATE "${SIR_GAMEINPUT_INCLUDE_DIR}")
            target_compile_definitions(SIRCore PUBLIC SIR_INPUT_GAMEINPUT_AVAILABLE=1)
        elseif(SIR_REQUIRE_GAMEINPUT_SDK)
            message(FATAL_ERROR "Native GameInput SDK v1 header required but GameInput.h was not found")
        endif()
    elseif(SIR_REQUIRE_GAMEINPUT_SDK)
        message(FATAL_ERROR "SIR_REQUIRE_GAMEINPUT_SDK requires SIR_WITH_GAMEINPUT=ON")
    endif()
    target_link_libraries(SIRCore PRIVATE psapi user32 imm32)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(SIR_CORE_PLATFORM_OS
        Linux/SIRPlatformFileLinux.cpp
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
        Linux/SIRPlatformFileSystemLinux.cpp
        Linux/SIRPlatformThreadControlLinux.cpp
    )
    option(SIR_WITH_X11_INPUT "Build native X11 input backend when X11 is available" ON)
    if(SIR_WITH_X11_INPUT)
        find_package(X11 QUIET)
        if(X11_FOUND)
            list(APPEND SIR_CORE_PLATFORM_OS Input/Linux/SIRLinuxX11Backend.cpp Input/Linux/SIRInputWindowX11.cpp)
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
