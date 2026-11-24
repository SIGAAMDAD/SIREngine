# All full-engine CTest executables, fixtures and registration live here.
# The project root enables CTest once; this directory is the sole test owner.

if(SIR_BUILD_TESTS)
include("${PROJECT_SOURCE_DIR}/Tests/Unit/Registration/Foundation.cmake")
include("${PROJECT_SOURCE_DIR}/Tests/Unit/Registration/Resources.cmake")
include("${PROJECT_SOURCE_DIR}/Tests/Unit/Registration/Platform.cmake")
include("${PROJECT_SOURCE_DIR}/Tests/Unit/Registration/GraphicsABI.cmake")
# Former Config directory test, retained in the consolidated test owner.
add_executable(SIRConfigDelimitedTests "${PROJECT_SOURCE_DIR}/Tests/Unit/Config/SIRDelimitedTests.cpp")
target_link_libraries(SIRConfigDelimitedTests PRIVATE SIR::Core)
add_test(NAME core.config.delimited COMMAND SIRConfigDelimitedTests)
sir_configure_owned_target(SIRConfigDelimitedTests)
include("${PROJECT_SOURCE_DIR}/Tests/Unit/Registration/VFS.cmake")
include("${PROJECT_SOURCE_DIR}/Tests/Unit/Registration/FrameGraph.cmake")
include("${PROJECT_SOURCE_DIR}/Tests/Unit/Registration/WorldRenderer.cmake")
include("${PROJECT_SOURCE_DIR}/Tests/Unit/Registration/Console.cmake")
include("${PROJECT_SOURCE_DIR}/Tests/Unit/Registration/InputAudio.cmake")
include("${PROJECT_SOURCE_DIR}/Tests/Unit/Registration/Scripting.cmake")
include("${PROJECT_SOURCE_DIR}/Tests/Unit/Registration/Editor.cmake")

# -- Tests/Unit/AnimationContracts --
# The contracts test uses the shipping animation target; no duplicate build or project.
set(SIR_ANIMATION_CONTRACT_TARGET SIR::Animation)
add_executable(SIRAnimationContractTests ${PROJECT_SOURCE_DIR}/Tests/Unit/AnimationContracts/SIRAnimationContractTests.cpp)
target_link_libraries(SIRAnimationContractTests PRIVATE ${SIR_ANIMATION_CONTRACT_TARGET})
add_test(NAME ANM00_Contracts COMMAND SIRAnimationContractTests)
if(COMMAND sir_configure_owned_target)
    sir_configure_owned_target(SIRAnimationContractTests)
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(SIRAnimationContractTests PRIVATE stdc++)
endif()

# GoogleTest is the repository-pinned optional submodule. The standalone
# CTest binary remains usable on hosts without the GoogleTest checkout.
if(TARGET gtest_main)
    set(SIR_ANM00_GTEST_TARGET gtest_main)
else()
    find_package(GTest QUIET)
    if(TARGET GTest::gtest_main)
        set(SIR_ANM00_GTEST_TARGET GTest::gtest_main)
    endif()
endif()
if(SIR_ANM00_GTEST_TARGET)
    add_executable(SIRAnimationContractGoogleTests ${PROJECT_SOURCE_DIR}/Tests/Unit/AnimationContracts/SIRAnimationContractGoogleTests.cpp)
    target_link_libraries(SIRAnimationContractGoogleTests PRIVATE ${SIR_ANIMATION_CONTRACT_TARGET} ${SIR_ANM00_GTEST_TARGET})
    target_include_directories(SIRAnimationContractGoogleTests PRIVATE "${PROJECT_SOURCE_DIR}/Source" "${PROJECT_SOURCE_DIR}/Build/CMake")
    include(GoogleTest)
    gtest_discover_tests(SIRAnimationContractGoogleTests TEST_PREFIX "animation.contracts.")
endif()

# -- Tests/Unit/AnimationCook --
add_executable(SIRAnimationCookSmokeTests ${PROJECT_SOURCE_DIR}/Tests/Unit/AnimationCook/SIRAnimationCookSmoke.cpp)
target_link_libraries(SIRAnimationCookSmokeTests PRIVATE SIR::AnimationCook)
add_test(NAME ANM01_CookSmoke COMMAND SIRAnimationCookSmokeTests)
sir_configure_owned_target(SIRAnimationCookSmokeTests)

add_executable(SIRAnimationCookTests ${PROJECT_SOURCE_DIR}/Tests/Unit/AnimationCook/SIRAnimationCookTests.cpp)
target_link_libraries(SIRAnimationCookTests PRIVATE SIR::AnimationCook)
add_test(NAME ANM01_CookRegression COMMAND SIRAnimationCookTests)
sir_configure_owned_target(SIRAnimationCookTests)

# -- Tests/Unit/AnimationSampling --
add_executable(SIRAnimationSamplingSmokeTests ${PROJECT_SOURCE_DIR}/Tests/Unit/AnimationSampling/SIRAnimationSamplingSmoke.cpp)
target_link_libraries(SIRAnimationSamplingSmokeTests PRIVATE SIR::AnimationCook SIR::Animation)
add_test(NAME ANM02_SamplingSmoke COMMAND SIRAnimationSamplingSmokeTests)
sir_configure_owned_target(SIRAnimationSamplingSmokeTests)

add_executable(SIRAnimationSamplingTests ${PROJECT_SOURCE_DIR}/Tests/Unit/AnimationSampling/SIRAnimationSamplingTests.cpp)
target_link_libraries(SIRAnimationSamplingTests PRIVATE SIR::AnimationCook SIR::Animation)
target_compile_definitions(SIRAnimationSamplingTests PRIVATE SIR_ANIM_WITH_JOBS=1)
add_test(NAME ANM02_Sampling COMMAND SIRAnimationSamplingTests)
sir_configure_owned_target(SIRAnimationSamplingTests)

# -- Tests/Unit/Renderer/D3D11 --
# Capability admission runs on every platform without a Windows SDK.
add_executable(SIRD3D11FeatureLevelTests ${PROJECT_SOURCE_DIR}/Tests/Unit/Renderer/D3D11/SIRD3D11FeatureLevelTests.cpp)
target_link_libraries(SIRD3D11FeatureLevelTests PRIVATE SIR::D3D11FeatureProfile)
add_test(NAME renderer.d3d11.feature-levels COMMAND SIRD3D11FeatureLevelTests)
set_tests_properties(renderer.d3d11.feature-levels PROPERTIES LABELS "d3d11;cpu;conformance" TIMEOUT 30)
sir_configure_owned_target(SIRD3D11FeatureLevelTests)

add_executable(SIRD3D11FormatValidationTests ${PROJECT_SOURCE_DIR}/Tests/Unit/Renderer/D3D11/SIRD3D11FormatValidationTests.cpp)
target_link_libraries(SIRD3D11FormatValidationTests PRIVATE SIR::D3D11FeatureProfile)
add_test(NAME renderer.d3d11.format-validation COMMAND SIRD3D11FormatValidationTests)
set_tests_properties(renderer.d3d11.format-validation PROPERTIES LABELS "d3d11;cpu;conformance" TIMEOUT 30)
sir_configure_owned_target(SIRD3D11FormatValidationTests)

if(SIR_WITH_D3D11 AND WIN32)
    add_executable(SIRD3D11RegistrationTests ${PROJECT_SOURCE_DIR}/Tests/Unit/Renderer/D3D11/SIRD3D11RegistrationTests.cpp)
    target_link_libraries(SIRD3D11RegistrationTests PRIVATE SIR::D3D11)
    add_test(NAME renderer.d3d11.registration COMMAND SIRD3D11RegistrationTests)
    set_tests_properties(renderer.d3d11.registration PROPERTIES LABELS "d3d11;windows;conformance" TIMEOUT 30)
    sir_configure_owned_target(SIRD3D11RegistrationTests)

    add_executable(SIRD3D11FormatProbeTests ${PROJECT_SOURCE_DIR}/Tests/Unit/Renderer/D3D11/SIRD3D11FormatProbeTests.cpp)
    target_link_libraries(SIRD3D11FormatProbeTests PRIVATE SIR::D3D11)
    add_test(NAME renderer.d3d11.format-probe COMMAND SIRD3D11FormatProbeTests)
    set_tests_properties(renderer.d3d11.format-probe PROPERTIES LABELS "d3d11;windows;conformance" TIMEOUT 30)
    sir_configure_owned_target(SIRD3D11FormatProbeTests)
endif()

add_executable(SIRD3D11HandleTests ${PROJECT_SOURCE_DIR}/Tests/Unit/Renderer/D3D11/SIRD3D11HandleTests.cpp)
target_link_libraries(SIRD3D11HandleTests PRIVATE SIR::D3D11FeatureProfile)
add_test(NAME renderer.d3d11.handles COMMAND SIRD3D11HandleTests)
set_tests_properties(renderer.d3d11.handles PROPERTIES LABELS "d3d11;cpu;conformance" TIMEOUT 30)
sir_configure_owned_target(SIRD3D11HandleTests)

add_executable(SIRD3D11BufferValidationTests ${PROJECT_SOURCE_DIR}/Tests/Unit/Renderer/D3D11/SIRD3D11BufferValidationTests.cpp)
target_link_libraries(SIRD3D11BufferValidationTests PRIVATE SIR::D3D11FeatureProfile)
add_test(NAME renderer.d3d11.buffer-validation COMMAND SIRD3D11BufferValidationTests)
set_tests_properties(renderer.d3d11.buffer-validation PROPERTIES LABELS "d3d11;cpu;conformance" TIMEOUT 30)
sir_configure_owned_target(SIRD3D11BufferValidationTests)

add_executable(SIRD3D11TextureValidationTests ${PROJECT_SOURCE_DIR}/Tests/Unit/Renderer/D3D11/SIRD3D11TextureValidationTests.cpp)
target_link_libraries(SIRD3D11TextureValidationTests PRIVATE SIR::D3D11FeatureProfile)
add_test(NAME renderer.d3d11.texture-validation COMMAND SIRD3D11TextureValidationTests)
set_tests_properties(renderer.d3d11.texture-validation PROPERTIES LABELS "d3d11;cpu;conformance" TIMEOUT 30)
sir_configure_owned_target(SIRD3D11TextureValidationTests)

# -- Tests/Dependencies --
# Compile and link the actual optional dependency targets when enabled.
if(SIR_WITH_MESHOPTIMIZER)
    add_executable(sir_meshoptimizer_dependency_smoke ${PROJECT_SOURCE_DIR}/Tests/Dependencies/MeshOptimizerSmoke.cpp)
    target_compile_features(sir_meshoptimizer_dependency_smoke PRIVATE cxx_std_17)
    target_link_libraries(sir_meshoptimizer_dependency_smoke PRIVATE SIR::MeshOptimizer)
    add_test(NAME sir.meshoptimizer.dependency COMMAND sir_meshoptimizer_dependency_smoke)
endif()

if(SIR_WITH_OPENAL_SOFT OR SIR_WITH_OPENAL)
    add_executable(sir_openal_dependency_smoke ${PROJECT_SOURCE_DIR}/Tests/Dependencies/OpenALSmoke.cpp)
    target_compile_features(sir_openal_dependency_smoke PRIVATE cxx_std_17)
    target_link_libraries(sir_openal_dependency_smoke PRIVATE SIR::OpenAL)
    add_test(NAME sir.openal.dependency COMMAND sir_openal_dependency_smoke)
endif()


# -- D3D9 native acceptance --
if(SIR_BUILD_TESTS AND SIR_WITH_D3D9)
        add_executable(SIRD3D9NativeSmoke "${PROJECT_SOURCE_DIR}/Tests/Renderer/D3D9/SIRD3D9NativeSmoke.cpp" ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D9/SIRD3D9Native.cpp ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D9/SIRD3D9Formats.cpp ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D9/SIRD3D9Targets.cpp)
        target_include_directories(SIRD3D9NativeSmoke PRIVATE "${PROJECT_SOURCE_DIR}/Source" "${PROJECT_SOURCE_DIR}/Build/CMake")
        target_compile_features(SIRD3D9NativeSmoke PRIVATE cxx_std_17)
        target_link_libraries(SIRD3D9NativeSmoke PRIVATE SIRD3D9Portable SIR::Graphics d3d9 dxguid user32)
        sir_configure_owned_target(SIRD3D9NativeSmoke)
        add_test(NAME renderer.d3d9.native-smoke COMMAND SIRD3D9NativeSmoke)
        add_test(NAME renderer.d3d9ex.native-smoke COMMAND SIRD3D9NativeSmoke --ex)
        if(SIR_SHADER_FXC_EXECUTABLE AND EXISTS "${SIR_SHADER_FXC_EXECUTABLE}")
            set(SIR_D3D9_SHADER_FIXTURE "${PROJECT_SOURCE_DIR}/Tools/ShaderCooker/Tests/Fixtures/d3d9.hlsl")
            set(SIR_D3D9_TEST_VS "${PROJECT_BINARY_DIR}/Source/Renderer/Drivers/D3D9/D3D9NativeSmoke.vso")
            set(SIR_D3D9_TEST_PS "${PROJECT_BINARY_DIR}/Source/Renderer/Drivers/D3D9/D3D9NativeSmoke.pso")
            add_custom_command(OUTPUT "${SIR_D3D9_TEST_VS}" "${SIR_D3D9_TEST_PS}"
                COMMAND "${SIR_SHADER_FXC_EXECUTABLE}" /nologo /T vs_2_0 /E VSMain /Fo "${SIR_D3D9_TEST_VS}" "${SIR_D3D9_SHADER_FIXTURE}"
                COMMAND "${SIR_SHADER_FXC_EXECUTABLE}" /nologo /T ps_2_0 /E PSMain /Fo "${SIR_D3D9_TEST_PS}" "${SIR_D3D9_SHADER_FIXTURE}"
                DEPENDS "${SIR_D3D9_SHADER_FIXTURE}" VERBATIM)
            add_custom_target(SIRD3D9NativeShaderFixtures DEPENDS "${SIR_D3D9_TEST_VS}" "${SIR_D3D9_TEST_PS}")
            add_dependencies(SIRD3D9NativeSmoke SIRD3D9NativeShaderFixtures)
            add_test(NAME renderer.d3d9.native-shaders COMMAND SIRD3D9NativeSmoke --shaders "${SIR_D3D9_TEST_VS}" "${SIR_D3D9_TEST_PS}")
        endif()
    endif()

# -- Drivers/D3D9 test fixtures --
if(SIR_BUILD_TESTS)
    add_executable(SIRD3D9AttachmentTests "${PROJECT_SOURCE_DIR}/Tests/Renderer/D3D9/SIRD3D9AttachmentTests.cpp")
    target_link_libraries(SIRD3D9AttachmentTests PRIVATE SIRD3D9Portable)
    sir_configure_owned_target(SIRD3D9AttachmentTests)
    add_test(NAME renderer.d3d9.attachments COMMAND SIRD3D9AttachmentTests)
endif()

# -- Drivers/D3D9 test fixtures --
if(SIR_BUILD_TESTS)
    add_executable(SIRD3D9BarrierTests "${PROJECT_SOURCE_DIR}/Tests/Renderer/D3D9/SIRD3D9BarrierTests.cpp")
    target_link_libraries(SIRD3D9BarrierTests PRIVATE SIRD3D9Portable)
    sir_configure_owned_target(SIRD3D9BarrierTests)
    add_test(NAME renderer.d3d9.barriers COMMAND SIRD3D9BarrierTests)
endif()

# -- Drivers/D3D9 test fixtures --
if(SIR_BUILD_TESTS)
    add_executable(SIRD3D9MappingTests "${PROJECT_SOURCE_DIR}/Tests/Renderer/D3D9/SIRD3D9MappingTests.cpp")
    target_link_libraries(SIRD3D9MappingTests PRIVATE SIRD3D9Portable)
    sir_configure_owned_target(SIRD3D9MappingTests)
    add_test(NAME renderer.d3d9.mapping COMMAND SIRD3D9MappingTests)
endif()

# -- Drivers/D3D9 test fixtures --
if(SIR_BUILD_TESTS)
    add_executable(SIRD3D9FixedPipelineTests "${PROJECT_SOURCE_DIR}/Tests/Renderer/D3D9/SIRD3D9FixedPipelineTests.cpp")
    target_link_libraries(SIRD3D9FixedPipelineTests PRIVATE SIRD3D9Portable)
    sir_configure_owned_target(SIRD3D9FixedPipelineTests)
    add_test(NAME renderer.d3d9.fixed-pipelines COMMAND SIRD3D9FixedPipelineTests)
endif()

# -- Drivers/D3D9 test fixtures --
if(SIR_BUILD_TESTS)
    add_library(SIRD3D9ABIStub STATIC ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D9/SIRD3D9Driver.cpp)
    target_include_directories(SIRD3D9ABIStub PUBLIC "${PROJECT_SOURCE_DIR}/Source" "${PROJECT_SOURCE_DIR}/Build/CMake")
    target_link_libraries(SIRD3D9ABIStub PUBLIC SIR::Graphics)
    target_compile_features(SIRD3D9ABIStub PUBLIC cxx_std_17)
    sir_configure_owned_target(SIRD3D9ABIStub)
    add_executable(SIRD3D9ABITests "${PROJECT_SOURCE_DIR}/Tests/Renderer/D3D9/SIRD3D9ABITests.cpp")
    target_link_libraries(SIRD3D9ABITests PRIVATE SIRD3D9ABIStub)
    sir_configure_owned_target(SIRD3D9ABITests)
    add_test(NAME renderer.d3d9.abi COMMAND SIRD3D9ABITests)
endif()

# -- Drivers/D3D9 test fixtures --
if(SIR_BUILD_TESTS)
    add_executable(SIRD3D9ResourcesTests "${PROJECT_SOURCE_DIR}/Tests/Renderer/D3D9/SIRD3D9ResourcesTests.cpp")
    target_link_libraries(SIRD3D9ResourcesTests PRIVATE SIRD3D9Portable)
    sir_configure_owned_target(SIRD3D9ResourcesTests)
    add_test(NAME renderer.d3d9.resources COMMAND SIRD3D9ResourcesTests)
endif()

# -- Drivers/D3D9 test fixtures --
if(SIR_BUILD_TESTS)
    add_executable(SIRD3D9ValidationTests "${PROJECT_SOURCE_DIR}/Tests/Renderer/D3D9/SIRD3D9ValidationTests.cpp")
    target_link_libraries(SIRD3D9ValidationTests PRIVATE SIRD3D9Portable)
    sir_configure_owned_target(SIRD3D9ValidationTests)
    add_test(NAME renderer.d3d9.validation COMMAND SIRD3D9ValidationTests)
endif()

# -- Drivers/D3D9 test fixtures --
if(SIR_BUILD_TESTS)
    add_executable(SIRD3D9RingTests "${PROJECT_SOURCE_DIR}/Tests/Renderer/D3D9/SIRD3D9RingTests.cpp")
    target_link_libraries(SIRD3D9RingTests PRIVATE SIRD3D9Portable)
    sir_configure_owned_target(SIRD3D9RingTests)
    add_test(NAME renderer.d3d9.upload-ring COMMAND SIRD3D9RingTests)
endif()

# -- Drivers/D3D10 test fixtures --
if(SIR_BUILD_TESTS AND EXISTS "${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D10/SIRD3D10Driver.cpp")
    add_library(SIRD3D10ABIStub STATIC ${PROJECT_SOURCE_DIR}/Source/Renderer/Drivers/D3D10/SIRD3D10Driver.cpp)
    target_include_directories(SIRD3D10ABIStub PUBLIC
        "${PROJECT_SOURCE_DIR}/Source"
        "${PROJECT_SOURCE_DIR}/Build/CMake")
    target_compile_features(SIRD3D10ABIStub PUBLIC cxx_std_17)
    target_link_libraries(SIRD3D10ABIStub PUBLIC SIR::Graphics)
    sir_configure_owned_target(SIRD3D10ABIStub)

    add_executable(SIRD3D10ABITests "${PROJECT_SOURCE_DIR}/Tests/Renderer/D3D10/SIRD3D10ABITests.cpp")
    target_link_libraries(SIRD3D10ABITests PRIVATE SIRD3D10ABIStub)
    sir_configure_owned_target(SIRD3D10ABITests)
    add_test(NAME renderer.d3d10.abi-scaffold COMMAND SIRD3D10ABITests)
endif()

# -- ShaderCooker acceptance --
set(SIR_SHADER_FXC_EXECUTABLE "" CACHE FILEPATH "Offline FXC executable for native D3D9 shader acceptance")
if(WIN32 AND SIR_BUILD_TESTS AND NOT SIR_SHADER_FXC_EXECUTABLE STREQUAL "")
    if(NOT EXISTS "${SIR_SHADER_FXC_EXECUTABLE}")
        message(FATAL_ERROR "SIR_SHADER_FXC_EXECUTABLE does not exist")
    endif()
    add_test(NAME shader.cooker.native-fxc-d3d9
        COMMAND "${CMAKE_COMMAND}"
            "-DSIR_COOKER=$<TARGET_FILE:SIRShaderCooker>"
            "-DSIR_FXC=${SIR_SHADER_FXC_EXECUTABLE}"
            "-DSIR_FIXTURES=${PROJECT_SOURCE_DIR}/Tools/ShaderCooker/Tests/Fixtures"
            "-DSIR_OUTPUT_DIR=${PROJECT_BINARY_DIR}/Tools/ShaderCooker/NativeFXCD3D9Test"
            -P "${PROJECT_SOURCE_DIR}/Tools/ShaderCooker/Tests/SIRShaderCookerNativeFXCTests.cmake")
endif()

# -- ShaderCooker acceptance --
set(SIR_SHADER_DXC_EXECUTABLE "" CACHE FILEPATH "Pinned offline DXC executable for shader acceptance")
if(SIR_BUILD_TESTS AND NOT SIR_SHADER_DXC_EXECUTABLE STREQUAL "")
    find_program(SIR_SHADER_SPIRV_VAL NAMES spirv-val REQUIRED)
    find_program(SIR_SHADER_SPIRV_DIS NAMES spirv-dis REQUIRED)
    add_test(NAME shader.cooker.native-dxc
        COMMAND "${CMAKE_COMMAND}"
            "-DSIR_COOKER=$<TARGET_FILE:SIRShaderCooker>"
            "-DSIR_DXC=${SIR_SHADER_DXC_EXECUTABLE}"
            "-DSIR_SPIRV_VAL=${SIR_SHADER_SPIRV_VAL}"
            "-DSIR_SPIRV_DIS=${SIR_SHADER_SPIRV_DIS}"
            "-DSIR_FIXTURES=${PROJECT_SOURCE_DIR}/Tools/ShaderCooker/Tests/Fixtures"
            "-DSIR_OUTPUT_DIR=${PROJECT_BINARY_DIR}/Tools/ShaderCooker/NativeDXCTest"
            -P "${PROJECT_SOURCE_DIR}/Tools/ShaderCooker/Tests/SIRShaderCookerNativeDXCTests.cmake")
    add_executable(SIRMaterialIREmitterDxcFixture ${PROJECT_SOURCE_DIR}/Tools/ShaderCooker/Tests/SIRMaterialIREmitterDxcFixture.cpp)
    target_link_libraries(SIRMaterialIREmitterDxcFixture PRIVATE SIR::ShaderCookFrontend)
    if(COMMAND sir_configure_owned_target)
        sir_configure_owned_target(SIRMaterialIREmitterDxcFixture)
    endif()
    add_test(NAME shader.cooker.material-ir-native-dxc
        COMMAND "${CMAKE_COMMAND}"
            "-DSIR_EMITTER_FIXTURE=$<TARGET_FILE:SIRMaterialIREmitterDxcFixture>"
            "-DSIR_DXC=${SIR_SHADER_DXC_EXECUTABLE}"
            "-DSIR_SPIRV_VAL=${SIR_SHADER_SPIRV_VAL}"
            "-DSIR_SPIRV_DIS=${SIR_SHADER_SPIRV_DIS}"
            "-DSIR_OUTPUT_DIR=${PROJECT_BINARY_DIR}/Tools/ShaderCooker/MaterialIRNativeDXCTest"
            -P "${PROJECT_SOURCE_DIR}/Tools/ShaderCooker/Tests/SIRMaterialIREmitterNativeDXCTests.cmake")

endif()

# -- ShaderCooker acceptance --
if(SIR_BUILD_TESTS)
    add_executable(SIRMockDxc ${PROJECT_SOURCE_DIR}/Tools/ShaderCooker/Tests/SIRMockDxc.cpp)
    add_test(NAME shader.cooker.cli
        COMMAND "${CMAKE_COMMAND}"
            "-DSIR_COOKER=$<TARGET_FILE:SIRShaderCooker>"
            "-DSIR_MOCK_COMPILER=$<TARGET_FILE:SIRMockDxc>"
            "-DSIR_FIXTURES=${PROJECT_SOURCE_DIR}/Tools/ShaderCooker/Tests/Fixtures"
            "-DSIR_OUTPUT_DIR=${PROJECT_BINARY_DIR}/Tools/ShaderCooker/CLITest"
            -P "${PROJECT_SOURCE_DIR}/Tools/ShaderCooker/Tests/SIRShaderCookerCLITests.cmake")
    if(COMMAND sir_configure_owned_target)
        sir_configure_owned_target(SIRMockDxc)
    endif()
endif()

# -- ShaderCooker acceptance --
if(SIR_BUILD_TESTS)
    add_executable(SIRMockFxc ${PROJECT_SOURCE_DIR}/Tools/ShaderCooker/Tests/SIRMockFxc.cpp)
    sir_configure_owned_target(SIRMockFxc)
    add_test(NAME shader.cooker.d3d9
        COMMAND "${CMAKE_COMMAND}"
            "-DSIR_COOKER=$<TARGET_FILE:SIRShaderCooker>"
            "-DSIR_MOCK_COMPILER=$<TARGET_FILE:SIRMockFxc>"
            "-DSIR_FIXTURES=${PROJECT_SOURCE_DIR}/Tools/ShaderCooker/Tests/Fixtures"
            "-DSIR_OUTPUT_DIR=${PROJECT_BINARY_DIR}/Tools/ShaderCooker/D3D9CLITest"
            -P "${PROJECT_SOURCE_DIR}/Tools/ShaderCooker/Tests/SIRShaderCookerD3D9Tests.cmake")
endif()

# -- ShaderCooker acceptance --
if(SIR_BUILD_TESTS)
    add_executable(SIRMaterialDerivativesTests ${PROJECT_SOURCE_DIR}/Tools/ShaderCooker/Tests/Derivatives/SIRMaterialDerivativesTests.cpp)
    target_link_libraries(SIRMaterialDerivativesTests PRIVATE SIR::ShaderCookFrontend)
    add_test(NAME shader.cooker.derivatives COMMAND SIRMaterialDerivativesTests)
    sir_configure_owned_target(SIRMaterialDerivativesTests)
endif()

# -- ShaderCooker acceptance --
if(SIR_BUILD_TESTS)
    add_executable(SIRShaderArchiveTests ${PROJECT_SOURCE_DIR}/Tools/ShaderCooker/Tests/SIRShaderArchiveTests.cpp)
    target_link_libraries(SIRShaderArchiveTests PRIVATE SIR::ShaderCookFrontend)
    add_test(NAME shader.cooker.archive COMMAND SIRShaderArchiveTests)
    sir_configure_owned_target(SIRShaderArchiveTests)
endif()

# -- ShaderCooker acceptance --
if(SIR_BUILD_TESTS)
    add_executable(SIRFixedRecipeCookerTests ${PROJECT_SOURCE_DIR}/Tools/ShaderCooker/Tests/SIRFixedRecipeCookerTests.cpp)
    target_link_libraries(SIRFixedRecipeCookerTests PRIVATE SIR::ShaderCookFrontend)
    add_test(NAME shader.cooker.fixed-recipe COMMAND SIRFixedRecipeCookerTests)
    sir_configure_owned_target(SIRFixedRecipeCookerTests)
endif()

# -- ShaderCooker acceptance --
if(SIR_BUILD_TESTS)
    add_executable(SIRGraphicsCookIntegrationTests ${PROJECT_SOURCE_DIR}/Tools/ShaderCooker/Tests/SIRGraphicsCookIntegrationTests.cpp)
    target_link_libraries(SIRGraphicsCookIntegrationTests PRIVATE SIR::ShaderCookFrontend SIR::Graphics)
    add_test(NAME shader.cooker.graphics-integration COMMAND SIRGraphicsCookIntegrationTests)
    sir_configure_owned_target(SIRGraphicsCookIntegrationTests)
endif()

# -- ShaderCooker acceptance --
if(SIR_BUILD_TESTS)
    add_executable(SIRMaterialIREmitterTests ${PROJECT_SOURCE_DIR}/Tools/ShaderCooker/Tests/SIRMaterialIREmitterTests.cpp)
    target_link_libraries(SIRMaterialIREmitterTests PRIVATE SIR::ShaderCookFrontend)
    add_test(NAME shader.cooker.material-ir-emitter COMMAND SIRMaterialIREmitterTests)
    sir_configure_owned_target(SIRMaterialIREmitterTests)
endif()

# -- ShaderCooker acceptance --
if(SIR_BUILD_TESTS)
    add_executable(SIRMaterialIRTests ${PROJECT_SOURCE_DIR}/Tools/ShaderCooker/Tests/SIRMaterialIRTests.cpp)
    target_link_libraries(SIRMaterialIRTests PRIVATE SIR::ShaderCookFrontend)
    add_test(NAME shader.cooker.material-ir COMMAND SIRMaterialIRTests)
    sir_configure_owned_target(SIRMaterialIRTests)
endif()

# -- ShaderCooker acceptance --
if(SIR_BUILD_TESTS)
    add_executable(SIRShaderFrontendTests ${PROJECT_SOURCE_DIR}/Tools/ShaderCooker/Tests/SIRShaderFrontendTests.cpp)
    target_link_libraries(SIRShaderFrontendTests PRIVATE SIR::ShaderCookFrontend)
    add_test(NAME shader.cooker.frontend COMMAND SIRShaderFrontendTests)
    if(COMMAND sir_configure_owned_target)
        sir_configure_owned_target(SIRShaderFrontendTests)
    endif()
endif()

# Imported container and synchronization acceptance, registered before label classification.
foreach(sirTest IN ITEMS
    SIRGenericOptimizationLoopTests
    SIRAdvancedOptimizationTests
    SIRAdvancedNextOptimizationTests
    SIRAdvancedContainerTests
    SIRVectorSpecializationTests)
    add_executable(${sirTest} "${PROJECT_SOURCE_DIR}/Tests/Unit/${sirTest}.cpp")
    target_link_libraries(${sirTest} PRIVATE SIR::Core)
    sir_configure_owned_target(${sirTest})
    add_test(NAME "core.containers.${sirTest}" COMMAND ${sirTest})
endforeach()
add_executable(SIRGRDGVectorViewTests "${PROJECT_SOURCE_DIR}/Tests/Unit/SIRGRDGVectorViewTests.cpp")
target_link_libraries(SIRGRDGVectorViewTests PRIVATE SIR::Resources)
sir_configure_owned_target(SIRGRDGVectorViewTests)
add_test(NAME resources.grdg.vector-views COMMAND SIRGRDGVectorViewTests)
add_executable(SIRAdvancedSyncTests "${PROJECT_SOURCE_DIR}/Tests/Unit/SIRAdvancedSyncTests.cpp")
target_link_libraries(SIRAdvancedSyncTests PRIVATE SIR::Core)
sir_configure_owned_target(SIRAdvancedSyncTests)
add_test(NAME core.platform.advanced-sync COMMAND SIRAdvancedSyncTests)
add_executable(SIRFrameSchedulingTests "${PROJECT_SOURCE_DIR}/Tests/Unit/SIRFrameSchedulingTests.cpp")
target_link_libraries(SIRFrameSchedulingTests PRIVATE SIR::Core)
sir_configure_owned_target(SIRFrameSchedulingTests)
add_test(NAME core.scheduling.versioned-foundation COMMAND SIRFrameSchedulingTests)

add_executable(SIRResourceGraphSyncDonorTests "${PROJECT_SOURCE_DIR}/Tests/Unit/SIRResourceGraphSyncTests.cpp")
target_link_libraries(SIRResourceGraphSyncDonorTests PRIVATE SIR::Resources)
sir_configure_owned_target(SIRResourceGraphSyncDonorTests)
add_test(NAME resources.grdg.dedicated-sync-donor COMMAND SIRResourceGraphSyncDonorTests)

add_executable(SIRResourceGraphSyncStandaloneTests "${PROJECT_SOURCE_DIR}/Tests/Unit/SIRResourceGraphSyncStandaloneTests.cpp")
target_link_libraries(SIRResourceGraphSyncStandaloneTests PRIVATE SIR::Resources)
sir_configure_owned_target(SIRResourceGraphSyncStandaloneTests)
add_test(NAME resources.grdg.dedicated-sync COMMAND SIRResourceGraphSyncStandaloneTests)
include("${PROJECT_SOURCE_DIR}/Build/CMake/SIRTestRegistration.cmake")
sir_classify_tests("${PROJECT_SOURCE_DIR}")
include("${PROJECT_SOURCE_DIR}/Build/CMake/SIRQualificationScopes.cmake")
sir_verify_qualification_contracts("${PROJECT_SOURCE_DIR}/Tests")
endif() # SIR_BUILD_TESTS

if(SIR_BUILD_BENCHMARKS)
add_executable(SIRCoreBenchmarks
    ${PROJECT_SOURCE_DIR}/Tests/Benchmarks/CoreBenchmarks.cpp
    ${PROJECT_SOURCE_DIR}/Tests/Benchmarks/SIRContainerAllocatorBenchmarks.cpp
    ${PROJECT_SOURCE_DIR}/Tests/Benchmarks/EventBenchmarks.cpp
    ${PROJECT_SOURCE_DIR}/Tests/Benchmarks/SIRTaskSchedulerBenchmarks.cpp
    ${PROJECT_SOURCE_DIR}/Tests/Benchmarks/SIRSchedulerExperimentBenchmarks.cpp
    ${PROJECT_SOURCE_DIR}/Tests/Benchmarks/SIRSchedulerWorkloadReplay.cpp
    ${PROJECT_SOURCE_DIR}/Tests/Benchmarks/SIRSchedulerQueueCandidates.cpp
    ${PROJECT_SOURCE_DIR}/Tests/Benchmarks/SIRSchedulerQueueCandidateBenchmarks.cpp
    ${PROJECT_SOURCE_DIR}/Tests/Benchmarks/SIRGRDGInfrastructureBenchmarks.cpp
    ${PROJECT_SOURCE_DIR}/Tests/Benchmarks/SIRPlatformIOBenchmarks.cpp
    ${PROJECT_SOURCE_DIR}/Tests/Benchmarks/SIRAudioBenchmarks.cpp
    ${PROJECT_SOURCE_DIR}/Tests/Benchmarks/SIRLangBenchmarks.cpp)
target_compile_features(SIRCoreBenchmarks PRIVATE cxx_std_17)
target_link_libraries(SIRCoreBenchmarks PRIVATE SIR::Core SIR::Resources SIR::FMODContext SIR::Scripting benchmark::benchmark_main)
if(WIN32)
    # Windows headers may otherwise expose max/min macros into Google Benchmark.
    target_compile_definitions(SIRCoreBenchmarks PRIVATE NOMINMAX)
endif()
sir_configure_owned_target(SIRCoreBenchmarks)
endif()

# Optimized smart pointer family, allocator ownership, and GRDG lifetime contracts.
if(SIR_BUILD_TESTS)
foreach(sirPointerTest IN ITEMS SIRSmartPointerTests SIREngineAllocationPtrTests SIRPointerOptimizationTests SIRPointerFamilyTests)
    add_executable(${sirPointerTest} "${PROJECT_SOURCE_DIR}/Tests/Unit/${sirPointerTest}.cpp")
    target_link_libraries(${sirPointerTest} PRIVATE SIR::Core SIR::Resources)
    sir_configure_owned_target(${sirPointerTest})
    add_test(NAME ${sirPointerTest} COMMAND ${sirPointerTest})
    set_tests_properties(${sirPointerTest} PROPERTIES LABELS "gate:pr;unit;memory" TIMEOUT 60)
endforeach()
endif()
