# HIZ-00 CPU reference oracle: no GPU, windowing, or FrameGraph required.
add_executable(SIRVisibilityFoundationTests SIRVisibilityFoundationTests.cpp)
target_link_libraries(SIRVisibilityFoundationTests PRIVATE SIR::Graphics)
add_test(NAME renderer.visibility.foundation COMMAND SIRVisibilityFoundationTests)
sir_configure_owned_target(SIRVisibilityFoundationTests)
