cmake_minimum_required(VERSION 3.22.1)
project(SIREngineApplication VERSION 1.0 LANGUAGES CXX)

# Compiler and build configuration
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast -g -Og -std=c++17")

# Detect platform and architecture
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	if (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
		set(CMAKE_SYSTEM_PROCESSOR "x86")
	endif()
endif()

# Find packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
pkg_check_modules(PANGO REQUIRED pango)
pkg_check_modules(FREETYPE2 REQUIRED freetype2)
pkg_check_modules(SDL2 REQUIRED sdl2)

# Include directories
include_directories(${GTK3_INCLUDE_DIRS} ${PANGO_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS} ${FREETYPE2_INCLUDE_DIRS})

# Link directories
link_directories(${GTK3_LIBRARY_DIRS} ${PANGO_LIBRARY_DIRS} ${SDL2_LIBRARY_DIRS} ${FREETYPE2_LIBRARY_DIRS})

# Source files
set(SOURCE_FILES
	Engine/Core/Application/GenericPlatform/GenericApplication.cpp
	Engine/Core/Application/Posix/PosixApplication.cpp
	Engine/Core/Application/Posix/PosixCrashHandler.cpp
	Engine/Memory/Allocators/SlabAllocator.cpp
	Engine/Memory/Allocators/VirtualStackAllocator.cpp
	Engine/Memory/Backend/TagArenaAllocator.cpp
	Engine/Memory/Memory.cpp
	Engine/Core/FileSystem/FileSystem.cpp
	Engine/Core/FileSystem/FileCache.cpp
	Engine/Core/Serialization/JSon/JsonCache.cpp
	Engine/Core/Serialization/Ini/IniReader.cpp
	Engine/Core/Serialization/Ini/IniWriter.cpp
	Engine/Core/Serialization/Ini/ini.cpp
	Engine/Core/Serialization/TBon/TBonSerializer.cpp
	Engine/Core/Logging/Logger.cpp
	Engine/Core/Events/EventManager.cpp
	Engine/Core/Events/KeyEvent.cpp
	Engine/Core/Input/InputManager.cpp
	Engine/Core/Util.cpp
	Engine/Core/ConsoleManager.cpp
	Engine/RenderLib/DrawBuffer.cpp
	Engine/RenderLib/RenderMain.cpp
	Engine/RenderLib/Backend/ImageLib/LoadPNG.cpp
	Engine/RenderLib/Backend/ImageLoader.cpp
	Engine/RenderLib/Backend/RenderContext.cpp
	Engine/RenderLib/Backend/RenderShaderPipeline.cpp
	Engine/RenderLib/Backend/OpenGL/GLBuffer.cpp
	Engine/RenderLib/Backend/OpenGL/GLContext.cpp
	Engine/RenderLib/Backend/OpenGL/GLFramebuffer.cpp
	Engine/RenderLib/Backend/OpenGL/GLProcs.cpp
	Engine/RenderLib/Backend/OpenGL/GLProgram.cpp
	Engine/RenderLib/Backend/OpenGL/GLShader.cpp
	Engine/RenderLib/Backend/OpenGL/GLShaderBuffer.cpp
	Engine/RenderLib/Backend/OpenGL/GLShaderPipeline.cpp
	Engine/RenderLib/Backend/OpenGL/GLTexture.cpp
	Engine/RenderLib/Backend/OpenGL/GLVertexArray.cpp
	Engine/RenderLib/Backend/Vulkan/VKBuffer.cpp
	Engine/RenderLib/Backend/Vulkan/VKContext.cpp
	Engine/RenderLib/Backend/Vulkan/VKFramebuffer.cpp
	Engine/RenderLib/Backend/Vulkan/VKProgram.cpp
	Engine/RenderLib/Backend/Vulkan/VKShader.cpp
	Engine/RenderLib/Backend/Vulkan/VKShaderBuffer.cpp
	Engine/RenderLib/Backend/Vulkan/VKShaderPipeline.cpp
	Engine/RenderLib/Backend/Vulkan/VKTexture.cpp
	pngloader/lodepng.cpp
	Engine/Core/SmMalloc/smmalloc_generic.cpp
	Engine/Core/SmMalloc/smmalloc_tls.cpp
	Engine/Core/SmMalloc/smmalloc.cpp
)

# Add executable
add_executable(SIREngineApplication ${SOURCE_FILES})

# Link libraries
target_link_libraries(SIREngineApplication
	${GTK3_LIBRARIES}
	${PANGO_LIBRARIES}
	${SDL2_LIBRARIES}
	-lGL
	-lvulkan
	${CMAKE_SOURCE_DIR}/libEASTL.a
	-lbacktrace
	-lfreetype
	-lboost_atomic-mt-x64
	-lboost_thread-mt-x64
	-lboost_chrono-mt-x64
)

# Shader files
set(SHADER_FILES
	${CMAKE_BINARY_DIR}/Resources/Shaders/Vulkan/Generic.frag.spv
	${CMAKE_BINARY_DIR}/Resources/Shaders/Vulkan/Generic.vert.spv
)

add_custom_command(
	OUTPUT ${CMAKE_BINARY_DIR}/Resources/Shaders/Vulkan/Generic.frag.spv
	COMMAND glslangValidator -H -V -o ${CMAKE_BINARY_DIR}/Resources/Shaders/Vulkan/Generic.frag.spv -S frag ${CMAKE_SOURCE_DIR}/Resources/Shaders/Vulkan/Generic.frag.glsl
	DEPENDS ${CMAKE_SOURCE_DIR}/Resources/Shaders/Vulkan/Generic.frag.glsl
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/Resources/Shaders/Vulkan
)

add_custom_command(
	OUTPUT ${CMAKE_BINARY_DIR}/Resources/Shaders/Vulkan/Generic.vert.spv
	COMMAND glslangValidator -H -V -o ${CMAKE_BINARY_DIR}/Resources/Shaders/Vulkan/Generic.vert.spv -S vert ${CMAKE_SOURCE_DIR}/Resources/Shaders/Vulkan/Generic.vert.glsl
	DEPENDS ${CMAKE_SOURCE_DIR}/Resources/Shaders/Vulkan/Generic.vert.glsl
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/Resources/Shaders/Vulkan
)

add_custom_target(
	shaders ALL
	DEPENDS ${SHADER_FILES}
)

# Create output directory
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/obj)

# Add include directories and compiler flags
target_include_directories(SIREngineApplication PRIVATE ${CMAKE_SOURCE_DIR})
target_compile_options(SIREngineApplication PRIVATE
	-Wnon-virtual-dtor
	-Wnull-dereference
	-Wcast-align
	-Wduplicated-cond
	-Wduplicated-branches
	-Wlogical-op
	-Wno-format
	-Werror=pointer-arith
	-Werror=write-strings
)

# Install rules
install(TARGETS SIREngineApplication DESTINATION .)

# Clean target
add_custom_target(clean-all
	COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/obj
	COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/SIREngineApplication
)