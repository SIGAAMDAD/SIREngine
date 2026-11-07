# GRDG resource graph regression coverage.
add_executable(SIRResourceGraphTests SIRResourceGraphTests.cpp SIRResourceInterfaceTests.cpp)
target_link_libraries(SIRResourceGraphTests PRIVATE SIR::Resources)
add_test(NAME resources.graph.lifecycle COMMAND SIRResourceGraphTests)
sir_configure_owned_target(SIRResourceGraphTests)

add_executable(SIRResourceMemoryIntegrationTests SIRResourceMemoryIntegrationTests.cpp)
target_link_libraries(SIRResourceMemoryIntegrationTests PRIVATE SIR::Resources)
add_test(NAME resources.memory.domains COMMAND SIRResourceMemoryIntegrationTests)
sir_configure_owned_target(SIRResourceMemoryIntegrationTests)

# Domain, byte-size and alignment rejection in the GRDG-to-Core adapter.
add_executable(SIRResourceMemoryAdapterBoundaryTests SIRResourceMemoryAdapterBoundaryTests.cpp)
target_link_libraries(SIRResourceMemoryAdapterBoundaryTests PRIVATE SIR::Resources)
add_test(NAME resources.memory.adapter-rejection COMMAND SIRResourceMemoryAdapterBoundaryTests)
sir_configure_owned_target(SIRResourceMemoryAdapterBoundaryTests)

# Source-derived negative cases for caller-owned catalog metadata.
add_executable(SIRResourceCatalogBoundaryTests SIRResourceCatalogBoundaryTests.cpp)
target_link_libraries(SIRResourceCatalogBoundaryTests PRIVATE SIR::Resources)
add_test(NAME resources.catalog.boundary-negative COMMAND SIRResourceCatalogBoundaryTests)
sir_configure_owned_target(SIRResourceCatalogBoundaryTests)

# Native file/mapped-file loading through Core Jobs and GRDG staging.
add_executable(SIRResourceFileLoadTests SIRResourceFileLoadTests.cpp)
target_link_libraries(SIRResourceFileLoadTests PRIVATE SIR::Resources)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    foreach(sirWrappedCall IN ITEMS fstat mmap munmap pread)
        target_link_options(SIRResourceFileLoadTests PRIVATE "LINKER:--wrap=${sirWrappedCall}")
    endforeach()
endif()
add_test(NAME resources.streaming.native-file-io COMMAND SIRResourceFileLoadTests)
sir_configure_owned_target(SIRResourceFileLoadTests)

# Cross-subsystem stress matrix for native GRDG I/O, Core Jobs and staging.
add_executable(SIRInfrastructureIntegrationTests SIRInfrastructureIntegrationTests.cpp)
target_link_libraries(SIRInfrastructureIntegrationTests PRIVATE SIR::Resources)
add_test(NAME resources.integration.stress COMMAND SIRInfrastructureIntegrationTests)
set_tests_properties(resources.integration.stress PROPERTIES TIMEOUT 120)
sir_configure_owned_target(SIRInfrastructureIntegrationTests)

# Bounded provider-owned derived representations and fence-aware retirement.
add_executable(SIRDerivedResourceRegistryTests SIRDerivedResourceRegistryTests.cpp)
target_link_libraries(SIRDerivedResourceRegistryTests PRIVATE SIR::Resources)
add_test(NAME resources.derived.retirement COMMAND SIRDerivedResourceRegistryTests)
sir_configure_owned_target(SIRDerivedResourceRegistryTests)

add_executable(SIRTaskSchedulerTests SIRTaskSchedulerTests.cpp)
target_link_libraries(SIRTaskSchedulerTests PRIVATE SIR::Core)
add_test(NAME core.jobs.scheduler COMMAND SIRTaskSchedulerTests)
sir_configure_owned_target(SIRTaskSchedulerTests)

add_executable(SIRTaskGraphTests SIRTaskGraphTests.cpp)
target_link_libraries(SIRTaskGraphTests PRIVATE SIR::Core)
add_test(NAME core.jobs.graph COMMAND SIRTaskGraphTests)
sir_configure_owned_target(SIRTaskGraphTests)

add_executable(SIRTaskProviderTests SIRTaskProviderTests.cpp)
target_link_libraries(SIRTaskProviderTests PRIVATE SIR::Core)
add_test(NAME core.jobs.provider COMMAND SIRTaskProviderTests)
sir_configure_owned_target(SIRTaskProviderTests)

add_executable(SIRTaskFenceTrackerTests SIRTaskFenceTrackerTests.cpp)
target_link_libraries(SIRTaskFenceTrackerTests PRIVATE SIR::Core)
add_test(NAME core.jobs.fences COMMAND SIRTaskFenceTrackerTests)
sir_configure_owned_target(SIRTaskFenceTrackerTests)

add_executable(SIRTaskSchedulerStressTests SIRTaskSchedulerStressTests.cpp)
target_link_libraries(SIRTaskSchedulerStressTests PRIVATE SIR::Core)
add_test(NAME core.jobs.stress COMMAND SIRTaskSchedulerStressTests)
set_tests_properties(core.jobs.stress PROPERTIES TIMEOUT 90)
sir_configure_owned_target(SIRTaskSchedulerStressTests)

add_executable(SIRSchedulerExperimentTests SIRSchedulerExperimentTests.cpp)
target_link_libraries(SIRSchedulerExperimentTests PRIVATE SIR::Core)
add_test(NAME core.jobs.experiments COMMAND SIRSchedulerExperimentTests)
set_tests_properties(core.jobs.experiments PROPERTIES TIMEOUT 90)
sir_configure_owned_target(SIRSchedulerExperimentTests)

add_executable(SIRSchedulerQueueCandidateTests
    SIRSchedulerQueueCandidateTests.cpp
    ${PROJECT_SOURCE_DIR}/Tests/Benchmarks/SIRSchedulerQueueCandidates.cpp)
target_link_libraries(SIRSchedulerQueueCandidateTests PRIVATE SIR::Core)
add_test(NAME core.jobs.queue-candidates COMMAND SIRSchedulerQueueCandidateTests)
set_tests_properties(core.jobs.queue-candidates PROPERTIES TIMEOUT 90)
sir_configure_owned_target(SIRSchedulerQueueCandidateTests)


# Offline deterministic SIRPack writer and immutable catalog/shard reader.
add_executable(SIRPackFoundationTests SIRPackFoundationTests.cpp
    ${PROJECT_SOURCE_DIR}/Tools/SIRPackCooker/SIRPackCooker.cpp)
target_include_directories(SIRPackFoundationTests PRIVATE "${PROJECT_SOURCE_DIR}")
target_compile_definitions(SIRPackFoundationTests PRIVATE SIR_PACK_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/Tests/Fixtures/SIRPack")
target_link_libraries(SIRPackFoundationTests PRIVATE SIR::Resources)
add_test(NAME resources.sirpack.foundation COMMAND SIRPackFoundationTests)
sir_configure_owned_target(SIRPackFoundationTests)
