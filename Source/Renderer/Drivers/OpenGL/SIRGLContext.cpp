/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Renderer/Drivers/OpenGL/SIRGLContext.h>
#include <Renderer/Drivers/OpenGL/SIRGLFunctionLoader.h>
#include <string.h>

#if defined( _WIN32 )
#include <Core/Platform/Window/Windows/SIRWGLBootstrapWindow.h>
#include <windows.h>
#else
#include <dlfcn.h>
#include <pthread.h>
#endif

// Private native loader. No GL/EGL headers or SDK pointers cross the shared ABI.
#if !defined( _WIN32 )
typedef void *sirEGLDisplay_t;
typedef void *sirEGLSurface_t;
typedef void *sirEGLContext_t;
typedef void *sirEGLConfig_t;
typedef int32_t sirEGLInt_t;
typedef uint32_t sirEGLBoolean_t;
typedef void *( *sirEGLGetProcAddress_t )( const char *pszName );
typedef sirEGLDisplay_t ( *sirEGLGetDisplay_t )( void *pNative );
typedef sirEGLDisplay_t ( *sirEGLGetPlatformDisplay_t )( uint32_t nPlatform, void *pNative, const sirEGLInt_t *pAttrs );
typedef const char *( *sirEGLQueryString_t )( sirEGLDisplay_t pDisplay, sirEGLInt_t nName );
typedef sirEGLBoolean_t ( *sirEGLInitialize_t )( sirEGLDisplay_t pDisplay, sirEGLInt_t *pMajor, sirEGLInt_t *pMinor );
typedef sirEGLBoolean_t ( *sirEGLChooseConfig_t )( sirEGLDisplay_t pDisplay, const sirEGLInt_t *pAttrs, sirEGLConfig_t *pConfig,
                                                   sirEGLInt_t nCount, sirEGLInt_t *pFound );
typedef sirEGLBoolean_t ( *sirEGLBindAPI_t )( uint32_t nApi );
typedef sirEGLSurface_t ( *sirEGLCreatePbufferSurface_t )( sirEGLDisplay_t pDisplay, sirEGLConfig_t pConfig, const sirEGLInt_t *pAttrs );
typedef sirEGLContext_t ( *sirEGLCreateContext_t )( sirEGLDisplay_t pDisplay, sirEGLConfig_t pConfig, sirEGLContext_t pShare,
                                                    const sirEGLInt_t *pAttrs );
typedef sirEGLBoolean_t ( *sirEGLMakeCurrent_t )( sirEGLDisplay_t pDisplay, sirEGLSurface_t pDraw, sirEGLSurface_t pRead,
                                                  sirEGLContext_t pContext );
typedef sirEGLBoolean_t ( *sirEGLDestroyContext_t )( sirEGLDisplay_t pDisplay, sirEGLContext_t pContext );
typedef sirEGLBoolean_t ( *sirEGLDestroySurface_t )( sirEGLDisplay_t pDisplay, sirEGLSurface_t pSurface );
typedef sirEGLBoolean_t ( *sirEGLTerminate_t )( sirEGLDisplay_t pDisplay );
typedef sirEGLContext_t ( *sirEGLGetCurrentContext_t )( void );
typedef sirEGLSurface_t ( *sirEGLCreateWindowSurface_t )( sirEGLDisplay_t, sirEGLConfig_t, void *, const sirEGLInt_t * );
typedef sirEGLBoolean_t ( *sirEGLSwapBuffers_t )( sirEGLDisplay_t, sirEGLSurface_t );
typedef sirEGLBoolean_t ( *sirEGLSwapInterval_t )( sirEGLDisplay_t, sirEGLInt_t );
typedef void *( *sirWaylandEGLWindowCreate_t )( void *, int32_t, int32_t );
typedef void ( *sirWaylandEGLWindowResize_t )( void *, int32_t, int32_t, int32_t, int32_t );
typedef void ( *sirWaylandEGLWindowDestroy_t )( void * );

static constexpr uint32_t SIR_EGL_OPENGL_API = 0x30A2;
static constexpr uint32_t SIR_EGL_PLATFORM_SURFACELESS = 0x31DD;
static constexpr uint32_t SIR_EGL_PLATFORM_X11 = 0x31D5;
static constexpr uint32_t SIR_EGL_PLATFORM_WAYLAND = 0x31D8;
static constexpr sirEGLInt_t SIR_EGL_WINDOW_BIT = 4;
static constexpr sirEGLInt_t SIR_EGL_NONE = 0x3038;
static constexpr sirEGLInt_t SIR_EGL_SURFACE_TYPE = 0x3033;
static constexpr sirEGLInt_t SIR_EGL_PBUFFER_BIT = 1;
static constexpr sirEGLInt_t SIR_EGL_RENDERABLE_TYPE = 0x3040;
static constexpr sirEGLInt_t SIR_EGL_OPENGL_BIT = 8;
static constexpr sirEGLInt_t SIR_EGL_RED_SIZE = 0x3024;
static constexpr sirEGLInt_t SIR_EGL_GREEN_SIZE = 0x3023;
static constexpr sirEGLInt_t SIR_EGL_BLUE_SIZE = 0x3022;
static constexpr sirEGLInt_t SIR_EGL_ALPHA_SIZE = 0x3021;
static constexpr sirEGLInt_t SIR_EGL_WIDTH = 0x3057;
static constexpr sirEGLInt_t SIR_EGL_HEIGHT = 0x3056;
static constexpr sirEGLInt_t SIR_EGL_CONTEXT_MAJOR = 0x3098;
static constexpr sirEGLInt_t SIR_EGL_CONTEXT_MINOR = 0x30FB;
static constexpr sirEGLInt_t SIR_EGL_CONTEXT_PROFILE = 0x30FD;
static constexpr sirEGLInt_t SIR_EGL_CORE_PROFILE = 1;
static constexpr sirEGLInt_t SIR_EGL_EXTENSIONS = 0x3055;
#endif

typedef const uint8_t *( *sirGLGetString_t )( uint32_t nName );
typedef void ( *sirGLGetIntegerv_t )( uint32_t nName, int32_t *pValue );

typedef struct sirGLNativeContext_s
{
	sirGraphicsABIHost_t host;
	sirGLGetString_t pGetString;
	sirGLGetIntegerv_t pGetInteger;
#if defined( _WIN32 )
	DWORD nOwnerThread;
	HMODULE pLibrary;
	sirWGLBootstrapWindow_t window;
	HGLRC pRenderContext;
#else
	pthread_t ownerThread;
	void *pLibraryEGL;
	void *pLibraryGL;
	sirEGLDisplay_t pDisplay;
	sirEGLSurface_t pSurface;
	sirEGLConfig_t pConfig;
	void *pNativeDisplay;
	uint32_t nNativePlatform;
	sirEGLContext_t pRenderContext;
	sirEGLMakeCurrent_t pMakeCurrent;
	sirEGLDestroyContext_t pDestroyContext;
	sirEGLDestroySurface_t pDestroySurface;
	sirEGLTerminate_t pTerminate;
	sirEGLGetCurrentContext_t pGetCurrentContext;
	sirEGLCreateWindowSurface_t pCreateWindowSurface;
	sirEGLSwapBuffers_t pSwapBuffers;
	sirEGLSwapInterval_t pSwapInterval;
	sirEGLGetProcAddress_t pGetProcAddress;
	void *pWaylandEGLLibrary;
	sirWaylandEGLWindowCreate_t pWaylandCreate;
	sirWaylandEGLWindowResize_t pWaylandResize;
	sirWaylandEGLWindowDestroy_t pWaylandDestroy;
#endif
} sirGLNativeContext_t;

void *sirGLResolveNativeProcedure( void *pUserData, const char *pszName ) noexcept
{
	sirGLNativeContext_t *pContext = ( sirGLNativeContext_t * )pUserData;
	if ( pContext == nullptr || pszName == nullptr || !sirGLContextOwnsThread( pContext ) ) {
		return nullptr;
	}
#if defined( _WIN32 )
	void *pAddress = ( void * )wglGetProcAddress( pszName );
	if ( !sirGLIsUsableProcedure( pAddress ) ) {
		pAddress = ( void * )GetProcAddress( pContext->pLibrary, pszName );
	}
#else
	void *pAddress = pContext->pGetProcAddress != nullptr ? pContext->pGetProcAddress( pszName ) : nullptr;
	if ( !sirGLIsUsableProcedure( pAddress ) && pContext->pLibraryGL != nullptr ) {
		pAddress = dlsym( pContext->pLibraryGL, pszName );
	}
#endif
	return sirGLIsUsableProcedure( pAddress ) ? pAddress : nullptr;
}

bool sirGLContextOwnsThread( const sirGLNativeContext_t *pContext ) noexcept
{
	if ( pContext == nullptr ) {
		return false;
	}
#if defined( _WIN32 )
	return pContext->nOwnerThread == GetCurrentThreadId();
#else
	return pthread_equal( pContext->ownerThread, pthread_self() ) != 0;
#endif
}

static void sirGLCopyString( char *pszOutput, size_t nCapacity, const uint8_t *pString ) noexcept
{
	if ( nCapacity == 0 ) {
		return;
	}
	pszOutput[0] = '\0';
	if ( pString == nullptr ) {
		return;
	}
	strncpy( pszOutput, ( const char * )pString, nCapacity - 1 );
	pszOutput[nCapacity - 1] = '\0';
}

sirGraphicsResult_t sirGLReadNativeProbe( sirGLNativeContext_t *pContext, sirGLProbe_t *pProbe ) noexcept
{
	if ( pContext == nullptr || pProbe == nullptr || !sirGLContextOwnsThread( pContext ) || pContext->pGetString == nullptr ||
	     pContext->pGetInteger == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
#if defined( _WIN32 )
	if ( wglGetCurrentContext() != pContext->pRenderContext ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
#else
	if ( pContext->pGetCurrentContext == nullptr || pContext->pGetCurrentContext() != pContext->pRenderContext ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
#endif
	sirGLProbe_t probe = {};
	int32_t nValue = 0;
	pContext->pGetInteger( 0x821B, &nValue ); // GL_MAJOR_VERSION
	probe.nMajor = nValue > 0 ? ( uint32_t )nValue : 0;
	pContext->pGetInteger( 0x821C, &nValue ); // GL_MINOR_VERSION
	probe.nMinor = nValue > 0 ? ( uint32_t )nValue : 0;
	pContext->pGetInteger( 0x9126, &nValue ); // GL_CONTEXT_PROFILE_MASK
	probe.nCoreProfileMask = ( uint32_t )nValue;
	pContext->pGetInteger( 0x0D33, &nValue ); // GL_MAX_TEXTURE_SIZE
	probe.nMaximumTextureSize = nValue > 0 ? ( uint32_t )nValue : 0;
	pContext->pGetInteger( 0x8CDF, &nValue ); // GL_MAX_COLOR_ATTACHMENTS
	probe.nMaximumColorAttachments = nValue > 0 ? ( uint32_t )nValue : 0;
	pContext->pGetInteger( 0x8A2F, &nValue ); // GL_MAX_UNIFORM_BUFFER_BINDINGS
	probe.nMaximumUniformBindings = nValue > 0 ? ( uint32_t )nValue : 0;
	pContext->pGetInteger( 0x90DD, &nValue ); // GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS
	probe.nMaximumStorageBindings = nValue > 0 ? ( uint32_t )nValue : 0;
	pContext->pGetInteger( 0x8D57, &nValue ); // GL_MAX_SAMPLES
	probe.nMaximumSamples = nValue > 0 ? ( uint32_t )nValue : 0;
	sirGLCopyString( probe.szVendor, sizeof( probe.szVendor ), pContext->pGetString( 0x1F00 ) );
	sirGLCopyString( probe.szRenderer, sizeof( probe.szRenderer ), pContext->pGetString( 0x1F01 ) );
	*pProbe = probe;
	return SIR_GRAPHICS_SUCCESS;
}

#if defined( _WIN32 )
typedef HGLRC( WINAPI *sirWGLCreateContextAttribs_t )( HDC pDC, HGLRC pShare, const int *pAttrs );

static bool sirGLInitWindows( sirGLNativeContext_t *pContext ) noexcept
{
	pContext->pLibrary = LoadLibraryA( "opengl32.dll" );
	if ( pContext->pLibrary == nullptr ) {
		return false;
	}
	if ( !sirCreateWGLBootstrapWindow( &pContext->window ) ) {
		return false;
	}
	PIXELFORMATDESCRIPTOR pixel = {};
	pixel.nSize = sizeof( pixel );
	pixel.nVersion = 1;
	pixel.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pixel.iPixelType = PFD_TYPE_RGBA;
	pixel.cColorBits = 32;
	pixel.cDepthBits = 24;
	const int nFormat = ChoosePixelFormat( pContext->window.hDC, &pixel );
	if ( nFormat == 0 || !SetPixelFormat( pContext->window.hDC, nFormat, &pixel ) ) {
		return false;
	}
	HGLRC pLegacy = wglCreateContext( pContext->window.hDC );
	if ( pLegacy == nullptr ) {
		return false;
	}
	if ( !wglMakeCurrent( pContext->window.hDC, pLegacy ) ) {
		wglDeleteContext( pLegacy );
		return false;
	}
	sirWGLCreateContextAttribs_t pCreate = ( sirWGLCreateContextAttribs_t )wglGetProcAddress( "wglCreateContextAttribsARB" );
	if ( !sirGLIsUsableProcedure( ( void * )pCreate ) ) {
		wglMakeCurrent( nullptr, nullptr );
		wglDeleteContext( pLegacy );
		return false;
	}
	const int szAttrs[] = { 0x2091, 4, 0x2092, 6, 0x9126, 1, 0 }; // WGL 4.6 core
	HGLRC pCore = pCreate( pContext->window.hDC, nullptr, szAttrs );
	wglMakeCurrent( nullptr, nullptr );
	wglDeleteContext( pLegacy );
	if ( pCore == nullptr || !wglMakeCurrent( pContext->window.hDC, pCore ) ) {
		if ( pCore != nullptr ) {
			wglDeleteContext( pCore );
		}
		return false;
	}
	pContext->pRenderContext = pCore;
	pContext->pGetString = ( sirGLGetString_t )GetProcAddress( pContext->pLibrary, "glGetString" );
	pContext->pGetInteger = ( sirGLGetIntegerv_t )GetProcAddress( pContext->pLibrary, "glGetIntegerv" );
	return pContext->pGetString != nullptr && pContext->pGetInteger != nullptr;
}
#else
static bool sirGLInitLinux( sirGLNativeContext_t *pContext, const sirGraphicsABINativeSurface_t *pNative ) noexcept
{
	pContext->pLibraryEGL = dlopen( "libEGL.so.1", RTLD_NOW | RTLD_LOCAL );
	pContext->pLibraryGL = dlopen( "libGL.so.1", RTLD_NOW | RTLD_LOCAL );
	if ( pContext->pLibraryEGL == nullptr || pContext->pLibraryGL == nullptr ) {
		return false;
	}
	const sirEGLGetProcAddress_t pProc = ( sirEGLGetProcAddress_t )dlsym( pContext->pLibraryEGL, "eglGetProcAddress" );
	pContext->pGetProcAddress = pProc;
	const sirEGLGetDisplay_t pGetDisplay = ( sirEGLGetDisplay_t )dlsym( pContext->pLibraryEGL, "eglGetDisplay" );
	const sirEGLQueryString_t pQueryString = ( sirEGLQueryString_t )dlsym( pContext->pLibraryEGL, "eglQueryString" );
	const sirEGLInitialize_t pInitialize = ( sirEGLInitialize_t )dlsym( pContext->pLibraryEGL, "eglInitialize" );
	const sirEGLChooseConfig_t pChoose = ( sirEGLChooseConfig_t )dlsym( pContext->pLibraryEGL, "eglChooseConfig" );
	const sirEGLBindAPI_t pBind = ( sirEGLBindAPI_t )dlsym( pContext->pLibraryEGL, "eglBindAPI" );
	const sirEGLCreatePbufferSurface_t pPbuffer = ( sirEGLCreatePbufferSurface_t )dlsym( pContext->pLibraryEGL, "eglCreatePbufferSurface" );
	const sirEGLCreateContext_t pCreate = ( sirEGLCreateContext_t )dlsym( pContext->pLibraryEGL, "eglCreateContext" );
	pContext->pMakeCurrent = ( sirEGLMakeCurrent_t )dlsym( pContext->pLibraryEGL, "eglMakeCurrent" );
	pContext->pDestroyContext = ( sirEGLDestroyContext_t )dlsym( pContext->pLibraryEGL, "eglDestroyContext" );
	pContext->pDestroySurface = ( sirEGLDestroySurface_t )dlsym( pContext->pLibraryEGL, "eglDestroySurface" );
	pContext->pTerminate = ( sirEGLTerminate_t )dlsym( pContext->pLibraryEGL, "eglTerminate" );
	pContext->pGetCurrentContext = ( sirEGLGetCurrentContext_t )dlsym( pContext->pLibraryEGL, "eglGetCurrentContext" );
	pContext->pCreateWindowSurface = ( sirEGLCreateWindowSurface_t )dlsym( pContext->pLibraryEGL, "eglCreateWindowSurface" );
	pContext->pSwapBuffers = ( sirEGLSwapBuffers_t )dlsym( pContext->pLibraryEGL, "eglSwapBuffers" );
	pContext->pSwapInterval = ( sirEGLSwapInterval_t )dlsym( pContext->pLibraryEGL, "eglSwapInterval" );
	if ( pProc == nullptr || pGetDisplay == nullptr || pQueryString == nullptr || pInitialize == nullptr || pChoose == nullptr ||
	     pBind == nullptr || pPbuffer == nullptr || pCreate == nullptr || pContext->pMakeCurrent == nullptr ||
	     pContext->pDestroyContext == nullptr || pContext->pDestroySurface == nullptr || pContext->pTerminate == nullptr ||
	     pContext->pGetCurrentContext == nullptr ) {
		return false;
	}
	const char *pszExtensions = pQueryString( nullptr, SIR_EGL_EXTENSIONS );
	const sirEGLGetPlatformDisplay_t pGetPlatform = ( sirEGLGetPlatformDisplay_t )pProc( "eglGetPlatformDisplayEXT" );
	if ( pNative != nullptr ) {
		if ( pNative->pDisplayOrInstance == nullptr ||
		     ( pNative->nPlatform != SIR_GRAPHICS_SURFACE_X11 && pNative->nPlatform != SIR_GRAPHICS_SURFACE_WAYLAND ) ) {
			return false;
		}
		const uint32_t nPlatform = pNative->nPlatform == SIR_GRAPHICS_SURFACE_X11 ?
		                           SIR_EGL_PLATFORM_X11 : SIR_EGL_PLATFORM_WAYLAND;
		if ( pGetPlatform != nullptr ) {
			pContext->pDisplay = pGetPlatform( nPlatform, pNative->pDisplayOrInstance, nullptr );
		}
		if ( pContext->pDisplay == nullptr ) {
			pContext->pDisplay = pGetDisplay( pNative->pDisplayOrInstance );
		}
		pContext->pNativeDisplay = pNative->pDisplayOrInstance;
		pContext->nNativePlatform = pNative->nPlatform;
	} else {
		if ( pGetPlatform != nullptr && pszExtensions != nullptr &&
		     strstr( pszExtensions, "EGL_MESA_platform_surfaceless" ) != nullptr ) {
			pContext->pDisplay = pGetPlatform( SIR_EGL_PLATFORM_SURFACELESS, nullptr, nullptr );
		}
		if ( pContext->pDisplay == nullptr ) {
			pContext->pDisplay = pGetDisplay( nullptr );
		}
	}
	sirEGLInt_t nMajor = 0;
	sirEGLInt_t nMinor = 0;
	if ( pContext->pDisplay == nullptr || !pInitialize( pContext->pDisplay, &nMajor, &nMinor ) || !pBind( SIR_EGL_OPENGL_API ) ) {
		return false;
	}
	const sirEGLInt_t szConfig[] = { SIR_EGL_SURFACE_TYPE,
	                                 pNative == nullptr ? SIR_EGL_PBUFFER_BIT : SIR_EGL_PBUFFER_BIT | SIR_EGL_WINDOW_BIT,
	                                 SIR_EGL_RENDERABLE_TYPE,
	                                 SIR_EGL_OPENGL_BIT,
	                                 SIR_EGL_RED_SIZE,
	                                 8,
	                                 SIR_EGL_GREEN_SIZE,
	                                 8,
	                                 SIR_EGL_BLUE_SIZE,
	                                 8,
	                                 SIR_EGL_ALPHA_SIZE,
	                                 8,
	                                 SIR_EGL_NONE };
	sirEGLConfig_t pConfig = nullptr;
	sirEGLInt_t nFound = 0;
	if ( !pChoose( pContext->pDisplay, szConfig, &pConfig, 1, &nFound ) || nFound != 1 || pConfig == nullptr ) {
		return false;
	}
	pContext->pConfig = pConfig;
	if ( pNative != nullptr && ( pContext->pCreateWindowSurface == nullptr || pContext->pSwapBuffers == nullptr ) ) {
		return false;
	}
	if ( pNative != nullptr && pNative->nPlatform == SIR_GRAPHICS_SURFACE_WAYLAND ) {
		pContext->pWaylandEGLLibrary = dlopen( "libwayland-egl.so.1", RTLD_NOW | RTLD_LOCAL );
		if ( pContext->pWaylandEGLLibrary == nullptr ) {
			return false;
		}
		pContext->pWaylandCreate = ( sirWaylandEGLWindowCreate_t )dlsym( pContext->pWaylandEGLLibrary, "wl_egl_window_create" );
		pContext->pWaylandResize = ( sirWaylandEGLWindowResize_t )dlsym( pContext->pWaylandEGLLibrary, "wl_egl_window_resize" );
		pContext->pWaylandDestroy = ( sirWaylandEGLWindowDestroy_t )dlsym( pContext->pWaylandEGLLibrary, "wl_egl_window_destroy" );
		if ( pContext->pWaylandCreate == nullptr || pContext->pWaylandResize == nullptr ||
		     pContext->pWaylandDestroy == nullptr ) {
			return false;
		}
	}
	const sirEGLInt_t szPbuffer[] = { SIR_EGL_WIDTH, 1, SIR_EGL_HEIGHT, 1, SIR_EGL_NONE };
	pContext->pSurface = pPbuffer( pContext->pDisplay, pConfig, szPbuffer );
	const sirEGLInt_t szContext[] = { SIR_EGL_CONTEXT_MAJOR, 4,           SIR_EGL_CONTEXT_MINOR, 6, SIR_EGL_CONTEXT_PROFILE,
	                                  SIR_EGL_CORE_PROFILE,  SIR_EGL_NONE };
	pContext->pRenderContext = pCreate( pContext->pDisplay, pConfig, nullptr, szContext );
	if ( pContext->pSurface == nullptr || pContext->pRenderContext == nullptr ||
	     !pContext->pMakeCurrent( pContext->pDisplay, pContext->pSurface, pContext->pSurface, pContext->pRenderContext ) ) {
		return false;
	}
	pContext->pGetString = ( sirGLGetString_t )dlsym( pContext->pLibraryGL, "glGetString" );
	pContext->pGetInteger = ( sirGLGetIntegerv_t )dlsym( pContext->pLibraryGL, "glGetIntegerv" );
	if ( pContext->pGetString == nullptr ) {
		pContext->pGetString = ( sirGLGetString_t )pProc( "glGetString" );
	}
	if ( pContext->pGetInteger == nullptr ) {
		pContext->pGetInteger = ( sirGLGetIntegerv_t )pProc( "glGetIntegerv" );
	}
	return pContext->pGetString != nullptr && pContext->pGetInteger != nullptr;
}
#endif

sirGraphicsResult_t sirGLCreateNativeContextForWindow( const sirGraphicsABIHost_t *pHost, const sirGraphicsABINativeSurface_t *pNative,
                                                      sirGLNativeContext_t **ppContext ) noexcept
{
	if ( pHost == nullptr || ppContext == nullptr || pHost->pAllocate == nullptr || pHost->pFree == nullptr ||
	     pHost->nSize < sizeof( *pHost ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*ppContext = nullptr;
	void *pMemory = pHost->pAllocate( pHost->pContext, sizeof( sirGLNativeContext_t ), alignof( sirGLNativeContext_t ) );
	if ( pMemory == nullptr ) {
		return SIR_GRAPHICS_OUT_OF_MEMORY;
	}
	sirGLNativeContext_t *pContext = ( sirGLNativeContext_t * )pMemory;
	memset( pContext, 0, sizeof( *pContext ) );
	pContext->host = *pHost;
#if defined( _WIN32 )
	pContext->nOwnerThread = GetCurrentThreadId();
	const bool bReady = sirGLInitWindows( pContext );
#else
	pContext->ownerThread = pthread_self();
	const bool bReady = sirGLInitLinux( pContext, pNative );
#endif
	if ( !bReady || !sirGLValidateRequired46Functions( sirGLResolveNativeProcedure, pContext ) ) {
		sirGLDestroyNativeContext( pContext );
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	*ppContext = pContext;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirGLCreateNativeContext( const sirGraphicsABIHost_t *pHost, sirGLNativeContext_t **ppContext ) noexcept
{
	return sirGLCreateNativeContextForWindow( pHost, nullptr, ppContext );
}

void sirGLDestroyNativeContext( sirGLNativeContext_t *pContext ) noexcept
{
	if ( pContext == nullptr ) {
		return;
	}
	// Destruction must run on the owning thread while the native context is current.
	if ( !sirGLContextOwnsThread( pContext ) ) {
		return;
	}
	const sirGraphicsABIHost_t host = pContext->host;
#if defined( _WIN32 )
	if ( pContext->pRenderContext != nullptr ) {
		wglMakeCurrent( nullptr, nullptr );
		wglDeleteContext( pContext->pRenderContext );
	}
	sirDestroyWGLBootstrapWindow( &pContext->window );
	if ( pContext->pLibrary != nullptr ) {
		FreeLibrary( pContext->pLibrary );
	}
#else
	if ( pContext->pDisplay != nullptr && pContext->pMakeCurrent != nullptr ) {
		pContext->pMakeCurrent( pContext->pDisplay, nullptr, nullptr, nullptr );
	}
	if ( pContext->pRenderContext != nullptr && pContext->pDestroyContext != nullptr ) {
		pContext->pDestroyContext( pContext->pDisplay, pContext->pRenderContext );
	}
	if ( pContext->pSurface != nullptr && pContext->pDestroySurface != nullptr ) {
		pContext->pDestroySurface( pContext->pDisplay, pContext->pSurface );
	}
	if ( pContext->pDisplay != nullptr && pContext->pTerminate != nullptr ) {
		pContext->pTerminate( pContext->pDisplay );
	}
	if ( pContext->pLibraryGL != nullptr ) {
		dlclose( pContext->pLibraryGL );
	}
	if ( pContext->pWaylandEGLLibrary != nullptr ) {
		dlclose( pContext->pWaylandEGLLibrary );
	}
	if ( pContext->pLibraryEGL != nullptr ) {
		dlclose( pContext->pLibraryEGL );
	}
#endif
	host.pFree( host.pContext, pContext );
}

sirGraphicsResult_t sirGLRestoreNativeContext( sirGLNativeContext_t *pContext ) noexcept
{
	if ( pContext == nullptr || !sirGLContextOwnsThread( pContext ) ) {
		return SIR_GRAPHICS_BUSY;
	}
#if defined( _WIN32 )
	return wglMakeCurrent( pContext->window.hDC, pContext->pRenderContext ) ? SIR_GRAPHICS_SUCCESS :
	                                                                   SIR_GRAPHICS_DEVICE_LOST;
#else
	return pContext->pDisplay != nullptr && pContext->pMakeCurrent != nullptr &&
	               pContext->pMakeCurrent( pContext->pDisplay, pContext->pSurface, pContext->pSurface, pContext->pRenderContext ) ?
	           SIR_GRAPHICS_SUCCESS :
	           SIR_GRAPHICS_DEVICE_LOST;
#endif
}

sirGraphicsResult_t sirGLCreatePresentWindow( sirGLNativeContext_t *pContext, const sirGraphicsABINativeSurface_t *pNative,
                                              const sirGraphicsABIPresentDesc_t *pPresent, sirGLPresentWindow_t *pWindow ) noexcept
{
	if ( pContext == nullptr || pNative == nullptr || pPresent == nullptr || pWindow == nullptr ||
	     pNative->pWindow == nullptr || pNative->nWindowGeneration == 0 ||
	     pPresent->nWidth == 0 || pPresent->nHeight == 0 ||
	     pPresent->nWidth > INT32_MAX || pPresent->nHeight > INT32_MAX ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( !sirGLContextOwnsThread( pContext ) ) {
		return SIR_GRAPHICS_BUSY;
	}
	sirGLPresentWindow_t window = {};
	window.pNativeWindow = pNative->pWindow;
	window.pNativeDisplay = pNative->pDisplayOrInstance;
	window.nPlatform = pNative->nPlatform;
	window.nWindowGeneration = pNative->nWindowGeneration;
	window.nWidth = pPresent->nWidth;
	window.nHeight = pPresent->nHeight;
#if defined( _WIN32 )
	if ( pNative->nPlatform != SIR_GRAPHICS_SURFACE_WIN32 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	const HWND hWindow = ( HWND )pNative->pWindow;
	HDC hDC = GetDC( hWindow );
	if ( hDC == nullptr ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	const int nBootstrapFormat = GetPixelFormat( pContext->window.hDC );
	if ( nBootstrapFormat == 0 ) {
		ReleaseDC( hWindow, hDC );
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	int nExistingFormat = GetPixelFormat( hDC );
	if ( nExistingFormat == 0 ) {
		PIXELFORMATDESCRIPTOR pixel = {};
		if ( DescribePixelFormat( hDC, nBootstrapFormat, sizeof( pixel ), &pixel ) == 0 ||
		     !SetPixelFormat( hDC, nBootstrapFormat, &pixel ) ) {
			ReleaseDC( hWindow, hDC );
			return SIR_GRAPHICS_UNSUPPORTED;
		}
		nExistingFormat = nBootstrapFormat;
	}
	if ( nExistingFormat != nBootstrapFormat || !wglMakeCurrent( hDC, pContext->pRenderContext ) ) {
		ReleaseDC( hWindow, hDC );
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	window.pDrawable = ( void * )hDC;
#else
	if ( ( pNative->nPlatform != SIR_GRAPHICS_SURFACE_X11 &&
	       pNative->nPlatform != SIR_GRAPHICS_SURFACE_WAYLAND ) ||
	     pNative->pDisplayOrInstance != pContext->pNativeDisplay ||
	     pNative->nPlatform != pContext->nNativePlatform || pContext->pCreateWindowSurface == nullptr ||
	     pContext->pSwapBuffers == nullptr ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	void *pNativeDrawable = pNative->pWindow;
	if ( pNative->nPlatform == SIR_GRAPHICS_SURFACE_WAYLAND ) {
		if ( pContext->pWaylandCreate == nullptr || pContext->pWaylandDestroy == nullptr ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
		window.pWaylandWrapper = pContext->pWaylandCreate( pNative->pWindow, ( int32_t )pPresent->nWidth, ( int32_t )pPresent->nHeight );
		if ( window.pWaylandWrapper == nullptr ) {
			return SIR_GRAPHICS_DEVICE_LOST;
		}
		pNativeDrawable = window.pWaylandWrapper;
	}
	window.pDrawable = ( void * )pContext->pCreateWindowSurface( pContext->pDisplay, pContext->pConfig, pNativeDrawable, nullptr );
	if ( window.pDrawable == nullptr ||
	     !pContext->pMakeCurrent( pContext->pDisplay, ( sirEGLSurface_t )window.pDrawable,
	                             ( sirEGLSurface_t )window.pDrawable, pContext->pRenderContext ) ) {
		if ( window.pDrawable != nullptr ) {
			( void )pContext->pDestroySurface( pContext->pDisplay, ( sirEGLSurface_t )window.pDrawable );
		}
		if ( window.pWaylandWrapper != nullptr ) {
			pContext->pWaylandDestroy( window.pWaylandWrapper );
		}
		return SIR_GRAPHICS_UNSUPPORTED;
	}
#endif
	const sirGraphicsResult_t eRestore = sirGLRestoreNativeContext( pContext );
	if ( eRestore != SIR_GRAPHICS_SUCCESS ) {
#if defined( _WIN32 )
		ReleaseDC( hWindow, ( HDC )window.pDrawable );
#else
		( void )pContext->pDestroySurface( pContext->pDisplay, ( sirEGLSurface_t )window.pDrawable );
		if ( window.pWaylandWrapper != nullptr ) {
			pContext->pWaylandDestroy( window.pWaylandWrapper );
		}
#endif
		return eRestore;
	}
	*pWindow = window;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirGLResizePresentWindow( sirGLNativeContext_t *pContext, sirGLPresentWindow_t *pWindow,
                                              uint32_t nWidth, uint32_t nHeight ) noexcept
{
	if ( pContext == nullptr || pWindow == nullptr || pWindow->pDrawable == nullptr || nWidth == 0 || nHeight == 0 ||
	     nWidth > INT32_MAX || nHeight > INT32_MAX ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( !sirGLContextOwnsThread( pContext ) ) {
		return SIR_GRAPHICS_BUSY;
	}
#if !defined( _WIN32 )
	if ( pWindow->pWaylandWrapper != nullptr && pContext->pWaylandResize != nullptr ) {
		pContext->pWaylandResize( pWindow->pWaylandWrapper, ( int32_t )nWidth, ( int32_t )nHeight, 0, 0 );
	}
#endif
	pWindow->nWidth = nWidth;
	pWindow->nHeight = nHeight;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirGLBeginPresentWindow( sirGLNativeContext_t *pContext, const sirGLPresentWindow_t *pWindow ) noexcept
{
	if ( pContext == nullptr || pWindow == nullptr || pWindow->pDrawable == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( !sirGLContextOwnsThread( pContext ) ) {
		return SIR_GRAPHICS_BUSY;
	}
#if defined( _WIN32 )
	return wglMakeCurrent( ( HDC )pWindow->pDrawable, pContext->pRenderContext ) ? SIR_GRAPHICS_SUCCESS :
	                                                                             SIR_GRAPHICS_DEVICE_LOST;
#else
	return pContext->pMakeCurrent( pContext->pDisplay, ( sirEGLSurface_t )pWindow->pDrawable,
	                              ( sirEGLSurface_t )pWindow->pDrawable, pContext->pRenderContext ) ?
	           SIR_GRAPHICS_SUCCESS :
	           SIR_GRAPHICS_DEVICE_LOST;
#endif
}

sirGraphicsResult_t sirGLSwapPresentWindow( sirGLNativeContext_t *pContext, const sirGLPresentWindow_t *pWindow ) noexcept
{
	if ( pContext == nullptr || pWindow == nullptr || pWindow->pDrawable == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( !sirGLContextOwnsThread( pContext ) ) {
		return SIR_GRAPHICS_BUSY;
	}
#if defined( _WIN32 )
	const bool bSwapped = SwapBuffers( ( HDC )pWindow->pDrawable ) != FALSE;
#else
	const bool bSwapped = pContext->pSwapBuffers != nullptr &&
	                      pContext->pSwapBuffers( pContext->pDisplay, ( sirEGLSurface_t )pWindow->pDrawable ) != 0;
#endif
	const sirGraphicsResult_t eRestore = sirGLRestoreNativeContext( pContext );
	return !bSwapped ? SIR_GRAPHICS_DEVICE_LOST : eRestore;
}

sirGraphicsResult_t sirGLDestroyPresentWindow( sirGLNativeContext_t *pContext, sirGLPresentWindow_t *pWindow ) noexcept
{
	if ( pContext == nullptr || pWindow == nullptr || pWindow->pDrawable == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( !sirGLContextOwnsThread( pContext ) ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( sirGLRestoreNativeContext( pContext ) != SIR_GRAPHICS_SUCCESS ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
#if defined( _WIN32 )
	const bool bReleased = ReleaseDC( ( HWND )pWindow->pNativeWindow, ( HDC )pWindow->pDrawable ) != 0;
#else
	const bool bReleased = pContext->pDestroySurface != nullptr &&
	                       pContext->pDestroySurface( pContext->pDisplay, ( sirEGLSurface_t )pWindow->pDrawable ) != 0;
	if ( pWindow->pWaylandWrapper != nullptr ) {
		pContext->pWaylandDestroy( pWindow->pWaylandWrapper );
	}
#endif
	*pWindow = {};
	return bReleased ? SIR_GRAPHICS_SUCCESS : SIR_GRAPHICS_DEVICE_LOST;
}
