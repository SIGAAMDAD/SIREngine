# NomadFramework-derived allocator regression target, built against SIRCore.
add_executable(SIRAllocatorMigrationTests SIRAllocatorMigrationTests.cpp)
target_link_libraries(SIRAllocatorMigrationTests PRIVATE SIR::Core)
add_test(NAME core.memory.allocators COMMAND SIRAllocatorMigrationTests)
sir_configure_owned_target(SIRAllocatorMigrationTests)

# The exhaustive allocator suite is mandatory in CI's GoogleTest-enabled
# Windows/Linux presets. Each individual TEST is registered with CTest.
if(SIR_WITH_GOOGLETEST)
    add_executable(SIRAllocatorExhaustiveTests
        SIRAllocatorCoreGoogleTests.cpp
        SIRAllocatorLifetimeGoogleTests.cpp
        SIRAllocatorIntegrationGoogleTests.cpp
        SIRAllocatorNegativeGoogleTests.cpp)
    target_compile_features(SIRAllocatorExhaustiveTests PRIVATE cxx_std_17)
    target_link_libraries(SIRAllocatorExhaustiveTests PRIVATE SIR::Core gtest_main)
    sir_configure_owned_target(SIRAllocatorExhaustiveTests)
    include(GoogleTest)
    gtest_discover_tests(SIRAllocatorExhaustiveTests
        TEST_PREFIX "core.memory.allocators."
        DISCOVERY_TIMEOUT 45
        PROPERTIES LABELS "subsystem:core;kind:unit;gate:pr;gate:develop;gate:nightly;gate:qualification;nightly:repeat")
endif()
