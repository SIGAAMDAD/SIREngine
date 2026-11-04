# ShaderCooker is an offline host tool. No native renderer/device linkage.
add_library(SIRShaderCookFrontend STATIC
    Frontend/SIRShaderFrontend.cpp
    Targets/SIRShaderTargets.cpp)
add_library(SIR::ShaderCookFrontend ALIAS SIRShaderCookFrontend)
target_compile_features(SIRShaderCookFrontend PUBLIC cxx_std_17)
target_include_directories(SIRShaderCookFrontend PUBLIC
    "${PROJECT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
if(COMMAND sir_configure_owned_target)
    sir_configure_owned_target(SIRShaderCookFrontend)
endif()

if(SIR_BUILD_TESTS)
    add_executable(SIRShaderFrontendTests Tests/SIRShaderFrontendTests.cpp)
    target_link_libraries(SIRShaderFrontendTests PRIVATE SIR::ShaderCookFrontend)
    add_test(NAME shader.cooker.frontend COMMAND SIRShaderFrontendTests)
    if(COMMAND sir_configure_owned_target)
        sir_configure_owned_target(SIRShaderFrontendTests)
    endif()
endif()

add_executable(SIRShaderCooker CLI/SIRShaderCookerMain.cpp)
target_link_libraries(SIRShaderCooker PRIVATE SIR::ShaderCookFrontend)
if(COMMAND sir_configure_owned_target)
    sir_configure_owned_target(SIRShaderCooker)
endif()

if(SIR_BUILD_TESTS)
    add_executable(SIRMockDxc Tests/SIRMockDxc.cpp)
    add_test(NAME shader.cooker.cli
        COMMAND "${CMAKE_COMMAND}"
            "-DSIR_COOKER=$<TARGET_FILE:SIRShaderCooker>"
            "-DSIR_MOCK_COMPILER=$<TARGET_FILE:SIRMockDxc>"
            "-DSIR_FIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/Tests/Fixtures"
            "-DSIR_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/CLITest"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/Tests/SIRShaderCookerCLITests.cmake")
    if(COMMAND sir_configure_owned_target)
        sir_configure_owned_target(SIRMockDxc)
    endif()
endif()
