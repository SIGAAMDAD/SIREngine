# Demand and lifecycle authority only. Physical payloads remain in GRDG.
add_library(SIRStreaming STATIC
    SIRStreamingCoordinator.cpp
    SIRStreamingDemandReconciler.cpp)
add_library(SIR::Streaming ALIAS SIRStreaming)
target_include_directories(SIRStreaming PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRStreaming PUBLIC cxx_std_17)
target_link_libraries(SIRStreaming PUBLIC SIR::Core)
sir_configure_owned_target(SIRStreaming)
