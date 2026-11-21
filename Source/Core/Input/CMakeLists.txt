include("${CMAKE_CURRENT_LIST_DIR}/../../../Build/CMake/SIRBuildTreeGuard.cmake")
# Logical input services belong to Core independent of the native OS providers.
target_sources(SIRCore PRIVATE
    SIRInputActions.cpp
    SIRInputBackend.cpp
    SIRInputControllerProfiles.cpp
    SIRInputDeviceRegistry.cpp
    SIRInputFrameState.cpp
    SIRInputHaptics.cpp
    SIRInputHistory.cpp
    SIRInputReplay.cpp
    SIRInputService.cpp
    SIRInputText.cpp
    SIRInputUIDispatch.cpp
)
