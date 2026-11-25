include("${CMAKE_CURRENT_LIST_DIR}/../../../Build/CMake/SIRBuildTreeGuard.cmake")

add_library(SIRPack STATIC
    SIRPackFormat.cpp
    SIRPackCatalog.cpp
    SIRPack.cpp
    SIRPackShard.cpp
    SIRPackMountService.cpp
    SIRCompressionRegistry.cpp)
add_library(SIR::Pack ALIAS SIRPack)
target_include_directories(SIRPack PUBLIC "${PROJECT_SOURCE_DIR}/Source")
target_compile_features(SIRPack PUBLIC cxx_std_17)
target_link_libraries(SIRPack PUBLIC SIR::Core)
if(MSVC)
    target_compile_options(SIRPack PRIVATE /W4)
else()
    target_compile_options(SIRPack PRIVATE -Wall -Wextra -Wpedantic)
endif()
sir_configure_owned_target(SIRPack)
