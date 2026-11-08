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
add_executable(SIRPackFoundationTests SIRPackFoundationTests.cpp)
target_include_directories(SIRPackFoundationTests PRIVATE "${PROJECT_SOURCE_DIR}")
target_compile_definitions(SIRPackFoundationTests PRIVATE SIR_PACK_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/Tests/Fixtures/SIRPack")
target_link_libraries(SIRPackFoundationTests PRIVATE SIR::PackCooker)
add_test(NAME resources.sirpack.foundation COMMAND SIRPackFoundationTests)
sir_configure_owned_target(SIRPackFoundationTests)

add_executable(SIRPackShippingGuardTests SIRPackShippingGuardTests.cpp)
target_link_libraries(SIRPackShippingGuardTests PRIVATE SIR::PackCooker)
add_test(NAME resources.sirpack.shipping-source-guard COMMAND SIRPackShippingGuardTests)
sir_configure_owned_target(SIRPackShippingGuardTests)

add_executable(SIRMeshGRDGSourceTests SIRMeshGRDGSourceTests.cpp)
target_link_libraries(SIRMeshGRDGSourceTests PRIVATE SIR::MeshGRDGSource SIR::MeshCollisionSource)
add_test(NAME mesh.grdg.canonical-source-lifetime COMMAND SIRMeshGRDGSourceTests)
sir_configure_owned_target(SIRMeshGRDGSourceTests)

add_executable(SIRMeshPackSchemaTests SIRMeshPackSchemaTests.cpp)
target_link_libraries(SIRMeshPackSchemaTests PRIVATE SIR::MeshPackSchema)
add_test(NAME mesh.sirpack.schema COMMAND SIRMeshPackSchemaTests)
sir_configure_owned_target(SIRMeshPackSchemaTests)

add_executable(SIRDisplacementCookerTests SIRDisplacementCookerTests.cpp)
target_include_directories(SIRDisplacementCookerTests PRIVATE "${PROJECT_SOURCE_DIR}")
target_link_libraries(SIRDisplacementCookerTests PRIVATE SIR::DisplacementCooker)
add_test(NAME mesh.geometry.displacement-bake COMMAND SIRDisplacementCookerTests)
sir_configure_owned_target(SIRDisplacementCookerTests)

if(TARGET SIRGeometryCooker)
    add_executable(SIRGeometryCookerTests SIRGeometryCookerTests.cpp)
    target_include_directories(SIRGeometryCookerTests PRIVATE "${PROJECT_SOURCE_DIR}")
    target_link_libraries(SIRGeometryCookerTests PRIVATE SIR::GeometryCooker)
    add_test(NAME mesh.geometry.cooker COMMAND SIRGeometryCookerTests)
    sir_configure_owned_target(SIRGeometryCookerTests)
endif()

if(TARGET sir-geometry-cooker)
    add_test(NAME mesh.geometry.cli.displacement
        COMMAND "${CMAKE_COMMAND}"
            "-DSIR_GEOMETRY_COOKER=$<TARGET_FILE:sir-geometry-cooker>"
            "-DSIR_DISPLACEMENT_OBJ=${PROJECT_SOURCE_DIR}/Tests/Fixtures/Geometry/displacement.obj"
            "-DSIR_DISPLACEMENT_IMAGE=${PROJECT_SOURCE_DIR}/Tests/Fixtures/Geometry/displacement.pgm"
            -P "${PROJECT_SOURCE_DIR}/Tests/Unit/VerifyDisplacementCookCLI.cmake")
    foreach(sirGeometryFixture IN ITEMS rock building terrain degenerate)
        add_test(NAME "mesh.geometry.cli.${sirGeometryFixture}"
            COMMAND "${CMAKE_COMMAND}"
                "-DSIR_GEOMETRY_COOKER=$<TARGET_FILE:sir-geometry-cooker>"
                "-DSIR_GEOMETRY_INPUT=${PROJECT_SOURCE_DIR}/Tests/Fixtures/Geometry/${sirGeometryFixture}.obj"
                "-DSIR_GEOMETRY_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/geometry-${sirGeometryFixture}"
                "-DSIR_GEOMETRY_EXPECT_FAILURE=$<STREQUAL:${sirGeometryFixture},degenerate>"
                -P "${PROJECT_SOURCE_DIR}/Tests/Unit/VerifyGeometryCookCLI.cmake")
    endforeach()
endif()

add_executable(SIRMeshCollisionSourceTests SIRMeshCollisionSourceTests.cpp)
target_link_libraries(SIRMeshCollisionSourceTests PRIVATE SIR::MeshCollisionSource)
add_test(NAME mesh.collision.shared-geometry-raycast COMMAND SIRMeshCollisionSourceTests)
sir_configure_owned_target(SIRMeshCollisionSourceTests)
