add_executable(SIRWorldHLODTests SIRWorldHLODTests.cpp)
target_link_libraries(SIRWorldHLODTests PRIVATE SIR::WorldHLOD)
add_test(NAME world.hlod.selection COMMAND SIRWorldHLODTests)
sir_configure_owned_target(SIRWorldHLODTests)
