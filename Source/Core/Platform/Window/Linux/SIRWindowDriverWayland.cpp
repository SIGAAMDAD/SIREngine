#include <Core/Platform/Window/Linux/SIRWindowDriverWayland.h>
#include <wayland-client.h>
#include <xdg-shell-client-protocol.h>
#ifdef SIR_WAYLAND_TEXT_INPUT_AVAILABLE
#include <text-input-unstable-v3-client-protocol.h>
#endif
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

wl_compositor *SIRWindowDriverWayland::borrowCompositor( void ) const noexcept
{
	return m_bLost ? nullptr : m_pCompositor;
}

void SIRWindowDriverWayland::notifyKeyboardFocus( wl_surface *pSurface, bool bFocused ) noexcept
{
	if ( m_bLost || pSurface == nullptr ) {
		return;
	}
	if ( bFocused ) {
		if ( m_pKeyboardFocus == pSurface ) {
			return;
		}
		wl_surface *pPrevious = m_pKeyboardFocus;
		if ( pPrevious != nullptr ) {
			notifyKeyboardFocus( pPrevious, false );
		}
		m_pKeyboardFocus = pSurface;
	} else if ( m_pKeyboardFocus != pSurface ) {
		return;
	} else {
		m_pKeyboardFocus = nullptr;
	}
	for ( uint32_t i = 0; i < SIR_WINDOW_MAX_WINDOWS; ++i ) {
		if ( m_Slots[i].pSurface == pSurface && !m_Slots[i].bLostPending ) {
			queueEvent( i, bFocused ? SIR_WINDOW_EVENT_FOCUS_GAINED : SIR_WINDOW_EVENT_FOCUS_LOST, 0, 0 );
			return;
		}
	}
}

zwp_text_input_manager_v3 *SIRWindowDriverWayland::borrowTextInputManager( void ) const noexcept
{
	return m_bLost ? nullptr : m_pTextInputManager;
}

bool SIRWindowDriverWayland::setTextInputProtocolLostSink( sirWaylandTextInputProtocolLostFn_t pSink, void *pContext ) noexcept
{
	if ( m_pDisplay == nullptr || m_bLost || ( pSink != nullptr && m_pTextInputLostSink != nullptr ) ) {
		return false;
	}
	m_pTextInputLostSink = pSink;
	m_pTextInputLostContext = pContext;
	return true;
}

bool SIRWindowDriverWayland::isDisplayLost( void ) const noexcept
{
	return m_bLost;
}

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
	else if ( strcmp( pszInterface, wp_fractional_scale_manager_v1_interface.name ) == 0 && pDriver->m_pFractionalManager == nullptr ) {
		pDriver->m_pFractionalManager =
		    ( wp_fractional_scale_manager_v1 * )wl_registry_bind( pRegistry, nName, &wp_fractional_scale_manager_v1_interface, 1 );
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
#ifdef SIR_WAYLAND_TEXT_INPUT_AVAILABLE
	else if ( strcmp( pszInterface, zwp_text_input_manager_v3_interface.name ) == 0 && pDriver->m_pTextInputManager == nullptr ) {
		pDriver->m_pTextInputManager =
		    ( zwp_text_input_manager_v3 * )wl_registry_bind( pRegistry, nName, &zwp_text_input_manager_v3_interface, 1 );
		pDriver->m_nTextInputGlobal = pDriver->m_pTextInputManager != nullptr ? nName : 0;
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
		pDriver->m_pKeyboardFocus = nullptr;
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
#ifdef SIR_WAYLAND_TEXT_INPUT_AVAILABLE
	if ( nName == pDriver->m_nTextInputGlobal ) {
		if ( pDriver->m_pTextInputLostSink != nullptr ) {
			pDriver->m_pTextInputLostSink( pDriver->m_pTextInputLostContext );
		}
		if ( pDriver->m_pTextInputManager != nullptr ) {
			zwp_text_input_manager_v3_destroy( pDriver->m_pTextInputManager );
		}
		pDriver->m_pTextInputManager = nullptr;
		pDriver->m_nTextInputGlobal = 0;
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
		const uint32_t nScale120 = pSlot->nFractionalScale120 != 0 && pSlot->pViewport != nullptr
		                               ? pSlot->nFractionalScale120
		                               : ( pSlot->nBufferScale == 0 ? 120 : pSlot->nBufferScale * 120 );
		const uint64_t nWidth64 = ( ( uint64_t )pSlot->nWidth * nScale120 + 60 ) / 120;
		const uint64_t nHeight64 = ( ( uint64_t )pSlot->nHeight * nScale120 + 60 ) / 120;
		if ( nWidth64 == 0 || nHeight64 == 0 || nWidth64 > UINT32_MAX || nHeight64 > UINT32_MAX ) {
			pSlot->bLostPending = true;
			return;
		}
		pSlot->pDriver->refreshSurfaceExtent( *pSlot, false );
		pSlot->pDriver->queueEvent( pSlot->nIndex, bFirst ? SIR_WINDOW_EVENT_READY : SIR_WINDOW_EVENT_RESIZED, ( uint32_t )nWidth64,
		                            ( uint32_t )nHeight64 );
	}
}

void SIRWindowDriverWayland::outputGeometry( void *pContext, wl_output *pOutput, int32_t nX, int32_t nY, int32_t nWidth, int32_t nHeight,
                                             int32_t nSubpixel, const char *pszMake, const char *pszModel, int32_t nTransform ) noexcept
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

void SIRWindowDriverWayland::refreshSurfaceExtent( sirWaylandWindowSlot_t& slot, bool bEmit ) noexcept
{
	if ( !slot.bConfigured || slot.bClosePending || m_bLost ) {
		return;
	}
	const uint32_t nScale120 = slot.nFractionalScale120 != 0 && slot.pViewport != nullptr
	                               ? slot.nFractionalScale120
	                               : ( slot.nBufferScale == 0 ? 120 : slot.nBufferScale * 120 );
#ifdef SIR_WAYLAND_FRACTIONAL_SCALE_AVAILABLE
	if ( slot.pViewport != nullptr && slot.nWidth <= INT32_MAX && slot.nHeight <= INT32_MAX ) {
		wp_viewport_set_destination( slot.pViewport, ( int32_t )slot.nWidth, ( int32_t )slot.nHeight );
	}
#endif
	if ( bEmit ) {
		const uint32_t nDpi = ( 96 * nScale120 + 60 ) / 120;
		const uint64_t nPixelsX = ( ( uint64_t )slot.nWidth * nScale120 + 60 ) / 120;
		const uint64_t nPixelsY = ( ( uint64_t )slot.nHeight * nScale120 + 60 ) / 120;
		if ( nPixelsX == 0 || nPixelsY == 0 || nPixelsX > UINT32_MAX || nPixelsY > UINT32_MAX ) {
			return;
		}
		queueEvent( slot.nIndex, SIR_WINDOW_EVENT_DPI_CHANGED, nDpi, 0 );
		queueEvent( slot.nIndex, SIR_WINDOW_EVENT_RESIZED, ( uint32_t )nPixelsX, ( uint32_t )nPixelsY );
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
			if ( ( slot.nOutputMask & ( UINT32_C( 1 ) << i ) ) != 0 && m_Outputs[i].pOutput != nullptr && m_Outputs[i].nScale > nScale ) {
				nScale = m_Outputs[i].nScale;
			}
		}
	}
	if ( slot.pFractionalScale != nullptr ) {
		return;
	}
	if ( slot.nBufferScale == nScale ) {
		return;
	}
	slot.nBufferScale = nScale;
	if ( wl_proxy_get_version( ( wl_proxy * )slot.pSurface ) >= 3 ) {
		wl_surface_set_buffer_scale( slot.pSurface, ( int32_t )nScale );
	}
	if ( slot.bConfigured && !slot.bClosePending ) {
		refreshSurfaceExtent( slot, true );
	}
}

#ifdef SIR_WAYLAND_FRACTIONAL_SCALE_AVAILABLE
void SIRWindowDriverWayland::fractionalPreferredScale( void *pContext, wp_fractional_scale_v1 *pScale, uint32_t nScale120 ) noexcept
{
	sirWaylandWindowSlot_t *pSlot = ( sirWaylandWindowSlot_t * )pContext;
	if ( pSlot->pDriver == nullptr || pSlot->pFractionalScale != pScale || pSlot->pViewport == nullptr || pSlot->pDriver->m_bLost ||
	     nScale120 == 0 || nScale120 > 1920 || pSlot->nFractionalScale120 == nScale120 ) {
		return;
	}
	pSlot->nFractionalScale120 = nScale120;
	pSlot->pDriver->refreshSurfaceExtent( *pSlot, true );
}
#endif

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
		// Keep one latest state-bearing event of each kind per window, so
		// configure readiness and framebuffer dimensions cannot be lost
		// behind a burst of output-scale or resize notifications.
		sirWindowBackendEvent_t *pDeferred = nullptr;
		if ( eKind == SIR_WINDOW_EVENT_FOCUS_GAINED || eKind == SIR_WINDOW_EVENT_FOCUS_LOST ) {
			pDeferred = &m_DeferredFocus[nSlot];
		} else if ( eKind == SIR_WINDOW_EVENT_READY ) {
			pDeferred = &m_DeferredReady[nSlot];
		} else if ( eKind == SIR_WINDOW_EVENT_DPI_CHANGED ) {
			pDeferred = &m_DeferredDpi[nSlot];
		} else if ( eKind == SIR_WINDOW_EVENT_RESIZED ) {
			pDeferred = &m_DeferredResize[nSlot];
		}
		if ( pDeferred != nullptr ) {
			*pDeferred = { nSlot, ( uintptr_t )m_Slots[nSlot].pSurface, eKind, nWidth, nHeight };
		}
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
	m_pKeyboardFocus = nullptr;

	// Notify text input before the associated compositor seat and manager are retired.
	if ( m_pTextInputLostSink != nullptr ) {
		m_pTextInputLostSink( m_pTextInputLostContext );
	}
	m_pTextInputLostSink = nullptr;
	m_pTextInputLostContext = nullptr;

	// Input's borrowed seat and pointer objects are invalid immediately on
	// compositor loss. Notify once, before the next input sampling window.
	const sirWaylandPointerProtocolLostFn_t pPointerSink = m_pPointerProtocolLostSink;
	void *pPointerContext = m_pPointerProtocolLostContext;
	m_pPointerProtocolLostSink = nullptr;
	m_pPointerProtocolLostContext = nullptr;
	if ( pPointerSink != nullptr ) {
		pPointerSink( pPointerContext );
	}
	const sirWaylandSeatLostFn_t pSeatSink = m_pSeatLostSink;
	void *pSeatContext = m_pSeatLostContext;
	m_pSeatLostSink = nullptr;
	m_pSeatLostContext = nullptr;
	if ( pSeatSink != nullptr ) {
		pSeatSink( pSeatContext );
	}

	// The display connection is terminal. Buffered READY/RESIZED/DPI events
	// refer to native surfaces that can no longer be presented. Publish only
	// the reserved per-slot LOST notifications, even if the ring was full.
	m_nEventRead = 0;
	m_nEventWrite = 0;
	m_nEventCount = 0;
	m_bOverflow = false;
	for ( uint32_t i = 0; i < SIR_WINDOW_MAX_WINDOWS; ++i ) {
		m_DeferredReady[i] = {};
		m_DeferredDpi[i] = {};
		m_DeferredResize[i] = {};
		m_DeferredFocus[i] = {};
		if ( m_Slots[i].pSurface != nullptr ) {
			m_Slots[i].bLostPending = true;
			m_Slots[i].bClosePending = false;
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
#ifdef SIR_WAYLAND_FRACTIONAL_SCALE_AVAILABLE
	if ( m_pFractionalManager != nullptr && m_pViewporter != nullptr ) {
		slot.pViewport = wp_viewporter_get_viewport( m_pViewporter, slot.pSurface );
		if ( slot.pViewport != nullptr ) {
			slot.pFractionalScale = wp_fractional_scale_manager_v1_get_fractional_scale( m_pFractionalManager, slot.pSurface );
			if ( slot.pFractionalScale != nullptr ) {
				static const wp_fractional_scale_v1_listener s_FractionalListener = { fractionalPreferredScale };
				if ( wp_fractional_scale_v1_add_listener( slot.pFractionalScale, &s_FractionalListener, &slot ) != 0 ) {
					wp_fractional_scale_v1_destroy( slot.pFractionalScale );
					slot.pFractionalScale = nullptr;
				}
			}
			if ( slot.pFractionalScale == nullptr ) {
				wp_viewport_destroy( slot.pViewport );
				slot.pViewport = nullptr;
			} else {
				wl_surface_set_buffer_scale( slot.pSurface, 1 );
			}
		}
	}
#endif
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
#ifdef SIR_WAYLAND_FRACTIONAL_SCALE_AVAILABLE
		if ( slot.pFractionalScale != nullptr ) {
			wp_fractional_scale_v1_destroy( slot.pFractionalScale );
		}
		if ( slot.pViewport != nullptr ) {
			wp_viewport_destroy( slot.pViewport );
		}
#endif
		xdg_toplevel_destroy( slot.pToplevel );
		xdg_surface_destroy( slot.pXdgSurface );
		wl_surface_destroy( slot.pSurface );
	} else {
		if ( slot.pFractionalScale != nullptr ) {
			wl_proxy_destroy( ( wl_proxy * )slot.pFractionalScale );
		}
		if ( slot.pViewport != nullptr ) {
			wl_proxy_destroy( ( wl_proxy * )slot.pViewport );
		}
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
	m_DeferredReady[nSlot] = {};
	m_DeferredDpi[nSlot] = {};
	m_DeferredResize[nSlot] = {};
	m_DeferredFocus[nSlot] = {};
	if ( m_pKeyboardFocus == slot.pSurface ) {
		m_pKeyboardFocus = nullptr;
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
	// Initial configure is a lifetime transition. Publish it before any
	// subsequent resize already queued for the same window.
	for ( uint32_t i = 0; i < SIR_WINDOW_MAX_WINDOWS && nDelivered < nBudget; ++i ) {
		if ( m_DeferredReady[i].eKind != SIR_WINDOW_EVENT_READY ) {
			continue;
		}
		const sirWindowBackendEvent_t ready = m_DeferredReady[i];
		m_DeferredReady[i] = {};
		pCallback( pContext, ready );
		++nDelivered;
	}
	while ( m_nEventCount != 0 && nDelivered < nBudget ) {
		pCallback( pContext, m_Events[m_nEventRead] );
		m_nEventRead = ( m_nEventRead + 1 ) % SIR_WINDOW_EVENT_CAPACITY;
		--m_nEventCount;
		++nDelivered;
	}
	for ( uint32_t i = 0; i < SIR_WINDOW_MAX_WINDOWS && nDelivered < nBudget; ++i ) {
		if ( m_DeferredDpi[i].eKind == SIR_WINDOW_EVENT_DPI_CHANGED ) {
			const sirWindowBackendEvent_t dpi = m_DeferredDpi[i];
			m_DeferredDpi[i] = {};
			pCallback( pContext, dpi );
			++nDelivered;
		}
		if ( m_DeferredResize[i].eKind == SIR_WINDOW_EVENT_RESIZED && nDelivered < nBudget ) {
			const sirWindowBackendEvent_t resized = m_DeferredResize[i];
			m_DeferredResize[i] = {};
			pCallback( pContext, resized );
			++nDelivered;
		}
		if ( m_DeferredFocus[i].eKind != 0 && nDelivered < nBudget ) {
			const sirWindowBackendEvent_t focus = m_DeferredFocus[i];
			m_DeferredFocus[i] = {};
			pCallback( pContext, focus );
			++nDelivered;
		}
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
	if ( m_pTextInputLostSink != nullptr ) {
		m_pTextInputLostSink( m_pTextInputLostContext );
	}
	if ( m_pPointerProtocolLostSink != nullptr ) {
		m_pPointerProtocolLostSink( m_pPointerProtocolLostContext );
	}
	if ( m_pSeatLostSink != nullptr ) {
		m_pSeatLostSink( m_pSeatLostContext );
	}
	m_pTextInputLostSink = nullptr;
	m_pTextInputLostContext = nullptr;
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
			if ( m_bLost ) {
				wl_proxy_destroy( ( wl_proxy * )m_Outputs[i].pOutput );
			} else {
				wl_output_destroy( m_Outputs[i].pOutput );
			}
			m_Outputs[i] = {};
		}
	}
#ifdef SIR_WAYLAND_FRACTIONAL_SCALE_AVAILABLE
	if ( m_pFractionalManager != nullptr ) {
		if ( m_bLost ) {
			wl_proxy_destroy( ( wl_proxy * )m_pFractionalManager );
		} else {
			wp_fractional_scale_manager_v1_destroy( m_pFractionalManager );
		}
	}
	if ( m_pViewporter != nullptr ) {
		if ( m_bLost ) {
			wl_proxy_destroy( ( wl_proxy * )m_pViewporter );
		} else {
			wp_viewporter_destroy( m_pViewporter );
		}
	}
#endif
	if ( m_pSeat != nullptr ) {
		if ( m_bLost ) {
			wl_proxy_destroy( ( wl_proxy * )m_pSeat );
		} else if ( wl_proxy_get_version( ( wl_proxy * )m_pSeat ) >= 5 ) {
			wl_seat_release( m_pSeat );
		} else {
			wl_seat_destroy( m_pSeat );
		}
	}
#ifdef SIR_WAYLAND_POINTER_PROTOCOLS_AVAILABLE
	if ( m_pPointerConstraints != nullptr ) {
		if ( m_bLost ) {
			wl_proxy_destroy( ( wl_proxy * )m_pPointerConstraints );
		} else {
			zwp_pointer_constraints_v1_destroy( m_pPointerConstraints );
		}
	}
	if ( m_pRelativePointerManager != nullptr ) {
		if ( m_bLost ) {
			wl_proxy_destroy( ( wl_proxy * )m_pRelativePointerManager );
		} else {
			zwp_relative_pointer_manager_v1_destroy( m_pRelativePointerManager );
		}
	}
#endif
#ifdef SIR_WAYLAND_TEXT_INPUT_AVAILABLE
	if ( m_pTextInputManager != nullptr ) {
		if ( m_bLost ) {
			wl_proxy_destroy( ( wl_proxy * )m_pTextInputManager );
		} else {
			zwp_text_input_manager_v3_destroy( m_pTextInputManager );
		}
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
	m_pFractionalManager = nullptr;
	m_pViewporter = nullptr;
	m_nFractionalGlobal = 0;
	m_nViewporterGlobal = 0;
	m_pSeat = nullptr;
	m_pKeyboardFocus = nullptr;
	m_pBase = nullptr;
	m_nSeatGlobal = 0;
	m_nConstraintsGlobal = 0;
	m_nRelativePointerGlobal = 0;
	m_pPointerConstraints = nullptr;
	m_pRelativePointerManager = nullptr;
	m_pTextInputManager = nullptr;
	m_nTextInputGlobal = 0;
	m_nEventRead = 0;
	m_nEventWrite = 0;
	m_nEventCount = 0;
	for ( uint32_t i = 0; i < SIR_WINDOW_MAX_WINDOWS; ++i ) {
		m_DeferredReady[i] = {};
		m_DeferredDpi[i] = {};
		m_DeferredResize[i] = {};
		m_DeferredFocus[i] = {};
	}
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
