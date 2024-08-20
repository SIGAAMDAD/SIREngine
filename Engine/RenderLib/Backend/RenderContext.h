#ifndef __SIRENGINE_RENDER_CONTEXT_H__
#define __SIRENGINE_RENDER_CONTEXT_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include <Engine/RenderLib/RenderCommon.h>
#include <Engine/Core/SIREngine.h>
#include "RenderBuffer.h"
#include "RenderTexture.h"
#include "RenderShader.h"
#include "RenderShaderPipeline.h"
#include <EASTL/fixed_vector.h>
#include <Engine/Memory/Allocators/VirtualStackAllocator.h>
#if defined(SIRENGINE_BUILD_RENDERLIB_GLFW3)
#include <GLFW/glfw3.h>
#else
#include <SDL2/SDL.h>
#endif

#define WF_OPENGL_CONTEXT               0x000100
#define WF_VULKAN_CONTEXT               0x000200
#define WF_D3D11_CONTEXT                0x000400
#define WF_SOFTWARE_CONTEXT             0x000800
#define WF_CONTEXT_BITS                 0x000f00

#define WF_MODE_FULLSCREEN              0x000001
#define WF_MODE_WINDOWED                0x000002
#define WF_MODE_BORDERLESS              0x000010
#define WF_MODE_BITS                    0x00000f

namespace SIREngine::RenderLib::Backend {
	typedef struct GPUMemoryUsage {
		uint32_t usedMemory;
		uint32_t remainingMemory;
		uint32_t totalMemory;
	} GPUMemoryUsage_t;

#if defined(SIRENGINE_BUILD_RENDERLIB_GLFW3)
	using NativeWindow_t = GLFWwindow *;
#else
	using NativeWindow_t = SDL_Window *;
#endif

	class IRenderContext
	{
	public:
		IRenderContext( const Application::ApplicationInfo_t& appInfo );
		virtual ~IRenderContext();

		SIRENGINE_FORCEINLINE NativeWindow_t GetWindowHandle( void )
		{ return m_pWindow; }

		virtual void Init( void ) = 0;
		virtual void Shutdown( void ) = 0;

		virtual void SetupShaderPipeline( void ) = 0;
		virtual void SwapBuffers( void ) = 0;
		virtual void BeginFrame( void ) = 0;
		virtual void CompleteRenderPass( IRenderShaderPipeline *pShaderPipeline ) = 0;
		virtual void FinalizeResources( void ) = 0;

		virtual void *Alloc( size_t nBytes, size_t nAligment = 16 ) = 0;
		virtual void Free( void *pBuffer ) = 0;

		static IRenderContext *CreateRenderContext( void );

		virtual IRenderProgram *AllocateProgram( const RenderProgramInit_t& programInfo ) = 0;
		virtual IRenderShader *AllocateShader( const RenderShaderInit_t& shaderInit ) = 0;
		virtual IRenderBuffer *AllocateBuffer( GPUBufferType_t nType, GPUBufferUsage_t nUsage, uint64_t nSize ) = 0;
		virtual IRenderTexture *AllocateTexture( const TextureInit_t& textureInfo ) = 0;

		virtual const GPUMemoryUsage_t GetMemoryUsage( void ) = 0;
		virtual void PrintMemoryInfo( void ) const = 0;

		SIRENGINE_FORCEINLINE CHashMap<FileSystem::CFilePath, IRenderTexture *>& GetTextures( void )
		{ return m_Textures; }
	protected:
		virtual void GetGPUExtensionList( void ) = 0;

		static void *VirtualPoolAllocate( size_t nSize );
		static void *VirtualPoolClearedAllocate( size_t nElems, size_t nSize );
		static void *VirtualPoolReallocate( void *pOriginalPointer, size_t nSize );
		static void VirtualPoolFree( void *pMemory );

		Application::ApplicationInfo_t m_AppInfo;
		NativeWindow_t m_pWindow;

		IRenderShaderPipeline *m_hShaderPipeline;
		CHashMap<FileSystem::CFilePath, IRenderTexture *> m_Textures;

		eastl::fixed_vector<CString, 1024, true, MemoryAllocator<char>> m_GPUExtensionList;
	};

	extern IRenderContext *g_pRenderContext;
	SIRENGINE_DECLARE_LOG_CATEGORY( RenderBackend, ELogLevel::Info );

	SIRENGINE_FORCEINLINE IRenderContext *GetRenderContext( void ) {
		return g_pRenderContext;
	}
};

#endif