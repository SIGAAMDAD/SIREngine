# Platform file-lock provider and full SIRLang compiler/runtime fixtures.
add_executable(SIRFileLockTests SIRFileLockTests.cpp)
target_link_libraries(SIRFileLockTests PRIVATE SIR::Core)
add_test(NAME core.platform.file-lock COMMAND SIRFileLockTests)
sir_configure_owned_target(SIRFileLockTests)
include(${CMAKE_CURRENT_SOURCE_DIR}/ScriptingTests.cmake)

