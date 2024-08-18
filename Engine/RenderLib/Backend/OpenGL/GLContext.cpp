#include "../RenderBuffer.h"
#include "../RenderShaderPipeline.h"
#include "../RenderContext.h"
#include "GLContext.h"
#include "GLCommon.h"
#include "GLBuffer.h"
#include "GLShader.h"
#include "GLProgram.h"
#include "GLTexture.h"
#include "GLVertexArray.h"
#include "GLShaderPipeline.h"
#include <Engine/Core/ThreadSystem/Thread.h>

using namespace SIREngine;
using namespace SIREngine::RenderLib;
using namespace SIREngine::RenderLib::Backend;

namespace SIREngine::RenderLib::Backend::OpenGL {

	CVector<GLTexture *> GLContext::g_EvictionLRUCache;
	CThread s_RenderThread( "GLBackendThread" );

	CVar<bool32> r_GLES(
		"r.OpenGL.ES",
		0,
		Cvar_Save,
		"Enables OpenGL ES",
		CVG_RENDERER
	);
	CVar<int> r_GLVersionMajor(
		"r.OpenGL.VersionMajor",
		3,
		Cvar_Save,
		"Sets OpenGL major version",
		CVG_RENDERER
	);
	CVar<int> r_GLVersionMinor(
		"r.OpenGL.VersionMinor",
		3,
		Cvar_Save,
		"Sets OpenGL minor version",
		CVG_RENDERER
	);
	CVar<bool32> r_UseShaderStorageBufferObjects(
		"r.OpenGL.UseShaderStorageBufferObjects",
		0,
		Cvar_Default,
		"Allow usage of the extension GL_ARB_shader_storage_buffer_object\n"
		"  0: Use a uniform buffer for shader data transmission\n"
		"  1: Use a shader storage buffer for shader data transmission",
		CVG_RENDERER
	);
	CVar<bool32> r_UseMappedBufferObjects(
		"r.OpenGL.UseMappedBufferObjects",
		0,
		Cvar_Save,
		"Use glMapBuffer instead of glBufferSubData, GL_ARB_map_buffer_range must be available on this device.",
		CVG_RENDERER
	);
	CVar<uint32_t> r_GLTetureMinLRUSize(
		"r.OpenGL.TextureMinLRUSize",
		528,
		Cvar_Save | Cvar_Developer,
		"Sets the maximum number of textures that can be in GPU memory\n"
		"before the engine begins purging memory.\n",
		CVG_RENDERER
	);

	extern CVar<bool32> r_IgnoreShaderCompilerFailure;
	extern CVar<bool32> r_IgnoreShaderLinkFailure;
	extern CVar<bool32> r_EnableShaderLRU;
};

/* TODO: make this a little bit more relevant
uint32_t g_nMaxSubDataSize = 2*1024*1024;
CVarRef<uint32_t> r_MaxSubBufferSize(
	"r.OpenGL.MaxSubBufferSize",
	g_nMaxSubDataSize,
	Cvar_Developer,
	"Sets the maximum amount of data the can be sent per glBufferSubData call.",
	CVG_RENDERER
);
*/

bool32 g_bUseBufferDiscard = true;
CVarRef<bool32> r_UseBufferDiscard(
	"r.OpenGL.UseBufferDiscard",
	g_bUseBufferDiscard,
	Cvar_Developer,
	"Toggles buffer orphaning for faster OpenGL synching.",
	CVG_RENDERER
);

namespace SIREngine::Application { extern CThreadAtomic<bool> g_bExitApp; };
extern CVar<bool32> r_UsePixelBufferObjects;

namespace SIREngine::RenderLib::Backend::OpenGL {

void GLError( const char *pCall, GLenum nResult )
{
	switch ( nResult ) {
	case GL_OUT_OF_MEMORY:
		SIRENGINE_ERROR( "%s = GL_OUT_OF_MEMORY", pCall );
		break; // TODO: write some eviction functions
	case GL_INVALID_ENUM:
		SIRENGINE_WARNING( "%s = GL_INVALID_ENUM", pCall );
		break;
	case GL_INVALID_INDEX:
		SIRENGINE_WARNING( "%s = GL_INVALID_INDEX", pCall );
		break;
	case GL_INVALID_OPERATION:
		SIRENGINE_WARNING( "%s = GL_INVALID_OPERATION", pCall );
		break;
	case GL_INVALID_VALUE:
		SIRENGINE_WARNING( "%s = GL_INVALID_VALUE", pCall );
		break;
	case GL_STACK_OVERFLOW:
		SIRENGINE_WARNING( "%s = GL_STACK_OVERFLOW", pCall );
		break;
	case GL_STACK_UNDERFLOW:
		SIRENGINE_WARNING( "%s = GL_STACK_UNDERFLOW", pCall );
		break;
	case GL_CONTEXT_LOST:
		SIRENGINE_WARNING( "%s = GL_CONTEXT_LOST", pCall );
		break;
	case GL_INVALID_FRAMEBUFFER_OPERATION:
		SIRENGINE_WARNING( "%s = GL_INVALID_FRAMEBUFFER_OPERATION", pCall );
		break;
	default:
		SIRENGINE_WARNING( "Unknown GLError 0x%x after \"%s\"", nResult, pCall );
	};
}

GLContext::GLContext( const Application::ApplicationInfo_t& appInfo )
	: IRenderContext( appInfo )
{
	r_UseBufferDiscard.Register();
	r_UseShaderStorageBufferObjects.Register();
	r_GLTetureMinLRUSize.Register();
	r_UseMappedBufferObjects.Register();
	r_UsePixelBufferObjects.Register();
	r_GLES.Register();
	r_GLVersionMajor.Register();
	r_GLVersionMinor.Register();
}

GLContext::~GLContext()
{
}

void GLContext::Init( void )
{
#if !defined(SIRENGINE_BUILD_RENDERLIB_GLFW3)
	m_pGLContext = SDL_GL_CreateContext( m_pWindow );
	if ( !m_pGLContext ) {
		SIRENGINE_ERROR( "SDL_GL_CreateContext failed: %s", SDL_GetError() );
	}
	SIRENGINE_LOG_LEVEL( RenderBackend, ELogLevel::Info, "Created SDL2 OpenGL Context" );

	SDL_GL_MakeCurrent( m_pWindow, m_pGLContext );

	SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 24 );
	SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, 8 );
	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );

	if ( r_GLES.GetValue() ) {
		SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES );
		SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 3 );
		SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 0 );
	} else {
		SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE );
		SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, r_GLVersionMajor.GetValue() );
		SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, r_GLVersionMinor.GetValue() );
	}
#else
	glfwInitHint( GLFW_DOUBLEBUFFER, GLFW_TRUE );
	glfwInitHint( GLFW_DEPTH_BITS, 24 );
	glfwInitHint( GLFW_STENCIL_BITS, 8 );
	glfwInitHint( GLFW_CONTEXT_VERSION_MAJOR, r_GLVersionMajor.GetValue() );
	glfwInitHint( GLFW_CONTEXT_VERSION_MINOR, r_GLVersionMinor.GetValue() );
	glfwInitHint( GLFW_CONTEXT_CREATION_API, GLFW_NATIVE_CONTEXT_API );
	if ( glfwInit() == GLFW_FALSE ) {
		SIRENGINE_ERROR( "glfwInit failed" );
	}

	m_pWindow = glfwCreateWindow( m_AppInfo.nWindowWidth, m_AppInfo.nWindowHeight, m_AppInfo.pszWindowName, NULL, NULL );
	if ( !m_pWindow ) {
		const char *description;
		glfwGetError( &description );
		SIRENGINE_ERROR( "glfwCreateWindow failed: %s", description );
	}

	glfwMakeContextCurrent( m_pWindow );
#endif
	
	InitGLProcs();
	SIRENGINE_LOG_LEVEL( RenderBackend, ELogLevel::Info, "Loaded GL Procs" );

	SIREngine_strncpyz( m_szGLSLVersion, (const char *)nglGetString( GL_SHADING_LANGUAGE_VERSION ),
		sizeof( m_szGLSLVersion ) );
	SIREngine_strncpyz( m_szDriverVersion, (const char *)nglGetString( GL_VERSION ),
		sizeof( m_szDriverVersion ) );
	SIREngine_strncpyz( m_szVendorString, (const char *)nglGetString( GL_VENDOR ),
		sizeof( m_szVendorString ) );
	SIREngine_strncpyz( m_szRendererString, (const char *)nglGetString( GL_RENDERER ),
		sizeof( m_szRendererString ) );
	
	SIRENGINE_LOG_LEVEL( RenderBackend, ELogLevel::Info, "OpenGL Driver Query:" );
	SIRENGINE_LOG_LEVEL( RenderBackend, ELogLevel::Info, " Driver Version: %s", m_szDriverVersion );
	SIRENGINE_LOG_LEVEL( RenderBackend, ELogLevel::Info, " Shader Language Version (GLSL): %s", m_szGLSLVersion );
	SIRENGINE_LOG_LEVEL( RenderBackend, ELogLevel::Info, " Vendor: %s", m_szVendorString );
	SIRENGINE_LOG_LEVEL( RenderBackend, ELogLevel::Info, " Renderer: %s", m_szRendererString );
	SIRENGINE_LOG_LEVEL( RenderBackend, ELogLevel::Info, " SDL2 GLVersion Hint: v%i.%i", r_GLVersionMajor.GetValue(), r_GLVersionMinor.GetValue() );

	GetGPUExtensionList();

	CheckExtensionsSupport();
}

void GLContext::Shutdown( void )
{
#if defined(SIRENGINE_BUILD_RENDERLIB_GLFW3)
	glfwMakeContextCurrent( NULL );
	glfwDestroyWindow( m_pWindow );
#else
	SDL_GL_MakeCurrent( m_pWindow, NULL );
	if ( m_pGLContext ) {
		SDL_GL_DeleteContext( m_pGLContext );
	}
#endif
}

void GLContext::CheckExtensionsSupport( void )
{
	auto GPUHasExtension = [&]( const char *pExtensionName ) -> bool {
		return eastl::find( m_GPUExtensionList.cbegin(), m_GPUExtensionList.cend(),
			pExtensionName ) != m_GPUExtensionList.cend();
	};

	if ( GPUHasExtension( "GL_ARB_pixel_buffer_object" ) ) {
		r_UsePixelBufferObjects.SetValue( true );
		SIRENGINE_LOG_LEVEL( RenderBackend, ELogLevel::Info, "Using GL_ARB_pixel_buffer_object." );
	} else {
		SIRENGINE_WARNING( "GL_ARB_pixel_buffer_object not found, texture streaming objects not available." );
	}
}

void GLContext::SetupShaderPipeline( void )
{
}

void GLContext::BeginFrame( void )
{
	GL_CALL( nglClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ) );
	GL_CALL( nglClearColor( 0.1f, 0.1f, 0.1f, 1.0f ) );
	GL_CALL( nglViewport( 0, 0, Application::Get()->GetAppInfo().nWindowWidth, Application::Get()->GetAppInfo().nWindowHeight ) );
	GL_CALL( nglScissor( 0, 0, Application::Get()->GetAppInfo().nWindowWidth, Application::Get()->GetAppInfo().nWindowHeight ) );
}

void GLContext::SwapBuffers( void )
{
	nglFlush();
#if !defined(SIRENGINE_BUILD_RENDERLIB_GLFW3)
	SDL_GL_MakeCurrent( m_pWindow, m_pGLContext );
	SDL_GL_SwapWindow( m_pWindow );
#else
	glfwSwapBuffers( m_pWindow );
#endif

	for ( uint32_t nEvictionCount = 0; g_EvictionLRUCache.size() > r_GLTetureMinLRUSize.GetValue(); nEvictionCount++ ) {
		GLTexture *pTexture = g_EvictionLRUCache.back();
		pTexture->EvictGLResource();
		g_EvictionLRUCache.pop_back();
	}
}

void GLContext::CompleteRenderPass( IRenderShaderPipeline *pShaderPipeline )
{
	GLShaderPipeline *pRenderData = dynamic_cast<GLShaderPipeline *>( pShaderPipeline );
	GLProgram *pShader = pRenderData->GetPipelineData()->pShader;

	CVector<GLShaderBuffer *>& inputBuffers = pRenderData->GetPipelineData()->dataInputBuffers;
	for ( auto& it : inputBuffers ) {
//        it->SwapData(  );
	}

	pRenderData->GetVertexArray()->Bind();

	GL_CALL( nglDrawElements( GL_TRIANGLES, pRenderData->GetIndexCount(), GL_UNSIGNED_INT, NULL ) );

	pRenderData->GetVertexArray()->Unbind();
}

void GLContext::GetGPUExtensionList( void )
{
	char szExtensions[80000];
	GLint nExtensionCount, i;
	const char *pSrc;
	char *pDest;
	char szExtensionName[128];

	SIREngine_strncpyz( szExtensions, (const char *)nglGetString( GL_EXTENSIONS ), sizeof( szExtensions ) );
	GL_CALL( nglGetIntegerv( GL_NUM_EXTENSIONS, &nExtensionCount ) );
	SIRENGINE_LOG_LEVEL( RenderBackend, ELogLevel::Info, "Found %i OpenGL Extensions", nExtensionCount );
	SIRENGINE_LOG_LEVEL( RenderBackend, ELogLevel::Info, "Got OpenGL Extensions: %s", szExtensions );

	m_GPUExtensionList.reserve( nExtensionCount );
	pSrc = szExtensions;
	for ( pSrc = szExtensions; *pSrc; ) {
		pDest = szExtensionName;
		while ( *pSrc != ' ' ) {
			*pDest++ = *pSrc++;
		}
		pSrc++;
		*pDest++ = '\0';
		m_GPUExtensionList.emplace_back( szExtensionName );

		SIRENGINE_LOG_LEVEL( RenderBackend, ELogLevel::Info, "Found OpenGL Extension \"%s\"", szExtensionName );
	}
}

void *GLContext::Alloc( size_t nBytes, size_t nAlignment )
{
	return malloc( nBytes );
}

void GLContext::Free( void *pBuffer )
{
	free( pBuffer );
}

const GPUMemoryUsage_t GLContext::GetMemoryUsage( void )
{
	GPUMemoryUsage_t MemUsage;
	GLint nValue;

	nglGetIntegerv( GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &nValue );
	MemUsage.remainingMemory = nValue;

	nglGetIntegerv( GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &nValue );
	MemUsage.totalMemory = nValue;

	MemUsage.usedMemory = MemUsage.totalMemory - MemUsage.remainingMemory;
	
	return MemUsage;
}

void GLContext::PrintMemoryInfo( void ) const
{
}


IRenderProgram *GLContext::AllocateProgram( const RenderProgramInit_t& programInfo )
{
	return new ( Alloc( sizeof( GLProgram ) ) ) GLProgram( programInfo );
}

IRenderShader *GLContext::AllocateShader( const RenderShaderInit_t& shaderInit )
{
	return new ( Alloc( sizeof( GLShader ) ) ) GLShader( shaderInit );
}

IRenderBuffer *GLContext::AllocateBuffer( GPUBufferType_t nType, GPUBufferUsage_t nUsage, uint64_t nSize )
{
	return new ( Alloc( sizeof( GLBuffer ) ) ) GLBuffer( nType, nUsage, nSize );
}

IRenderTexture *GLContext::AllocateTexture( const TextureInit_t& textureInfo )
{
	return new ( Alloc( sizeof( GLTexture ) ) ) GLTexture( textureInfo );
}

};