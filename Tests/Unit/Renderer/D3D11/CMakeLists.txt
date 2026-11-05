# Capability admission runs on every platform without a Windows SDK.
add_executable(SIRD3D11FeatureLevelTests SIRD3D11FeatureLevelTests.cpp)
target_link_libraries(SIRD3D11FeatureLevelTests PRIVATE SIR::D3D11FeatureProfile)
add_test(NAME renderer.d3d11.feature-levels COMMAND SIRD3D11FeatureLevelTests)
set_tests_properties(renderer.d3d11.feature-levels PROPERTIES LABELS "d3d11;cpu;conformance" TIMEOUT 30)
sir_configure_owned_target(SIRD3D11FeatureLevelTests)

if(SIR_WITH_D3D11 AND WIN32)
    add_executable(SIRD3D11RegistrationTests SIRD3D11RegistrationTests.cpp)
    target_link_libraries(SIRD3D11RegistrationTests PRIVATE SIR::D3D11)
    add_test(NAME renderer.d3d11.registration COMMAND SIRD3D11RegistrationTests)
    set_tests_properties(renderer.d3d11.registration PROPERTIES LABELS "d3d11;windows;conformance" TIMEOUT 30)
    sir_configure_owned_target(SIRD3D11RegistrationTests)
endif()
