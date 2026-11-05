# Shared, platform-independent rendering ABI. Always compiled, even when no
# native driver is enabled, so backends cannot silently fork its contracts.
add_library(SIRGraphics STATIC
    SIRGraphicsRegistry.cpp
    SIRGraphicsValidation.cpp
    SIRShaderPipeline.cpp
    Shaders/SIRShaderContracts.cpp
    Shaders/SIRShaderMaterial.cpp
    Shaders/SIRShaderGRDG.cpp)
add_library(SIR::Graphics ALIAS SIRGraphics)
target_include_directories(SIRGraphics PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRGraphics PUBLIC cxx_std_17)
target_link_libraries(SIRGraphics PUBLIC SIR::Resources)
sir_configure_owned_target(SIRGraphics)

# Each native driver implements the same SIRRenderingDevice contract.
# Their opt-in flags and targets are owned by the driver branches.
if(SIR_WITH_D3D12)
    add_subdirectory(Drivers/D3D12)
endif()
if(SIR_WITH_VULKAN)
    add_subdirectory(Drivers/Vulkan)
endif()
