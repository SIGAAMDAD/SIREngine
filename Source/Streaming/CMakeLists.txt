# Demand and lifecycle authority only. Physical payloads remain in GRDG.
add_library(SIRStreaming STATIC
    SIRStreamingCoordinator.cpp
    SIRStreamingDemandReconciler.cpp SIRStreamingProviderBridge.cpp)
add_library(SIR::Streaming ALIAS SIRStreaming)
target_include_directories(SIRStreaming PUBLIC
    "${PROJECT_SOURCE_DIR}/Source"
    "${PROJECT_SOURCE_DIR}/Build/CMake")
target_compile_features(SIRStreaming PUBLIC cxx_std_17)
target_link_libraries(SIRStreaming PUBLIC SIR::Core)
sir_configure_owned_target(SIRStreaming)

# Links the canonical GRDG and Core Jobs implementations when available.
# Streaming remains buildable by itself while the integration PR is open.
if(TARGET SIR::Resources)
    add_library(SIRStreamingGRDG STATIC SIRStreamingGRDGAdapter.cpp)
    add_library(SIR::StreamingGRDG ALIAS SIRStreamingGRDG)
    target_include_directories(SIRStreamingGRDG PUBLIC
        "${PROJECT_SOURCE_DIR}/Source"
        "${PROJECT_SOURCE_DIR}/Build/CMake")
    target_compile_features(SIRStreamingGRDG PUBLIC cxx_std_17)
    target_link_libraries(SIRStreamingGRDG PUBLIC SIR::Streaming SIR::Resources)
    sir_configure_owned_target(SIRStreamingGRDG)
endif()
