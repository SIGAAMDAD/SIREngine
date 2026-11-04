add_executable(SIRCoreBenchmarks
    CoreBenchmarks.cpp
    SIRContainerAllocatorBenchmarks.cpp
    EventBenchmarks.cpp
    SIRTaskSchedulerBenchmarks.cpp
    SIRSchedulerExperimentBenchmarks.cpp
    SIRSchedulerWorkloadReplay.cpp
    SIRSchedulerQueueCandidates.cpp
    SIRSchedulerQueueCandidateBenchmarks.cpp
    SIRGRDGInfrastructureBenchmarks.cpp)
target_compile_features(SIRCoreBenchmarks PRIVATE cxx_std_17)
target_link_libraries(SIRCoreBenchmarks PRIVATE SIR::Core SIR::Resources benchmark::benchmark_main)
if(WIN32)
    # Windows headers may otherwise expose max/min macros into Google Benchmark.
    target_compile_definitions(SIRCoreBenchmarks PRIVATE NOMINMAX)
endif()
sir_configure_owned_target(SIRCoreBenchmarks)
