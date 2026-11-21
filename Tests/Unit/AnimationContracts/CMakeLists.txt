# Standalone contract qualification remains usable without the full engine.
# In the native build, link the same production Animation target that ships
# to consumers; do not maintain a separate contract implementation library.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.20)
    project(SIRAnimationContractsQualification LANGUAGES CXX)
    include(CTest)
    add_library(SIRAnimationContracts STATIC
        ../../../Source/Animation/Contracts/SIRAnimationContractValidation.cpp
        ../../../Source/Animation/Contracts/SIRAnimationContractLeasePool.cpp)
    target_compile_features(SIRAnimationContracts PUBLIC cxx_std_17)
    target_include_directories(SIRAnimationContracts PUBLIC ../../../Source ../../../Build/CMake)
    set(SIR_ANIMATION_CONTRACT_TARGET SIRAnimationContracts)
else()
    set(SIR_ANIMATION_CONTRACT_TARGET SIR::Animation)
endif()
add_executable(SIRAnimationContractTests SIRAnimationContractTests.cpp)
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
    add_executable(SIRAnimationContractGoogleTests SIRAnimationContractGoogleTests.cpp)
    target_link_libraries(SIRAnimationContractGoogleTests PRIVATE ${SIR_ANIMATION_CONTRACT_TARGET} ${SIR_ANM00_GTEST_TARGET})
    target_include_directories(SIRAnimationContractGoogleTests PRIVATE ../../../Source ../../../Build/CMake)
    include(GoogleTest)
    gtest_discover_tests(SIRAnimationContractGoogleTests TEST_PREFIX "animation.contracts.")
endif()
