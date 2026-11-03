cmake_minimum_required(VERSION 3.25)
get_filename_component(SIR_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
find_package(Git REQUIRED)
execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${SIR_ROOT}" ls-files
    RESULT_VARIABLE sirStatus OUTPUT_VARIABLE sirFiles ERROR_VARIABLE sirError)
if(NOT sirStatus EQUAL 0)
    message(FATAL_ERROR "Cannot inspect tracked source: ${sirError}")
endif()
string(REPLACE "\r" "" sirFiles "${sirFiles}")
string(REPLACE "\n" ";" sirFiles "${sirFiles}")
set(sirRequired
    CMakeLists.txt CMakePresets.json .gitmodules .clang-format
    Build/CMake/CIHygiene.cmake Build/CMake/CIFormat.cmake
    Build/CMake/CIThirdParty.cmake ThirdParty/IMPORT_LOCK.json
    Build/CMake/NightlyMetrics.cmake Build/CMake/NightlyCompare.cmake
    Build/Scripts/NightlyBuild.sh
    Source/CMakeLists.txt Source/Core/CMakeLists.txt
    Source/Core/SIRApplication.h Source/Core/SIRApplication.cpp Source/Main.cpp
    .github/workflows/ci.yml .github/workflows/nightly.yml
    Docs/Milestones/README.md
    Docs/Milestones/03-Terrain.md
    Source/Terrain/README.md Source/Terrain/Editor/README.md)
foreach(sirPath IN LISTS sirRequired)
    if(NOT sirPath IN_LIST sirFiles)
        message(FATAL_ERROR "Required source missing: ${sirPath}")
    endif()
endforeach()
set(sirAllowedDirectories .github Build Docs Source Tests ThirdParty Tools)
foreach(sirPath IN LISTS sirFiles)
    if(sirPath MATCHES "^(out/|build/|CMakeFiles/|\\.vs/|include/|src/|scripts/|engine/|third_party/|tests/)")
        message(FATAL_ERROR "Obsolete/generated directory tracked: ${sirPath}")
    endif()
    if(sirPath MATCHES "^[^/]+/")
        string(REGEX REPLACE "/.*$" "" sirRootPath "${sirPath}")
        if(NOT sirRootPath IN_LIST sirAllowedDirectories)
            message(FATAL_ERROR "Unexpected top-level source directory: ${sirPath}")
        endif()
    endif()
    string(TOLOWER "${sirPath}" sirLower)
    if(sirLower MATCHES "\\.(exe|dll|so|dylib|pdb|obj|o|spv|pyc|log)$")
        message(FATAL_ERROR "Generated artifact tracked: ${sirPath}")
    endif()
endforeach()
include("${CMAKE_CURRENT_LIST_DIR}/CIThirdParty.cmake")
message(STATUS "PASS: native source hygiene and locked third-party dependencies")
