#include <Core/Platform/SIRPlatformContext.h>
#include <Core/Platform/SIRPlatformNativeWindow.h>
#include <Core/Platform/SIRThread.h>
#include <assert.h>
#include <string.h>

static void sirCaptureWorkerPlatform( void *pContext )
{
	sirPlatformContext_t *pOut = ( sirPlatformContext_t * )pContext;
	( void )sirPlatformCaptureContext( pOut );
}

int main( void )
{
	sirPlatformContext_t context = {};
	assert( !sirPlatformCaptureContext( nullptr ) );
	assert( sirPlatformCaptureContext( &context ) );
	assert( context.nAbiVersion == SIR_PLATFORM_CONTEXT_ABI_VERSION );
	assert( context.nCapabilities == 0 );
#if defined( _WIN32 )
	assert( context.ePlatform == SIR_PLATFORM_WINDOWS );
	assert( context.native.windows.nInstance != 0 );
	assert( context.native.windows.nProcessId != 0 );
	assert( context.native.windows.nOwnerThreadId != 0 );
#else
	assert( context.ePlatform == SIR_PLATFORM_LINUX );
	assert( context.native.linux.nProcessId != 0 );
	assert( context.native.linux.nOwnerThreadId != 0 );
#endif
	sirPlatformContext_t worker = {};
	SIRThread thread;
	assert( thread.start( sirCaptureWorkerPlatform, &worker ) );
	assert( thread.join() );
	assert( worker.nAbiVersion == context.nAbiVersion && worker.ePlatform == context.ePlatform );
#if defined( _WIN32 )
	assert( worker.native.windows.nProcessId == context.native.windows.nProcessId );
	assert( worker.native.windows.nOwnerThreadId != context.native.windows.nOwnerThreadId );
#else
	assert( worker.native.linux.nProcessId == context.native.linux.nProcessId );
	assert( worker.native.linux.nOwnerThreadId != context.native.linux.nOwnerThreadId );
#endif
	sirPlatformContext_t second = {};
	assert( sirPlatformCaptureContext( &second ) );
	assert( memcmp( &context, &second, sizeof( context ) ) == 0 );

	sirWindowsNativeWindow_t win = { 1, 2, 3 };
	sirLinuxNativeWindow_t linux = { 1, 2, 3, SIR_LINUX_DISPLAY_X11, 0 };
	assert( !sirPlatformDecodeWindowsWindow( {}, nullptr ) );
	assert( !sirPlatformDecodeLinuxWindow( {}, nullptr ) );
	assert( !sirPlatformDecodeWindowsWindow( {}, &win ) );
	assert( win.nHWND == 0 );
	assert( !sirPlatformDecodeLinuxWindow( {}, &linux ) );
	assert( linux.nDisplay == 0 );

	const sirWindowNativeView_t winView = { SIR_WINDOW_PLATFORM_WIN32, 11, 22, 33 };
	assert( sirPlatformDecodeWindowsWindow( winView, &win ) );
	assert( win.nHWND == 11 && win.nHInstance == 22 && win.nWindowGeneration == 33 );
	assert( !sirPlatformDecodeLinuxWindow( winView, &linux ) );
	assert( sirPlatformDecodeLinuxWindow( { SIR_WINDOW_PLATFORM_X11, 17, 18, 19 }, &linux ) );
	assert( linux.eDisplayType == SIR_LINUX_DISPLAY_X11 && linux.nSurface == 17 && linux.nDisplay == 18 );
	assert( sirPlatformDecodeLinuxWindow( { SIR_WINDOW_PLATFORM_WAYLAND, 21, 22, 23 }, &linux ) );
	assert( linux.eDisplayType == SIR_LINUX_DISPLAY_WAYLAND && linux.nWindowGeneration == 23 );
	assert( !sirPlatformDecodeLinuxWindow( { SIR_WINDOW_PLATFORM_X11, 21, 0, 23 }, &linux ) );
	assert( !sirPlatformDecodeWindowsWindow( { SIR_WINDOW_PLATFORM_WIN32, 21, 22, 0 }, &win ) );
	return 0;
}
