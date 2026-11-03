# Native OS services belong to the Core library and share its lifecycle.
set(SIR_CORE_PLATFORM_COMMON
    SIRPlatformUtils.cpp
    SIRVirtualMemory.cpp
    SIRCPUInfo.cpp
    SIRThread.cpp
    SIRSync.cpp
    SIRAtomic.cpp
    SIRInput.cpp
    SIRInputControllerProfiles.cpp
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
        Windows/SIRWindowsRawInput.cpp
        Windows/SIRWindowsXInput.cpp
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
        Linux/SIRLinuxEvdevInput.cpp
    )
else()
    message(FATAL_ERROR "SIRCore platform services currently support Windows and Linux")
endif()

target_sources(SIRCore PRIVATE ${SIR_CORE_PLATFORM_COMMON} ${SIR_CORE_PLATFORM_OS})
if(CMAKE_DL_LIBS)
    target_link_libraries(SIRCore PRIVATE "${CMAKE_DL_LIBS}")
endif()
find_package(Threads REQUIRED)
target_link_libraries(SIRCore PRIVATE Threads::Threads)
