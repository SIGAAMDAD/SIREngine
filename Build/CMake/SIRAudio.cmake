# Optional SDK resolution. Backend implementations join the build in later milestones.
set(SIR_FMOD_SDK_ROOT "" CACHE PATH "FMOD Studio API root")
set(SIR_WWISE_SDK_ROOT "" CACHE PATH "Wwise SDK root")

if(SIR_WITH_FMOD)
    find_path(SIR_FMOD_HEADER fmod_studio.hpp
        PATHS "${SIR_FMOD_SDK_ROOT}/api/studio/inc" NO_DEFAULT_PATH REQUIRED)
    add_library(SIRFMODHeaders INTERFACE)
    target_include_directories(SIRFMODHeaders INTERFACE "${SIR_FMOD_HEADER}")
endif()

if(SIR_WITH_WWISE)
    find_path(SIR_WWISE_HEADER AK/SoundEngine/Common/AkSoundEngine.h
        PATHS "${SIR_WWISE_SDK_ROOT}/include" NO_DEFAULT_PATH REQUIRED)
    add_library(SIRWwiseHeaders INTERFACE)
    target_include_directories(SIRWwiseHeaders INTERFACE "${SIR_WWISE_HEADER}")
endif()

if(SIR_WITH_OPENAL)
    find_package(OpenAL REQUIRED)
    add_library(SIROpenAL INTERFACE)
    target_link_libraries(SIROpenAL INTERFACE OpenAL::OpenAL)
endif()
