#include <Renderer/SIRWindowABISurface.h>

bool SIRWindowABISurface::isActive( void ) const noexcept
{
	return m_pWindows != nullptr;
}

sirGraphicsHandle_t SIRWindowABISurface::getSurface( void ) const noexcept
{
	return m_hSurface;
}

uint64_t SIRWindowABISurface::getMetricsRevision( void ) const noexcept
{
	return m_nMetricsRevision;
}

sirGraphicsABINativeSurface_t SIRWindowABISurface::nativeSurface( void ) const noexcept
{
	const sirGraphicsABINativeSurface_t native = { sizeof( sirGraphicsABINativeSurface_t ), ( uint32_t )m_Native.ePlatform,
	                                               m_Native.pWindow, m_Native.pDisplayOrInstance, m_Native.nWindowGeneration };
	return native;
}

sirGraphicsABIResult_t SIRWindowABISurface::create( SIRWindowSystem& windows, sirWindowHandle_t hWindow,
                                                    const sirGraphicsABIDevice_t *pDevice,
                                                    const sirGraphicsABIPresentDesc_t *pPresent ) noexcept
{
	if ( isActive() || pDevice == nullptr || pPresent == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirWindowSnapshot_t snapshot = {};
	const sirWindowResult_t eSnapshot = windows.querySnapshot( hWindow, &snapshot );
	if ( eSnapshot == SIR_WINDOW_INVALID_HANDLE ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( eSnapshot != SIR_WINDOW_OK || !snapshot.bReady || snapshot.bCloseRequested || snapshot.nWidth == 0 || snapshot.nHeight == 0 ) {
		return SIR_GRAPHICS_SURFACE_CHANGED;
	}
	sirGraphicsNativeSurface_t native = {};
	sirWindowNativeLease_t lease = {};
	const sirGraphicsResult_t eAcquire = sirGraphicsAcquireWindowSurface( windows, hWindow, &native, &lease );
	if ( eAcquire != SIR_GRAPHICS_SUCCESS ) {
		return eAcquire;
	}
	sirWindowSnapshot_t after = {};
	if ( windows.querySnapshot( hWindow, &after ) != SIR_WINDOW_OK || !after.bReady || after.bCloseRequested ||
	     after.nMetricsRevision != snapshot.nMetricsRevision || after.nWindowGeneration != native.nWindowGeneration ) {
		( void )windows.releaseNativeLease( lease );
		return SIR_GRAPHICS_SURFACE_CHANGED;
	}
	const sirGraphicsABINativeSurface_t abiNative = { sizeof( abiNative ), ( uint32_t )native.ePlatform, native.pWindow,
	                                                  native.pDisplayOrInstance, native.nWindowGeneration };
	sirGraphicsABIPresentDesc_t present = *pPresent;
	present.nWidth = after.nWidth;
	present.nHeight = after.nHeight;
	sirGraphicsHandle_t hSurface = {};
	const sirGraphicsABIResult_t eCreate = sirGraphicsCreateABISurface( pDevice, &abiNative, &present, &hSurface );
	if ( eCreate != SIR_GRAPHICS_SUCCESS ) {
		( void )windows.releaseNativeLease( lease );
		return eCreate;
	}
	m_pWindows = &windows;
	m_pDevice = pDevice;
	m_hWindow = hWindow;
	m_hLease = lease;
	m_Native = native;
	m_hSurface = hSurface;
	m_nMetricsRevision = after.nMetricsRevision;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsABIResult_t SIRWindowABISurface::resize( const sirGraphicsABIPresentDesc_t *pPresent ) noexcept
{
	if ( !isActive() ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( pPresent == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirWindowSnapshot_t snapshot = {};
	if ( m_pWindows->querySnapshot( m_hWindow, &snapshot ) != SIR_WINDOW_OK || !snapshot.bReady || snapshot.bCloseRequested ||
	     snapshot.nWindowGeneration != m_Native.nWindowGeneration || snapshot.nWidth == 0 || snapshot.nHeight == 0 ) {
		return SIR_GRAPHICS_SURFACE_CHANGED;
	}
	if ( snapshot.nMetricsRevision == m_nMetricsRevision ) {
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsABIPresentDesc_t present = *pPresent;
	present.nWidth = snapshot.nWidth;
	present.nHeight = snapshot.nHeight;
	const sirGraphicsABINativeSurface_t native = nativeSurface();
	const sirGraphicsABIResult_t eResize = sirGraphicsResizeABISurface( m_pDevice, m_hSurface, &native, &present );
	if ( eResize == SIR_GRAPHICS_SUCCESS ) {
		m_nMetricsRevision = snapshot.nMetricsRevision;
	}
	return eResize;
}

sirGraphicsABIResult_t SIRWindowABISurface::retire( const sirGraphicsCompletionSet_t *pComplete ) noexcept
{
	if ( !isActive() ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( pComplete == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirGraphicsABIResult_t eRetire = sirGraphicsRetireABIResource( m_pDevice, m_hSurface, pComplete );
	if ( eRetire != SIR_GRAPHICS_SUCCESS ) {
		return eRetire;
	}
	const sirWindowResult_t eRelease = m_pWindows->releaseNativeLease( m_hLease );
	m_pWindows = nullptr;
	m_pDevice = nullptr;
	m_hWindow = {};
	m_hLease = {};
	m_Native = {};
	m_hSurface = {};
	m_nMetricsRevision = 0;
	return eRelease == SIR_WINDOW_OK ? SIR_GRAPHICS_SUCCESS : SIR_GRAPHICS_DRIVER_ERROR;
}
