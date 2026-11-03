# Core-owned platform and input services.
set(SIR_CORE_PLATFORM_COMMON
    SIRPlatformUtils.cpp
    SIRVirtualMemory.cpp
    SIRCPUInfo.cpp
    SIRThread.cpp
    SIRSync.cpp
    SIRAtomic.cpp
    Input/SIRInputDeviceRegistry.cpp
    Input/SIRInputFrameState.cpp
    Input/SIRInputService.cpp
)

if(WIN32)
    set(SIR_CORE_PLATFORM_OS
        Windows/SIRVirtualMemoryWindows.cpp
        Windows/SIRPlatformMemoryWindows.cpp
        Windows/SIRCPUInfoWindows.cpp
        Windows/SIRSystemInfoWindows.cpp
        Windows/SIRDynamicLibraryWindows.cpp
        Windows/SIRPlatformTimeWindows.cpp
        Windows/SIRPlatformProcessWindows.cpp
        Input/Windows/SIRWindowsRawBackend.cpp
        Input/Windows/SIRWindowsXInputBackend.cpp
    )
    target_link_libraries(SIRCore PRIVATE psapi user32)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(SIR_CORE_PLATFORM_OS
        Linux/SIRVirtualMemoryLinux.cpp
        Linux/SIRPlatformMemoryLinux.cpp
        Linux/SIRCPUInfoLinux.cpp
        Linux/SIRSystemInfoLinux.cpp
        Linux/SIRDynamicLibraryLinux.cpp
        Linux/SIRPlatformTimeLinux.cpp
        Linux/SIRPlatformProcessLinux.cpp
        Input/Linux/SIRLinuxEvdevBackend.cpp
    )
    option(SIR_WITH_X11_INPUT "Build native X11 input backend when X11 is available" ON)
    if(SIR_WITH_X11_INPUT)
        find_package(X11 QUIET)
        if(X11_FOUND)
            list(APPEND SIR_CORE_PLATFORM_OS Input/Linux/SIRLinuxX11Backend.cpp)
            target_link_libraries(SIRCore PRIVATE X11::X11)
            target_compile_definitions(SIRCore PUBLIC SIR_INPUT_X11_AVAILABLE=1)
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
