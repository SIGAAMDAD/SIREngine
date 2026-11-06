include_guard(GLOBAL)

# Source-owned qualification contracts and source scopes, not generated JSON.
set(SIR_QUALIFICATION_SYSTEMS all CoreMemoryResources Core Resources Renderer Terrain Editor Scripting Audio Mesh Streaming World UI GraphicsABI)
set(SIR_QUALIFICATION_SCOPE_all "Source")
set(SIR_QUALIFICATION_SCOPE_CoreMemoryResources "Source")
set(SIR_QUALIFICATION_SCOPE_GraphicsABI "Source/Renderer")
foreach(sirSystem IN ITEMS Core Resources Renderer Terrain Editor Scripting Audio Mesh Streaming World UI)
    set("SIR_QUALIFICATION_SCOPE_${sirSystem}" "Source/${sirSystem}")
endforeach()
# Contract-bearing tests must remain registered on every supported native host.
set(SIR_QUALIFICATION_CORE_REQUIRED_TESTS
    core.application.lifecycle
    core.platform.lifecycle
    core.memory.manager
    core.jobs.scheduler
    core.jobs.graph
    resources.graph.lifecycle
    renderer.graphics.interface
    renderer.framegraph.compiler
    launcher.version
    launcher.help)
set(SIR_QUALIFICATION_GRAPHICS_ABI_REQUIRED_TESTS
    renderer.graphics.interface
    renderer.graphics.binary-abi
    renderer.graphics.pipeline-eras
    renderer.graphics.pipeline-bridge
    renderer.graphics.binary-module
    renderer.graphics.contract
    renderer.graphics.properties
    renderer.graphics.negative-matrix
    renderer.graphics.era-matrix)

function(sir_verify_qualification_contracts sirDirectory)
    get_property(sirRegistered DIRECTORY "${sirDirectory}" PROPERTY TESTS)
    foreach(sirTest IN LISTS SIR_QUALIFICATION_CORE_REQUIRED_TESTS)
        if(NOT sirTest IN_LIST sirRegistered)
            message(FATAL_ERROR "Required engine contract test missing: ${sirTest}")
        endif()
    endforeach()
    foreach(sirTest IN LISTS SIR_QUALIFICATION_GRAPHICS_ABI_REQUIRED_TESTS)
        if(NOT sirTest IN_LIST sirRegistered)
            message(FATAL_ERROR "Required GraphicsABI contract test missing: ${sirTest}")
        endif()
    endforeach()
endfunction()

if(DEFINED SIR_QUALIFICATION_SYSTEM AND DEFINED SIR_QUALIFICATION_WRITE_PATH)
    if(NOT SIR_QUALIFICATION_SYSTEM IN_LIST SIR_QUALIFICATION_SYSTEMS)
        message(FATAL_ERROR "Unknown qualification system: ${SIR_QUALIFICATION_SYSTEM}")
    endif()
    file(WRITE "${SIR_QUALIFICATION_WRITE_PATH}" "${SIR_QUALIFICATION_SCOPE_${SIR_QUALIFICATION_SYSTEM}}\n")
endif()
