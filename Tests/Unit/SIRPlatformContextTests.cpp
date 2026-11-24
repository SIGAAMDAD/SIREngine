#include <Core/Platform/Shared/Diagnostics/SIRPlatformContext.h>
#include <Core/Platform/Shared/Window/SIRPlatformNativeWindow.h>
#include <Core/Platform/Shared/Threading/SIRThread.h>
#include <assert.h>
#include <string.h>

static void sirCaptureWorkerPlatform( void *pContext )
{
	sirPlatformContext_t *pOut = ( sirPlatformContext_t * )pContext;
	( void )SIRPlatformContext::capture( pOut );
}

int main( void )
{
	sirPlatformContext_t context = {};
	assert( !SIRPlatformContext::capture( nullptr ) );
	assert( SIRPlatformContext::capture( &context ) );
	assert( context.nCapabilities == 0 );
#if defined( _WIN32 )
	assert( context.ePlatform == sirPlatformKind::SIR_PLATFORM_WINDOWS );
#else
	assert( context.ePlatform == sirPlatformKind::SIR_PLATFORM_LINUX );
#endif
	assert( context.nProcessId != 0 && context.nThreadId != 0 );
	sirPlatformContext_t worker = {};
	SIRThread thread;
	assert( thread.start( sirCaptureWorkerPlatform, &worker ) );
	assert( thread.join() );
	assert( worker.ePlatform == context.ePlatform );
	assert( worker.nProcessId == context.nProcessId );
	assert( worker.nThreadId != context.nThreadId );
	sirPlatformContext_t second = {};
	assert( SIRPlatformContext::capture( &second ) );
	assert( memcmp( &context, &second, sizeof( context ) ) == 0 );

	sirWindowsNativeWindow_t win = { 1, 2, 3 };
	sirLinuxNativeWindow_t linux = { 1, 2, 3, sirLinuxDisplayKind::SIR_LINUX_DISPLAY_X11, 0 };
	assert( !SIRPlatformNativeWindow::decodeWindows( {}, nullptr ) );
	assert( !SIRPlatformNativeWindow::decodeLinux( {}, nullptr ) );
	assert( !SIRPlatformNativeWindow::decodeWindows( {}, &win ) );
	assert( win.nHWND == 0 );
	assert( !SIRPlatformNativeWindow::decodeLinux( {}, &linux ) );
	assert( linux.nDisplay == 0 );

	const sirWindowNativeView_t winView = { sirWindowPlatform::SIR_WINDOW_PLATFORM_WIN32, 11, 22, 33 };
	assert( SIRPlatformNativeWindow::decodeWindows( winView, &win ) );
	assert( win.nHWND == 11 && win.nHInstance == 22 && win.nWindowGeneration == 33 );
	assert( !SIRPlatformNativeWindow::decodeLinux( winView, &linux ) );
	assert( SIRPlatformNativeWindow::decodeLinux( { sirWindowPlatform::SIR_WINDOW_PLATFORM_X11, 17, 18, 19 }, &linux ) );
	assert( linux.eDisplayType == sirLinuxDisplayKind::SIR_LINUX_DISPLAY_X11 && linux.nSurface == 17 && linux.nDisplay == 18 );
	assert( SIRPlatformNativeWindow::decodeLinux( { sirWindowPlatform::SIR_WINDOW_PLATFORM_WAYLAND, 21, 22, 23 }, &linux ) );
	assert( linux.eDisplayType == sirLinuxDisplayKind::SIR_LINUX_DISPLAY_WAYLAND && linux.nWindowGeneration == 23 );
	assert( !SIRPlatformNativeWindow::decodeLinux( { sirWindowPlatform::SIR_WINDOW_PLATFORM_X11, 21, 0, 23 }, &linux ) );
	assert( !SIRPlatformNativeWindow::decodeWindows( { sirWindowPlatform::SIR_WINDOW_PLATFORM_WIN32, 21, 22, 0 }, &win ) );
	return 0;
}
