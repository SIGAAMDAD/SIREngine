# SDKs are intentionally external; no licensed audio binaries are vendored.
set(SIR_FMOD_SDK_ROOT "" CACHE PATH "FMOD Studio API installation directory")
set(SIR_WWISE_SDK_ROOT "" CACHE PATH "Wwise SDK installation directory")

if(SIR_WITH_FMOD)
    find_path(SIR_FMOD_INCLUDE_DIR fmod_studio.hpp
        PATHS "${SIR_FMOD_SDK_ROOT}/api/studio/inc" NO_DEFAULT_PATH REQUIRED)
    find_library(SIR_FMOD_STUDIO_LIBRARY NAMES fmodstudio fmodstudio_vc
        PATHS "${SIR_FMOD_SDK_ROOT}/api/studio/lib/x64"
              "${SIR_FMOD_SDK_ROOT}/api/studio/lib/x86_64"
        NO_DEFAULT_PATH REQUIRED)
    find_library(SIR_FMOD_CORE_LIBRARY NAMES fmod fmod_vc
        PATHS "${SIR_FMOD_SDK_ROOT}/api/core/lib/x64"
              "${SIR_FMOD_SDK_ROOT}/api/core/lib/x86_64"
        NO_DEFAULT_PATH REQUIRED)
    add_library(SIR::FMODStudio UNKNOWN IMPORTED)
    set_target_properties(SIR::FMODStudio PROPERTIES
        IMPORTED_LOCATION "${SIR_FMOD_STUDIO_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SIR_FMOD_INCLUDE_DIR}")
    add_library(SIR::FMODCore UNKNOWN IMPORTED)
    set_target_properties(SIR::FMODCore PROPERTIES
        IMPORTED_LOCATION "${SIR_FMOD_CORE_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SIR_FMOD_SDK_ROOT}/api/core/inc")
endif()

if(SIR_WITH_WWISE)
    find_path(SIR_WWISE_INCLUDE_DIR AK/SoundEngine/Common/AkSoundEngine.h
        PATHS "${SIR_WWISE_SDK_ROOT}/include" NO_DEFAULT_PATH REQUIRED)
    add_library(SIR::WwiseSDK INTERFACE IMPORTED)
    set_target_properties(SIR::WwiseSDK PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${SIR_WWISE_INCLUDE_DIR}")
    # Link-library resolution is platform/configuration-specific and will be
    # added with the Wwise adapter; this target records header dependency only.
endif()

if(SIR_WITH_OPENAL)
    find_package(OpenAL REQUIRED)
    add_library(SIR::OpenAL INTERFACE IMPORTED)
    target_link_libraries(SIR::OpenAL INTERFACE OpenAL::OpenAL)
endif()
