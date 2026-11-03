# Godot's SCons build is intentionally preserved. CMake is the entry point
# for orchestration; converting Godot's build graph is a later scoped patch.
if(NOT EXISTS "${PROJECT_SOURCE_DIR}/engine/godot/SConstruct")
    message(FATAL_ERROR "Missing engine/godot. Initialize submodules first.")
endif()

find_program(SIR_SCONS_EXECUTABLE NAMES scons REQUIRED)
set(SIR_GODOT_JOBS "2" CACHE STRING "Number of concurrent SCons compiler jobs")
set(SIR_GODOT_TARGET "editor" CACHE STRING "Godot SCons target")

if(WIN32)
    set(sirGodotPlatform "windows")
elseif(APPLE)
    set(sirGodotPlatform "macos")
else()
    set(sirGodotPlatform "linuxbsd")
endif()

add_custom_target(sir_godot_editor
    COMMAND "${SIR_SCONS_EXECUTABLE}"
        "platform=${sirGodotPlatform}"
        "target=${SIR_GODOT_TARGET}"
        "arch=x86_64"
        "-j${SIR_GODOT_JOBS}"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/engine/godot"
    COMMENT "Building pinned Godot checkout with its native SCons graph"
    VERBATIM
)
