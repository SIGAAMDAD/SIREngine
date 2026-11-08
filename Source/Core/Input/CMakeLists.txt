include("${CMAKE_CURRENT_LIST_DIR}/../../../Build/CMake/SIRBuildTreeGuard.cmake")
# Shared input owns logical devices, state, actions and frame publication.
target_sources(SIRCore PRIVATE
    SIRInputActions.cpp
    SIRInputReplay.cpp
    SIRInputHistory.cpp
    SIRInputBackend.cpp
    SIRInputText.cpp
    SIRInputUIDispatch.cpp
    SIRInputHaptics.cpp
    SIRInputDeviceRegistry.cpp
    SIRInputControllerProfiles.cpp
    SIRInputFrameState.cpp
    SIRInputService.cpp
)
