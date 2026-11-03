# Native driver implementations are opt-in pending the portable rendering ABI.
if(SIR_WITH_D3D12)
    add_subdirectory(Drivers/D3D12)
endif()
