# Compile and link the actual optional dependency targets when enabled.
if(SIR_WITH_MESHOPTIMIZER)
    add_executable(sir_meshoptimizer_dependency_smoke MeshOptimizerSmoke.cpp)
    target_compile_features(sir_meshoptimizer_dependency_smoke PRIVATE cxx_std_17)
    target_link_libraries(sir_meshoptimizer_dependency_smoke PRIVATE SIR::MeshOptimizer)
    add_test(NAME sir.meshoptimizer.dependency COMMAND sir_meshoptimizer_dependency_smoke)
endif()

if(SIR_WITH_OPENAL_SOFT OR SIR_WITH_OPENAL)
    add_executable(sir_openal_dependency_smoke OpenALSmoke.cpp)
    target_compile_features(sir_openal_dependency_smoke PRIVATE cxx_std_17)
    target_link_libraries(sir_openal_dependency_smoke PRIVATE SIR::OpenAL)
    add_test(NAME sir.openal.dependency COMMAND sir_openal_dependency_smoke)
endif()
