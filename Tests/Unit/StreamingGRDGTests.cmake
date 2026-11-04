# Concrete file/GRDG tests are available only after PR #23's Resources
# implementation is part of the same CMake source tree.
if(TARGET SIR::StreamingGRDG)
    add_executable(SIRStreamingGRDGAdapterTests SIRStreamingGRDGAdapterTests.cpp)
    target_link_libraries(SIRStreamingGRDGAdapterTests PRIVATE SIR::StreamingGRDG)
    add_test(NAME streaming.grdg.file-load COMMAND SIRStreamingGRDGAdapterTests)
    sir_configure_owned_target(SIRStreamingGRDGAdapterTests)
endif()
