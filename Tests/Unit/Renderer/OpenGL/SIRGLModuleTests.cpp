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

#include <Renderer/SIRGraphicsDriverABI.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>

#if defined( _WIN32 )
#include <windows.h>
#else
#include <dlfcn.h>
#ifdef SIR_GL_TEST_X11_AVAILABLE
#include <X11/Xlib.h>
#endif
#endif

#ifndef SIR_TEST_GL46_MODULE_PATH
#error SIR_TEST_GL46_MODULE_PATH must be the actual built driver module
#endif

static int s_nOutstanding = 0;
static int s_nFailures = 0;

static void *sirGLTestAllocate( void *, uint64_t nBytes, uint64_t )
{
	void *pMemory = malloc( ( size_t )nBytes );
	if ( pMemory != nullptr ) {
		++s_nOutstanding;
	}
	return pMemory;
}

static void sirGLTestFree( void *, void *pMemory )
{
	if ( pMemory != nullptr ) {
		--s_nOutstanding;
		free( pMemory );
	}
}

static void sirGLTestLog( void *, uint32_t, const char *pszMessage )
{
	fprintf( stderr, "GL46: %s\n", pszMessage );
}

static void sirGLTestCheck( bool bGood, const char *pszCase )
{
	if ( !bGood ) {
		fprintf( stderr, "FAIL: %s\n", pszCase );
		++s_nFailures;
	}
}


static void sirGLTestWindowPresentation( sirGraphicsABIDriver_t *pDriver,
                                             const sirGraphicsABINativeSurface_t& native )
{
	if ( pDriver == nullptr ) {
		return;
	}
	sirGraphicsABIDeviceRequest_t request = {};
	request.nSize = sizeof( request );
	request.nHeadless = 0;
	request.nAllowSoftware = 1;
	request.nFramesInFlight = 1;
	request.nResidentBudgetBytes = 64 * 1024 * 1024;
	request.nRequiredFeatures = SIR_GRAPHICS_FEATURE_PRESENT;
	sirGraphicsABIDevice_t device = {};
	device.nSize = sizeof( device );
	const uint32_t nCreate = pDriver->pCreateDevice( pDriver->pDriver, &request, &device );
	sirGLTestCheck( nCreate == SIR_GRAPHICS_SUCCESS, "windowed GL46 device admission" );
	if ( nCreate == SIR_GRAPHICS_SUCCESS ) {
		
		const sirGraphicsABIPresentDesc_t present = { sizeof( present ), 320, 200, 1,
		                                             SIR_GRAPHICS_FORMAT_RGBA8_UNORM, SIR_GRAPHICS_PRESENT_FIFO, 0, 0 };
		sirGraphicsHandle_t hSurface = {};
		const uint32_t nSurface = device.pCreateSurface( device.pDevice, &native, &present, &hSurface );
		sirGLTestCheck( nSurface == SIR_GRAPHICS_SUCCESS, "real EGL-X11 drawable created" );
		if ( nSurface == SIR_GRAPHICS_SUCCESS ) {
			sirGraphicsSurfaceImage_t acquired = {};
			sirGLTestCheck( device.pAcquireSurfaceImage( device.pDevice, hSurface, &acquired ) == SIR_GRAPHICS_SUCCESS &&
			                    acquired.hImage.eType == SIR_GRAPHICS_HANDLE_TEXTURE && acquired.nAcquisitionId != 0,
			                "native GL texture acquired" );
			if ( acquired.nAcquisitionId != 0 ) {
				sirGLTestCheck( device.pRetireResource( device.pDevice, acquired.hImage, nullptr ) == SIR_GRAPHICS_BUSY,
				                "surface retains ownership of acquired GPU image" );
				const sirGraphicsCompletion_t unsubmitted = { acquired.hImage.nDeviceGeneration,
				                                               SIR_GRAPHICS_QUEUE_DIRECT, 1 };
				sirGLTestCheck( device.pPresent( device.pDevice, &acquired, &unsubmitted ) == SIR_GRAPHICS_INVALID_ARGUMENT,
				                "unsubmitted acquired image cannot present" );
				sirGraphicsABICommandList_t commands = {};
				commands.nSize = sizeof( commands );
				sirGLTestCheck( device.pAcquireCommands( device.pDevice, SIR_GRAPHICS_QUEUE_DIRECT, &commands ) ==
				                    SIR_GRAPHICS_SUCCESS,
				                "native GL DIRECT command list" );
				if ( commands.pCommands != nullptr ) {
					const sirGraphicsABIColorAttachment_t color = { acquired.hImage, { 0.15f, 0.2f, 0.35f, 1.0f }, 1, 1 };
					const sirGraphicsABIRenderPass_t pass = { sizeof( pass ), 1, &color, {}, 1.0f, 0, 0, 0 };
					sirGLTestCheck( commands.pReset( commands.pCommands ) == SIR_GRAPHICS_SUCCESS &&
					                    commands.pBeginRenderPass( commands.pCommands, &pass ) == SIR_GRAPHICS_SUCCESS &&
					                    commands.pEndRenderPass( commands.pCommands ) == SIR_GRAPHICS_SUCCESS &&
					                    commands.pClose( commands.pCommands ) == SIR_GRAPHICS_SUCCESS,
					                "GL render target clear pass encoded and closed" );
					const void *const pBatchCommands[] = { commands.pCommands };
					sirGraphicsABISubmission_t submit = {};
					submit.nSize = sizeof( submit );
					submit.nCommandListCount = 1;
					submit.ppCommandLists = pBatchCommands;
					submit.nAcquisitionCount = 1;
					submit.pAcquisitions = &acquired;
					sirGraphicsCompletion_t done = {};
					sirGLTestCheck( device.pSubmit( device.pDevice, SIR_GRAPHICS_QUEUE_DIRECT, &submit, &done ) ==
					                    SIR_GRAPHICS_SUCCESS &&
					                    done.nDeviceGeneration != 0 && done.nValue != 0,
					                "native glFinish direct submission" );
					if ( done.nValue != 0 ) {
						sirGLTestCheck( device.pPresent( device.pDevice, &acquired, &done ) == SIR_GRAPHICS_SUCCESS,
						                "real EGL swap presentation" );
						sirGLTestCheck( device.pPresent( device.pDevice, &acquired, &done ) == SIR_GRAPHICS_INVALID_HANDLE,
						                "double present rejected" );
					}
				}
			}
			sirGraphicsABIPresentDesc_t resized = present;
			resized.nWidth = 256;
			resized.nHeight = 160;
			sirGLTestCheck( device.pResizeSurface( device.pDevice, hSurface, &native, &resized ) == SIR_GRAPHICS_SUCCESS,
			                "native GL drawable resize" );
			sirGLTestCheck( device.pRetireResource( device.pDevice, hSurface, nullptr ) == SIR_GRAPHICS_SUCCESS,
			                "native EGL surface and image retired" );
			sirGLTestCheck( device.pRetireResource( device.pDevice, hSurface, nullptr ) == SIR_GRAPHICS_INVALID_HANDLE,
			                "stale EGL surface rejected" );
		}
		sirGLTestCheck( pDriver->pDestroyDevice( pDriver->pDriver, &device ) == SIR_GRAPHICS_SUCCESS,
		                "GL device destroyed after native present" );
	}
}

#ifdef SIR_GL_TEST_X11_AVAILABLE
static void sirGLTestX11Presentation( sirGraphicsABIDriver_t *pDriver )
{
	if ( pDriver == nullptr ) {
		return;
	}
	Display *pDisplay = XOpenDisplay( nullptr );
	if ( pDisplay == nullptr ) {
		return;
	}
	const Window hWindow = XCreateSimpleWindow( pDisplay, DefaultRootWindow( pDisplay ), 0, 0, 320, 200, 0, 0, 0 );
	if ( hWindow == 0 ) {
		XCloseDisplay( pDisplay );
		return;
	}
	XMapWindow( pDisplay, hWindow );
	XSync( pDisplay, False );
	const sirGraphicsABINativeSurface_t native = { sizeof( native ), SIR_GRAPHICS_SURFACE_X11,
	                                               ( void * )( uintptr_t )hWindow, pDisplay, 1 };
	sirGLTestWindowPresentation( pDriver, native );
	XDestroyWindow( pDisplay, hWindow );
	XCloseDisplay( pDisplay );
}
#endif

#if defined( _WIN32 )
static void sirGLTestWin32Presentation( sirGraphicsABIDriver_t *pDriver )
{
	const HINSTANCE hInstance = GetModuleHandleW( nullptr );
	const wchar_t *pszClassName = L"SIREngineGL46PresentTest";
	WNDCLASSW cls = {};
	cls.style = CS_OWNDC;
	cls.lpfnWndProc = DefWindowProcW;
	cls.hInstance = hInstance;
	cls.lpszClassName = pszClassName;
	if ( RegisterClassW( &cls ) == 0 ) {
		sirGLTestCheck( false, "register Win32 native GL presentation window" );
		return;
	}
	const HWND hWindow = CreateWindowExW( 0, pszClassName, L"SIR GL presentation", WS_OVERLAPPEDWINDOW,
	                                     CW_USEDEFAULT, CW_USEDEFAULT, 320, 200, nullptr, nullptr, hInstance, nullptr );
	if ( hWindow == nullptr ) {
		sirGLTestCheck( false, "create Win32 native GL presentation window" );
		( void )UnregisterClassW( pszClassName, hInstance );
		return;
	}
	ShowWindow( hWindow, SW_SHOW );
	const sirGraphicsABINativeSurface_t native = { sizeof( native ), SIR_GRAPHICS_SURFACE_WIN32,
	                                               ( void * )hWindow, ( void * )hInstance, 1 };
	sirGLTestWindowPresentation( pDriver, native );
	( void )DestroyWindow( hWindow );
	( void )UnregisterClassW( pszClassName, hInstance );
}
#endif


int main( void )
{
#if defined( _WIN32 )
	HMODULE pModule = LoadLibraryA( SIR_TEST_GL46_MODULE_PATH );
	sirGraphicsABIFactory_t pFactory =
	    pModule != nullptr ? ( sirGraphicsABIFactory_t )GetProcAddress( pModule, SIR_GRAPHICS_DRIVER_ABI_ENTRY ) : nullptr;
#else
	void *pModule = dlopen( SIR_TEST_GL46_MODULE_PATH, RTLD_NOW | RTLD_LOCAL );
	sirGraphicsABIFactory_t pFactory =
	    pModule != nullptr ? ( sirGraphicsABIFactory_t )dlsym( pModule, SIR_GRAPHICS_DRIVER_ABI_ENTRY ) : nullptr;
#endif
	sirGLTestCheck( pFactory != nullptr, "load native exported driver" );
	if ( pFactory == nullptr ) {
		return 1;
	}
	sirGraphicsABIHost_t host = {};
	host.nSize = sizeof( host );
	host.nMajor = SIR_GRAPHICS_DRIVER_ABI_MAJOR;
	host.nMinor = SIR_GRAPHICS_DRIVER_ABI_MINOR;
	host.pAllocate = sirGLTestAllocate;
	host.pFree = sirGLTestFree;
	host.pLog = sirGLTestLog;

	sirGraphicsABIDriver_t driver = {};
	driver.nSize = sizeof( driver );
	sirGLTestCheck( pFactory( 99, 0, &host, &driver ) == SIR_GRAPHICS_VERSION_MISMATCH, "wrong ABI version rejected" );
	sirGLTestCheck( s_nOutstanding == 0, "version mismatch is failure atomic" );
	sirGLTestCheck( pFactory( SIR_GRAPHICS_DRIVER_ABI_MAJOR, 0, &host, &driver ) == SIR_GRAPHICS_SUCCESS, "initialize native module" );
	sirGLTestCheck( driver.nBackend == SIR_GRAPHICS_BACKEND_OPENGL && driver.pDriver != nullptr, "correct exported backend" );
	sirGLTestCheck( driver.pAdapterCount != nullptr && driver.pCreateDevice != nullptr && driver.pDestroyDevice != nullptr,
	                "required factory callbacks" );
	uint32_t nAdapters = 999;
	sirGLTestCheck( driver.pAdapterCount( driver.pDriver, &nAdapters ) == SIR_GRAPHICS_SUCCESS && nAdapters <= 1,
	                "actual GL context admission query" );
	if ( nAdapters == 1 ) {
		sirGraphicsABIAdapter_t adapter = {};
		adapter.nSize = sizeof( adapter );
		sirGLTestCheck( driver.pQueryAdapter( driver.pDriver, &adapter ) == SIR_GRAPHICS_SUCCESS, "real adapter info" );
		sirGLTestCheck( adapter.nApiMajor >= 4 && adapter.nQueueMask == 1 && adapter.nFeatureMask == 0,
		                "core version / truthful serial capabilities" );
		sirGraphicsABIDeviceRequest_t request = {};
		request.nSize = sizeof( request );
		request.nHeadless = 1;
		request.nAllowSoftware = 1;
		request.nFramesInFlight = 1;
		request.nResidentBudgetBytes = 64 * 1024 * 1024;
		sirGraphicsABIDevice_t device = {};
		device.nSize = sizeof( device );
		request.nRequiredFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS;
		sirGLTestCheck( driver.pCreateDevice( driver.pDriver, &request, &device ) == SIR_GRAPHICS_UNSUPPORTED,
		                "cannot enable unimplemented graphics commands" );
		request.nRequiredFeatures = 0;
		sirGLTestCheck( driver.pCreateDevice( driver.pDriver, &request, &device ) == SIR_GRAPHICS_SUCCESS,
		                "real headless GL46 context created" );
		if ( device.pDevice != nullptr ) {
			sirGraphicsABIPipelineModelCaps_t model = {};
			model.nSize = sizeof( model );
			sirGLTestCheck( device.pQueryPipelineModelCaps != nullptr, "pipeline-model query callback provided" );
			sirGLTestCheck( device.pQueryPipelineModelCaps( device.pDevice, SIR_GRAPHICS_PIPELINE_MODEL_MODERN, &model ) ==
			                    SIR_GRAPHICS_UNSUPPORTED,
			                "unimplemented pipeline model denied" );
			sirGraphicsABICapabilities_t caps = {};
			caps.nSize = sizeof( caps );
			sirGLTestCheck( device.pQueryCapabilities( device.pDevice, &caps ) == SIR_GRAPHICS_SUCCESS, "real GL46 capabilities" );
			sirGLTestCheck( caps.nShaderProfile == 460 && caps.nEnabledQueues == 1 && caps.nEnabledFeatures == 0,
			                "usable context yet no false draw feature" );
			sirGraphicsABIFormat_t format = {};
			format.nSize = sizeof( format );
			format.nFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
			sirGLTestCheck( device.pQueryFormat( device.pDevice, &format ) == SIR_GRAPHICS_SUCCESS &&
			                    ( format.nOptimalFeatures & SIR_GRAPHICS_ABI_FORMAT_SAMPLED ) != 0 &&
			                    ( format.nOptimalFeatures & SIR_GRAPHICS_ABI_FORMAT_COLOR_ATTACHMENT ) != 0 && format.nSampleCountMask == 1,
			                "native RGBA8 format admitted without unimplemented capabilities" );
			format = {};
			format.nSize = sizeof( format );
			format.nFormat = SIR_GRAPHICS_FORMAT_BGRA8_UNORM;
			sirGLTestCheck( device.pQueryFormat( device.pDevice, &format ) == SIR_GRAPHICS_UNSUPPORTED,
			                "unsupported native texture format rejected" );
			sirGraphicsMemoryBudget_t budget = {};
			sirGLTestCheck( device.pQueryMemoryBudget( device.pDevice, &budget ) == SIR_GRAPHICS_SUCCESS &&
			                    budget.nBudgetBytes == request.nResidentBudgetBytes && budget.nCommittedBytes == 0,
			                "budget request and physical allocation truth" );
			sirGraphicsSamplerDesc_t sampler = {};
			sampler.eFilter = SIR_GRAPHICS_FILTER_TRILINEAR;
			sampler.eAddressU = SIR_GRAPHICS_ADDRESS_REPEAT;
			sampler.eAddressV = SIR_GRAPHICS_ADDRESS_CLAMP;
			sampler.eAddressW = SIR_GRAPHICS_ADDRESS_MIRROR;
			sampler.fMaximumAnisotropy = 1.0f;
			sampler.fMinimumLod = 0.0f;
			sampler.fMaximumLod = 12.0f;
			sirGraphicsHandle_t hSampler = {};
			sirGLTestCheck( device.pCreateSampler( device.pDevice, &sampler, &hSampler ) == SIR_GRAPHICS_SUCCESS &&
			                    hSampler.eType == SIR_GRAPHICS_HANDLE_SAMPLER,
			                "native sampler ABI creation" );
			if ( hSampler.nGeneration != 0 ) {
				sirGLTestCheck( device.pRetireResource( device.pDevice, hSampler, nullptr ) == SIR_GRAPHICS_SUCCESS,
				                "native sampler ABI retirement" );
				sirGLTestCheck( device.pRetireResource( device.pDevice, hSampler, nullptr ) == SIR_GRAPHICS_INVALID_HANDLE,
				                "stale sampler rejected" );
			}
			sirGraphicsTextureDesc_t nativeTexture = {};
			nativeTexture.nWidth = 16;
			nativeTexture.nHeight = 8;
			nativeTexture.nDepth = 1;
			nativeTexture.nArrayLayers = 1;
			nativeTexture.nMipLevels = 3;
			nativeTexture.nSampleCount = 1;
			nativeTexture.nUsage = SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_COPY_DESTINATION;
			nativeTexture.eDimension = SIR_GRAPHICS_TEXTURE_2D;
			nativeTexture.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
			nativeTexture.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
			sirGraphicsHandle_t hTexture = {};
			sirGLTestCheck( device.pCreateTexture( device.pDevice, &nativeTexture, &hTexture ) == SIR_GRAPHICS_SUCCESS &&
			                    hTexture.eType == SIR_GRAPHICS_HANDLE_TEXTURE,
			                "native 2D texture ABI allocation" );
			if ( hTexture.nGeneration != 0 ) {
				sirGraphicsViewDesc_t view = {};
				view.hResource = hTexture;
				view.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
				view.nBaseMip = 1;
				view.nMipLevels = 2;
				view.nLayers = 1;
				view.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
				sirGraphicsHandle_t hView = {};
				sirGLTestCheck( device.pCreateView( device.pDevice, &view, &hView ) == SIR_GRAPHICS_SUCCESS &&
				                    hView.eType == SIR_GRAPHICS_HANDLE_VIEW,
				                "native 2D mip view allocated" );
				view.nBaseMip = 3;
				sirGraphicsHandle_t hBadView = {};
				sirGLTestCheck( device.pCreateView( device.pDevice, &view, &hBadView ) == SIR_GRAPHICS_INVALID_ARGUMENT,
				                "out-of-range mip view refused" );
				view.nBaseMip = 1;
				if ( hView.nGeneration != 0 ) {
					sirGLTestCheck( device.pRetireResource( device.pDevice, hTexture, nullptr ) == SIR_GRAPHICS_BUSY,
					                "live child view prevents texture retirement" );
					sirGLTestCheck( device.pRetireResource( device.pDevice, hView, nullptr ) == SIR_GRAPHICS_SUCCESS,
					                "native mip view retirement" );
					sirGLTestCheck( device.pRetireResource( device.pDevice, hView, nullptr ) == SIR_GRAPHICS_INVALID_HANDLE,
					                "stale view rejected" );
				}
				sirGraphicsMemoryBudget_t textureBudget = {};
				sirGLTestCheck( device.pQueryMemoryBudget( device.pDevice, &textureBudget ) == SIR_GRAPHICS_SUCCESS &&
				                    textureBudget.nCommittedBytes == 672,
				                "native texture requested bytes accounted" );
				sirGLTestCheck( device.pRetireResource( device.pDevice, hTexture, nullptr ) == SIR_GRAPHICS_SUCCESS,
				                "native texture ABI retirement" );
				sirGLTestCheck( device.pRetireResource( device.pDevice, hTexture, nullptr ) == SIR_GRAPHICS_INVALID_HANDLE,
				                "retired texture handle rejected" );
			}
			nativeTexture.eFormat = SIR_GRAPHICS_FORMAT_R8_UNORM;
			sirGraphicsHandle_t hR8Texture = {};
			sirGLTestCheck( device.pCreateTexture( device.pDevice, &nativeTexture, &hR8Texture ) == SIR_GRAPHICS_SUCCESS,
			                "R8 storage gated by live GL format support" );
			if ( hR8Texture.nGeneration != 0 ) {
				sirGLTestCheck( device.pRetireResource( device.pDevice, hR8Texture, nullptr ) == SIR_GRAPHICS_SUCCESS,
				                "R8 live format resource retired" );
			}
			nativeTexture.eFormat = SIR_GRAPHICS_FORMAT_BGRA8_UNORM;
			sirGraphicsHandle_t hUnsupported = {};
			sirGLTestCheck( device.pCreateTexture( device.pDevice, &nativeTexture, &hUnsupported ) == SIR_GRAPHICS_UNSUPPORTED,
			                "unsupported storage rejected before GL allocation" );
			uint32_t nForeignResult = 0;
			std::thread foreign( [&]() { nForeignResult = device.pShutdown( device.pDevice ); } );
			foreign.join();
			sirGLTestCheck( nForeignResult == SIR_GRAPHICS_BUSY, "foreign thread cannot destroy context" );
			sirGLTestCheck( driver.pDestroyDevice( driver.pDriver, &device ) == SIR_GRAPHICS_SUCCESS, "owner-thread device retirement" );
		}
	} else {
		printf( "OpenGL 4.6 native GPU context unavailable here; ABI negatives verified\n" );
	}
#ifdef SIR_GL_TEST_X11_AVAILABLE
	if ( nAdapters == 1 ) {
		sirGLTestX11Presentation( &driver );
	}
#endif
#if defined( _WIN32 )
	if ( nAdapters == 1 ) {
		sirGLTestWin32Presentation( &driver );
	}
#endif
	driver.pReleaseDriver( driver.pDriver );
	sirGLTestCheck( s_nOutstanding == 0, "all native allocations released" );
#if defined( _WIN32 )
	FreeLibrary( pModule );
#else
	dlclose( pModule );
#endif
	if ( s_nFailures == 0 ) {
		printf( "OpenGL 4.6 binary module tests passed\n" );
	}
	return s_nFailures == 0 ? 0 : 1;
}
