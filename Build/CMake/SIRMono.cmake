# C# runtime dependency declaration. This selects the Mono embedding
# SDK, but does not yet implement an engine scripting bridge or load assemblies.
set(SIR_MONO_SDK_ROOT "" CACHE PATH "Mono embedding SDK installation root")

if(SIR_WITH_MONO)
    if(SIR_MONO_SDK_ROOT AND NOT IS_DIRECTORY "${SIR_MONO_SDK_ROOT}")
        message(FATAL_ERROR "Specified Mono SDK root does not exist: ${SIR_MONO_SDK_ROOT}")
    endif()
    add_library(SIRMonoRuntime INTERFACE)
    add_library(SIR::MonoRuntime ALIAS SIRMonoRuntime)

    # Prefer pkg-config's transitive link dependencies on Unix installations.
    if(NOT WIN32 AND NOT SIR_MONO_SDK_ROOT)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(SIR_MONO_PC QUIET IMPORTED_TARGET mono-2)
            if(NOT SIR_MONO_PC_FOUND)
                pkg_check_modules(SIR_MONO_PC QUIET IMPORTED_TARGET mono-2.0)
            endif()
        endif()
    endif()

    if(SIR_MONO_PC_FOUND)
        target_link_libraries(SIRMonoRuntime INTERFACE PkgConfig::SIR_MONO_PC)
    else()
        # Explicit SDK paths are useful for Windows and non-standard installs.
        find_path(SIR_MONO_INCLUDE_DIR mono/jit/jit.h
            HINTS "${SIR_MONO_SDK_ROOT}/include"
                  "${SIR_MONO_SDK_ROOT}/include/mono-2.0"
            PATH_SUFFIXES mono-2.0 REQUIRED)
        find_library(SIR_MONO_LIBRARY
            NAMES monosgen-2.0 mono-2.0 monosgen-2.0-static mono-2.0-sgen
            HINTS "${SIR_MONO_SDK_ROOT}/lib"
                  "${SIR_MONO_SDK_ROOT}/lib/x64"
                  "${SIR_MONO_SDK_ROOT}/lib/mono"
            REQUIRED)
        target_include_directories(SIRMonoRuntime INTERFACE "${SIR_MONO_INCLUDE_DIR}")
        target_link_libraries(SIRMonoRuntime INTERFACE "${SIR_MONO_LIBRARY}")
    endif()
endif()
