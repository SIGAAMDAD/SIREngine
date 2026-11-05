# HIZ-00 CPU reference oracle: no GPU, windowing, or FrameGraph required.
add_executable(SIRVisibilityFoundationTests SIRVisibilityFoundationTests.cpp)
target_link_libraries(SIRVisibilityFoundationTests PRIVATE SIR::Graphics)
add_test(NAME renderer.visibility.foundation COMMAND SIRVisibilityFoundationTests)
sir_configure_owned_target(SIRVisibilityFoundationTests)

# Discover each HIZ-00 GoogleTest case as an independent native CTest.
if(SIR_WITH_GOOGLETEST)
    include(GoogleTest)
    add_executable(SIRVisibilityFoundationGoogleTests SIRVisibilityFoundationGoogleTests.cpp)
    target_link_libraries(SIRVisibilityFoundationGoogleTests PRIVATE SIR::Graphics gtest_main)
    gtest_discover_tests(SIRVisibilityFoundationGoogleTests TEST_PREFIX "renderer.visibility.gtest.")
    sir_configure_owned_target(SIRVisibilityFoundationGoogleTests)
endif()

# HIZ-03 driver-neutral depth source, view-rect and mip dispatch planner.
add_executable(SIRHiZDispatchTests SIRHiZDispatchTests.cpp)
target_link_libraries(SIRHiZDispatchTests PRIVATE SIR::Graphics)
add_test(NAME renderer.visibility.dispatch COMMAND SIRHiZDispatchTests)
sir_configure_owned_target(SIRHiZDispatchTests)
