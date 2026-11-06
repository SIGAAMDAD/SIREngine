# Shared renderer contract tests run on Windows/Linux without a GPU or native driver.
add_executable(SIRGraphicsInterfaceTests SIRGraphicsInterfaceTests.cpp)
target_link_libraries(SIRGraphicsInterfaceTests PRIVATE SIR::Graphics)
add_test(NAME renderer.graphics.interface COMMAND SIRGraphicsInterfaceTests)
sir_configure_owned_target(SIRGraphicsInterfaceTests)

add_executable(SIRGraphicsABITests SIRGraphicsABITests.cpp)
target_link_libraries(SIRGraphicsABITests PRIVATE SIR::Graphics)
add_test(NAME renderer.graphics.binary-abi COMMAND SIRGraphicsABITests)
sir_configure_owned_target(SIRGraphicsABITests)

add_executable(SIRGraphicsPipelineABITests SIRGraphicsPipelineABITests.cpp)
target_link_libraries(SIRGraphicsPipelineABITests PRIVATE SIR::Graphics)
add_test(NAME renderer.graphics.pipeline-eras COMMAND SIRGraphicsPipelineABITests)
sir_configure_owned_target(SIRGraphicsPipelineABITests)

add_executable(SIRGraphicsPipelineBridgeTests SIRGraphicsPipelineBridgeTests.cpp)
target_link_libraries(SIRGraphicsPipelineBridgeTests PRIVATE SIR::Graphics)
add_test(NAME renderer.graphics.pipeline-bridge COMMAND SIRGraphicsPipelineBridgeTests)
sir_configure_owned_target(SIRGraphicsPipelineBridgeTests)

add_library(SIRGraphicsABIMockModule SHARED SIRGraphicsABIMockModule.cpp)
target_link_libraries(SIRGraphicsABIMockModule PRIVATE SIR::Graphics)
sir_configure_owned_target(SIRGraphicsABIMockModule)

add_executable(SIRGraphicsModuleTests SIRGraphicsModuleTests.cpp)
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
add_executable(SIRGraphicsABIContractTests SIRGraphicsABIContractTests.cpp)
target_link_libraries(SIRGraphicsABIContractTests PRIVATE SIR::Graphics)
add_test(NAME renderer.graphics.contract COMMAND SIRGraphicsABIContractTests)
sir_configure_owned_target(SIRGraphicsABIContractTests)

add_executable(SIRGraphicsABIPropertyTests SIRGraphicsABIPropertyTests.cpp)
target_link_libraries(SIRGraphicsABIPropertyTests PRIVATE SIR::Graphics)
add_test(NAME renderer.graphics.properties COMMAND SIRGraphicsABIPropertyTests)
sir_configure_owned_target(SIRGraphicsABIPropertyTests)

add_executable(SIRGraphicsABINegativeMatrixTests SIRGraphicsABINegativeMatrixTests.cpp)
target_link_libraries(SIRGraphicsABINegativeMatrixTests PRIVATE SIR::Graphics)
add_test(NAME renderer.graphics.negative-matrix COMMAND SIRGraphicsABINegativeMatrixTests)
sir_configure_owned_target(SIRGraphicsABINegativeMatrixTests)

add_executable(SIRGraphicsABIEraMatrixTests SIRGraphicsABIEraMatrixTests.cpp)
target_link_libraries(SIRGraphicsABIEraMatrixTests PRIVATE SIR::Graphics)
add_test(NAME renderer.graphics.era-matrix COMMAND SIRGraphicsABIEraMatrixTests)
sir_configure_owned_target(SIRGraphicsABIEraMatrixTests)

set_tests_properties(
    renderer.graphics.interface
    renderer.graphics.binary-abi
    renderer.graphics.pipeline-eras
    renderer.graphics.pipeline-bridge
    renderer.graphics.binary-module
    renderer.graphics.contract
    renderer.graphics.properties
    renderer.graphics.era-matrix
    renderer.graphics.negative-matrix
    PROPERTIES LABELS "graphics-abi;cpu;conformance" TIMEOUT 90)

if(SIR_WITH_GOOGLETEST)
    include(GoogleTest)
    add_executable(SIRGraphicsABIAssertionGoogleTests SIRGraphicsABIAssertionGoogleTests.cpp)
    target_link_libraries(SIRGraphicsABIAssertionGoogleTests PRIVATE SIR::Graphics gtest_main)
    sir_configure_owned_target(SIRGraphicsABIAssertionGoogleTests)
    gtest_discover_tests(SIRGraphicsABIAssertionGoogleTests
        TEST_PREFIX "renderer.graphics.gtest."
        DISCOVERY_MODE PRE_TEST
        PROPERTIES LABELS "graphics-abi;gtest;conformance;gate:pr;gate:main;gate:nightly;gate:qualification" TIMEOUT 90)
endif()

if(SIR_WITH_OPENGL)
    add_executable(SIRGL46FramebufferCacheTests
        Renderer/OpenGL/SIRGLFramebufferCacheTests.cpp
        "${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLFramebufferCache.cpp")
    target_link_libraries(SIRGL46FramebufferCacheTests PRIVATE SIR::Graphics)
    sir_configure_owned_target(SIRGL46FramebufferCacheTests)
    add_test(NAME renderer.opengl46.framebuffer-cache COMMAND SIRGL46FramebufferCacheTests)

    add_executable(SIRGL46FramebufferTests
        Renderer/OpenGL/SIRGLFramebuffersTests.cpp
        "${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLFramebuffers.cpp"
        "${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLFunctionLoader.cpp")
    target_link_libraries(SIRGL46FramebufferTests PRIVATE SIR::Graphics)
    sir_configure_owned_target(SIRGL46FramebufferTests)
    add_test(NAME renderer.opengl46.framebuffers COMMAND SIRGL46FramebufferTests)

    add_executable(SIRGL46FormatsTests
        Renderer/OpenGL/SIRGLFormatsTests.cpp
        "${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLFormats.cpp"
        "${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLFunctionLoader.cpp")
    target_link_libraries(SIRGL46FormatsTests PRIVATE SIR::Graphics)
    sir_configure_owned_target(SIRGL46FormatsTests)
    add_test(NAME renderer.opengl46.formats COMMAND SIRGL46FormatsTests)

    add_executable(SIRGL46TexturesTests
        Renderer/OpenGL/SIRGLTexturesTests.cpp
        "${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLTextures.cpp"
        "${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLFunctionLoader.cpp")
    target_link_libraries(SIRGL46TexturesTests PRIVATE SIR::Graphics)
    sir_configure_owned_target(SIRGL46TexturesTests)
    add_test(NAME renderer.opengl46.textures COMMAND SIRGL46TexturesTests)

    add_executable(SIRGL46SamplersTests
        Renderer/OpenGL/SIRGLSamplersTests.cpp
        "${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLSamplers.cpp"
        "${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLFunctionLoader.cpp")
    target_link_libraries(SIRGL46SamplersTests PRIVATE SIR::Graphics)
    sir_configure_owned_target(SIRGL46SamplersTests)
    add_test(NAME renderer.opengl46.samplers COMMAND SIRGL46SamplersTests)

    add_executable(SIRGL46BuffersTests
        Renderer/OpenGL/SIRGLBuffersTests.cpp
        "${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLBuffers.cpp"
        "${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLFunctionLoader.cpp")
    target_link_libraries(SIRGL46BuffersTests PRIVATE SIR::Graphics)
    sir_configure_owned_target(SIRGL46BuffersTests)
    add_test(NAME renderer.opengl46.buffers COMMAND SIRGL46BuffersTests)

    add_executable(SIRGL46HandlePoolTests
        Renderer/OpenGL/SIRGLHandlePoolTests.cpp
        "${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLHandlePool.cpp")
    target_link_libraries(SIRGL46HandlePoolTests PRIVATE SIR::Graphics)
    sir_configure_owned_target(SIRGL46HandlePoolTests)
    add_test(NAME renderer.opengl46.handle-pool COMMAND SIRGL46HandlePoolTests)

    add_executable(SIRGL46FunctionLoaderTests
        Renderer/OpenGL/SIRGLFunctionLoaderTests.cpp
        "${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLFunctionLoader.cpp")
    target_link_libraries(SIRGL46FunctionLoaderTests PRIVATE SIR::Graphics)
    sir_configure_owned_target(SIRGL46FunctionLoaderTests)
    add_test(NAME renderer.opengl46.function-loader COMMAND SIRGL46FunctionLoaderTests)

    add_executable(SIRGL46FoundationTests
        Renderer/OpenGL/SIRGLFoundationTests.cpp
        "${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/OpenGL/SIRGLCapabilities.cpp")
    target_link_libraries(SIRGL46FoundationTests PRIVATE SIR::Graphics)
    sir_configure_owned_target(SIRGL46FoundationTests)
    add_test(NAME renderer.opengl46.foundation COMMAND SIRGL46FoundationTests)

    find_package(Threads REQUIRED)
    add_executable(SIRGL46ModuleTests Renderer/OpenGL/SIRGLModuleTests.cpp)
    target_link_libraries(SIRGL46ModuleTests PRIVATE SIR::Graphics Threads::Threads "${CMAKE_DL_LIBS}")
    target_compile_definitions(SIRGL46ModuleTests PRIVATE SIR_TEST_GL46_MODULE_PATH="$<TARGET_FILE:SIRGL46Driver>")
    add_dependencies(SIRGL46ModuleTests SIRGL46Driver)
    sir_configure_owned_target(SIRGL46ModuleTests)
    add_test(NAME renderer.opengl46.module COMMAND SIRGL46ModuleTests)
endif()

add_subdirectory(Config)
include(${CMAKE_CURRENT_SOURCE_DIR}/StringPoolTests.cmake)

