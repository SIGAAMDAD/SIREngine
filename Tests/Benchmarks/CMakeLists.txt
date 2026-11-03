add_executable(SIRCoreBenchmarks CoreBenchmarks.cpp SIRContainerAllocatorBenchmarks.cpp EventBenchmarks.cpp SIRInputBenchmarks.cpp)
target_compile_features(SIRCoreBenchmarks PRIVATE cxx_std_17)
target_link_libraries(SIRCoreBenchmarks PRIVATE SIR::Core benchmark::benchmark_main)
if(WIN32)
    # Windows headers may otherwise expose max/min macros into Google Benchmark.
    target_compile_definitions(SIRCoreBenchmarks PRIVATE NOMINMAX)
endif()
sir_configure_owned_target(SIRCoreBenchmarks)

# Prove the input benchmark cases remain registered in profiling binaries.
add_test(NAME profiling.input.registration COMMAND SIRCoreBenchmarks --benchmark_list_tests=true)
set_tests_properties(profiling.input.registration PROPERTIES PASS_REGULAR_EXPRESSION "SIRBenchmarkInputFramePublish" TIMEOUT 15)
