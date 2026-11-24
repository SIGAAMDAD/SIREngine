#include <Renderer/SIRWindowSurfaceAdapter.h>
#include <Core/Platform/SIRPlatformNativeWindow.h>

SIRGraphicsResult sirGraphicsAcquireWindowSurface( SIRWindowSystem& windows, sirWindowHandle_t hWindow,
                                                     sirGraphicsNativeSurface_t *pNative, sirWindowNativeLease_t *pLease ) noexcept
{
	if ( pNative == nullptr || pLease == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pNative = {};
	*pLease = {};
	sirWindowNativeView_t view {};
	const SIRWindowResult eResult = windows.acquireNativeLease( hWindow, pLease, &view );
	if ( eResult != SIRWindowResult::SIR_WINDOW_OK ) {
		if ( eResult == SIRWindowResult::SIR_WINDOW_INVALID_HANDLE ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
		}
		if ( eResult == SIRWindowResult::SIR_WINDOW_NOT_READY ) {
			return SIRGraphicsResult::SIR_GRAPHICS_SURFACE_CHANGED;
		}
		if ( eResult == SIRWindowResult::SIR_WINDOW_OUT_OF_CAPACITY ) {
			return SIRGraphicsResult::SIR_GRAPHICS_CAPACITY_EXCEEDED;
		}
		if ( eResult == SIRWindowResult::SIR_WINDOW_BUSY ) {
			return SIRGraphicsResult::SIR_GRAPHICS_BUSY;
		}
		return SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR;
	}
	SIRGraphicsSurfacePlatform ePlatform {};
	if ( view.ePlatform == SIRWindowPlatform::SIR_WINDOW_PLATFORM_WIN32 ) {
		sirWindowsNativeWindow_t native {};
		if ( sirPlatformDecodeWindowsWindow( view, &native ) ) {
			ePlatform = SIRGraphicsSurfacePlatform::SIR_GRAPHICS_SURFACE_WIN32;
		}
	} else if ( view.ePlatform == SIRWindowPlatform::SIR_WINDOW_PLATFORM_X11 || view.ePlatform == SIRWindowPlatform::SIR_WINDOW_PLATFORM_WAYLAND ) {
		sirLinuxNativeWindow_t native {};
		if ( sirPlatformDecodeLinuxWindow( view, &native ) ) {
			ePlatform = native.eDisplayType == SIRLinuxDisplayKind::SIR_LINUX_DISPLAY_X11 ? SIRGraphicsSurfacePlatform::SIR_GRAPHICS_SURFACE_X11 : SIRGraphicsSurfacePlatform::SIR_GRAPHICS_SURFACE_WAYLAND;
		}
	} else {
		static_cast<void>(windows.releaseNativeLease( *pLease ));
		*pLease = {};
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( ePlatform == 0 ) {
		static_cast<void>(windows.releaseNativeLease( *pLease ));
		*pLease = {};
		return SIRGraphicsResult::SIR_GRAPHICS_SURFACE_CHANGED;
	}
	*pNative = { ePlatform, reinterpret_cast<void *>(view.nNativeWindow), reinterpret_cast<void *>(view.nDisplayOrInstance), view.nWindowGeneration };
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}
