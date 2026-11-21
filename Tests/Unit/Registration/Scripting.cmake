# Platform file-lock provider and full SIRLang compiler/runtime fixtures.
add_executable(SIRFileLockTests ${PROJECT_SOURCE_DIR}/Tests/Unit/SIRFileLockTests.cpp)
target_link_libraries(SIRFileLockTests PRIVATE SIR::Core)
add_test(NAME core.platform.file-lock COMMAND SIRFileLockTests)
sir_configure_owned_target(SIRFileLockTests)
include(${PROJECT_SOURCE_DIR}/Tests/Unit/ScriptingTests.cmake)

