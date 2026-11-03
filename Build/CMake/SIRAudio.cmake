# Audio implementation choices are independent of Core/Launcher bootstrap.
# Set SIR_WITH_OPENAL_SOFT to build the pinned implementation, or
# SIR_WITH_OPENAL to use a system-installed OpenAL implementation.
if(SIR_WITH_OPENAL_SOFT AND SIR_WITH_OPENAL)
    message(FATAL_ERROR
        "Select either SIR_WITH_OPENAL_SOFT or SIR_WITH_OPENAL; both provide OpenAL")
endif()

if(SIR_WITH_OPENAL_SOFT)
    if(NOT EXISTS "${PROJECT_SOURCE_DIR}/ThirdParty/OpenALSoft/CMakeLists.txt")
        message(FATAL_ERROR
            "Missing OpenAL Soft source; run git submodule update --init ThirdParty/OpenALSoft")
    endif()
    set(ALSOFT_UTILS OFF CACHE BOOL "Disable OpenAL Soft tools" FORCE)
    set(ALSOFT_EXAMPLES OFF CACHE BOOL "Disable OpenAL Soft examples" FORCE)
    set(ALSOFT_TESTS OFF CACHE BOOL "Disable OpenAL Soft vendor tests" FORCE)
    set(ALSOFT_INSTALL OFF CACHE BOOL "Do not install vendored OpenAL Soft" FORCE)
    set(ALSOFT_INSTALL_CONFIG OFF CACHE BOOL "No vendor config installation" FORCE)
    set(ALSOFT_UPDATE_BUILD_VERSION OFF CACHE BOOL "Avoid per-build vendor version probing" FORCE)
    add_subdirectory(ThirdParty/OpenALSoft "${PROJECT_BINARY_DIR}/openal-soft")
    if(NOT TARGET OpenAL::OpenAL)
        message(FATAL_ERROR "Pinned OpenAL Soft must provide OpenAL::OpenAL")
    endif()
    add_library(SIROpenAL INTERFACE)
    add_library(SIR::OpenAL ALIAS SIROpenAL)
    target_link_libraries(SIROpenAL INTERFACE OpenAL::OpenAL)
elseif(SIR_WITH_OPENAL)
    find_package(OpenAL REQUIRED)
    add_library(SIROpenAL INTERFACE)
    add_library(SIR::OpenAL ALIAS SIROpenAL)
    target_link_libraries(SIROpenAL INTERFACE OpenAL::OpenAL)
endif()

# FMOD SDK is licensed and deliberately not stored in the repository.
set(SIR_FMOD_SDK_ROOT "" CACHE PATH "FMOD Studio API SDK root")
if(SIR_WITH_FMOD)
    if(NOT IS_DIRECTORY "${SIR_FMOD_SDK_ROOT}")
        message(FATAL_ERROR
            "FMOD SDK not found; set SIR_FMOD_SDK_ROOT to an installed FMOD Studio API directory")
    endif()
    find_path(SIR_FMOD_CORE_INCLUDE_DIR fmod.h
        PATHS "${SIR_FMOD_SDK_ROOT}/api/core/inc" NO_DEFAULT_PATH REQUIRED)
    find_path(SIR_FMOD_STUDIO_INCLUDE_DIR fmod_studio.hpp
        PATHS "${SIR_FMOD_SDK_ROOT}/api/studio/inc" NO_DEFAULT_PATH REQUIRED)
    set(sirFmodLibDirs
        "${SIR_FMOD_SDK_ROOT}/api/core/lib/x64"
        "${SIR_FMOD_SDK_ROOT}/api/core/lib/x86_64"
        "${SIR_FMOD_SDK_ROOT}/api/core/lib"
        "${SIR_FMOD_SDK_ROOT}/api/studio/lib/x64"
        "${SIR_FMOD_SDK_ROOT}/api/studio/lib/x86_64"
        "${SIR_FMOD_SDK_ROOT}/api/studio/lib")
    find_library(SIR_FMOD_CORE_LIBRARY
        NAMES fmod fmodL fmod_vc fmodL_vc
        PATHS ${sirFmodLibDirs} NO_DEFAULT_PATH REQUIRED)
    find_library(SIR_FMOD_STUDIO_LIBRARY
        NAMES fmodstudio fmodstudioL fmodstudio_vc fmodstudioL_vc
        PATHS ${sirFmodLibDirs} NO_DEFAULT_PATH REQUIRED)
    add_library(SIRFMODCore INTERFACE)
    add_library(SIR::FMODCore ALIAS SIRFMODCore)
    target_include_directories(SIRFMODCore INTERFACE "${SIR_FMOD_CORE_INCLUDE_DIR}")
    target_link_libraries(SIRFMODCore INTERFACE "${SIR_FMOD_CORE_LIBRARY}")
    add_library(SIRFMODStudio INTERFACE)
    add_library(SIR::FMODStudio ALIAS SIRFMODStudio)
    target_include_directories(SIRFMODStudio INTERFACE
        "${SIR_FMOD_STUDIO_INCLUDE_DIR}" "${SIR_FMOD_CORE_INCLUDE_DIR}")
    target_link_libraries(SIRFMODStudio INTERFACE
        "${SIR_FMOD_STUDIO_LIBRARY}" SIR::FMODCore)
endif()

set(SIR_WWISE_SDK_ROOT "" CACHE PATH "Wwise SDK root")
if(SIR_WITH_WWISE)
    find_path(SIR_WWISE_HEADER AK/SoundEngine/Common/AkSoundEngine.h
        PATHS "${SIR_WWISE_SDK_ROOT}/include" NO_DEFAULT_PATH REQUIRED)
    add_library(SIRWwiseHeaders INTERFACE)
    target_include_directories(SIRWwiseHeaders INTERFACE "${SIR_WWISE_HEADER}")
endif()
