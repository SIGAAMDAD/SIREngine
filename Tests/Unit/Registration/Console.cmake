# Registration, Console and CVar admission and archive fixtures.
add_executable(SIRIniConfigTests ${PROJECT_SOURCE_DIR}/Tests/Unit/Config/SIRIniTests.cpp)
target_link_libraries(SIRIniConfigTests PRIVATE SIR::Core)
add_test(NAME core.config.ini COMMAND SIRIniConfigTests)
sir_configure_owned_target(SIRIniConfigTests)

add_executable(SIRConsoleUnifiedTests ${PROJECT_SOURCE_DIR}/Tests/Unit/Console/SIRConsoleUnifiedTests.cpp)
target_link_libraries(SIRConsoleUnifiedTests PRIVATE SIR::Core)
add_test(NAME core.console.unified COMMAND SIRConsoleUnifiedTests)
sir_configure_owned_target(SIRConsoleUnifiedTests)

