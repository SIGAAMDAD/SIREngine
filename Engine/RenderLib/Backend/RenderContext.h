#ifndef __SIRENGINE_RENDER_CONTEXT_H__
#define __SIRENGINE_RENDER_CONTEXT_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif

#include <Engine/RenderLib/RenderCommon.h>
#include <Engine/Core/SIREngine.h>
#include <SDL2/SDL.h>
#include "RenderBuffer.h"
#include "RenderTexture.h"
#include "RenderShader.h"
#include "RenderShaderPipeline.h"
#include <Engine/Memory/Allocators/VirtualStackAllocator.h>

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

	class IRenderContext
	{
	public:
		IRenderContext( const Application::ApplicationInfo_t& appInfo );
		virtual ~IRenderContext();

		SIRENGINE_FORCEINLINE SDL_Window *GetWindowHandle( void )
		{ return m_pWindow; }

		virtual void Init( void ) = 0;
		virtual void Shutdown( void ) = 0;

		virtual void SetupShaderPipeline( void ) = 0;
		virtual void SwapBuffers( void ) = 0;
		virtual void BeginFrame( void ) = 0;
		virtual void CompleteRenderPass( IRenderShaderPipeline *pShaderPipeline ) = 0;

		virtual void *Alloc( size_t nBytes, size_t nAligment = 16 ) = 0;
		virtual void Free( void *pBuffer ) = 0;

		static IRenderContext *CreateRenderContext( void );

		virtual IRenderProgram *AllocateProgram( const RenderProgramInit_t& programInfo ) = 0;
		virtual IRenderShader *AllocateShader( const RenderShaderInit_t& shaderInit ) = 0;
		virtual IRenderBuffer *AllocateBuffer( GPUBufferType_t nType, GPUBufferUsage_t nUsage, uint64_t nSize ) = 0;
		virtual IRenderTexture *AllocateTexture( const TextureInit_t& textureInfo ) = 0;

		virtual const GPUMemoryUsage_t GetMemoryUsage( void ) = 0;
		virtual void PrintMemoryInfo( void ) const = 0;
	protected:
		virtual void GetGPUExtensionList( void ) = 0;

		Application::ApplicationInfo_t m_AppInfo;
		SDL_Window *m_pWindow;

		CVirtualStackAllocator *m_pResourceAllocator;

		IRenderShaderPipeline *m_hShaderPipeline;

		CVector<CString> m_GPUExtensionList;
	};

	extern IRenderContext *g_pRenderContext;

	SIRENGINE_FORCEINLINE IRenderContext *GetRenderContext( void ) {
		return g_pRenderContext;
	}
};

#endif