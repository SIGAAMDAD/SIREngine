add_executable(SIRCoreBenchmarks CoreBenchmarks.cpp SIRContainerAllocatorBenchmarks.cpp EventBenchmarks.cpp)
target_compile_features(SIRCoreBenchmarks PRIVATE cxx_std_17)
target_link_libraries(SIRCoreBenchmarks PRIVATE SIR::Core benchmark::benchmark_main)
if(WIN32)
    # Windows headers may otherwise expose max/min macros into Google Benchmark.
    target_compile_definitions(SIRCoreBenchmarks PRIVATE NOMINMAX)
endif()
sir_configure_owned_target(SIRCoreBenchmarks)

# Benchmarks are compiled separately from coverage-instrumented test binaries.
add_executable(SIRScriptBenchmarks SIRLangBenchmarks.cpp)
target_compile_features(SIRScriptBenchmarks PRIVATE cxx_std_17)
target_link_libraries(SIRScriptBenchmarks PRIVATE SIR::Scripting benchmark::benchmark_main)
if(WIN32)
    target_compile_definitions(SIRScriptBenchmarks PRIVATE NOMINMAX)
endif()
sir_configure_owned_target(SIRScriptBenchmarks)
