COMPILE_PLATFORM=$(shell uname | sed -e 's/_.*//' | tr '[:upper:]' '[:lower:]' | sed -e 's/\//_/g')
COMPILE_ARCH=$(shell uname -m | sed -e 's/i.86/x86/' | sed -e 's/^arm.*/arm/')

ifeq ($(shell uname -m),arm64)
  COMPILE_ARCH=aarch64
endif

ifeq ($(COMPILE_PLATFORM),mingw32)
  ifeq ($(COMPILE_ARCH),i386)
    COMPILE_ARCH=x86
  endif
endif

ifeq ($(V),1)
  echo_cmd=@:
  Q=
else
  echo_cmd=@echo
  Q=@
endif

ifndef DESTDIR
DESTDIR=/usr/local/games/SIREngine
endif

CC		=g++

INCLUDE	=\
	-I. \

CFLAGS	=-Ofast -g -Og -std=c++17 $(INCLUDE) $(DEFINES) -Wall
O		=obj
EXE		=SIREngineApplication
LIB		=libSIREngine.so
LD_LIBS =-lGL -lSDL2 -lvulkan libEASTL.a -lbacktrace -Wl,-rpath='.' -lboost_atomic-mt-x64

MAKE=make
MKDIR=mkdir -p

.PHONY: all clean targets makedirs default

SRC=\
	$(O)/Engine/Core/Application/GenericPlatform/GenericApplication.o \
	$(O)/Engine/Core/Application/Posix/PosixApplication.o \
	\
	$(O)/Engine/Memory/Allocators/SlabAllocator.o \
	$(O)/Engine/Memory/Allocators/VirtualStackAllocator.o \
	\
	$(O)/Engine/Memory/Backend/TagArenaAllocator.o \
	$(O)/Engine/Memory/Memory.o \
	\
	$(O)/Engine/Core/FileSystem/FileSystem.o \
	\
	$(O)/Engine/Core/Serialization/JSon/JsonCache.o \
	\
	$(O)/Engine/Core/Serialization/Ini/IniReader.o \
	$(O)/Engine/Core/Serialization/Ini/IniWriter.o \
	$(O)/Engine/Core/Serialization/Ini/ini.o \
	\
	$(O)/Engine/Core/Logging/Logger.o \
	\
	$(O)/Engine/Core/Util.o \
	$(O)/Engine/Core/ConsoleManager.o \
	\
	$(O)/Engine/RenderLib/DrawBuffer.o \
	$(O)/Engine/RenderLib/RenderMain.o \
	\
	$(O)/Engine/RenderLib/Backend/ImageLoader.o \
	$(O)/Engine/RenderLib/Backend/RenderContext.o \
	$(O)/Engine/RenderLib/Backend/RenderShaderPipeline.o \
	\
	$(O)/Engine/RenderLib/Backend/OpenGL/GLBuffer.o \
	$(O)/Engine/RenderLib/Backend/OpenGL/GLContext.o \
	$(O)/Engine/RenderLib/Backend/OpenGL/GLFramebuffer.o \
	$(O)/Engine/RenderLib/Backend/OpenGL/GLProcs.o \
	$(O)/Engine/RenderLib/Backend/OpenGL/GLProgram.o \
	$(O)/Engine/RenderLib/Backend/OpenGL/GLShader.o \
	$(O)/Engine/RenderLib/Backend/OpenGL/GLShaderBuffer.o \
	$(O)/Engine/RenderLib/Backend/OpenGL/GLShaderPipeline.o \
	$(O)/Engine/RenderLib/Backend/OpenGL/GLTexture.o \
	$(O)/Engine/RenderLib/Backend/OpenGL/GLVertexArray.o \
	\
	$(O)/Engine/RenderLib/Backend/Vulkan/VKBuffer.o \
	$(O)/Engine/RenderLib/Backend/Vulkan/VKContext.o \
	$(O)/Engine/RenderLib/Backend/Vulkan/VKFramebuffer.o \
	$(O)/Engine/RenderLib/Backend/Vulkan/VKProgram.o \
	$(O)/Engine/RenderLib/Backend/Vulkan/VKShader.o \
	$(O)/Engine/RenderLib/Backend/Vulkan/VKShaderBuffer.o \
	$(O)/Engine/RenderLib/Backend/Vulkan/VKShaderPipeline.o \
	$(O)/Engine/RenderLib/Backend/Vulkan/VKTexture.o \
	\
	$(O)/Engine/RenderLib/Backend/ImageLib/LoadPNG.o \
	\
	$(O)/pngloader/lodepng.o \
	\
	$(O)/Engine/Core/SmMalloc/smmalloc_generic.o \
	$(O)/Engine/Core/SmMalloc/smmalloc_tls.o \
	$(O)/Engine/Core/SmMalloc/smmalloc.o \

VERTEX_SPIRV_FILES=\
	Resources/Shaders/Vulkan/Generic.vert.spv \

FRAGMENT_SPIRV_FILES=\
	Resources/Shaders/Vulkan/Generic.frag.spv \

default:
	$(MAKE) targets

all: default

makedirs:
	@if [ ! -d $(O) ];then mkdir $(O);fi
	@if [ ! -d $(O)/pngloader ];then mkdir $(O)/pngloader;fi
	@if [ ! -d $(O)/Engine ];then mkdir $(O)/Engine;fi
	@if [ ! -d $(O)/Engine/Core/ ];then mkdir $(O)/Engine/Core;fi
	@if [ ! -d $(O)/Engine/Core/Serialization ];then mkdir $(O)/Engine/Core/Serialization;fi
	@if [ ! -d $(O)/Engine/Core/Serialization/Ini ];then mkdir $(O)/Engine/Core/Serialization/Ini;fi
	@if [ ! -d $(O)/Engine/Core/Serialization/JSon ];then mkdir $(O)/Engine/Core/Serialization/JSon;fi
	@if [ ! -d $(O)/Engine/Core/SmMalloc ];then mkdir $(O)/Engine/Core/SmMalloc;fi
	@if [ ! -d $(O)/Engine/Memory ];then mkdir $(O)/Engine/Memory;fi
	@if [ ! -d $(O)/Engine/Memory/Allocators ];then mkdir $(O)/Engine/Memory/Allocators;fi
	@if [ ! -d $(O)/Engine/Memory/Backend ];then mkdir $(O)/Engine/Memory/Backend;fi
	@if [ ! -d $(O)/Engine/Core/Logging ];then mkdir $(O)/Engine/Core/Logging;fi
	@if [ ! -d $(O)/Engine/Core/FileSystem ];then mkdir $(O)/Engine/Core/FileSystem;fi
	@if [ ! -d $(O)/Engine/Core/Application/ ];then mkdir $(O)/Engine/Core/Application;fi
	@if [ ! -d $(O)/Engine/Core/Application/GenericPlatform/ ];then mkdir $(O)/Engine/Core/Application/GenericPlatform/;fi
	@if [ ! -d $(O)/Engine/Core/Application/Posix/ ];then mkdir $(O)/Engine/Core/Application/Posix/;fi
	@if [ ! -d $(O)/Engine/RenderLib ];then mkdir $(O)/Engine/RenderLib;fi
	@if [ ! -d $(O)/Engine/RenderLib/Backend ];then mkdir $(O)/Engine/RenderLib/Backend;fi
	@if [ ! -d $(O)/Engine/RenderLib/Backend/ImageLib ];then mkdir $(O)/Engine/RenderLib/Backend/ImageLib;fi
	@if [ ! -d $(O)/Engine/RenderLib/Backend/OpenGL ];then mkdir $(O)/Engine/RenderLib/Backend/OpenGL;fi
	@if [ ! -d $(O)/Engine/RenderLib/Backend/Vulkan ];then mkdir $(O)/Engine/RenderLib/Backend/Vulkan;fi


targets: makedirs
	@echo ""
	@echo "Building $(EXE):"
	@echo ""
	@echo "  VERSION: $(VERSION).$(VERSION_UPDATE).$(VERSION_PATCH)"
	@echo "  PLATFORM: $(PLATFORM)"
	@echo "  ARCH: $(ARCH)"
	@echo "  COMPILE_PLATFORM: $(COMPILE_PLATFORM)"
	@echo "  COMPILE_ARCH: $(COMPILE_ARCH)"
ifdef MINGW
	@echo "  WINDRES: $(WINDRES)"
endif
	@echo "  CC: $(CC)"
	@echo ""
	@echo "  CFLAGS:"
	@for i in $(CFLAGS); \
	do \
		echo "    $$i"; \
	done
	@echo ""
	@echo "  Output:"
	@for i in $(TARGETS); \
	do \
		echo "    $$i"; \
	done
	@echo ""
	$(MAKE) $(EXE)

Resources/Shaders/Vulkan/%.frag.spv: Resources/Shaders/Vulkan/%.frag.glsl
	glslangValidator -H -V -o $@ -S frag $<
Resources/Shaders/Vulkan/%.vert.spv: Resources/Shaders/Vulkan/%.vert.glsl
	glslangValidator -H -V -o $@ -S vert $<
$(O)/pngloader/%.o: pngloader/%.cpp
	$(CC) $(CFLAGS) -o $@ -c $<
$(O)/Engine/Core/%.o: Engine/Core/%.cpp
	$(CC) $(CFLAGS) -o $@ -c $<
$(O)/Engine/Core/%.o: Engine/Core/%.c
	$(CC) $(CFLAGS) -o $@ -c $<
$(O)/Engine/Memory/Allocators/%.o: Engine/Memory/Allocators/%.cpp
	$(CC) $(CFLAGS) -o $@ -c $<
$(O)/Engine/Memory/Backend/%.o: Engine/Memory/Backend/%.cpp
	$(CC) $(CFLAGS) -o $@ -c $<
$(O)/Engine/Memory/%.o: Engine/Memory/%.cpp
	$(CC) $(CFLAGS) -o $@ -c $<
$(O)/Engine/Core/Application/Posix/%.o: Engine/Application/Posix/%.cpp
	$(CC) $(CFLAGS) -o $@ -c $<
$(O)/Engine/RenderLib/%.o: Engine/RenderLib/%.cpp
	$(CC) $(CFLAGS) -o $@ -c $<
$(O)/Engine/RenderLib/Backend/%.o: Engine/RenderLib/Backend/%.cpp
	$(CC) $(CFLAGS) -o $@ -c $<
$(O)/Engine/RenderLib/Backend/Vulkan/%.o: Engine/RenderLib/Backend/Vulkan/%.cpp
	$(CC) $(CFLAGS) -o $@ -c $<
$(O)/Engine/RenderLib/Backend/OpenGL/%.o: Engine/RenderLib/Backend/OpenGL/%.cpp
	$(CC) $(CFLAGS) -o $@ -c $<

$(EXE): $(SRC) $(VERTEX_SPIRV_FILES) $(FRAGMENT_SPIRV_FILES)
	$(CC) $(CFLAGS) $(SRC) -o $(EXE) $(LD_LIBS)

clean:
	rm -rf $(O)
	rm $(EXE)

