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
#include <string.h>

#if defined( _WIN32 )
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

static constexpr uint32_t SIR_EGL_OPENGL_API = 0x30A2;
static constexpr uint32_t SIR_EGL_PLATFORM_SURFACELESS = 0x31DD;
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
	HWND pWindow;
	HDC pDC;
	HGLRC pRenderContext;
#else
	pthread_t ownerThread;
	void *pLibraryEGL;
	void *pLibraryGL;
	sirEGLDisplay_t pDisplay;
	sirEGLSurface_t pSurface;
	sirEGLContext_t pRenderContext;
	sirEGLMakeCurrent_t pMakeCurrent;
	sirEGLDestroyContext_t pDestroyContext;
	sirEGLDestroySurface_t pDestroySurface;
	sirEGLTerminate_t pTerminate;
	sirEGLGetCurrentContext_t pGetCurrentContext;
#endif
} sirGLNativeContext_t;

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
	const HINSTANCE hInstance = GetModuleHandleA( nullptr );
	WNDCLASSA windowClass = {};
	windowClass.lpfnWndProc = DefWindowProcA;
	windowClass.hInstance = hInstance;
	windowClass.lpszClassName = "SIREngineGL46Context";
	if ( RegisterClassA( &windowClass ) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS ) {
		return false;
	}
	pContext->pWindow = CreateWindowExA( 0, windowClass.lpszClassName, "", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, hInstance, nullptr );
	if ( pContext->pWindow == nullptr ) {
		return false;
	}
	pContext->pDC = GetDC( pContext->pWindow );
	if ( pContext->pDC == nullptr ) {
		return false;
	}
	PIXELFORMATDESCRIPTOR pixel = {};
	pixel.nSize = sizeof( pixel );
	pixel.nVersion = 1;
	pixel.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pixel.iPixelType = PFD_TYPE_RGBA;
	pixel.cColorBits = 32;
	pixel.cDepthBits = 24;
	const int nFormat = ChoosePixelFormat( pContext->pDC, &pixel );
	if ( nFormat == 0 || !SetPixelFormat( pContext->pDC, nFormat, &pixel ) ) {
		return false;
	}
	HGLRC pLegacy = wglCreateContext( pContext->pDC );
	if ( pLegacy == nullptr ) {
		return false;
	}
	if ( !wglMakeCurrent( pContext->pDC, pLegacy ) ) {
		wglDeleteContext( pLegacy );
		return false;
	}
	sirWGLCreateContextAttribs_t pCreate = ( sirWGLCreateContextAttribs_t )wglGetProcAddress( "wglCreateContextAttribsARB" );
	if ( pCreate == nullptr ) {
		wglMakeCurrent( nullptr, nullptr );
		wglDeleteContext( pLegacy );
		return false;
	}
	const int szAttrs[] = { 0x2091, 4, 0x2092, 6, 0x9126, 1, 0 }; // WGL 4.6 core
	HGLRC pCore = pCreate( pContext->pDC, nullptr, szAttrs );
	wglMakeCurrent( nullptr, nullptr );
	wglDeleteContext( pLegacy );
	if ( pCore == nullptr || !wglMakeCurrent( pContext->pDC, pCore ) ) {
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
static bool sirGLInitLinux( sirGLNativeContext_t *pContext ) noexcept
{
	pContext->pLibraryEGL = dlopen( "libEGL.so.1", RTLD_NOW | RTLD_LOCAL );
	pContext->pLibraryGL = dlopen( "libGL.so.1", RTLD_NOW | RTLD_LOCAL );
	if ( pContext->pLibraryEGL == nullptr || pContext->pLibraryGL == nullptr ) {
		return false;
	}
	const sirEGLGetProcAddress_t pProc = ( sirEGLGetProcAddress_t )dlsym( pContext->pLibraryEGL, "eglGetProcAddress" );
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
	if ( pProc == nullptr || pGetDisplay == nullptr || pQueryString == nullptr || pInitialize == nullptr || pChoose == nullptr ||
	     pBind == nullptr || pPbuffer == nullptr || pCreate == nullptr || pContext->pMakeCurrent == nullptr ||
	     pContext->pDestroyContext == nullptr || pContext->pDestroySurface == nullptr || pContext->pTerminate == nullptr ||
	     pContext->pGetCurrentContext == nullptr ) {
		return false;
	}
	const char *pszExtensions = pQueryString( nullptr, SIR_EGL_EXTENSIONS );
	const sirEGLGetPlatformDisplay_t pGetPlatform = ( sirEGLGetPlatformDisplay_t )pProc( "eglGetPlatformDisplayEXT" );
	if ( pGetPlatform != nullptr && pszExtensions != nullptr && strstr( pszExtensions, "EGL_MESA_platform_surfaceless" ) != nullptr ) {
		pContext->pDisplay = pGetPlatform( SIR_EGL_PLATFORM_SURFACELESS, nullptr, nullptr );
	}
	if ( pContext->pDisplay == nullptr ) {
		pContext->pDisplay = pGetDisplay( nullptr );
	}
	sirEGLInt_t nMajor = 0;
	sirEGLInt_t nMinor = 0;
	if ( pContext->pDisplay == nullptr || !pInitialize( pContext->pDisplay, &nMajor, &nMinor ) || !pBind( SIR_EGL_OPENGL_API ) ) {
		return false;
	}
	const sirEGLInt_t szConfig[] = { SIR_EGL_SURFACE_TYPE,
	                                 SIR_EGL_PBUFFER_BIT,
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

sirGraphicsResult_t sirGLCreateNativeContext( const sirGraphicsABIHost_t *pHost, sirGLNativeContext_t **ppContext ) noexcept
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
	const bool bReady = sirGLInitLinux( pContext );
#endif
	if ( !bReady ) {
		sirGLDestroyNativeContext( pContext );
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	*ppContext = pContext;
	return SIR_GRAPHICS_SUCCESS;
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
	if ( pContext->pDC != nullptr && pContext->pWindow != nullptr ) {
		ReleaseDC( pContext->pWindow, pContext->pDC );
	}
	if ( pContext->pWindow != nullptr ) {
		DestroyWindow( pContext->pWindow );
	}
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
	if ( pContext->pLibraryEGL != nullptr ) {
		dlclose( pContext->pLibraryEGL );
	}
#endif
	host.pFree( host.pContext, pContext );
}
