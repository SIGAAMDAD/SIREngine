add_executable(SIRWorldHLODTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRWorldHLODTests.cpp)
target_link_libraries(SIRWorldHLODTests PRIVATE SIR::WorldHLOD)
add_test(NAME world.hlod.selection COMMAND SIRWorldHLODTests)
sir_configure_owned_target(SIRWorldHLODTests)

add_executable(SIRWorldHLODGRDGTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRWorldHLODGRDGTests.cpp)
target_link_libraries(SIRWorldHLODGRDGTests PRIVATE SIR::WorldHLOD)
add_test(NAME world.hlod.grdg-residency COMMAND SIRWorldHLODGRDGTests)
sir_configure_owned_target(SIRWorldHLODGRDGTests)

add_executable(SIRWorldHLODManifestTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRWorldHLODManifestTests.cpp)
target_link_libraries(SIRWorldHLODManifestTests PRIVATE SIR::WorldHLOD)
add_test(NAME world.hlod.manifest COMMAND SIRWorldHLODManifestTests)
sir_configure_owned_target(SIRWorldHLODManifestTests)

add_executable(SIRWorldHLODPinTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRWorldHLODPinTests.cpp)
target_link_libraries(SIRWorldHLODPinTests PRIVATE SIR::WorldHLOD)
add_test(NAME world.hlod.pin-cut COMMAND SIRWorldHLODPinTests)
sir_configure_owned_target(SIRWorldHLODPinTests)

add_executable(SIRWorldHLODDemandTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRWorldHLODDemandTests.cpp)
target_link_libraries(SIRWorldHLODDemandTests PRIVATE SIR::WorldHLOD)
add_test(NAME world.hlod.demand-reconcile COMMAND SIRWorldHLODDemandTests)
sir_configure_owned_target(SIRWorldHLODDemandTests)

add_executable(SIRWorldHLODProjectionTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRWorldHLODProjectionTests.cpp)
target_link_libraries(SIRWorldHLODProjectionTests PRIVATE SIR::WorldHLOD)
add_test(NAME world.hlod.projection COMMAND SIRWorldHLODProjectionTests)
sir_configure_owned_target(SIRWorldHLODProjectionTests)
