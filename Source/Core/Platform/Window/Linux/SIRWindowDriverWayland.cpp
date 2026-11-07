#include <Core/Platform/Window/Linux/SIRWindowDriverWayland.h>
#include <wayland-client.h>
#include <xdg-shell-client-protocol.h>
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
	} else if ( strcmp( pszInterface, wl_seat_interface.name ) == 0 && pDriver->m_pSeat == nullptr ) {
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
	if ( nWidth > 0 && nHeight > 0 ) {
		pSlot->nPendingWidth = ( uint32_t )nWidth;
		pSlot->nPendingHeight = ( uint32_t )nHeight;
	}
}

void SIRWindowDriverWayland::surfaceConfigure( void *pContext, xdg_surface *pSurface, uint32_t nSerial ) noexcept
{
	sirWaylandWindowSlot_t *pSlot = ( sirWaylandWindowSlot_t * )pContext;
	if ( pSlot->pXdgSurface != pSurface || pSlot->pDriver == nullptr || pSlot->bLostPending ) {
		return;
	}
	xdg_surface_ack_configure( pSurface, nSerial );
	const bool bFirst = !pSlot->bConfigured;
	const bool bResized = pSlot->nPendingWidth != pSlot->nWidth || pSlot->nPendingHeight != pSlot->nHeight;
	pSlot->nWidth = pSlot->nPendingWidth;
	pSlot->nHeight = pSlot->nPendingHeight;
	pSlot->bConfigured = true;
	if ( bFirst || bResized ) {
		pSlot->pDriver->queueEvent( pSlot->nIndex, bFirst ? SIR_WINDOW_EVENT_READY : SIR_WINDOW_EVENT_RESIZED, pSlot->nWidth,
		                            pSlot->nHeight );
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
