include("${CMAKE_CURRENT_LIST_DIR}/../../../Build/CMake/SIRBuildTreeGuard.cmake")
# Core-owned, bounded CPU task execution foundation.
target_sources(SIRCore PRIVATE SIRTaskScheduler.cpp SIRTaskGraph.cpp SIRTaskFenceTracker.cpp)
