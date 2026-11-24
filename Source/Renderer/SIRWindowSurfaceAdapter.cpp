#include <Renderer/SIRWindowSurfaceAdapter.h>
#include <Core/Platform/Shared/Window/SIRPlatformNativeWindow.h>

SIRGraphicsResult sirGraphicsAcquireWindowSurface( SIRWindowSystem& windows, sirWindowHandle_t hWindow,
                                                     sirGraphicsNativeSurface_t *pNative, SIRNativeWindowLease *pLease ) noexcept
{
	if ( pNative == nullptr || pLease == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pNative = {};
	const sirWindowResult eResult = windows.acquireNativeLease( hWindow, *pLease );
	if ( eResult != sirWindowResult::SIR_WINDOW_OK ) {
		if ( eResult == sirWindowResult::SIR_WINDOW_INVALID_HANDLE ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
		}
		if ( eResult == sirWindowResult::SIR_WINDOW_NOT_READY ) {
			return SIRGraphicsResult::SIR_GRAPHICS_SURFACE_CHANGED;
		}
		if ( eResult == sirWindowResult::SIR_WINDOW_OUT_OF_CAPACITY ) {
			return SIRGraphicsResult::SIR_GRAPHICS_CAPACITY_EXCEEDED;
		}
		if ( eResult == sirWindowResult::SIR_WINDOW_BUSY ) {
			return SIRGraphicsResult::SIR_GRAPHICS_BUSY;
		}
		return SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR;
	}
	const sirWindowNativeView_t& view = pLease->getView();
	SIRGraphicsSurfacePlatform ePlatform {};
	if ( view.ePlatform == sirWindowPlatform::SIR_WINDOW_PLATFORM_WIN32 ) {
		sirWindowsNativeWindow_t native {};
		if ( SIRPlatformNativeWindow::decodeWindows( view, &native ) ) {
			ePlatform = SIRGraphicsSurfacePlatform::SIR_GRAPHICS_SURFACE_WIN32;
		}
	} else if ( view.ePlatform == sirWindowPlatform::SIR_WINDOW_PLATFORM_X11 || view.ePlatform == sirWindowPlatform::SIR_WINDOW_PLATFORM_WAYLAND ) {
		sirLinuxNativeWindow_t native {};
		if ( SIRPlatformNativeWindow::decodeLinux( view, &native ) ) {
			ePlatform = native.eDisplayType == sirLinuxDisplayKind::SIR_LINUX_DISPLAY_X11 ? SIRGraphicsSurfacePlatform::SIR_GRAPHICS_SURFACE_X11 : SIRGraphicsSurfacePlatform::SIR_GRAPHICS_SURFACE_WAYLAND;
		}
	} else {
		static_cast<void>(pLease->reset());
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( ePlatform == SIRGraphicsSurfacePlatform{} ) {
		static_cast<void>(pLease->reset());
		return SIRGraphicsResult::SIR_GRAPHICS_SURFACE_CHANGED;
	}
	*pNative = { ePlatform, reinterpret_cast<void *>(view.nNativeWindow), reinterpret_cast<void *>(view.nDisplayOrInstance), view.nWindowGeneration };
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}
