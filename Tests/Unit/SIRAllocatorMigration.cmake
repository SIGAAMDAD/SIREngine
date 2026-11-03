# NomadFramework-derived allocator regression target, built against SIRCore.
add_executable(SIRAllocatorMigrationTests SIRAllocatorMigrationTests.cpp)
target_link_libraries(SIRAllocatorMigrationTests PRIVATE SIR::Core)
add_test(NAME core.memory.allocators COMMAND SIRAllocatorMigrationTests)
sir_configure_owned_target(SIRAllocatorMigrationTests)
