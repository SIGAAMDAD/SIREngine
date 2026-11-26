/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

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
	const sirGraphicsABINativeSurface_t native = { sizeof( sirGraphicsABINativeSurface_t ), static_cast<uint32_t>(m_Native.ePlatform),
	                                               m_Native.pWindow, m_Native.pDisplayOrInstance, m_Native.nWindowGeneration };
	return native;
}

sirGraphicsABIResult_t SIRWindowABISurface::create( SIRWindowSystem& windows, sirWindowHandle_t hWindow,
                                                    const sirGraphicsABIDevice_t *pDevice,
                                                    const sirGraphicsABIPresentDesc_t *pPresent ) noexcept
{
	if ( isActive() || pDevice == nullptr || pPresent == nullptr || pPresent->nSize != sizeof( sirGraphicsABIPresentDesc_t ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirWindowSnapshot_t snapshot {};
	const sirWindowResult eSnapshot = windows.querySnapshot( hWindow, &snapshot );
	if ( eSnapshot == sirWindowResult::SIR_WINDOW_INVALID_HANDLE ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( eSnapshot != sirWindowResult::SIR_WINDOW_OK || !snapshot.bReady || snapshot.bCloseRequested || snapshot.nWidth == 0 || snapshot.nHeight == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_SURFACE_CHANGED;
	}
	sirGraphicsNativeSurface_t native {};
	const SIRGraphicsResult eAcquire = sirGraphicsAcquireWindowSurface( windows, hWindow, &native, &m_hLease );
	if ( eAcquire != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eAcquire;
	}
	sirWindowSnapshot_t after {};
	if ( windows.querySnapshot( hWindow, &after ) != sirWindowResult::SIR_WINDOW_OK || !after.bReady || after.bCloseRequested ||
	     after.nMetricsRevision != snapshot.nMetricsRevision || after.nWindowGeneration != native.nWindowGeneration ) {
		static_cast<void>(m_hLease.reset());
		return SIRGraphicsResult::SIR_GRAPHICS_SURFACE_CHANGED;
	}
	const sirGraphicsABINativeSurface_t abiNative = { sizeof( abiNative ), static_cast<uint32_t>(native.ePlatform), native.pWindow,
	                                                  native.pDisplayOrInstance, native.nWindowGeneration };
	sirGraphicsABIPresentDesc_t present = *pPresent;
	present.nWidth = after.nWidth;
	present.nHeight = after.nHeight;
	sirGraphicsHandle_t hSurface {};
	const sirGraphicsABIResult_t eCreate = sirGraphicsCreateABISurface( pDevice, &abiNative, &present, &hSurface );
	if ( eCreate != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		static_cast<void>(m_hLease.reset());
		return eCreate;
	}
	m_pWindows = &windows;
	m_pDevice = pDevice;
	m_hWindow = hWindow;
	m_Native = native;
	m_hSurface = hSurface;
	m_nMetricsRevision = after.nMetricsRevision;
	m_Present = present;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

sirGraphicsABIResult_t SIRWindowABISurface::acquireImage( sirGraphicsSurfaceImage_t *pImage ) noexcept
{
	if ( !isActive() ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( pImage == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( m_bImageAcquired ) {
		return SIRGraphicsResult::SIR_GRAPHICS_BUSY;
	}
	sirWindowSnapshot_t snapshot {};
	if ( m_pWindows->querySnapshot( m_hWindow, &snapshot ) != sirWindowResult::SIR_WINDOW_OK || !snapshot.bReady || snapshot.bCloseRequested ||
	     snapshot.nWindowGeneration != m_Native.nWindowGeneration || snapshot.nMetricsRevision != m_nMetricsRevision ) {
		return SIRGraphicsResult::SIR_GRAPHICS_SURFACE_CHANGED;
	}
	sirGraphicsSurfaceImage_t acquired {};
	const sirGraphicsABIResult_t eAcquire = sirGraphicsAcquireABISurfaceImage( m_pDevice, m_hSurface, &acquired );
	if ( eAcquire != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eAcquire;
	}
	m_Acquired = acquired;
	m_bImageAcquired = true;
	*pImage = acquired;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

sirGraphicsABIResult_t SIRWindowABISurface::presentImage( const sirGraphicsCompletion_t *pCompletion ) noexcept
{
	if ( !isActive() ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( pCompletion == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( !m_bImageAcquired ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	const sirGraphicsABIResult_t ePresent = sirGraphicsPresentABISurfaceImage( m_pDevice, &m_Acquired, pCompletion );
	if ( ePresent == SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		m_Acquired = {};
		m_bImageAcquired = false;
	}
	return ePresent;
}

sirGraphicsABIResult_t SIRWindowABISurface::resize( const sirGraphicsABIPresentDesc_t *pPresent ) noexcept
{
	if ( !isActive() ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( pPresent == nullptr || pPresent->nSize != sizeof( sirGraphicsABIPresentDesc_t ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( m_bImageAcquired ) {
		return SIRGraphicsResult::SIR_GRAPHICS_BUSY;
	}
	sirWindowSnapshot_t snapshot {};
	if ( m_pWindows->querySnapshot( m_hWindow, &snapshot ) != sirWindowResult::SIR_WINDOW_OK || !snapshot.bReady || snapshot.bCloseRequested ||
	     snapshot.nWindowGeneration != m_Native.nWindowGeneration || snapshot.nWidth == 0 || snapshot.nHeight == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_SURFACE_CHANGED;
	}
	// Window geometry alone does not determine swapchain compatibility.
	// Callers can change present mode, format, tearing or buffer count.
	if ( snapshot.nMetricsRevision == m_nMetricsRevision && snapshot.nWidth == m_Present.nWidth && snapshot.nHeight == m_Present.nHeight &&
	     pPresent->nFormat == m_Present.nFormat && pPresent->nMode == m_Present.nMode &&
	     pPresent->nBackbufferCount == m_Present.nBackbufferCount && pPresent->nAllowTearing == m_Present.nAllowTearing &&
	     pPresent->nHdr == m_Present.nHdr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsABIPresentDesc_t present = *pPresent;
	present.nWidth = snapshot.nWidth;
	present.nHeight = snapshot.nHeight;
	const sirGraphicsABINativeSurface_t native = nativeSurface();
	const sirGraphicsABIResult_t eResize = sirGraphicsResizeABISurface( m_pDevice, m_hSurface, &native, &present );
	if ( eResize == SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		m_nMetricsRevision = snapshot.nMetricsRevision;
		m_Present = present;
	}
	return eResize;
}

sirGraphicsABIResult_t SIRWindowABISurface::retire( const sirGraphicsCompletionSet_t *pComplete ) noexcept
{
	if ( !isActive() ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( pComplete == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( m_bImageAcquired ) {
		return SIRGraphicsResult::SIR_GRAPHICS_BUSY;
	}
	const sirGraphicsABIResult_t eRetire = sirGraphicsRetireABIResource( m_pDevice, m_hSurface, pComplete );
	if ( eRetire != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eRetire;
	}
	const sirWindowResult eRelease = m_hLease.reset();
	m_pWindows = nullptr;
	m_pDevice = nullptr;
	m_hWindow = {};
	m_Native = {};
	m_hSurface = {};
	m_nMetricsRevision = 0;
	m_Present = {};
	m_Acquired = {};
	m_bImageAcquired = false;
	return eRelease == sirWindowResult::SIR_WINDOW_OK ? SIRGraphicsResult::SIR_GRAPHICS_SUCCESS : SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR;
}
