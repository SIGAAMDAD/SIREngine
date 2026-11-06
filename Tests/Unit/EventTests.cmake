if(SIR_WITH_GOOGLETEST)
    add_executable(SIREventGoogleTests SIREventGoogleTests.cpp SIREventOptimizationGoogleTests.cpp SIREventFurtherOptimizationGoogleTests.cpp)
    target_compile_features(SIREventGoogleTests PRIVATE cxx_std_17)
    target_link_libraries(SIREventGoogleTests PRIVATE SIR::Core gtest_main)
    sir_configure_owned_target(SIREventGoogleTests)
    include(GoogleTest)
    gtest_discover_tests(SIREventGoogleTests TEST_PREFIX "core.events." DISCOVERY_TIMEOUT 45
        PROPERTIES LABELS "subsystem:core;kind:unit;gate:pr;gate:main;gate:nightly;gate:qualification")
endif()

# Standalone registration, capacity, invalid-handle and lifecycle coverage.
add_executable(SIREventRegistryStandaloneTests SIREventRegistryStandaloneTests.cpp)
target_link_libraries(SIREventRegistryStandaloneTests PRIVATE SIR::Core)
add_test(NAME core.events.registry-standalone COMMAND SIREventRegistryStandaloneTests)
sir_configure_owned_target(SIREventRegistryStandaloneTests)

# Independent allocation smoke without the GoogleTest runtime. Counts C++
# operator new/new[] after initialization across core event hot paths.
add_executable(SIREventNoAllocationTests SIREventNoAllocationTests.cpp)
target_link_libraries(SIREventNoAllocationTests PRIVATE SIR::Core)
add_test(NAME core.events.no-allocations COMMAND SIREventNoAllocationTests)
sir_configure_owned_target(SIREventNoAllocationTests)
