# Shared renderer contract tests run on Windows/Linux without a GPU or native driver.
add_executable(SIRGraphicsInterfaceTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRGraphicsInterfaceTests.cpp)
target_link_libraries(SIRGraphicsInterfaceTests PRIVATE SIR::Graphics)
add_test(NAME renderer.graphics.interface COMMAND SIRGraphicsInterfaceTests)
sir_configure_owned_target(SIRGraphicsInterfaceTests)

add_executable(SIRGraphicsABITests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRGraphicsABITests.cpp)
target_link_libraries(SIRGraphicsABITests PRIVATE SIR::Graphics)
add_test(NAME renderer.graphics.binary-abi COMMAND SIRGraphicsABITests)
sir_configure_owned_target(SIRGraphicsABITests)

add_executable(SIRGraphicsPipelineABITests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRGraphicsPipelineABITests.cpp)
target_link_libraries(SIRGraphicsPipelineABITests PRIVATE SIR::Graphics)
add_test(NAME renderer.graphics.pipeline-eras COMMAND SIRGraphicsPipelineABITests)
sir_configure_owned_target(SIRGraphicsPipelineABITests)

add_executable(SIRGraphicsEraRHITests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRGraphicsEraRHITests.cpp)
target_link_libraries(SIRGraphicsEraRHITests PRIVATE SIR::Graphics)
add_test(NAME renderer.graphics.era-rhi COMMAND SIRGraphicsEraRHITests)
sir_configure_owned_target(SIRGraphicsEraRHITests)

add_executable(SIRGraphicsModernRHITests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRGraphicsModernRHITests.cpp)
target_link_libraries(SIRGraphicsModernRHITests PRIVATE SIR::Graphics)
add_test(NAME renderer.graphics.modern-rhi COMMAND SIRGraphicsModernRHITests)
sir_configure_owned_target(SIRGraphicsModernRHITests)

add_executable(SIRModernFrameGraphExecutionTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRModernFrameGraphExecutionTests.cpp)
target_link_libraries(SIRModernFrameGraphExecutionTests PRIVATE SIR::Graphics)
add_test(NAME renderer.graphics.modern-framegraph COMMAND SIRModernFrameGraphExecutionTests)
sir_configure_owned_target(SIRModernFrameGraphExecutionTests)

add_executable(SIRGraphicsPipelineBridgeTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRGraphicsPipelineBridgeTests.cpp)
target_link_libraries(SIRGraphicsPipelineBridgeTests PRIVATE SIR::Graphics)
add_test(NAME renderer.graphics.pipeline-bridge COMMAND SIRGraphicsPipelineBridgeTests)
sir_configure_owned_target(SIRGraphicsPipelineBridgeTests)

add_library(SIRGraphicsABIMockModule SHARED ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRGraphicsABIMockModule.cpp)
target_link_libraries(SIRGraphicsABIMockModule PRIVATE SIR::Graphics)
sir_configure_owned_target(SIRGraphicsABIMockModule)

add_executable(SIRGraphicsModuleTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRGraphicsModuleTests.cpp)
target_link_libraries(SIRGraphicsModuleTests PRIVATE SIR::Graphics)
add_dependencies(SIRGraphicsModuleTests SIRGraphicsABIMockModule)
target_compile_definitions(SIRGraphicsModuleTests PRIVATE SIR_TEST_ABI_MODULE_PATH="$<TARGET_FILE:SIRGraphicsABIMockModule>")
add_dependencies(SIRGraphicsModuleTests SIRPlatformTestModule)
target_compile_definitions(SIRGraphicsModuleTests PRIVATE SIR_TEST_NO_ABI_MODULE_PATH="$<TARGET_FILE:SIRPlatformTestModule>")
# Inject one OS unload error only inside this unit executable, never in the
# production library or other tests. The injection is Linux linker-local.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(SIRGraphicsModuleTests PRIVATE "LINKER:--wrap=dlclose")
endif()
add_test(NAME renderer.graphics.binary-module COMMAND SIRGraphicsModuleTests)
sir_configure_owned_target(SIRGraphicsModuleTests)

# Shared ABI v1 conformance is runnable without Vulkan, Direct3D or a GPU.
# The independent suites keep layout drift, pipeline eras and invalid inputs
# separately addressable by CTest, sanitizers and source-based LLVM coverage.
add_executable(SIRGraphicsABIContractTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRGraphicsABIContractTests.cpp)
target_link_libraries(SIRGraphicsABIContractTests PRIVATE SIR::Graphics)
add_test(NAME renderer.graphics.contract COMMAND SIRGraphicsABIContractTests)
sir_configure_owned_target(SIRGraphicsABIContractTests)

add_executable(SIRGraphicsABIPropertyTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRGraphicsABIPropertyTests.cpp)
target_link_libraries(SIRGraphicsABIPropertyTests PRIVATE SIR::Graphics)
add_test(NAME renderer.graphics.properties COMMAND SIRGraphicsABIPropertyTests)
sir_configure_owned_target(SIRGraphicsABIPropertyTests)

add_executable(SIRGraphicsABINegativeMatrixTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRGraphicsABINegativeMatrixTests.cpp)
target_link_libraries(SIRGraphicsABINegativeMatrixTests PRIVATE SIR::Graphics)
add_test(NAME renderer.graphics.negative-matrix COMMAND SIRGraphicsABINegativeMatrixTests)
sir_configure_owned_target(SIRGraphicsABINegativeMatrixTests)

add_executable(SIRGraphicsABIEraMatrixTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRGraphicsABIEraMatrixTests.cpp)
target_link_libraries(SIRGraphicsABIEraMatrixTests PRIVATE SIR::Graphics)
add_test(NAME renderer.graphics.era-matrix COMMAND SIRGraphicsABIEraMatrixTests)
sir_configure_owned_target(SIRGraphicsABIEraMatrixTests)

set_tests_properties(
    renderer.graphics.interface
    renderer.graphics.binary-abi
    renderer.graphics.pipeline-eras
    renderer.graphics.pipeline-bridge
    renderer.graphics.modern-rhi
    renderer.graphics.era-rhi
    renderer.graphics.modern-framegraph
    renderer.graphics.binary-module
    renderer.graphics.contract
    renderer.graphics.properties
    renderer.graphics.era-matrix
    renderer.graphics.negative-matrix
    PROPERTIES LABELS "graphics-abi;cpu;conformance" TIMEOUT 90)

# GPU surface owns the native window lease until backend retirement succeeds.
# Compile the real production adapter/surface against a fault-injected ABI shim.
add_executable(SIRWindowABISurfaceTests
    ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRWindowABISurfaceTests.cpp
    "${PROJECT_SOURCE_DIR}/Source/Renderer/SIRWindowABISurface.cpp"
    "${PROJECT_SOURCE_DIR}/Source/Renderer/SIRWindowSurfaceAdapter.cpp")
target_link_libraries(SIRWindowABISurfaceTests PRIVATE SIR::Core)
add_test(NAME renderer.window.abi-lease COMMAND SIRWindowABISurfaceTests)
set_tests_properties(renderer.window.abi-lease PROPERTIES LABELS "graphics-abi;window;cpu;integration" TIMEOUT 60)
sir_configure_owned_target(SIRWindowABISurfaceTests)

if(SIR_WITH_GOOGLETEST)
    string(TOLOWER "${CMAKE_SYSTEM_NAME}" sirGTestHost)
    include(GoogleTest)
    add_executable(SIRGraphicsABIAssertionGoogleTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRGraphicsABIAssertionGoogleTests.cpp)
    target_link_libraries(SIRGraphicsABIAssertionGoogleTests PRIVATE SIR::Graphics gtest_main)
    sir_configure_owned_target(SIRGraphicsABIAssertionGoogleTests)
    gtest_discover_tests(SIRGraphicsABIAssertionGoogleTests
        TEST_PREFIX "renderer.graphics.gtest."
        DISCOVERY_MODE PRE_TEST
        PROPERTIES LABELS "graphics-abi;gtest;conformance;subsystem:renderer;kind:unit;platform:${sirGTestHost};resource:cpu;gate:pr" TIMEOUT 90)
endif()

if(SIR_WITH_OPENGL)
    add_executable(SIRGLCore33Tests ${PROJECT_SOURCE_DIR}/Tests/Unit/Renderer/OpenGL/SIRGLCore33Tests.cpp)
    target_link_libraries(SIRGLCore33Tests PRIVATE SIR::GLProgrammableRHI)
    sir_configure_owned_target(SIRGLCore33Tests)
    add_test(NAME renderer.opengl.core33 COMMAND SIRGLCore33Tests)

endif()


include(${PROJECT_SOURCE_DIR}/Tests/Unit/StringPoolTests.cmake)


add_executable(SIRSharedNativeGPUBackingTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRSharedNativeGPUBackingTests.cpp)
target_link_libraries(SIRSharedNativeGPUBackingTests PRIVATE SIR::Graphics SIR::Resources)
add_test(NAME renderer.grdg.native-backing-ownership COMMAND SIRSharedNativeGPUBackingTests)
sir_configure_owned_target(SIRSharedNativeGPUBackingTests)

if(SIR_WITH_VULKAN)
    add_executable(SIRSharedNativeGPUVulkanTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRSharedNativeGPUVulkanTests.cpp)
    target_link_libraries(SIRSharedNativeGPUVulkanTests PRIVATE SIR::Vulkan SIR::Graphics SIR::Resources)
    sir_configure_owned_target(SIRSharedNativeGPUVulkanTests)
    add_test(NAME renderer.grdg.vulkan-native-single-backing COMMAND SIRSharedNativeGPUVulkanTests)
    set_tests_properties(renderer.grdg.vulkan-native-single-backing PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 60)
endif()
