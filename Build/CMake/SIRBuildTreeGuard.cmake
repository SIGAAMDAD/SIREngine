# All engine-generated CMake files belong under out/<host>/<preset> or outside the checkout.
# This include runs before project() so misuse fails before target generation.
include_guard(GLOBAL)

get_filename_component(_sir_repository_root "${CMAKE_CURRENT_LIST_DIR}/../.." REALPATH)
file(REAL_PATH "${CMAKE_SOURCE_DIR}" _sir_configure_source)
file(REAL_PATH "${CMAKE_BINARY_DIR}" _sir_configure_binary)

set(_sir_mesh_standalone "${_sir_repository_root}/Tests/Standalone/MeshCache")
if(NOT _sir_configure_source STREQUAL _sir_repository_root AND
   NOT _sir_configure_source STREQUAL _sir_mesh_standalone)
    message(FATAL_ERROR
        "Configure SIREngine from its repository root (cmake --preset dev). "
        "Source subdirectories are not independently configurable projects.")
endif()

cmake_path(IS_PREFIX _sir_repository_root "${_sir_configure_binary}" NORMALIZE _sir_inside_checkout)
if(_sir_inside_checkout)
    file(RELATIVE_PATH _sir_relative_binary "${_sir_repository_root}" "${_sir_configure_binary}")
    if(NOT _sir_relative_binary MATCHES "^out(/|$)")
        message(FATAL_ERROR
            "CMake build files must not be generated in source directories. "
            "Use cmake --preset dev (out/<OS>/dev), the vs2022 preset, "
            "or cmake -S <repo> -B <external-build-directory>.")
    endif()
endif()
