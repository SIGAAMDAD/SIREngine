# Audio provider-neutral contracts and FMOD mock backend tests.
include(${CMAKE_CURRENT_SOURCE_DIR}/SIRAudioContract.cmake)

# Native Input regression and integration tests from the existing input branch.
add_executable(SIRApplicationInputTests SIRApplicationInputTests.cpp)
target_link_libraries(SIRApplicationInputTests PRIVATE SIR::Core)
add_test(NAME core.application.input-lifecycle COMMAND SIRApplicationInputTests)
set_tests_properties(core.application.input-lifecycle PROPERTIES LABELS "core;input;integration" TIMEOUT 60)
sir_configure_owned_target(SIRApplicationInputTests)

add_executable(SIRInputActionTests SIRInputActionTests.cpp)
target_link_libraries(SIRInputActionTests PRIVATE SIR::Core)
add_test(NAME core.platform.input.actions COMMAND SIRInputActionTests)
set_tests_properties(core.platform.input.actions PROPERTIES LABELS "core;input;actions" TIMEOUT 60)
sir_configure_owned_target(SIRInputActionTests)

add_executable(SIRInputHapticNegativeTests SIRInputHapticNegativeTests.cpp)
target_link_libraries(SIRInputHapticNegativeTests PRIVATE SIR::Core)
add_test(NAME core.platform.input.haptic-negative COMMAND SIRInputHapticNegativeTests)
set_tests_properties(core.platform.input.haptic-negative PROPERTIES LABELS "core;input;haptics;regression" TIMEOUT 60)
sir_configure_owned_target(SIRInputHapticNegativeTests)

add_executable(SIRInputProfileNegativeTests SIRInputProfileNegativeTests.cpp)
target_link_libraries(SIRInputProfileNegativeTests PRIVATE SIR::Core)
add_test(NAME core.platform.input.profile-negative COMMAND SIRInputProfileNegativeTests)
set_tests_properties(core.platform.input.profile-negative PROPERTIES LABELS "core;input;profiles;regression" TIMEOUT 60)
sir_configure_owned_target(SIRInputProfileNegativeTests)

add_executable(SIRInputFrameNegativeTests SIRInputFrameNegativeTests.cpp)
target_link_libraries(SIRInputFrameNegativeTests PRIVATE SIR::Core)
add_test(NAME core.platform.input.frame-negative COMMAND SIRInputFrameNegativeTests)
set_tests_properties(core.platform.input.frame-negative PROPERTIES LABELS "core;input;regression" TIMEOUT 60)
sir_configure_owned_target(SIRInputFrameNegativeTests)

add_executable(SIRInputServiceBoundaryTests SIRInputServiceBoundaryTests.cpp)
target_link_libraries(SIRInputServiceBoundaryTests PRIVATE SIR::Core)
add_test(NAME core.platform.input.service-boundary COMMAND SIRInputServiceBoundaryTests)
set_tests_properties(core.platform.input.service-boundary PROPERTIES LABELS "core;input;regression" TIMEOUT 60)
sir_configure_owned_target(SIRInputServiceBoundaryTests)

add_executable(SIRInputContractGapTests SIRInputContractGapTests.cpp)
target_link_libraries(SIRInputContractGapTests PRIVATE SIR::Core)
add_test(NAME core.platform.input.contract-regressions COMMAND SIRInputContractGapTests)
set_tests_properties(core.platform.input.contract-regressions PROPERTIES LABELS "core;input;regression" TIMEOUT 60)
sir_configure_owned_target(SIRInputContractGapTests)

add_executable(SIRInputTextValidationTests SIRInputTextValidationTests.cpp)
target_link_libraries(SIRInputTextValidationTests PRIVATE SIR::Core)
add_test(NAME core.platform.input.text-validation COMMAND SIRInputTextValidationTests)
set_tests_properties(core.platform.input.text-validation PROPERTIES LABELS "core;input;text;regression" TIMEOUT 60)
sir_configure_owned_target(SIRInputTextValidationTests)

add_executable(SIRInputPhase45Tests SIRInputPhase45Tests.cpp)
target_link_libraries(SIRInputPhase45Tests PRIVATE SIR::Core)
add_test(NAME core.platform.input.ui-haptics-profiles COMMAND SIRInputPhase45Tests)
set_tests_properties(core.platform.input.ui-haptics-profiles PROPERTIES LABELS "core;input;ui;haptics;profiles" TIMEOUT 60)
sir_configure_owned_target(SIRInputPhase45Tests)

add_executable(SIRInputHistoryTests SIRInputHistoryTests.cpp)
target_link_libraries(SIRInputHistoryTests PRIVATE SIR::Core)
add_test(NAME core.platform.input.history COMMAND SIRInputHistoryTests)
set_tests_properties(core.platform.input.history PROPERTIES LABELS "core;input;rollback" TIMEOUT 60)
sir_configure_owned_target(SIRInputHistoryTests)

add_executable(SIRInputReplayTests SIRInputReplayTests.cpp)
target_link_libraries(SIRInputReplayTests PRIVATE SIR::Core)
add_test(NAME core.platform.input.replay COMMAND SIRInputReplayTests)
set_tests_properties(core.platform.input.replay PROPERTIES LABELS "core;input;replay" TIMEOUT 60)
sir_configure_owned_target(SIRInputReplayTests)

add_executable(SIRInputTests SIRInputTests.cpp)
target_link_libraries(SIRInputTests PRIVATE SIR::Core)
add_test(NAME core.platform.input COMMAND SIRInputTests)
sir_configure_owned_target(SIRInputTests)

# Deterministic headless stress suite: enabled in every Windows/Linux CTest run.
add_executable(SIRInputStressTests SIRInputStressTests.cpp)
target_link_libraries(SIRInputStressTests PRIVATE SIR::Core)
sir_configure_owned_target(SIRInputStressTests)
add_test(NAME core.platform.input.stress COMMAND SIRInputStressTests)
set_tests_properties(core.platform.input.stress PROPERTIES LABELS "core;input;stress" TIMEOUT 60)

# The longer 9.6M-request soak runs in nightly and sanitizer configurations.
option(SIR_INPUT_SOAK_TESTS "Include extended native input stress soak in CTest" OFF)
if(SIR_INPUT_SOAK_TESTS)
    add_test(NAME core.platform.input.soak COMMAND SIRInputStressTests --soak)
    set_tests_properties(core.platform.input.soak PROPERTIES LABELS "core;input;soak" TIMEOUT 180)
endif()

# Sparse/dense frame snapshot differential, portable across Windows and Linux.
add_executable(SIRInputSnapshotDeltaTests SIRInputSnapshotDeltaTests.cpp)
target_link_libraries(SIRInputSnapshotDeltaTests PRIVATE SIR::Core)
add_test(NAME core.platform.input.snapshot-delta COMMAND SIRInputSnapshotDeltaTests)
set_tests_properties(core.platform.input.snapshot-delta PROPERTIES LABELS "core;input;regression" TIMEOUT 60)
sir_configure_owned_target(SIRInputSnapshotDeltaTests)

# Native evdev report batching under an instrumented pipe; no device permissions needed.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(SIRInputEvdevBatchTests SIRInputEvdevBatchTests.cpp)
    target_link_libraries(SIRInputEvdevBatchTests PRIVATE SIR::Core)
    target_link_options(SIRInputEvdevBatchTests PRIVATE "LINKER:--wrap=read" "LINKER:--wrap=ioctl")
    add_test(NAME core.platform.input.evdev-batch COMMAND SIRInputEvdevBatchTests)
    set_tests_properties(core.platform.input.evdev-batch PROPERTIES LABELS "core;input;regression" TIMEOUT 60)
    sir_configure_owned_target(SIRInputEvdevBatchTests)
endif()

# Linux X11 window/event acceptance with an actual ephemeral Xvfb server.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND SIR_WITH_X11_INPUT)
    find_package(X11 QUIET)
    find_program(SIR_XVFB_RUN NAMES xvfb-run)
    if(X11_FOUND AND SIR_XVFB_RUN)
        add_executable(SIRInputX11WindowTests SIRInputX11WindowTests.cpp)
        target_link_libraries(SIRInputX11WindowTests PRIVATE SIR::Core X11::X11)
        target_link_options(SIRInputX11WindowTests PRIVATE
            "LINKER:--wrap=XCreateSimpleWindow"
            "LINKER:--wrap=XCreateBitmapFromData"
            "LINKER:--wrap=XCreatePixmapCursor"
            "LINKER:--wrap=XGrabPointer"
            "LINKER:--wrap=XFilterEvent"
            "LINKER:--wrap=XNextEvent")
        add_test(NAME core.platform.input.x11-window COMMAND "${SIR_XVFB_RUN}" -a "$<TARGET_FILE:SIRInputX11WindowTests>")
        set_tests_properties(core.platform.input.x11-window PROPERTIES LABELS "core;input;x11;integration" TIMEOUT 60)
        sir_configure_owned_target(SIRInputX11WindowTests)
        add_executable(SIRInputX11IMEFallbackTests SIRInputX11IMEFallbackTests.cpp)
        target_link_libraries(SIRInputX11IMEFallbackTests PRIVATE SIR::Core X11::X11)
        target_link_options(SIRInputX11IMEFallbackTests PRIVATE "LINKER:--wrap=XCreateIC")
        add_test(NAME core.platform.input.x11-ime-fallback COMMAND "${SIR_XVFB_RUN}" -a "$<TARGET_FILE:SIRInputX11IMEFallbackTests>")
        set_tests_properties(core.platform.input.x11-ime-fallback PROPERTIES LABELS "core;input;x11;integration" TIMEOUT 60)
        sir_configure_owned_target(SIRInputX11IMEFallbackTests)
    endif()
endif()

# Win32 packet decoder regression: real MSVC / Windows SDK build, no hardware required.
if(WIN32)
    add_executable(SIRWindowsRawBatchTests SIRWindowsRawBatchTests.cpp)
    target_link_libraries(SIRWindowsRawBatchTests PRIVATE SIR::Core)
    add_test(NAME core.platform.input.windows-raw-batch COMMAND SIRWindowsRawBatchTests)
    set_tests_properties(core.platform.input.windows-raw-batch PROPERTIES LABELS "core;input;regression" TIMEOUT 60)
    sir_configure_owned_target(SIRWindowsRawBatchTests)
endif()

# Portable deterministic GameInput decoder regression; uses no OS or SDK.
add_executable(SIRGameInputCodecTests
    SIRGameInputCodecTests.cpp
    "${PROJECT_SOURCE_DIR}/Source/Core/Platform/Windows/SIRWindowsGameInputCodec.cpp")
target_link_libraries(SIRGameInputCodecTests PRIVATE SIR::Core)
add_test(NAME core.platform.input.gameinput-codec COMMAND SIRGameInputCodecTests)
set_tests_properties(core.platform.input.gameinput-codec PROPERTIES LABELS "core;input;regression" TIMEOUT 60)
sir_configure_owned_target(SIRGameInputCodecTests)


