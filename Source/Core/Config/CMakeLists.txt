include("${CMAKE_CURRENT_LIST_DIR}/../../../Build/CMake/SIRBuildTreeGuard.cmake")
target_sources(SIRCore PRIVATE
    SIRConfigBudget.cpp
    Formats/Delimited/SIRDelimited.cpp
    Formats/INI/SIRIni.cpp
    SIRConfigAtomicFile.cpp)
