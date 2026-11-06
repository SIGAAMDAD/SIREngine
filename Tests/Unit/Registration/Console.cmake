# Registration, Console and CVar admission and archive fixtures.
add_executable(SIRRegistrationContractTests SIRRegistrationContractTests.cpp)
target_link_libraries(SIRRegistrationContractTests PRIVATE SIR::Core)
add_test(NAME core.registration.contract COMMAND SIRRegistrationContractTests)
sir_configure_owned_target(SIRRegistrationContractTests)

add_executable(SIRRegistrationStringBridgeTests SIRRegistrationStringBridgeTests.cpp)
target_link_libraries(SIRRegistrationStringBridgeTests PRIVATE SIR::Core)
add_test(NAME core.registration.string-bridge COMMAND SIRRegistrationStringBridgeTests)
sir_configure_owned_target(SIRRegistrationStringBridgeTests)

add_executable(SIRRegistrationCookerTests SIRRegistrationCookerTests.cpp)
target_link_libraries(SIRRegistrationCookerTests PRIVATE SIRRegistrationCookerLib SIR::Core)
add_test(NAME core.registration.cooker COMMAND SIRRegistrationCookerTests)
sir_configure_owned_target(SIRRegistrationCookerTests)

add_test(NAME core.registration.cooker-cli COMMAND "${CMAKE_COMMAND}"
    "-DSIR_COOKER=$<TARGET_FILE:SIRRegistrationCooker>"
    "-DSIR_FIXTURES=${PROJECT_SOURCE_DIR}/Tests/Unit/Registration"
    "-DSIR_OUT=${PROJECT_BINARY_DIR}/RegistrationCookerSmoke"
    -P "${PROJECT_SOURCE_DIR}/Tests/Unit/Registration/CookerCLI.cmake")

add_executable(SIRCVarRegistryTests CVars/SIRCVarRegistryIntegrationTests.cpp)
target_link_libraries(SIRCVarRegistryTests PRIVATE SIRRegistrationCookerLib SIR::Core)
add_test(NAME core.cvars.registry COMMAND SIRCVarRegistryTests)
sir_configure_owned_target(SIRCVarRegistryTests)

# Exhaustive strict textual codec admission, overflow and malformed-input checks.
add_executable(SIRCVarValueCodecBoundaryTests CVars/SIRCVarValueCodecBoundaryTests.cpp)
target_link_libraries(SIRCVarValueCodecBoundaryTests PRIVATE SIR::Core)
add_test(NAME core.cvars.codec-boundaries COMMAND SIRCVarValueCodecBoundaryTests)
sir_configure_owned_target(SIRCVarValueCodecBoundaryTests)

add_executable(SIRConsoleServiceTests Console/SIRConsoleServiceTests.cpp)
target_link_libraries(SIRConsoleServiceTests PRIVATE SIRRegistrationCookerLib SIR::Core)
add_test(NAME core.console.service COMMAND SIRConsoleServiceTests)
sir_configure_owned_target(SIRConsoleServiceTests)

add_executable(SIRIniConfigTests Config/SIRIniTests.cpp)
target_link_libraries(SIRIniConfigTests PRIVATE SIR::Core)
add_test(NAME core.config.ini COMMAND SIRIniConfigTests)
sir_configure_owned_target(SIRIniConfigTests)

add_executable(SIRCVarConfigTests CVars/SIRCVarConfigTests.cpp)
target_link_libraries(SIRCVarConfigTests PRIVATE SIRRegistrationCookerLib SIR::Core)
add_test(NAME core.cvars.config COMMAND SIRCVarConfigTests)
sir_configure_owned_target(SIRCVarConfigTests)

