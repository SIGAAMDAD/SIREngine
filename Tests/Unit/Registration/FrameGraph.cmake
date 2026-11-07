# Device-independent FrameGraph DAG, hazard and subresource validation.
add_executable(SIRFrameGraphTests SIRFrameGraphTests.cpp)
target_link_libraries(SIRFrameGraphTests PRIVATE SIR::Graphics)
add_test(NAME renderer.framegraph.compiler COMMAND SIRFrameGraphTests)
set_tests_properties(renderer.framegraph.compiler PROPERTIES LABELS "framegraph;cpu;conformance" TIMEOUT 90)
sir_configure_owned_target(SIRFrameGraphTests)

# Provider-admitted FrameGraph transient residency planning and negative cases.
add_executable(SIRFrameGraphTransientTests SIRFrameGraphTransientTests.cpp)
target_link_libraries(SIRFrameGraphTransientTests PRIVATE SIR::Graphics)
add_test(NAME renderer.framegraph.transient COMMAND SIRFrameGraphTransientTests)
set_tests_properties(renderer.framegraph.transient PROPERTIES LABELS "framegraph;cpu;conformance" TIMEOUT 90)
sir_configure_owned_target(SIRFrameGraphTransientTests)

include(${CMAKE_CURRENT_SOURCE_DIR}/MeshCacheTests.cmake)

# Cross-owner provider completion and graphics binary boundary conformance.
add_executable(SIRProviderCompletionTests SIRProviderCompletionTests.cpp)
target_link_libraries(SIRProviderCompletionTests PRIVATE SIR::Resources)
add_test(NAME resources.provider.multilane-contract COMMAND SIRProviderCompletionTests)
sir_configure_owned_target(SIRProviderCompletionTests)

add_executable(SIRGraphicsABISharedBoundaryTests SIRGraphicsABISharedBoundaryTests.cpp)
target_link_libraries(SIRGraphicsABISharedBoundaryTests PRIVATE SIR::Graphics)
add_test(NAME renderer.graphics.shared-boundary COMMAND SIRGraphicsABISharedBoundaryTests)
sir_configure_owned_target(SIRGraphicsABISharedBoundaryTests)


if(SIR_WITH_VULKAN)
    add_executable(SIRVulkanMemoryPolicyTests SIRVulkanMemoryPolicyTests.cpp)
    target_link_libraries(SIRVulkanMemoryPolicyTests PRIVATE SIR::Vulkan)
    add_test(NAME render.vulkan.memory-policy COMMAND SIRVulkanMemoryPolicyTests)
    sir_configure_owned_target(SIRVulkanMemoryPolicyTests)

    add_executable(SIRVulkanSealedPoolTests SIRVulkanSealedPoolTests.cpp)
    target_link_libraries(SIRVulkanSealedPoolTests PRIVATE SIR::Vulkan)
    add_test(NAME render.vulkan.sealed-pools COMMAND SIRVulkanSealedPoolTests)
    set_tests_properties(render.vulkan.sealed-pools PROPERTIES SKIP_RETURN_CODE 77)
    sir_configure_owned_target(SIRVulkanSealedPoolTests)

    add_executable(SIRVulkanDeviceTests SIRVulkanDeviceTests.cpp)
    target_link_libraries(SIRVulkanDeviceTests PRIVATE SIR::Vulkan)
    add_test(NAME render.vulkan.headless COMMAND SIRVulkanDeviceTests)
    set_tests_properties(render.vulkan.headless PROPERTIES SKIP_RETURN_CODE 77)
    sir_configure_owned_target(SIRVulkanDeviceTests)

    add_executable(SIRVulkanContractTests SIRVulkanContractTests.cpp)
    target_link_libraries(SIRVulkanContractTests PRIVATE SIR::Vulkan)
    add_test(NAME render.vulkan.contracts COMMAND SIRVulkanContractTests)
    sir_configure_owned_target(SIRVulkanContractTests)

    add_executable(SIRVulkanSharedContractTests SIRVulkanSharedContractTests.cpp)
    target_link_libraries(SIRVulkanSharedContractTests PRIVATE SIR::Vulkan)
    add_test(NAME render.vulkan.shared-contracts COMMAND SIRVulkanSharedContractTests)
    sir_configure_owned_target(SIRVulkanSharedContractTests)

    add_executable(SIRVulkanShaderTests SIRVulkanShaderTests.cpp)
    target_link_libraries(SIRVulkanShaderTests PRIVATE SIR::Vulkan)
    add_test(NAME render.vulkan.shader-envelope COMMAND SIRVulkanShaderTests)
    sir_configure_owned_target(SIRVulkanShaderTests)

    add_executable(SIRVulkanBufferBarrierTests SIRVulkanBufferBarrierTests.cpp)
    target_link_libraries(SIRVulkanBufferBarrierTests PRIVATE SIR::Vulkan)
    add_test(NAME render.vulkan.buffer-barriers COMMAND SIRVulkanBufferBarrierTests)
    sir_configure_owned_target(SIRVulkanBufferBarrierTests)

    add_executable(SIRVulkanBarrierTests SIRVulkanBarrierTests.cpp)
    target_link_libraries(SIRVulkanBarrierTests PRIVATE SIR::Vulkan)
    add_test(NAME render.vulkan.barriers COMMAND SIRVulkanBarrierTests)
    sir_configure_owned_target(SIRVulkanBarrierTests)

    add_executable(SIRVulkanTransferLayoutTests SIRVulkanTransferLayoutTests.cpp)
    target_link_libraries(SIRVulkanTransferLayoutTests PRIVATE SIR::Vulkan)
    add_test(NAME render.vulkan.transfer-layout COMMAND SIRVulkanTransferLayoutTests)
    sir_configure_owned_target(SIRVulkanTransferLayoutTests)

    add_executable(SIRVulkanRecordedCommandsTests SIRVulkanRecordedCommandsTests.cpp)
    target_link_libraries(SIRVulkanRecordedCommandsTests PRIVATE SIR::Vulkan)
    add_test(NAME render.vulkan.recorded-commands COMMAND SIRVulkanRecordedCommandsTests)
    sir_configure_owned_target(SIRVulkanRecordedCommandsTests)

    add_executable(SIRVulkanCopyBatchTests SIRVulkanCopyBatchTests.cpp)
    target_link_libraries(SIRVulkanCopyBatchTests PRIVATE SIR::Vulkan)
    add_test(NAME render.vulkan.copy-batch COMMAND SIRVulkanCopyBatchTests)
    sir_configure_owned_target(SIRVulkanCopyBatchTests)

    add_executable(SIRVulkanSharedTests SIRVulkanSharedTests.cpp)
    target_link_libraries(SIRVulkanSharedTests PRIVATE SIR::Vulkan)
    add_test(NAME render.vulkan.shared-api COMMAND SIRVulkanSharedTests)
    set_tests_properties(render.vulkan.shared-api PROPERTIES SKIP_RETURN_CODE 77)
    sir_configure_owned_target(SIRVulkanSharedTests)
endif()


include(${CMAKE_CURRENT_SOURCE_DIR}/SIRD3D12Tests.cmake)

# Native D3D11 registration remains owned by its local CMake fragment.
add_subdirectory(Renderer/D3D11)


# Driver-neutral per-pass barriers and cross-queue dependency lowering.
add_executable(SIRFrameGraphSubmissionTests SIRFrameGraphSubmissionTests.cpp)
target_link_libraries(SIRFrameGraphSubmissionTests PRIVATE SIR::Graphics)
add_test(NAME renderer.framegraph.submission COMMAND SIRFrameGraphSubmissionTests)
set_tests_properties(renderer.framegraph.submission PROPERTIES LABELS "framegraph;cpu;conformance" TIMEOUT 90)
sir_configure_owned_target(SIRFrameGraphSubmissionTests)

# Native-ABI mock execution: barriers, submission waits, partial completions.
add_executable(SIRFrameGraphNativeTests SIRFrameGraphNativeTests.cpp)
target_link_libraries(SIRFrameGraphNativeTests PRIVATE SIR::Graphics)
add_test(NAME renderer.framegraph.native COMMAND SIRFrameGraphNativeTests)
set_tests_properties(renderer.framegraph.native PROPERTIES LABELS "framegraph;cpu;conformance" TIMEOUT 90)
sir_configure_owned_target(SIRFrameGraphNativeTests)

# Direct C++ native rendering-device bridge; same lowerer as binary ABI.
add_executable(SIRFrameGraphDeviceBridgeTests SIRFrameGraphDeviceBridgeTests.cpp)
target_link_libraries(SIRFrameGraphDeviceBridgeTests PRIVATE SIR::Graphics)
add_test(NAME renderer.framegraph.device-bridge COMMAND SIRFrameGraphDeviceBridgeTests)
set_tests_properties(renderer.framegraph.device-bridge PROPERTIES LABELS "framegraph;cpu;conformance" TIMEOUT 90)
sir_configure_owned_target(SIRFrameGraphDeviceBridgeTests)

# Translates device queue fences to explicit GRDG provider lanes.
add_executable(SIRFrameGraphCompletionTests SIRFrameGraphCompletionTests.cpp)
target_link_libraries(SIRFrameGraphCompletionTests PRIVATE SIR::Graphics)
add_test(NAME renderer.framegraph.completion COMMAND SIRFrameGraphCompletionTests)
set_tests_properties(renderer.framegraph.completion PROPERTIES LABELS "framegraph;cpu;conformance" TIMEOUT 90)
sir_configure_owned_target(SIRFrameGraphCompletionTests)
