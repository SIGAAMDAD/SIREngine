#include <Core/Platform/Window/Linux/SIRWindowDriverWayland.h>
#include <wayland-client.h>
#include <xdg-shell-client-protocol.h>
#ifdef SIR_WAYLAND_FRACTIONAL_SCALE_AVAILABLE
#include <fractional-scale-v1-client-protocol.h>
#include <viewporter-client-protocol.h>
#endif
#ifdef SIR_WAYLAND_POINTER_PROTOCOLS_AVAILABLE
#include <pointer-constraints-unstable-v1-client-protocol.h>
#include <relative-pointer-unstable-v1-client-protocol.h>
#endif
#include <errno.h>
#include <poll.h>
#include <string.h>

SIRWindowDriverWayland::~SIRWindowDriverWayland( void ) noexcept
{
	shutdown();
}

void SIRWindowDriverWayland::registryGlobal( void *pContext, wl_registry *pRegistry, uint32_t nName, const char *pszInterface,
                                             uint32_t nVersion ) noexcept
{
	SIRWindowDriverWayland *pDriver = ( SIRWindowDriverWayland * )pContext;
	if ( strcmp( pszInterface, wl_compositor_interface.name ) == 0 && pDriver->m_pCompositor == nullptr ) {
		const uint32_t nBindVersion = nVersion < 4 ? nVersion : 4;
		pDriver->m_pCompositor = ( wl_compositor * )wl_registry_bind( pRegistry, nName, &wl_compositor_interface, nBindVersion );
		pDriver->m_nCompositorGlobal = nName;
	} else if ( strcmp( pszInterface, xdg_wm_base_interface.name ) == 0 && pDriver->m_pBase == nullptr ) {
		pDriver->m_pBase = ( xdg_wm_base * )wl_registry_bind( pRegistry, nName, &xdg_wm_base_interface, 1 );
		pDriver->m_nShellGlobal = nName;
	} else if ( strcmp( pszInterface, wl_output_interface.name ) == 0 ) {
		for ( uint32_t i = 0; i < SIR_WAYLAND_MAX_OUTPUTS; ++i ) {
			sirWaylandOutput_t& output = pDriver->m_Outputs[i];
			if ( output.pOutput != nullptr ) {
				continue;
			}
			const uint32_t nBindVersion = nVersion < 2 ? nVersion : 2;
			output.pOutput = ( wl_output * )wl_registry_bind( pRegistry, nName, &wl_output_interface, nBindVersion );
			if ( output.pOutput == nullptr ) {
				break;
			}
			output.pDriver = pDriver;
			output.nGlobal = nName;
			output.nScale = 1;
			static const wl_output_listener s_OutputListener = { outputGeometry, outputMode, outputDone, outputScale };
			if ( wl_output_add_listener( output.pOutput, &s_OutputListener, &output ) != 0 ) {
				wl_output_destroy( output.pOutput );
				output = {};
			}
			break;
		}
	} 
#ifdef SIR_WAYLAND_FRACTIONAL_SCALE_AVAILABLE
	else if ( strcmp( pszInterface, wp_fractional_scale_manager_v1_interface.name ) == 0 &&
	            pDriver->m_pFractionalManager == nullptr ) {
		pDriver->m_pFractionalManager = ( wp_fractional_scale_manager_v1 * )wl_registry_bind(
		    pRegistry, nName, &wp_fractional_scale_manager_v1_interface, 1 );
		pDriver->m_nFractionalGlobal = nName;
	} else if ( strcmp( pszInterface, wp_viewporter_interface.name ) == 0 && pDriver->m_pViewporter == nullptr ) {
		pDriver->m_pViewporter = ( wp_viewporter * )wl_registry_bind( pRegistry, nName, &wp_viewporter_interface, 1 );
		pDriver->m_nViewporterGlobal = nName;
	}
#endif
	else if ( strcmp( pszInterface, wl_seat_interface.name ) == 0 && pDriver->m_pSeat == nullptr ) {
		const uint32_t nBindVersion = nVersion < 7 ? nVersion : 7;
		pDriver->m_pSeat = ( wl_seat * )wl_registry_bind( pRegistry, nName, &wl_seat_interface, nBindVersion );
		pDriver->m_nSeatGlobal = nName;
	}
#ifdef SIR_WAYLAND_POINTER_PROTOCOLS_AVAILABLE
	else if ( strcmp( pszInterface, zwp_pointer_constraints_v1_interface.name ) == 0 && pDriver->m_pPointerConstraints == nullptr ) {
		pDriver->m_pPointerConstraints =
		    ( zwp_pointer_constraints_v1 * )wl_registry_bind( pRegistry, nName, &zwp_pointer_constraints_v1_interface, 1 );
		pDriver->m_nConstraintsGlobal = nName;
	} else if ( strcmp( pszInterface, zwp_relative_pointer_manager_v1_interface.name ) == 0 &&
	            pDriver->m_pRelativePointerManager == nullptr ) {
		pDriver->m_pRelativePointerManager =
		    ( zwp_relative_pointer_manager_v1 * )wl_registry_bind( pRegistry, nName, &zwp_relative_pointer_manager_v1_interface, 1 );
		pDriver->m_nRelativePointerGlobal = nName;
	}
#endif
}

void SIRWindowDriverWayland::registryRemove( void *pContext, wl_registry *pRegistry, uint32_t nName ) noexcept
{
	( void )pRegistry;
	SIRWindowDriverWayland *pDriver = ( SIRWindowDriverWayland * )pContext;
	if ( nName == pDriver->m_nSeatGlobal && pDriver->m_pSeat != nullptr ) {
		if ( pDriver->m_pSeatLostSink != nullptr ) {
			pDriver->m_pSeatLostSink( pDriver->m_pSeatLostContext );
		}
		if ( wl_proxy_get_version( ( wl_proxy * )pDriver->m_pSeat ) >= 5 ) {
			wl_seat_release( pDriver->m_pSeat );
		} else {
			wl_seat_destroy( pDriver->m_pSeat );
		}
		pDriver->m_pSeat = nullptr;
		pDriver->m_nSeatGlobal = 0;
	}
#ifdef SIR_WAYLAND_POINTER_PROTOCOLS_AVAILABLE
	if ( nName == pDriver->m_nConstraintsGlobal || nName == pDriver->m_nRelativePointerGlobal ) {
		if ( pDriver->m_pPointerProtocolLostSink != nullptr ) {
			pDriver->m_pPointerProtocolLostSink( pDriver->m_pPointerProtocolLostContext );
		}
		if ( nName == pDriver->m_nConstraintsGlobal && pDriver->m_pPointerConstraints != nullptr ) {
			zwp_pointer_constraints_v1_destroy( pDriver->m_pPointerConstraints );
			pDriver->m_pPointerConstraints = nullptr;
			pDriver->m_nConstraintsGlobal = 0;
		}
		if ( nName == pDriver->m_nRelativePointerGlobal && pDriver->m_pRelativePointerManager != nullptr ) {
			zwp_relative_pointer_manager_v1_destroy( pDriver->m_pRelativePointerManager );
			pDriver->m_pRelativePointerManager = nullptr;
			pDriver->m_nRelativePointerGlobal = 0;
		}
	}
#endif
	for ( uint32_t i = 0; i < SIR_WAYLAND_MAX_OUTPUTS; ++i ) {
		sirWaylandOutput_t& output = pDriver->m_Outputs[i];
		if ( output.pOutput == nullptr || output.nGlobal != nName ) {
			continue;
		}
		for ( uint32_t j = 0; j < SIR_WINDOW_MAX_WINDOWS; ++j ) {
			sirWaylandWindowSlot_t& slot = pDriver->m_Slots[j];
			slot.nOutputMask &= ~( UINT32_C( 1 ) << i );
			pDriver->refreshOutputScale( slot );
		}
		wl_output_destroy( output.pOutput );
		output = {};
		break;
	}
	#ifdef SIR_WAYLAND_FRACTIONAL_SCALE_AVAILABLE
	if ( nName == pDriver->m_nFractionalGlobal ) {
		if ( pDriver->m_pFractionalManager != nullptr ) {
			wp_fractional_scale_manager_v1_destroy( pDriver->m_pFractionalManager );
		}
		pDriver->m_pFractionalManager = nullptr;
		pDriver->m_nFractionalGlobal = 0;
	}
	if ( nName == pDriver->m_nViewporterGlobal ) {
		if ( pDriver->m_pViewporter != nullptr ) {
			wp_viewporter_destroy( pDriver->m_pViewporter );
		}
		pDriver->m_pViewporter = nullptr;
		pDriver->m_nViewporterGlobal = 0;
	}
#endif
	if ( nName == pDriver->m_nCompositorGlobal || nName == pDriver->m_nShellGlobal ) {
		pDriver->loseDisplay();
	}
}

void SIRWindowDriverWayland::wmBasePing( void *pContext, xdg_wm_base *pBase, uint32_t nSerial ) noexcept
{
	( void )pContext;
	xdg_wm_base_pong( pBase, nSerial );
}

void SIRWindowDriverWayland::topConfigure( void *pContext, xdg_toplevel *pTop, int32_t nWidth, int32_t nHeight, wl_array *pStates ) noexcept
{
	( void )pTop;
	( void )pStates;
	sirWaylandWindowSlot_t *pSlot = ( sirWaylandWindowSlot_t * )pContext;
	if ( nWidth > 0 ) {
		pSlot->nPendingWidth = ( uint32_t )nWidth;
	}
	if ( nHeight > 0 ) {
		pSlot->nPendingHeight = ( uint32_t )nHeight;
	}
}

void SIRWindowDriverWayland::surfaceConfigure( void *pContext, xdg_surface *pSurface, uint32_t nSerial ) noexcept
{
	sirWaylandWindowSlot_t *pSlot = ( sirWaylandWindowSlot_t * )pContext;
	if ( pSlot->pXdgSurface != pSurface || pSlot->pDriver == nullptr || pSlot->bLostPending || pSlot->pDriver->m_bLost ) {
		return;
	}
	// Ignore duplicate or older xdg configure notifications. Serial arithmetic
	// intentionally handles the Wayland protocol's uint32 wraparound.
	if ( pSlot->bConfigured && ( int32_t )( nSerial - pSlot->nLastConfigureSerial ) <= 0 ) {
		pSlot->nPendingWidth = pSlot->nWidth;
		pSlot->nPendingHeight = pSlot->nHeight;
		return;
	}
	xdg_surface_ack_configure( pSurface, nSerial );
	pSlot->nLastConfigureSerial = nSerial;
	const bool bFirst = !pSlot->bConfigured;
	const bool bResized = pSlot->nPendingWidth != pSlot->nWidth || pSlot->nPendingHeight != pSlot->nHeight;
	pSlot->nWidth = pSlot->nPendingWidth;
	pSlot->nHeight = pSlot->nPendingHeight;
	pSlot->bConfigured = true;
	if ( ( bFirst || bResized ) && !pSlot->bClosePending ) {
		const uint32_t nScale = pSlot->nBufferScale == 0 ? 1 : pSlot->nBufferScale;
		const uint32_t nWidth = pSlot->nWidth <= UINT32_MAX / nScale ? pSlot->nWidth * nScale : 0;
		const uint32_t nHeight = pSlot->nHeight <= UINT32_MAX / nScale ? pSlot->nHeight * nScale : 0;
		pSlot->pDriver->queueEvent( pSlot->nIndex, bFirst ? SIR_WINDOW_EVENT_READY : SIR_WINDOW_EVENT_RESIZED, nWidth, nHeight );
	}
}

void SIRWindowDriverWayland::outputGeometry( void *pContext, wl_output *pOutput, int32_t nX, int32_t nY, int32_t nWidth,
                                              int32_t nHeight, int32_t nSubpixel, const char *pszMake,
                                              const char *pszModel, int32_t nTransform ) noexcept
{
	( void )pContext;
	( void )pOutput;
	( void )nX;
	( void )nY;
	( void )nWidth;
	( void )nHeight;
	( void )nSubpixel;
	( void )pszMake;
	( void )pszModel;
	( void )nTransform;
}

void SIRWindowDriverWayland::outputMode( void *pContext, wl_output *pOutput, uint32_t nFlags, int32_t nWidth, int32_t nHeight,
                                          int32_t nRefresh ) noexcept
{
	( void )pContext;
	( void )pOutput;
	( void )nFlags;
	( void )nWidth;
	( void )nHeight;
	( void )nRefresh;
}

void SIRWindowDriverWayland::outputDone( void *pContext, wl_output *pOutput ) noexcept
{
	( void )pContext;
	( void )pOutput;
}

void SIRWindowDriverWayland::outputScale( void *pContext, wl_output *pOutput, int32_t nScale ) noexcept
{
	sirWaylandOutput_t *pEntry = ( sirWaylandOutput_t * )pContext;
	if ( pEntry->pOutput != pOutput || pEntry->pDriver == nullptr || pEntry->pDriver->m_bLost || nScale < 1 ) {
		return;
	}
	const uint32_t nAccepted = nScale > 16 ? 16 : ( uint32_t )nScale;
	if ( nAccepted == pEntry->nScale ) {
		return;
	}
	pEntry->nScale = nAccepted;
	const uint32_t nIndex = ( uint32_t )( pEntry - pEntry->pDriver->m_Outputs );
	for ( uint32_t i = 0; i < SIR_WINDOW_MAX_WINDOWS; ++i ) {
		sirWaylandWindowSlot_t& slot = pEntry->pDriver->m_Slots[i];
		if ( ( slot.nOutputMask & ( UINT32_C( 1 ) << nIndex ) ) != 0 ) {
			pEntry->pDriver->refreshOutputScale( slot );
		}
	}
}

void SIRWindowDriverWayland::surfaceEnter( void *pContext, wl_surface *pSurface, wl_output *pOutput ) noexcept
{
	sirWaylandWindowSlot_t *pSlot = ( sirWaylandWindowSlot_t * )pContext;
	if ( pSlot->pDriver == nullptr || pSlot->pSurface != pSurface || pSlot->pDriver->m_bLost ) {
		return;
	}
	for ( uint32_t i = 0; i < SIR_WAYLAND_MAX_OUTPUTS; ++i ) {
		if ( pSlot->pDriver->m_Outputs[i].pOutput == pOutput ) {
			pSlot->nOutputMask |= UINT32_C( 1 ) << i;
			pSlot->pDriver->refreshOutputScale( *pSlot );
			return;
		}
	}
}

void SIRWindowDriverWayland::surfaceLeave( void *pContext, wl_surface *pSurface, wl_output *pOutput ) noexcept
{
	sirWaylandWindowSlot_t *pSlot = ( sirWaylandWindowSlot_t * )pContext;
	if ( pSlot->pDriver == nullptr || pSlot->pSurface != pSurface || pSlot->pDriver->m_bLost ) {
		return;
	}
	for ( uint32_t i = 0; i < SIR_WAYLAND_MAX_OUTPUTS; ++i ) {
		if ( pSlot->pDriver->m_Outputs[i].pOutput == pOutput ) {
			pSlot->nOutputMask &= ~( UINT32_C( 1 ) << i );
			pSlot->pDriver->refreshOutputScale( *pSlot );
			return;
		}
	}
}

void SIRWindowDriverWayland::refreshOutputScale( sirWaylandWindowSlot_t& slot ) noexcept
{
	if ( slot.pSurface == nullptr || m_bLost ) {
		return;
	}
	uint32_t nScale = 1;
	if ( wl_proxy_get_version( ( wl_proxy * )slot.pSurface ) >= 3 ) {
		for ( uint32_t i = 0; i < SIR_WAYLAND_MAX_OUTPUTS; ++i ) {
			if ( ( slot.nOutputMask & ( UINT32_C( 1 ) << i ) ) != 0 && m_Outputs[i].pOutput != nullptr &&
			     m_Outputs[i].nScale > nScale ) {
				nScale = m_Outputs[i].nScale;
			}
		}
	}
	if ( slot.nBufferScale == nScale ) {
		return;
	}
	slot.nBufferScale = nScale;
	if ( wl_proxy_get_version( ( wl_proxy * )slot.pSurface ) >= 3 ) {
		wl_surface_set_buffer_scale( slot.pSurface, ( int32_t )nScale );
	}
	if ( slot.bConfigured && !slot.bClosePending ) {
		queueEvent( slot.nIndex, SIR_WINDOW_EVENT_DPI_CHANGED, 96 * nScale, 0 );
		const uint32_t nWidth = slot.nWidth <= UINT32_MAX / nScale ? slot.nWidth * nScale : 0;
		const uint32_t nHeight = slot.nHeight <= UINT32_MAX / nScale ? slot.nHeight * nScale : 0;
		queueEvent( slot.nIndex, SIR_WINDOW_EVENT_RESIZED, nWidth, nHeight );
	}
}

void SIRWindowDriverWayland::topClose( void *pContext, xdg_toplevel *pTop ) noexcept
{
	sirWaylandWindowSlot_t *pSlot = ( sirWaylandWindowSlot_t * )pContext;
	if ( pSlot->pToplevel == pTop ) {
		pSlot->bClosePending = true;
	}
}

void SIRWindowDriverWayland::queueEvent( uint32_t nSlot, sirWindowEventKind_t eKind, uint32_t nWidth, uint32_t nHeight ) noexcept
{
	if ( m_nEventCount != 0 && eKind == SIR_WINDOW_EVENT_RESIZED ) {
		const uint32_t nPrevious = ( m_nEventWrite + SIR_WINDOW_EVENT_CAPACITY - 1 ) % SIR_WINDOW_EVENT_CAPACITY;
		sirWindowBackendEvent_t& previous = m_Events[nPrevious];
		if ( previous.nSlot == nSlot && previous.eKind == SIR_WINDOW_EVENT_RESIZED ) {
			previous.nWidth = nWidth;
			previous.nHeight = nHeight;
			return;
		}
	}
	if ( m_nEventCount == SIR_WINDOW_EVENT_CAPACITY ) {
		m_bOverflow = true;
		return;
	}
	m_Events[m_nEventWrite] = { nSlot, ( uintptr_t )m_Slots[nSlot].pSurface, eKind, nWidth, nHeight };
	m_nEventWrite = ( m_nEventWrite + 1 ) % SIR_WINDOW_EVENT_CAPACITY;
	++m_nEventCount;
}

void SIRWindowDriverWayland::loseDisplay( void ) noexcept
{
	if ( m_bLost ) {
		return;
	}
	m_bLost = true;
	for ( uint32_t i = 0; i < SIR_WINDOW_MAX_WINDOWS; ++i ) {
		if ( m_Slots[i].pSurface != nullptr ) {
			m_Slots[i].bLostPending = true;
		}
	}
}

sirWindowResult_t SIRWindowDriverWayland::initialize( void ) noexcept
{
	if ( m_pDisplay != nullptr ) {
		return SIR_WINDOW_BUSY;
	}
	m_pDisplay = wl_display_connect( nullptr );
	if ( m_pDisplay == nullptr ) {
		return SIR_WINDOW_NOT_READY;
	}
	m_pRegistry = wl_display_get_registry( m_pDisplay );
	static const wl_registry_listener s_RegistryListener = { registryGlobal, registryRemove };
	static const xdg_wm_base_listener s_WmListener = { wmBasePing };
	if ( m_pRegistry == nullptr || wl_registry_add_listener( m_pRegistry, &s_RegistryListener, this ) != 0 ||
	     wl_display_roundtrip( m_pDisplay ) < 0 || m_bLost || m_pCompositor == nullptr || m_pBase == nullptr ||
	     xdg_wm_base_add_listener( m_pBase, &s_WmListener, this ) != 0 ) {
		shutdown();
		return SIR_WINDOW_NATIVE_FAILURE;
	}
	return SIR_WINDOW_OK;
}

sirWindowResult_t SIRWindowDriverWayland::createWindow( const sirWindowCreateDesc_t& desc, uint32_t nSlot,
                                                        sirWindowNativeView_t *pView ) noexcept
{
	if ( pView == nullptr || desc.pszTitle == nullptr || desc.nWidth == 0 || desc.nHeight == 0 || nSlot >= SIR_WINDOW_MAX_WINDOWS ) {
		return SIR_WINDOW_INVALID_ARGUMENT;
	}
	if ( m_pDisplay == nullptr || m_bLost ) {
		return SIR_WINDOW_STOPPED;
	}
	sirWaylandWindowSlot_t& slot = m_Slots[nSlot];
	if ( slot.pSurface != nullptr ) {
		return SIR_WINDOW_BUSY;
	}
	static const xdg_surface_listener s_SurfaceListener = { surfaceConfigure };
	static const xdg_toplevel_listener s_ToplevelListener = { topConfigure, topClose, nullptr, nullptr };
	slot.pSurface = wl_compositor_create_surface( m_pCompositor );
	if ( slot.pSurface == nullptr ) {
		return SIR_WINDOW_NATIVE_FAILURE;
	}
	slot.pDriver = this;
	slot.nIndex = nSlot;
	slot.nBufferScale = 1;
	static const wl_surface_listener s_SurfaceOutputListener = { surfaceEnter, surfaceLeave };
	if ( wl_surface_add_listener( slot.pSurface, &s_SurfaceOutputListener, &slot ) != 0 ) {
		wl_surface_destroy( slot.pSurface );
		slot = {};
		return SIR_WINDOW_NATIVE_FAILURE;
	}
	slot.pXdgSurface = xdg_wm_base_get_xdg_surface( m_pBase, slot.pSurface );
	if ( slot.pXdgSurface == nullptr ) {
		wl_surface_destroy( slot.pSurface );
		slot = {};
		return SIR_WINDOW_NATIVE_FAILURE;
	}
	slot.pToplevel = xdg_surface_get_toplevel( slot.pXdgSurface );
	if ( slot.pToplevel == nullptr ) {
		xdg_surface_destroy( slot.pXdgSurface );
		wl_surface_destroy( slot.pSurface );
		slot = {};
		return SIR_WINDOW_NATIVE_FAILURE;
	}
	slot.pDriver = this;
	slot.nIndex = nSlot;
	slot.nWidth = desc.nWidth;
	slot.nHeight = desc.nHeight;
	slot.nPendingWidth = desc.nWidth;
	slot.nPendingHeight = desc.nHeight;
	if ( xdg_surface_add_listener( slot.pXdgSurface, &s_SurfaceListener, &slot ) != 0 ||
	     xdg_toplevel_add_listener( slot.pToplevel, &s_ToplevelListener, &slot ) != 0 ) {
		( void )destroyWindow( nSlot );
		return SIR_WINDOW_NATIVE_FAILURE;
	}
	xdg_toplevel_set_title( slot.pToplevel, desc.pszTitle );
	wl_surface_commit( slot.pSurface ); // Empty initial commit: renderer may not attach a buffer yet.
	*pView = { SIR_WINDOW_PLATFORM_WAYLAND, ( uintptr_t )slot.pSurface, ( uintptr_t )m_pDisplay, 0 };
	return SIR_WINDOW_OK;
}

sirWindowResult_t SIRWindowDriverWayland::destroyWindow( uint32_t nSlot ) noexcept
{
	if ( nSlot >= SIR_WINDOW_MAX_WINDOWS || m_Slots[nSlot].pSurface == nullptr ) {
		return SIR_WINDOW_INVALID_HANDLE;
	}
	sirWaylandWindowSlot_t& slot = m_Slots[nSlot];
	if ( !m_bLost ) {
		xdg_toplevel_destroy( slot.pToplevel );
		xdg_surface_destroy( slot.pXdgSurface );
		wl_surface_destroy( slot.pSurface );
	} else {
		wl_proxy_destroy( ( wl_proxy * )slot.pToplevel );
		wl_proxy_destroy( ( wl_proxy * )slot.pXdgSurface );
		wl_proxy_destroy( ( wl_proxy * )slot.pSurface );
	}
	// Drop all queued events for this retired native surface. A newly created
	// window in the same slot must not observe stale configure/resize events.
	const uint32_t nOriginal = m_nEventCount;
	uint32_t nRetained = 0;
	for ( uint32_t i = 0; i < nOriginal; ++i ) {
		const sirWindowBackendEvent_t pending = m_Events[( m_nEventRead + i ) % SIR_WINDOW_EVENT_CAPACITY];
		if ( pending.nSlot != nSlot ) {
			m_Events[( m_nEventRead + nRetained ) % SIR_WINDOW_EVENT_CAPACITY] = pending;
			++nRetained;
		}
	}
	m_nEventCount = nRetained;
	m_nEventWrite = ( m_nEventRead + nRetained ) % SIR_WINDOW_EVENT_CAPACITY;
	slot = {};
	return SIR_WINDOW_OK;
}

sirWindowResult_t SIRWindowDriverWayland::mutateWindow( uint32_t nSlot, const sirWindowMutation_t& mutation ) noexcept
{
	if ( m_pDisplay == nullptr || m_bLost ) {
		return SIR_WINDOW_STOPPED;
	}
	if ( nSlot >= SIR_WINDOW_MAX_WINDOWS || m_Slots[nSlot].pToplevel == nullptr ) {
		return SIR_WINDOW_INVALID_HANDLE;
	}
	xdg_toplevel *pTop = m_Slots[nSlot].pToplevel;
	switch ( mutation.eKind ) {
	case SIR_WINDOW_MUTATION_TITLE:
		if ( mutation.pszTitle == nullptr ) {
			return SIR_WINDOW_INVALID_ARGUMENT;
		}
		xdg_toplevel_set_title( pTop, mutation.pszTitle );
		return SIR_WINDOW_OK;
	case SIR_WINDOW_MUTATION_MINIMIZE:
		xdg_toplevel_set_minimized( pTop );
		return SIR_WINDOW_OK;
	case SIR_WINDOW_MUTATION_MAXIMIZE:
		xdg_toplevel_set_maximized( pTop );
		return SIR_WINDOW_OK;
	case SIR_WINDOW_MUTATION_FULLSCREEN_ENTER:
		xdg_toplevel_set_fullscreen( pTop, nullptr );
		return SIR_WINDOW_OK;
	case SIR_WINDOW_MUTATION_FULLSCREEN_EXIT:
		xdg_toplevel_unset_fullscreen( pTop );
		return SIR_WINDOW_OK;
	default:
		return SIR_WINDOW_UNSUPPORTED;
	}
}

sirWindowResult_t SIRWindowDriverWayland::pumpEvents( uint32_t nBudget, sirWindowDriverEventFn_t pCallback, void *pContext ) noexcept
{
	if ( m_pDisplay == nullptr ) {
		return SIR_WINDOW_STOPPED;
	}
	if ( nBudget == 0 || pCallback == nullptr ) {
		return SIR_WINDOW_INVALID_ARGUMENT;
	}
	if ( !m_bLost ) {
		while ( wl_display_prepare_read( m_pDisplay ) != 0 ) {
			if ( wl_display_dispatch_pending( m_pDisplay ) < 0 ) {
				loseDisplay();
				break;
			}
		}
		if ( !m_bLost ) {
			const int nFlushed = wl_display_flush( m_pDisplay );
			if ( nFlushed < 0 && errno != EAGAIN ) {
				wl_display_cancel_read( m_pDisplay );
				loseDisplay();
			} else {
				pollfd fd = { wl_display_get_fd( m_pDisplay ), POLLIN, 0 };
				const int nReadable = poll( &fd, 1, 0 );
				if ( nReadable > 0 && ( fd.revents & ( POLLERR | POLLHUP | POLLNVAL ) ) != 0 ) {
					wl_display_cancel_read( m_pDisplay );
					loseDisplay();
				} else if ( nReadable > 0 && ( fd.revents & POLLIN ) != 0 ) {
					if ( wl_display_read_events( m_pDisplay ) < 0 ) {
						loseDisplay();
					}
				} else {
					wl_display_cancel_read( m_pDisplay );
					if ( nReadable < 0 && errno != EINTR ) {
						loseDisplay();
					}
				}
				if ( !m_bLost && wl_display_dispatch_pending( m_pDisplay ) < 0 ) {
					loseDisplay();
				}
			}
		}
	}
	uint32_t nDelivered = 0;
	while ( m_nEventCount != 0 && nDelivered < nBudget ) {
		pCallback( pContext, m_Events[m_nEventRead] );
		m_nEventRead = ( m_nEventRead + 1 ) % SIR_WINDOW_EVENT_CAPACITY;
		--m_nEventCount;
		++nDelivered;
	}
	for ( uint32_t i = 0; i < SIR_WINDOW_MAX_WINDOWS; ++i ) {
		sirWaylandWindowSlot_t& slot = m_Slots[i];
		if ( slot.bLostPending && nDelivered < nBudget ) {
			slot.bLostPending = false;
			const sirWindowBackendEvent_t event = { i, ( uintptr_t )slot.pSurface, SIR_WINDOW_EVENT_LOST, 0, 0 };
			pCallback( pContext, event );
			++nDelivered;
		}
		if ( slot.bClosePending && nDelivered < nBudget ) {
			slot.bClosePending = false;
			const sirWindowBackendEvent_t event = { i, ( uintptr_t )slot.pSurface, SIR_WINDOW_EVENT_CLOSE_REQUESTED, 0, 0 };
			pCallback( pContext, event );
			++nDelivered;
		}
	}
	const bool bOverflow = m_bOverflow;
	m_bOverflow = false;
	return m_bLost ? SIR_WINDOW_NATIVE_FAILURE : bOverflow ? SIR_WINDOW_EVENTS_LOST : SIR_WINDOW_OK;
}

void SIRWindowDriverWayland::shutdown( void ) noexcept
{
	if ( m_pDisplay == nullptr ) {
		return;
	}
	if ( m_pPointerProtocolLostSink != nullptr ) {
		m_pPointerProtocolLostSink( m_pPointerProtocolLostContext );
	}
	if ( m_pSeatLostSink != nullptr ) {
		m_pSeatLostSink( m_pSeatLostContext );
	}
	m_pPointerProtocolLostSink = nullptr;
	m_pPointerProtocolLostContext = nullptr;
	m_pSeatLostSink = nullptr;
	m_pSeatLostContext = nullptr;
	for ( uint32_t i = 0; i < SIR_WINDOW_MAX_WINDOWS; ++i ) {
		if ( m_Slots[i].pSurface != nullptr ) {
			( void )destroyWindow( i );
		}
	}
	for ( uint32_t i = 0; i < SIR_WAYLAND_MAX_OUTPUTS; ++i ) {
		if ( m_Outputs[i].pOutput != nullptr ) {
			wl_output_destroy( m_Outputs[i].pOutput );
			m_Outputs[i] = {};
		}
	}
	if ( m_pSeat != nullptr ) {
		if ( !m_bLost && wl_proxy_get_version( ( wl_proxy * )m_pSeat ) >= 5 ) {
			wl_seat_release( m_pSeat );
		} else {
			wl_seat_destroy( m_pSeat );
		}
	}
#ifdef SIR_WAYLAND_POINTER_PROTOCOLS_AVAILABLE
	if ( m_pPointerConstraints != nullptr ) {
		zwp_pointer_constraints_v1_destroy( m_pPointerConstraints );
	}
	if ( m_pRelativePointerManager != nullptr ) {
		zwp_relative_pointer_manager_v1_destroy( m_pRelativePointerManager );
	}
#endif
	if ( m_pBase != nullptr ) {
		if ( !m_bLost ) {
			xdg_wm_base_destroy( m_pBase );
		} else {
			wl_proxy_destroy( ( wl_proxy * )m_pBase );
		}
	}
	if ( m_pCompositor != nullptr ) {
		wl_compositor_destroy( m_pCompositor );
	}
	if ( m_pRegistry != nullptr ) {
		wl_registry_destroy( m_pRegistry );
	}
	wl_display_disconnect( m_pDisplay );
	m_pDisplay = nullptr;
	m_pRegistry = nullptr;
	m_pCompositor = nullptr;
	m_pSeat = nullptr;
	m_pBase = nullptr;
	m_nSeatGlobal = 0;
	m_nConstraintsGlobal = 0;
	m_nRelativePointerGlobal = 0;
	m_pPointerConstraints = nullptr;
	m_pRelativePointerManager = nullptr;
	m_nEventRead = 0;
	m_nEventWrite = 0;
	m_nEventCount = 0;
	m_nCompositorGlobal = 0;
	m_nShellGlobal = 0;
	m_bLost = false;
	m_bOverflow = false;
}

wl_seat *SIRWindowDriverWayland::borrowSeat( void ) const noexcept
{
	return m_bLost ? nullptr : m_pSeat;
}

bool SIRWindowDriverWayland::setSeatLostSink( sirWaylandSeatLostFn_t pSink, void *pContext ) noexcept
{
	if ( pSink != nullptr && ( pContext == nullptr || m_pSeatLostSink != nullptr ) ) {
		return false;
	}
	m_pSeatLostSink = pSink;
	m_pSeatLostContext = pContext;
	return true;
}

zwp_pointer_constraints_v1 *SIRWindowDriverWayland::borrowPointerConstraints( void ) const noexcept
{
	return m_bLost ? nullptr : m_pPointerConstraints;
}

zwp_relative_pointer_manager_v1 *SIRWindowDriverWayland::borrowRelativePointerManager( void ) const noexcept
{
	return m_bLost ? nullptr : m_pRelativePointerManager;
}

bool SIRWindowDriverWayland::setPointerProtocolLostSink( sirWaylandPointerProtocolLostFn_t pSink, void *pContext ) noexcept
{
	if ( pSink != nullptr && ( pContext == nullptr || m_pPointerProtocolLostSink != nullptr ) ) {
		return false;
	}
	m_pPointerProtocolLostSink = pSink;
	m_pPointerProtocolLostContext = pContext;
	return true;
}
