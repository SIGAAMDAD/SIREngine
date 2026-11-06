include("${CMAKE_CURRENT_LIST_DIR}/../../../Build/CMake/SIRBuildTreeGuard.cmake")
target_sources(SIRCore PRIVATE SIRVFSPath.cpp SIRVFSMountTable.cpp SIRVFSOverlayResolver.cpp SIRVFSFileHandleTable.cpp SIRVFSDirectorySource.cpp)
