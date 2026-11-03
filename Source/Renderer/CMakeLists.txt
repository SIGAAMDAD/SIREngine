# Concrete graphics drivers are opt-in until the shared rendering-device ABI is frozen.
if(SIR_WITH_VULKAN)
    add_subdirectory(Drivers/Vulkan)
endif()
