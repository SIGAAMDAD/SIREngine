add_executable(SIRWorldPartitionTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRWorldPartitionTests.cpp)
target_link_libraries(SIRWorldPartitionTests PRIVATE SIR::World)
add_test(NAME streaming.world.partition COMMAND SIRWorldPartitionTests)
sir_configure_owned_target(SIRWorldPartitionTests)
