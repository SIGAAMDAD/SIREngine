#include <Renderer/SIRGraphicsWindowBridge.h>

bool sirGraphicsAcquireWindowSurface( SIRWindowSystem& windows, sirWindowHandle_t hWindow, sirGraphicsWindowBorrow_t *pBorrow ) noexcept
{
	if ( pBorrow == nullptr || pBorrow->lease.nToken != 0 ) {
		return false;
	}
	sirWindowNativeView_t view = {};
	sirWindowLease_t lease = {};
	if ( !windows.acquireNativeView( hWindow, &view, &lease ) ) {
		return false;
	}
	sirGraphicsSurfacePlatform_t ePlatform = SIR_GRAPHICS_SURFACE_WIN32;
	switch ( view.ePlatform ) {
	case SIR_WINDOW_PLATFORM_WIN32:
		ePlatform = SIR_GRAPHICS_SURFACE_WIN32;
		break;
	case SIR_WINDOW_PLATFORM_X11:
		ePlatform = SIR_GRAPHICS_SURFACE_X11;
		break;
	case SIR_WINDOW_PLATFORM_WAYLAND:
		ePlatform = SIR_GRAPHICS_SURFACE_WAYLAND;
		break;
	default:
		( void )windows.releaseNativeView( lease );
		return false;
	}
	if ( view.pWindow == nullptr ) {
		( void )windows.releaseNativeView( lease );
		return false;
	}
	pBorrow->surface = { ePlatform, view.pWindow, view.pDisplayOrInstance, view.nWindowGeneration };
	pBorrow->lease = lease;
	return true;
}

bool sirGraphicsReleaseWindowSurface( SIRWindowSystem& windows, sirGraphicsWindowBorrow_t *pBorrow ) noexcept
{
	if ( pBorrow == nullptr || pBorrow->lease.nToken == 0 ) {
		return false;
	}
	if ( !windows.releaseNativeView( pBorrow->lease ) ) {
		return false;
	}
	*pBorrow = {};
	return true;
}
