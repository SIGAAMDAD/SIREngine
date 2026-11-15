include("${CMAKE_CURRENT_LIST_DIR}/../../../Build/CMake/SIRBuildTreeGuard.cmake")
# Bounded dedicated native file-worker baseline. IOCP/io_uring acceleration uses
# the same request and completion model and may be added without caller changes.
target_sources(SIRCore PRIVATE SIRAsyncIOService.cpp)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(SIRCore PRIVATE "${PROJECT_SOURCE_DIR}/Source/Core/Platform/Linux/SIRIOUringReaderLinux.cpp")
endif()

if(WIN32)
    target_sources(SIRCore PRIVATE "${PROJECT_SOURCE_DIR}/Source/Core/Platform/Windows/SIRIOCPReaderWindows.cpp")
endif()
