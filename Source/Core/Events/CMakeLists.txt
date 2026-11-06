include("${CMAKE_CURRENT_LIST_DIR}/../../../Build/CMake/SIRBuildTreeGuard.cmake")
target_sources(SIRCore PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/SIREventRegistry.cpp
)
