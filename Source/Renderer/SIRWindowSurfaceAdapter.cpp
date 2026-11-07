#include <Renderer/SIRWindowSurfaceAdapter.h>

sirGraphicsResult_t sirGraphicsAcquireWindowSurface( SIRWindowSystem& windows, sirWindowHandle_t hWindow,
                                                     sirGraphicsNativeSurface_t *pNative, sirWindowNativeLease_t *pLease ) noexcept
{
	if ( pNative == nullptr || pLease == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pNative = {};
	*pLease = {};
	sirWindowNativeView_t view = {};
	const sirWindowResult_t eResult = windows.acquireNativeLease( hWindow, pLease, &view );
	if ( eResult != SIR_WINDOW_OK ) {
		if ( eResult == SIR_WINDOW_INVALID_HANDLE ) {
			return SIR_GRAPHICS_INVALID_HANDLE;
		}
		if ( eResult == SIR_WINDOW_NOT_READY ) {
			return SIR_GRAPHICS_SURFACE_CHANGED;
		}
		if ( eResult == SIR_WINDOW_OUT_OF_CAPACITY ) {
			return SIR_GRAPHICS_CAPACITY_EXCEEDED;
		}
		if ( eResult == SIR_WINDOW_BUSY ) {
			return SIR_GRAPHICS_BUSY;
		}
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	sirGraphicsSurfacePlatform_t ePlatform = {};
	if ( view.ePlatform == SIR_WINDOW_PLATFORM_WIN32 ) {
		ePlatform = SIR_GRAPHICS_SURFACE_WIN32;
	} else if ( view.ePlatform == SIR_WINDOW_PLATFORM_X11 ) {
		ePlatform = SIR_GRAPHICS_SURFACE_X11;
	} else if ( view.ePlatform == SIR_WINDOW_PLATFORM_WAYLAND ) {
		ePlatform = SIR_GRAPHICS_SURFACE_WAYLAND;
	} else {
		( void )windows.releaseNativeLease( *pLease );
		*pLease = {};
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( view.nNativeWindow == 0 || view.nDisplayOrInstance == 0 || view.nWindowGeneration == 0 ) {
		( void )windows.releaseNativeLease( *pLease );
		*pLease = {};
		return SIR_GRAPHICS_SURFACE_CHANGED;
	}
	*pNative = { ePlatform, ( void * )view.nNativeWindow, ( void * )view.nDisplayOrInstance, view.nWindowGeneration };
	return SIR_GRAPHICS_SUCCESS;
}
