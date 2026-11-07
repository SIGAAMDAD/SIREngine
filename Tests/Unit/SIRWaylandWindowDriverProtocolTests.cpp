/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/
#include <Core/Platform/Window/Linux/SIRWindowDriverWayland.h>
#include <Core/Platform/Input/Linux/SIRWaylandInputBridge.h>
#include <wayland-client.h>
#include <xdg-shell-client-protocol.h>
#include <pointer-constraints-unstable-v1-client-protocol.h>
#include <relative-pointer-unstable-v1-client-protocol.h>
#include <xkbcommon/xkbcommon.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

struct wl_display
{
};
struct wl_registry
{
};
struct wl_compositor
{
};
struct wl_seat
{
};
struct wl_keyboard
{
};
struct wl_pointer
{
};
struct zwp_pointer_constraints_v1
{
};
struct zwp_relative_pointer_manager_v1
{
};
struct zwp_locked_pointer_v1
{
};
struct zwp_relative_pointer_v1
{
};
struct xkb_context
{
};
struct xkb_keymap
{
};
struct xkb_state
{
};
struct wl_surface
{
	uint32_t id;
};
struct xdg_wm_base
{
};
struct xdg_surface
{
	wl_surface *surface;
};
struct xdg_toplevel
{
	xdg_surface *surface;
};

static wl_display s_Display;
static wl_registry s_Registry;
static wl_compositor s_Compositor;
static wl_seat s_Seat;
static wl_keyboard s_Keyboard;
static wl_pointer s_Pointer;
static zwp_pointer_constraints_v1 s_Constraints;
static zwp_relative_pointer_manager_v1 s_RelativeManager;
static zwp_locked_pointer_v1 s_Lock;
static zwp_relative_pointer_v1 s_Relative;
static xkb_context s_XkbContext;
static xkb_keymap s_XkbKeymap;
static xkb_state s_XkbState;
static uint32_t s_nKeymapsCreated = 0;
static uint32_t s_nKeymapsReleased = 0;
static uint32_t s_nXkbMasks = 0;
static xdg_wm_base s_WmBase;
static wl_surface s_Surfaces[8];
static xdg_surface s_XdgSurfaces[8];
static xdg_toplevel s_Toplevels[8];
static const wl_registry_listener *s_pRegistryListener = nullptr;
static void *s_pRegistryContext = nullptr;
static const xdg_surface_listener *s_pSurfaceListener[8] = {};
static void *s_pSurfaceContext[8] = {};
static const xdg_toplevel_listener *s_pToplevelListener[8] = {};
static void *s_pToplevelContext[8] = {};
static uint32_t s_nCreated = 0;
static int s_nDisplayFd = -1;
static uint32_t s_nCancelledReads = 0;
static uint32_t s_nAckCount = 0;
static bool s_bConnectFailure = false;
static bool s_bSurfaceFailure = false;
static bool s_bXdgSurfaceFailure = false;
static bool s_bToplevelFailure = false;
static bool s_bSurfaceListenerFailure = false;
static bool s_bXdgListenerFailure = false;
static bool s_bRegistryFailure = false;
static bool s_bFlushFailure = false;
static bool s_bReadFailure = false;
static bool s_bOldProtocol = false;
static uint32_t s_nWmPings = 0;
static const xdg_wm_base_listener *s_pWmListener = nullptr;
static void *s_pWmContext = nullptr;
static const wl_seat_listener *s_pSeatListener = nullptr;
static void *s_pSeatContext = nullptr;
static const wl_keyboard_listener *s_pKeyboardListener = nullptr;
static void *s_pKeyboardContext = nullptr;
static const wl_pointer_listener *s_pPointerListener = nullptr;
static void *s_pPointerContext = nullptr;
static const zwp_locked_pointer_v1_listener *s_pLockListener = nullptr;
static void *s_pLockContext = nullptr;
static const zwp_relative_pointer_v1_listener *s_pRelativeListener = nullptr;
static void *s_pRelativeContext = nullptr;
static uint32_t s_nLocksCreated = 0;
static uint32_t s_nLocksDestroyed = 0;
static uint32_t s_nRelativeDestroyed = 0;

extern "C" {
const wl_interface wl_compositor_interface = { "wl_compositor" };
const wl_interface wl_seat_interface = { "wl_seat" };
const wl_interface xdg_wm_base_interface = { "xdg_wm_base" };
const wl_interface zwp_pointer_constraints_v1_interface = { "zwp_pointer_constraints_v1" };
const wl_interface zwp_relative_pointer_manager_v1_interface = { "zwp_relative_pointer_manager_v1" };
wl_display *wl_display_connect( const char * )
{
	return s_bConnectFailure ? nullptr : &s_Display;
}
void wl_display_disconnect( wl_display * )
{
}
wl_registry *wl_display_get_registry( wl_display * )
{
	return &s_Registry;
}
int wl_registry_add_listener( wl_registry *, const wl_registry_listener *pListener, void *pContext )
{
	s_pRegistryListener = pListener;
	s_pRegistryContext = pContext;
	return s_bRegistryFailure ? -1 : 0;
}
void *wl_registry_bind( wl_registry *, uint32_t, const wl_interface *pInterface, uint32_t )
{
	if ( strcmp( pInterface->name, "wl_compositor" ) == 0 )
		return &s_Compositor;
	if ( strcmp( pInterface->name, "wl_seat" ) == 0 )
		return &s_Seat;
	if ( strcmp( pInterface->name, "zwp_pointer_constraints_v1" ) == 0 )
		return &s_Constraints;
	if ( strcmp( pInterface->name, "zwp_relative_pointer_manager_v1" ) == 0 )
		return &s_RelativeManager;
	return &s_WmBase;
}
int wl_display_roundtrip( wl_display * )
{
	s_pRegistryListener->global( s_pRegistryContext, &s_Registry, 11, "wl_compositor", 4 );
	s_pRegistryListener->global( s_pRegistryContext, &s_Registry, 12, "xdg_wm_base", 1 );
	s_pRegistryListener->global( s_pRegistryContext, &s_Registry, 13, "wl_seat", 7 );
	s_pRegistryListener->global( s_pRegistryContext, &s_Registry, 15, "zwp_pointer_constraints_v1", 1 );
	s_pRegistryListener->global( s_pRegistryContext, &s_Registry, 16, "zwp_relative_pointer_manager_v1", 1 );
	return 0;
}
int wl_display_prepare_read( wl_display * )
{
	return 0;
}
void wl_display_cancel_read( wl_display * )
{
	++s_nCancelledReads;
}
int wl_display_dispatch_pending( wl_display * )
{
	return 0;
}
int wl_display_flush( wl_display * )
{
	if ( s_bFlushFailure ) {
		errno = EPIPE;
		return -1;
	}
	return 0;
}
int wl_display_get_fd( wl_display * )
{
	return s_nDisplayFd;
}
int wl_display_read_events( wl_display * )
{
	return s_bReadFailure ? -1 : 0;
}
void wl_registry_destroy( wl_registry * )
{
}
void wl_compositor_destroy( wl_compositor * )
{
}
void wl_surface_commit( wl_surface * )
{
}
void wl_surface_destroy( wl_surface * )
{
}
void wl_proxy_destroy( wl_proxy * )
{
}
uint32_t wl_proxy_get_version( wl_proxy * )
{
	return s_bOldProtocol ? 2 : 7;
}
void wl_seat_destroy( wl_seat * )
{
}
void zwp_pointer_constraints_v1_destroy( zwp_pointer_constraints_v1 * )
{
}
void zwp_relative_pointer_manager_v1_destroy( zwp_relative_pointer_manager_v1 * )
{
}
xkb_context *xkb_context_new( int )
{
	return &s_XkbContext;
}
void xkb_context_unref( xkb_context * )
{
}
xkb_keymap *xkb_keymap_new_from_string( xkb_context *, const char *pszKeymap, int, int )
{
	if ( pszKeymap == nullptr || pszKeymap[0] == 0 ) {
		return nullptr;
	}
	++s_nKeymapsCreated;
	return &s_XkbKeymap;
}
void xkb_keymap_unref( xkb_keymap * )
{
	++s_nKeymapsReleased;
}
xkb_state *xkb_state_new( xkb_keymap * )
{
	return &s_XkbState;
}
void xkb_state_unref( xkb_state * )
{
}
uint32_t xkb_state_key_get_utf32( xkb_state *, uint32_t nKey )
{
	return nKey == 38 ? uint32_t( 'a' ) : 0;
}
uint32_t xkb_state_update_mask( xkb_state *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t )
{
	++s_nXkbMasks;
	return 0;
}
zwp_relative_pointer_v1 *zwp_relative_pointer_manager_v1_get_relative_pointer( zwp_relative_pointer_manager_v1 *, wl_pointer * )
{
	return &s_Relative;
}
int zwp_relative_pointer_v1_add_listener( zwp_relative_pointer_v1 *, const zwp_relative_pointer_v1_listener *pListener, void *pContext )
{
	s_pRelativeListener = pListener;
	s_pRelativeContext = pContext;
	return 0;
}
void zwp_relative_pointer_v1_destroy( zwp_relative_pointer_v1 * )
{
	++s_nRelativeDestroyed;
}
zwp_locked_pointer_v1 *zwp_pointer_constraints_v1_lock_pointer( zwp_pointer_constraints_v1 *, wl_surface *, wl_pointer *,
                                                                 void *, uint32_t )
{
	++s_nLocksCreated;
	return &s_Lock;
}
int zwp_locked_pointer_v1_add_listener( zwp_locked_pointer_v1 *, const zwp_locked_pointer_v1_listener *pListener, void *pContext )
{
	s_pLockListener = pListener;
	s_pLockContext = pContext;
	return 0;
}
void zwp_locked_pointer_v1_destroy( zwp_locked_pointer_v1 * )
{
	++s_nLocksDestroyed;
}

void wl_seat_release( wl_seat * )
{
}
int wl_seat_add_listener( wl_seat *, const wl_seat_listener *pListener, void *pContext )
{
	s_pSeatListener = pListener;
	s_pSeatContext = pContext;
	return 0;
}
wl_keyboard *wl_seat_get_keyboard( wl_seat * )
{
	return &s_Keyboard;
}
wl_pointer *wl_seat_get_pointer( wl_seat * )
{
	return &s_Pointer;
}
int wl_keyboard_add_listener( wl_keyboard *, const wl_keyboard_listener *pListener, void *pContext )
{
	s_pKeyboardListener = pListener;
	s_pKeyboardContext = pContext;
	return 0;
}
int wl_pointer_add_listener( wl_pointer *, const wl_pointer_listener *pListener, void *pContext )
{
	s_pPointerListener = pListener;
	s_pPointerContext = pContext;
	return 0;
}
void wl_keyboard_destroy( wl_keyboard * )
{
}
void wl_keyboard_release( wl_keyboard * )
{
}
void wl_pointer_destroy( wl_pointer * )
{
}
void wl_pointer_release( wl_pointer * )
{
}

wl_surface *wl_compositor_create_surface( wl_compositor * )
{
	if ( s_bSurfaceFailure )
		return nullptr;
	if ( s_nCreated == 8 )
		return nullptr;
	const uint32_t nSlot = s_nCreated++;
	s_Surfaces[nSlot].id = nSlot;
	return &s_Surfaces[nSlot];
}
int xdg_wm_base_add_listener( xdg_wm_base *, const xdg_wm_base_listener *pListener, void *pContext )
{
	s_pWmListener = pListener;
	s_pWmContext = pContext;
	return s_bXdgListenerFailure ? -1 : 0;
}
void xdg_wm_base_pong( xdg_wm_base *, uint32_t )
{
	++s_nWmPings;
}
xdg_surface *xdg_wm_base_get_xdg_surface( xdg_wm_base *, wl_surface *pSurface )
{
	if ( s_bXdgSurfaceFailure )
		return nullptr;
	const uint32_t nIndex = pSurface->id;
	s_XdgSurfaces[nIndex].surface = pSurface;
	return &s_XdgSurfaces[nIndex];
}
void xdg_wm_base_destroy( xdg_wm_base * )
{
}
int xdg_surface_add_listener( xdg_surface *pSurface, const xdg_surface_listener *pListener, void *pContext )
{
	const uint32_t nIndex = pSurface->surface->id;
	s_pSurfaceListener[nIndex] = pListener;
	s_pSurfaceContext[nIndex] = pContext;
	return s_bSurfaceListenerFailure ? -1 : 0;
}
xdg_toplevel *xdg_surface_get_toplevel( xdg_surface *pSurface )
{
	if ( s_bToplevelFailure )
		return nullptr;
	const uint32_t nIndex = pSurface->surface->id;
	s_Toplevels[nIndex].surface = pSurface;
	return &s_Toplevels[nIndex];
}
void xdg_surface_ack_configure( xdg_surface *, uint32_t )
{
	++s_nAckCount;
}
void xdg_surface_destroy( xdg_surface * )
{
}
int xdg_toplevel_add_listener( xdg_toplevel *pTop, const xdg_toplevel_listener *pListener, void *pContext )
{
	const uint32_t nIndex = pTop->surface->surface->id;
	s_pToplevelListener[nIndex] = pListener;
	s_pToplevelContext[nIndex] = pContext;
	return 0;
}
void xdg_toplevel_set_title( xdg_toplevel *, const char * )
{
}
void xdg_toplevel_set_minimized( xdg_toplevel * )
{
}
void xdg_toplevel_set_maximized( xdg_toplevel * )
{
}
void xdg_toplevel_destroy( xdg_toplevel * )
{
}
}

typedef struct sirWaylandTestEvents_s
{
	sirWindowBackendEvent_t events[16];
	uint32_t nCount;
} sirWaylandTestEvents_t;

static void sirCollectWaylandEvent( void *pContext, const sirWindowBackendEvent_t& event ) noexcept
{
	sirWaylandTestEvents_t *pEvents = ( sirWaylandTestEvents_t * )pContext;
	if ( pEvents->nCount < 16 ) {
		pEvents->events[pEvents->nCount++] = event;
	}
}

static uint32_t s_nSeatLossCallbacks = 0;
static void sirOnWaylandSeatLost( void *pContext ) noexcept
{
	uint32_t *pCount = ( uint32_t * )pContext;
	++*pCount;
}

static bool sirTestNativeFailureInjection( void ) noexcept
{
	s_bConnectFailure = true;
	SIRWindowDriverWayland noDisplay;
	if ( noDisplay.initialize() != SIR_WINDOW_NOT_READY )
		return false;
	s_bConnectFailure = false;

	s_bRegistryFailure = true;
	SIRWindowDriverWayland noRegistry;
	if ( noRegistry.initialize() != SIR_WINDOW_NATIVE_FAILURE )
		return false;
	s_bRegistryFailure = false;

	s_bXdgListenerFailure = true;
	SIRWindowDriverWayland noShell;
	if ( noShell.initialize() != SIR_WINDOW_NATIVE_FAILURE )
		return false;
	s_bXdgListenerFailure = false;

	s_nCreated = 0;
	s_nWmPings = 0;
	SIRWindowDriverWayland driver;
	if ( driver.initialize() != SIR_WINDOW_OK || driver.initialize() != SIR_WINDOW_BUSY || s_pWmListener == nullptr )
		return false;
	s_pWmListener->ping( s_pWmContext, &s_WmBase, 5 );
	if ( s_nWmPings != 1 )
		return false;
	sirWindowNativeView_t view = {};
	const sirWindowCreateDesc_t desc = { "negative", 640, 480 };
	if ( driver.createWindow( desc, SIR_WINDOW_MAX_WINDOWS, &view ) != SIR_WINDOW_INVALID_ARGUMENT ||
	     driver.createWindow( { nullptr, 0, 0 }, 0, &view ) != SIR_WINDOW_INVALID_ARGUMENT )
		return false;
	s_bSurfaceFailure = true;
	if ( driver.createWindow( desc, 0, &view ) != SIR_WINDOW_NATIVE_FAILURE )
		return false;
	s_bSurfaceFailure = false;
	s_bXdgSurfaceFailure = true;
	if ( driver.createWindow( desc, 0, &view ) != SIR_WINDOW_NATIVE_FAILURE )
		return false;
	s_bXdgSurfaceFailure = false;
	s_bToplevelFailure = true;
	if ( driver.createWindow( desc, 0, &view ) != SIR_WINDOW_NATIVE_FAILURE )
		return false;
	s_bToplevelFailure = false;
	s_bSurfaceListenerFailure = true;
	if ( driver.createWindow( desc, 0, &view ) != SIR_WINDOW_NATIVE_FAILURE )
		return false;
	s_bSurfaceListenerFailure = false;
	if ( driver.createWindow( desc, 0, &view ) != SIR_WINDOW_OK || driver.createWindow( desc, 0, &view ) != SIR_WINDOW_BUSY )
		return false;
	if ( driver.mutateWindow( 0, { SIR_WINDOW_MUTATION_TITLE, nullptr, 0, 0 } ) != SIR_WINDOW_INVALID_ARGUMENT ||
	     driver.mutateWindow( 0, { SIR_WINDOW_MUTATION_TITLE, "new", 0, 0 } ) != SIR_WINDOW_OK ||
	     driver.mutateWindow( 0, { SIR_WINDOW_MUTATION_MINIMIZE, nullptr, 0, 0 } ) != SIR_WINDOW_OK ||
	     driver.mutateWindow( 0, { SIR_WINDOW_MUTATION_MAXIMIZE, nullptr, 0, 0 } ) != SIR_WINDOW_OK ||
	     driver.mutateWindow( 0, { SIR_WINDOW_MUTATION_RESIZE, nullptr, 120, 100 } ) != SIR_WINDOW_UNSUPPORTED ||
	     driver.mutateWindow( 1, { SIR_WINDOW_MUTATION_TITLE, "missing", 0, 0 } ) != SIR_WINDOW_INVALID_HANDLE )
		return false;
	sirWaylandTestEvents_t events = {};
	if ( driver.pumpEvents( 0, sirCollectWaylandEvent, &events ) != SIR_WINDOW_INVALID_ARGUMENT )
		return false;
	s_bFlushFailure = true;
	if ( driver.pumpEvents( 1, sirCollectWaylandEvent, &events ) != SIR_WINDOW_NATIVE_FAILURE )
		return false;
	s_bFlushFailure = false;
	if ( driver.mutateWindow( 0, { SIR_WINDOW_MUTATION_TITLE, "stopped", 0, 0 } ) != SIR_WINDOW_STOPPED )
		return false;
	driver.shutdown();
	if ( driver.pumpEvents( 1, sirCollectWaylandEvent, &events ) != SIR_WINDOW_STOPPED )
		return false;
	return true;
}

static bool sirTestInitialConfigureAndRetirement( void ) noexcept
{
	s_nCreated = 0;
	s_nDisplayFd = -1;
	s_nAckCount = 0;
	SIRWindowDriverWayland driver;
	if ( driver.initialize() != SIR_WINDOW_OK )
		return false;
	sirWindowNativeView_t view = {};
	if ( driver.createWindow( { "configure", 640, 480 }, 0, &view ) != SIR_WINDOW_OK || s_pSurfaceListener[0] == nullptr ||
	     s_pToplevelListener[0] == nullptr )
		return false;
	sirWaylandTestEvents_t events = {};
	if ( driver.pumpEvents( 1, sirCollectWaylandEvent, &events ) != SIR_WINDOW_OK || events.nCount != 0 || s_nAckCount != 0 )
		return false;
	s_pToplevelListener[0]->configure( s_pToplevelContext[0], &s_Toplevels[0], 512, 320, nullptr );
	s_pSurfaceListener[0]->configure( s_pSurfaceContext[0], &s_XdgSurfaces[0], 77 );
	if ( s_nAckCount != 1 || driver.pumpEvents( 1, sirCollectWaylandEvent, &events ) != SIR_WINDOW_OK || events.nCount != 1 ||
	     events.events[0].eKind != SIR_WINDOW_EVENT_READY || events.events[0].nWidth != 512 || events.events[0].nHeight != 320 )
		return false;
	if ( driver.destroyWindow( 0 ) != SIR_WINDOW_OK )
		return false;
	s_pSurfaceListener[0]->configure( s_pSurfaceContext[0], &s_XdgSurfaces[0], 78 );
	if ( s_nAckCount != 1 )
		return false;
	driver.shutdown();
	return true;
}

static bool sirTestBoundedCloseAndLoss( void ) noexcept
{
	s_nCreated = 0;
	s_nDisplayFd = -1;
	s_nCancelledReads = 0;
	SIRWindowDriverWayland driver;
	if ( driver.initialize() != SIR_WINDOW_OK )
		return false;
	const sirWindowCreateDesc_t desc = { "test", 640, 480 };
	sirWindowNativeView_t v0 = {}, v1 = {};
	if ( driver.createWindow( desc, 0, &v0 ) != SIR_WINDOW_OK || driver.createWindow( desc, 1, &v1 ) != SIR_WINDOW_OK )
		return false;
	if ( s_pToplevelListener[0] == nullptr || s_pToplevelListener[1] == nullptr )
		return false;
	s_pToplevelListener[0]->close( s_pToplevelContext[0], &s_Toplevels[0] );
	s_pToplevelListener[1]->close( s_pToplevelContext[1], &s_Toplevels[1] );
	sirWaylandTestEvents_t events = {};
	if ( driver.pumpEvents( 1, sirCollectWaylandEvent, &events ) != SIR_WINDOW_OK || events.nCount != 1 ||
	     events.events[0].eKind != SIR_WINDOW_EVENT_CLOSE_REQUESTED || events.events[0].nSlot != 0 )
		return false;
	if ( driver.pumpEvents( 1, sirCollectWaylandEvent, &events ) != SIR_WINDOW_OK || events.nCount != 2 ||
	     events.events[1].eKind != SIR_WINDOW_EVENT_CLOSE_REQUESTED || events.events[1].nSlot != 1 )
		return false;
	s_pRegistryListener->global_remove( s_pRegistryContext, &s_Registry, 11 );
	if ( driver.pumpEvents( 1, sirCollectWaylandEvent, &events ) != SIR_WINDOW_NATIVE_FAILURE || events.nCount != 3 ||
	     events.events[2].eKind != SIR_WINDOW_EVENT_LOST || events.events[2].nSlot != 0 )
		return false;
	if ( driver.pumpEvents( 1, sirCollectWaylandEvent, &events ) != SIR_WINDOW_NATIVE_FAILURE || events.nCount != 4 ||
	     events.events[3].eKind != SIR_WINDOW_EVENT_LOST || events.events[3].nSlot != 1 )
		return false;
	driver.shutdown();
	return true;
}

static bool sirTestCompositorHangup( void ) noexcept
{
	s_nCreated = 0;
	s_nCancelledReads = 0;
	int fds[2] = {};
	if ( pipe( fds ) != 0 )
		return false;
	s_nDisplayFd = fds[0];
	SIRWindowDriverWayland driver;
	if ( driver.initialize() != SIR_WINDOW_OK ) {
		close( fds[0] );
		close( fds[1] );
		return false;
	}
	const sirWindowCreateDesc_t desc = { "hangup", 640, 480 };
	sirWindowNativeView_t view = {};
	if ( driver.createWindow( desc, 0, &view ) != SIR_WINDOW_OK ) {
		close( fds[0] );
		close( fds[1] );
		return false;
	}
	close( fds[1] );
	sirWaylandTestEvents_t events = {};
	const sirWindowResult_t result = driver.pumpEvents( 1, sirCollectWaylandEvent, &events );
	driver.shutdown();
	close( fds[0] );
	s_nDisplayFd = -1;
	return result == SIR_WINDOW_NATIVE_FAILURE && s_nCancelledReads == 1 && events.nCount == 1 &&
	       events.events[0].eKind == SIR_WINDOW_EVENT_LOST;
}

static bool sirFeedFakeXkbKeymap( void ) noexcept
{
	FILE *pFile = tmpfile();
	if ( pFile == nullptr ) {
		return false;
	}
	const char szKeymap[] = "test-keymap";
	const bool bWritten = fwrite( szKeymap, 1, sizeof( szKeymap ), pFile ) == sizeof( szKeymap ) && fflush( pFile ) == 0;
	const int nFd = bWritten ? dup( fileno( pFile ) ) : -1;
	fclose( pFile );
	if ( nFd < 0 || s_pKeyboardListener == nullptr ) {
		if ( nFd >= 0 ) {
			close( nFd );
		}
		return false;
	}
	s_pKeyboardListener->keymap( s_pKeyboardContext, &s_Keyboard, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, nFd, sizeof( szKeymap ) );
	return fcntl( nFd, F_GETFD ) == -1 && errno == EBADF;
}

static bool sirTestWaylandInputRouting( void ) noexcept
{
	s_nCreated = 0;
	s_nDisplayFd = -1;
	SIRWindowDriverWayland driver;
	if ( driver.initialize() != SIR_WINDOW_OK )
		return false;
	const sirWindowCreateDesc_t desc = { "input", 640, 480 };
	sirWindowNativeView_t view = {};
	if ( driver.createWindow( desc, 0, &view ) != SIR_WINDOW_OK )
		return false;
	view.nWindowGeneration = 1;
	SIRInputService input;
	SIRWaylandInputBridge bridge;
	if ( !bridge.bind( input, driver, view ) || !bridge.isBound() || bridge.registerWindow( view ) )
		return false;
	if ( !input.begin( 1 ) || !bridge.beginFrame() )
		return false;
	if ( s_pSeatListener == nullptr )
		return false;
	s_pSeatListener->capabilities( s_pSeatContext, &s_Seat, WL_SEAT_CAPABILITY_KEYBOARD | WL_SEAT_CAPABILITY_POINTER );
	if ( !bridge.beginFrame() || input.activeDeviceCount() != 2 || s_pKeyboardListener == nullptr || s_pPointerListener == nullptr )
		return false;
	bridge.setTextEnabled( true );
	if ( !sirFeedFakeXkbKeymap() || !sirFeedFakeXkbKeymap() || s_nKeymapsCreated != 2 || s_nKeymapsReleased != 1 ) {
		return false;
	}
	s_pKeyboardListener->modifiers( s_pKeyboardContext, &s_Keyboard, 5, 1, 0, 0, 0 );
	if ( s_nXkbMasks != 1 ) {
		return false;
	}
	s_pKeyboardListener->enter( s_pKeyboardContext, &s_Keyboard, 5, &s_Surfaces[0], nullptr );
	s_pKeyboardListener->key( s_pKeyboardContext, &s_Keyboard, 5, 100, 30, WL_KEYBOARD_KEY_STATE_PRESSED );
	s_pPointerListener->enter( s_pPointerContext, &s_Pointer, 6, &s_Surfaces[0], 0, 0 );
	s_pPointerListener->button( s_pPointerContext, &s_Pointer, 6, 100, 0x110, WL_POINTER_BUTTON_STATE_PRESSED );
	s_pPointerListener->motion( s_pPointerContext, &s_Pointer, 100, 512, 256 );
	input.sample();
	if ( !input.publish() )
		return false;
	const sirInputFrame_t *pFrame = input.frame();
	if ( pFrame == nullptr || pFrame->nCount < 3 || input.text().snapshot().nCount != 1 ||
	     input.text().snapshot().szCodepoints[0] != uint32_t( 'a' ) ) {
		return false;
	}
	uint32_t nKey = 0, nMouse = 0, nMove = 0;
	for ( uint32_t i = 0; i < pFrame->nCount; ++i ) {
		const sirInputEvent_t& event = pFrame->events[i];
		if ( event.eClass == SIR_INPUT_KEYBOARD && event.eSignal == SIR_INPUT_BUTTON && event.nControl == 38 && event.nValue == 1 )
			++nKey;
		if ( event.eClass == SIR_INPUT_POINTER && event.eSignal == SIR_INPUT_BUTTON && event.nControl == 0 && event.nValue == 1 )
			++nMouse;
		if ( event.eClass == SIR_INPUT_POINTER && event.eSignal == SIR_INPUT_MOTION && event.nValue == 2 && event.nValueY == 1 )
			++nMove;
	}
	if ( nKey != 1 || nMouse != 1 || nMove != 1 )
		return false;
	bridge.setTextEnabled( true );
	bridge.resynchronize( input );
	if ( bridge.vibrate( {}, 10, 10 ) )
		return false;
	// One physical keyboard/pointer pair must route across several leased surfaces.
	sirWindowNativeView_t secondView = {};
	if ( driver.createWindow( desc, 1, &secondView ) != SIR_WINDOW_OK )
		return false;
	secondView.nWindowGeneration = 2;
	if ( !bridge.registerWindow( secondView ) || bridge.registerWindow( secondView ) )
		return false;
	sirWindowNativeView_t foreign = secondView;
	foreign.nDisplayOrInstance = 17;
	if ( bridge.registerWindow( foreign ) )
		return false;
	if ( !input.begin( 2 ) || !bridge.beginFrame() )
		return false;
	uint32_t nInvalidKeys[1] = { UINT32_MAX };
	wl_array invalidKeys = { sizeof( nInvalidKeys ), sizeof( nInvalidKeys ), nInvalidKeys };
	s_pKeyboardListener->enter( s_pKeyboardContext, &s_Keyboard, 8, &s_Surfaces[1], &invalidKeys );
	s_pKeyboardListener->key( s_pKeyboardContext, &s_Keyboard, 8, 100, UINT32_MAX, WL_KEYBOARD_KEY_STATE_PRESSED );
	s_pKeyboardListener->key( s_pKeyboardContext, &s_Keyboard, 8, 100, 20, WL_KEYBOARD_KEY_STATE_PRESSED );
	s_pPointerListener->enter( s_pPointerContext, &s_Pointer, 9, &s_Surfaces[1], 0, 0 );
	if ( !bridge.setCursorMode( SIR_INPUT_CURSOR_CAPTURED ) || bridge.cursorMode() != SIR_INPUT_CURSOR_CAPTURED ||
	     s_pLockListener == nullptr || s_pRelativeListener == nullptr ) {
		return false;
	}
	const uint32_t nLocksBeforeRelease = s_nLocksCreated;
	s_pLockListener->locked( s_pLockContext, &s_Lock );
	s_pRelativeListener->relative_motion( s_pRelativeContext, &s_Relative, 0, 10, 512, 256, 512, 256 );
	s_pPointerListener->motion( s_pPointerContext, &s_Pointer, 100, 1024, 768 );
	if ( !bridge.setCursorMode( SIR_INPUT_CURSOR_FREE ) || s_nLocksDestroyed != nLocksBeforeRelease ||
	     s_nRelativeDestroyed != nLocksBeforeRelease ) {
		return false;
	}
	s_pPointerListener->axis( s_pPointerContext, &s_Pointer, 100, WL_POINTER_AXIS_VERTICAL_SCROLL, 256 );
	for ( uint32_t n = 0; n < 8; ++n ) {
		s_pPointerListener->axis( s_pPointerContext, &s_Pointer, 100, WL_POINTER_AXIS_VERTICAL_SCROLL, 20 );
	}
	if ( !input.publish() )
		return false;
	pFrame = input.frame();
	uint32_t nSecondKey = 0, nWheel = 0, nUnexpected = 0, nKeyboardResync = 0, nPointerResync = 0;
	uint32_t nRelativeMotion = 0;
	int32_t nFractionalWheel = 0;
	for ( uint32_t i = 0; i < pFrame->nCount; ++i ) {
		const sirInputEvent_t& e = pFrame->events[i];
		if ( e.eClass == SIR_INPUT_KEYBOARD && e.eSignal == SIR_INPUT_BUTTON && e.nControl == 28 && e.nValue == 1 )
			++nSecondKey;
		if ( e.eClass == SIR_INPUT_POINTER && e.eSignal == SIR_INPUT_WHEEL && e.nValue == -12 )
			++nWheel;
		if ( e.eClass == SIR_INPUT_POINTER && e.eSignal == SIR_INPUT_WHEEL && e.nValue != -12 )
			nFractionalWheel += e.nValue;
		if ( e.eClass == SIR_INPUT_KEYBOARD && e.eSignal == SIR_INPUT_BUTTON && e.nControl == 7 )
			++nUnexpected;
		if ( e.eClass == SIR_INPUT_KEYBOARD && e.eSignal == SIR_INPUT_RESYNC )
			++nKeyboardResync;
		if ( e.eClass == SIR_INPUT_POINTER && e.eSignal == SIR_INPUT_RESYNC )
			++nPointerResync;
		if ( e.eClass == SIR_INPUT_POINTER && e.eSignal == SIR_INPUT_MOTION && e.nValue == 2 && e.nValueY == 1 )
			++nRelativeMotion;
	}
	if ( nSecondKey != 1 || nWheel != 1 || nFractionalWheel != -7 || nUnexpected != 0 || nKeyboardResync != 1 ||
	     nPointerResync != 1 || nRelativeMotion != 1 || input.activeDeviceCount() != 2 )
		return false;
	if ( !input.begin( 3 ) || !bridge.beginFrame() )
		return false;
	bridge.unregisterWindow( secondView.nNativeWindow );
	if ( !input.publish() )
		return false;
	if ( !input.begin( 4 ) || !bridge.beginFrame() )
		return false;
	s_pKeyboardListener->leave( s_pKeyboardContext, &s_Keyboard, 7, &s_Surfaces[0] );
	s_pPointerListener->leave( s_pPointerContext, &s_Pointer, 7, &s_Surfaces[0] );
	s_pSeatListener->capabilities( s_pSeatContext, &s_Seat, 0 );
	if ( input.activeDeviceCount() != 0 || !input.publish() )
		return false;
	// A removed compositor seat must not terminate the active input loop.
	s_pRegistryListener->global_remove( s_pRegistryContext, &s_Registry, 13 );
	if ( driver.borrowSeat() != nullptr || !input.begin( 5 ) || !bridge.beginFrame() || !input.publish() )
		return false;
	// New seat globals must reattach the single canonical input backend.
	s_pRegistryListener->global( s_pRegistryContext, &s_Registry, 14, "wl_seat", 7 );
	if ( !input.begin( 6 ) || !bridge.beginFrame() || driver.borrowSeat() == nullptr )
		return false;
	s_pSeatListener->capabilities( s_pSeatContext, &s_Seat, WL_SEAT_CAPABILITY_KEYBOARD | WL_SEAT_CAPABILITY_POINTER );
	if ( input.activeDeviceCount() != 2 || !input.publish() )
		return false;
	bridge.unbind();
	if ( s_nKeymapsReleased != s_nKeymapsCreated ) {
		return false;
	}
	input.shutdown();
	driver.shutdown();
	return true;
}

int main( void )
{
	if ( !sirTestNativeFailureInjection() ) {
		fputs( "Wayland native failure injection failed\n", stderr );
		return 1;
	}
	if ( !sirTestInitialConfigureAndRetirement() ) {
		fputs( "Wayland initial xdg configure and stale-ack rejection failed\n", stderr );
		return 1;
	}
	if ( !sirTestBoundedCloseAndLoss() ) {
		fputs( "Wayland bounded close/loss event budget failed\n", stderr );
		return 1;
	}
	if ( !sirTestCompositorHangup() ) {
		fputs( "Wayland compositor hangup failed\n", stderr );
		return 1;
	}
	if ( !sirTestWaylandInputRouting() ) {
		fputs( "Wayland native keyboard/pointer routing failed\n", stderr );
		return 1;
	}
	return 0;
}
