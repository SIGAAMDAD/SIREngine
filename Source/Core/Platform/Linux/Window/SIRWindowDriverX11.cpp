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

#include <Core/Platform/Linux/Window/SIRWindowDriverX11.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <string.h>
#include <poll.h>
#include <sys/socket.h>
#include <setjmp.h>
#include <errno.h>
#include <Core/Platform/Shared/Threading/SIRSync.h>
#include <Core/Platform/Shared/Threading/SIRScopedLock.h>
#ifdef SIR_WINDOW_XRANDR_AVAILABLE
#include <X11/extensions/Xrandr.h>
#endif

static SIRMutex s_X11IOHandlerMutex;
static uint32_t s_nX11HandlerUsers = 0;
static XIOErrorHandler s_pPreviousX11IOHandler = nullptr;
static thread_local sigjmp_buf *s_pX11RecoveryPoint = nullptr;
static thread_local Display *s_pX11RecoveryDisplay = nullptr;

// Xlib's default behavior is process termination. The recovery point is
// restricted to native driver calls; never re-enter the failed Display.
static int sirX11IOErrorRecovery( Display *pDisplay )
{
	if ( pDisplay == s_pX11RecoveryDisplay && s_pX11RecoveryPoint != nullptr ) {
		siglongjmp( *s_pX11RecoveryPoint, 1 );
	}
	return s_pPreviousX11IOHandler != nullptr ? s_pPreviousX11IOHandler( pDisplay ) : 0;
}

static void sirX11ClearRecovery( void ) noexcept
{
	s_pX11RecoveryPoint = nullptr;
	s_pX11RecoveryDisplay = nullptr;
}

// Set the ICCCM name and UTF-8 EWMH name for window managers and IME users.
static sirWindowResult sirSetX11Title( Display *pDisplay, Window hWindow, const char *pszTitle ) noexcept
{
	if ( pszTitle == nullptr || strlen( pszTitle ) > 0x7fffffffU ) {
		return sirWindowResult::SIR_WINDOW_INVALID_ARGUMENT;
	}
	if ( XStoreName( pDisplay, hWindow, pszTitle ) == 0 ) {
		return sirWindowResult::SIR_WINDOW_NATIVE_FAILURE;
	}
	const Atom hName = XInternAtom( pDisplay, "_NET_WM_NAME", False );
	const Atom hUtf8 = XInternAtom( pDisplay, "UTF8_STRING", False );
	if ( hName != None && hUtf8 != None ) {
		XChangeProperty( pDisplay, hWindow, hName, hUtf8, 8, PropModeReplace, reinterpret_cast<const unsigned char *>(pszTitle),
		                 static_cast<int>(strlen( pszTitle ) ));
	}
	return sirWindowResult::SIR_WINDOW_OK;
}

bool SIRWindowDriverX11::setEventSink( sirWindowX11EventSink_t pSink, void *pContext ) noexcept
{
	if ( pSink != nullptr && pContext == nullptr ) {
		return false;
	}
	m_pSink = pSink;
	m_pSinkContext = pContext;
	return true;
}

SIRWindowDriverX11::~SIRWindowDriverX11( void ) noexcept
{
	shutdown();
}

sirWindowResult SIRWindowDriverX11::initialize( void ) noexcept
{
	if ( m_pDisplay != nullptr ) {
		return sirWindowResult::SIR_WINDOW_BUSY;
	}
	Display *pDisplay = nullptr;
	for ( uint32_t nAttempt = 0; nAttempt < 10; ++nAttempt ) {
		pDisplay = XOpenDisplay( nullptr );
		// Some X servers reset new connections while reaping a recently
		// disconnected client. Only transient ECONNRESET is retried.
		if ( pDisplay != nullptr || errno != ECONNRESET ) {
			break;
		}
		if ( nAttempt != 9 ) {
			static_cast<void>(poll( nullptr, 0, 5 ));
		}
	}
	if ( pDisplay == nullptr ) {
		return sirWindowResult::SIR_WINDOW_NOT_READY;
	}
	m_pDisplay = pDisplay;
	m_bServerLost = false;
	m_nLossPendingMask = 0;
	{
		SIRScopedLock<SIRMutex> lock( s_X11IOHandlerMutex );
		if ( s_nX11HandlerUsers == 0 ) {
			s_pPreviousX11IOHandler = XSetIOErrorHandler( sirX11IOErrorRecovery );
		}
		++s_nX11HandlerUsers;
		m_bHandlerRegistered = true;
	}
	sigjmp_buf recovery;
	if ( sigsetjmp( recovery, 1 ) != 0 ) {
		sirX11ClearRecovery();
		markServerLost();
		return sirWindowResult::SIR_WINDOW_NATIVE_FAILURE;
	}
	s_pX11RecoveryDisplay = pDisplay;
	s_pX11RecoveryPoint = &recovery;
	m_nDeleteWindow = static_cast<uintptr_t>(XInternAtom( pDisplay, "WM_DELETE_WINDOW", False ));
	m_nProtocols = static_cast<uintptr_t>(XInternAtom( pDisplay, "WM_PROTOCOLS", False ));
	if ( m_nDeleteWindow == 0 || m_nProtocols == 0 ) {
		sirX11ClearRecovery();
		shutdown();
		return sirWindowResult::SIR_WINDOW_NATIVE_FAILURE;
	}
	m_nRootWindow = static_cast<uintptr_t>(DefaultRootWindow( pDisplay ));
	XWindowAttributes attributes {};
	if ( !XGetWindowAttributes( pDisplay, static_cast<Window>(m_nRootWindow), &attributes ) ) {
		sirX11ClearRecovery();
		shutdown();
		return sirWindowResult::SIR_WINDOW_NATIVE_FAILURE;
	}
	m_nServerWidth = static_cast<uint32_t>(attributes.width);
	m_nServerHeight = static_cast<uint32_t>(attributes.height);
	XSelectInput( pDisplay, static_cast<Window>(m_nRootWindow), StructureNotifyMask );
#ifdef SIR_WINDOW_XRANDR_AVAILABLE
	int nRandrEvent = 0, nRandrError = 0;
	if ( XRRQueryExtension( pDisplay, &nRandrEvent, &nRandrError ) ) {
		int nMajor = 1, nMinor = 0;
		if ( XRRQueryVersion( pDisplay, &nMajor, &nMinor ) ) {
			const int nMask = RRScreenChangeNotifyMask |
			                  ( nMajor > 1 || ( nMajor == 1 && nMinor >= 2 ) ? RROutputChangeNotifyMask | RRCrtcChangeNotifyMask : 0 );
			XRRSelectInput( pDisplay, static_cast<Window>(m_nRootWindow), nMask );
			m_nRandrEventBase = nRandrEvent;
		}
	}
#endif
	sirX11ClearRecovery();
	return sirWindowResult::SIR_WINDOW_OK;
}

void SIRWindowDriverX11::markServerLost( void ) noexcept
{
	if ( m_bServerLost ) {
		return;
	}
	m_bServerLost = true;
	for ( uint32_t i = 0; i < SIR_WINDOW_MAX_WINDOWS; ++i ) {
		if ( m_hWindows[i] != 0 ) {
			m_nLostNative[i] = m_hWindows[i];
			m_nLossPendingMask |= UINT32_C( 1 ) << i;
			m_hWindows[i] = 0;
			m_bLost[i] = true;
		}
	}
	m_nDisplayChangePendingMask = 0;
}

bool SIRWindowDriverX11::transportAlive( void ) noexcept
{
	if ( m_bServerLost || m_pDisplay == nullptr ) {
		return false;
	}
	const int nSocket = ConnectionNumber( static_cast<Display *>(m_pDisplay) );
	if ( nSocket < 0 ) {
		markServerLost();
		return false;
	}
	pollfd descriptor {};
	descriptor.fd = nSocket;
	descriptor.events = POLLIN;
	const int nPoll = poll( &descriptor, 1, 0 );
	if ( ( nPoll < 0 && errno != EINTR ) || ( nPoll > 0 && ( descriptor.revents & ( POLLHUP | POLLERR | POLLNVAL ) ) != 0 ) ) {
		markServerLost();
		return false;
	}
	if ( nPoll > 0 && ( descriptor.revents & POLLIN ) != 0 ) {
		char byte = 0;
		const ssize_t nPeek = recv( nSocket, &byte, 1, MSG_PEEK | MSG_DONTWAIT );
		if ( nPeek == 0 || ( nPeek < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR ) ) {
			markServerLost();
			return false;
		}
	}
	return true;
}

sirWindowResult SIRWindowDriverX11::emitServerLoss( uint32_t nBudget, sirWindowDriverEventFn_t pCallback, void *pContext ) noexcept
{
	if ( nBudget == 0 || pCallback == nullptr ) {
		return sirWindowResult::SIR_WINDOW_INVALID_ARGUMENT;
	}
	for ( uint32_t i = 0; i < SIR_WINDOW_MAX_WINDOWS && nBudget != 0; ++i ) {
		if ( ( m_nLossPendingMask & ( UINT32_C( 1 ) << i ) ) == 0 ) {
			continue;
		}
		m_nLossPendingMask &= ~( UINT32_C( 1 ) << i );
		const sirWindowBackendEvent_t lost = { i, m_nLostNative[i], sirWindowEventKind::SIR_WINDOW_EVENT_LOST, 0, 0 };
		pCallback( pContext, lost );
		--nBudget;
	}
	return sirWindowResult::SIR_WINDOW_NATIVE_FAILURE;
}

sirWindowResult SIRWindowDriverX11::createWindow( const sirWindowCreateDesc_t& desc, uint32_t nSlot,
                                                    sirWindowNativeView_t *pView ) noexcept
{
	if ( m_bServerLost ) {
		return sirWindowResult::SIR_WINDOW_STOPPED;
	}
	if ( !transportAlive() ) {
		return sirWindowResult::SIR_WINDOW_NATIVE_FAILURE;
	}
	sigjmp_buf recovery;
	if ( sigsetjmp( recovery, 1 ) != 0 ) {
		sirX11ClearRecovery();
		markServerLost();
		return sirWindowResult::SIR_WINDOW_NATIVE_FAILURE;
	}
	s_pX11RecoveryDisplay = static_cast<Display *>(m_pDisplay);
	s_pX11RecoveryPoint = &recovery;
	const sirWindowResult eResult = createWindowNative( desc, nSlot, pView );
	sirX11ClearRecovery();
	return eResult;
}

sirWindowResult SIRWindowDriverX11::createWindowNative( const sirWindowCreateDesc_t& desc, uint32_t nSlot,
                                                          sirWindowNativeView_t *pView ) noexcept
{
	if ( nSlot >= SIR_WINDOW_MAX_WINDOWS || pView == nullptr || desc.pszTitle == nullptr || desc.nWidth == 0 || desc.nHeight == 0 ) {
		return sirWindowResult::SIR_WINDOW_INVALID_ARGUMENT;
	}
	Display *pDisplay = static_cast<Display *>(m_pDisplay);
	if ( pDisplay == nullptr ) {
		return sirWindowResult::SIR_WINDOW_STOPPED;
	}
	if ( m_hWindows[nSlot] != 0 ) {
		return sirWindowResult::SIR_WINDOW_BUSY;
	}
	const int nScreen = DefaultScreen( pDisplay );
	Window hWindow = XCreateSimpleWindow( pDisplay, RootWindow( pDisplay, nScreen ), 0, 0, desc.nWidth, desc.nHeight, 0, 0, 0 );
	if ( hWindow == 0 ) {
		return sirWindowResult::SIR_WINDOW_NATIVE_FAILURE;
	}
	XSelectInput( pDisplay, hWindow, StructureNotifyMask | FocusChangeMask );
	Atom hDelete = static_cast<Atom>(m_nDeleteWindow);
	if ( !XSetWMProtocols( pDisplay, hWindow, &hDelete, 1 ) || sirSetX11Title( pDisplay, hWindow, desc.pszTitle ) != sirWindowResult::SIR_WINDOW_OK ) {
		static_cast<void>(XDestroyWindow( pDisplay, hWindow ));
		return sirWindowResult::SIR_WINDOW_NATIVE_FAILURE;
	}
	static_cast<void>(XMapWindow( pDisplay, hWindow ));
	static_cast<void>(XFlush( pDisplay ));
	m_hWindows[nSlot] = static_cast<uintptr_t>(hWindow);
	m_bLost[nSlot] = false;
	m_nDisplayChangePendingMask &= ~( UINT32_C( 1 ) << nSlot );
	*pView = { sirWindowPlatform::SIR_WINDOW_PLATFORM_X11, static_cast<uintptr_t>(hWindow), reinterpret_cast<uintptr_t>(pDisplay), 0 };
	return sirWindowResult::SIR_WINDOW_OK;
}

sirWindowResult SIRWindowDriverX11::destroyWindow( uint32_t nSlot ) noexcept
{
	if ( m_bServerLost ) {
		if ( nSlot >= SIR_WINDOW_MAX_WINDOWS || !m_bLost[nSlot] ) {
			return sirWindowResult::SIR_WINDOW_INVALID_HANDLE;
		}
		m_bLost[nSlot] = false;
		m_nLossPendingMask &= ~( UINT32_C( 1 ) << nSlot );
		m_nLostNative[nSlot] = 0;
		return sirWindowResult::SIR_WINDOW_OK;
	}
	if ( !transportAlive() ) {
		return sirWindowResult::SIR_WINDOW_NATIVE_FAILURE;
	}
	sigjmp_buf recovery;
	if ( sigsetjmp( recovery, 1 ) != 0 ) {
		sirX11ClearRecovery();
		markServerLost();
		return sirWindowResult::SIR_WINDOW_NATIVE_FAILURE;
	}
	s_pX11RecoveryDisplay = static_cast<Display *>(m_pDisplay);
	s_pX11RecoveryPoint = &recovery;
	const sirWindowResult eResult = destroyWindowNative( nSlot );
	sirX11ClearRecovery();
	return eResult;
}

sirWindowResult SIRWindowDriverX11::destroyWindowNative( uint32_t nSlot ) noexcept
{
	if ( m_pDisplay == nullptr ) {
		return sirWindowResult::SIR_WINDOW_STOPPED;
	}
	if ( nSlot >= SIR_WINDOW_MAX_WINDOWS || ( m_hWindows[nSlot] == 0 && !m_bLost[nSlot] ) ) {
		return sirWindowResult::SIR_WINDOW_INVALID_HANDLE;
	}
	const Window hWindow = static_cast<Window>(m_hWindows[nSlot]);
	m_hWindows[nSlot] = 0;
	m_bLost[nSlot] = false;
	m_nDisplayChangePendingMask &= ~( UINT32_C( 1 ) << nSlot );
	if ( hWindow != 0 ) {
		static_cast<void>(XDestroyWindow( static_cast<Display *>(m_pDisplay), hWindow ));
	}
	static_cast<void>(XFlush( static_cast<Display *>(m_pDisplay) ));
	return sirWindowResult::SIR_WINDOW_OK;
}

sirWindowResult SIRWindowDriverX11::mutateWindow( uint32_t nSlot, const sirWindowMutation_t& mutation ) noexcept
{
	if ( m_bServerLost ) {
		return sirWindowResult::SIR_WINDOW_STOPPED;
	}
	if ( !transportAlive() ) {
		return sirWindowResult::SIR_WINDOW_NATIVE_FAILURE;
	}
	sigjmp_buf recovery;
	if ( sigsetjmp( recovery, 1 ) != 0 ) {
		sirX11ClearRecovery();
		markServerLost();
		return sirWindowResult::SIR_WINDOW_NATIVE_FAILURE;
	}
	s_pX11RecoveryDisplay = static_cast<Display *>(m_pDisplay);
	s_pX11RecoveryPoint = &recovery;
	const sirWindowResult eResult = mutateWindowNative( nSlot, mutation );
	sirX11ClearRecovery();
	return eResult;
}

sirWindowResult SIRWindowDriverX11::mutateWindowNative( uint32_t nSlot, const sirWindowMutation_t& mutation ) noexcept
{
	Display *pDisplay = static_cast<Display *>(m_pDisplay);
	if ( pDisplay == nullptr ) {
		return sirWindowResult::SIR_WINDOW_STOPPED;
	}
	if ( nSlot >= SIR_WINDOW_MAX_WINDOWS || m_hWindows[nSlot] == 0 ) {
		return sirWindowResult::SIR_WINDOW_INVALID_HANDLE;
	}
	const Window hWindow = static_cast<Window>(m_hWindows[nSlot]);
	switch ( mutation.eKind ) {
	case sirWindowMutationKind::SIR_WINDOW_MUTATION_TITLE: {
		if ( mutation.pszTitle == nullptr ) {
			return sirWindowResult::SIR_WINDOW_INVALID_ARGUMENT;
		}
		const sirWindowResult eTitle = sirSetX11Title( pDisplay, hWindow, mutation.pszTitle );
		if ( eTitle != sirWindowResult::SIR_WINDOW_OK ) {
			return eTitle;
		}
		break;
	}
	case sirWindowMutationKind::SIR_WINDOW_MUTATION_RESIZE:
		if ( mutation.nWidth == 0 || mutation.nHeight == 0 ) {
			return sirWindowResult::SIR_WINDOW_INVALID_ARGUMENT;
		}
		static_cast<void>(XResizeWindow( pDisplay, hWindow, mutation.nWidth, mutation.nHeight ));
		break;
	case sirWindowMutationKind::SIR_WINDOW_MUTATION_SHOW:
	case sirWindowMutationKind::SIR_WINDOW_MUTATION_RESTORE:
		static_cast<void>(XMapRaised( pDisplay, hWindow ));
		break;
	case sirWindowMutationKind::SIR_WINDOW_MUTATION_HIDE:
		static_cast<void>(XUnmapWindow( pDisplay, hWindow ));
		break;
	case sirWindowMutationKind::SIR_WINDOW_MUTATION_MINIMIZE:
		if ( !XIconifyWindow( pDisplay, hWindow, DefaultScreen( pDisplay ) ) ) {
			return sirWindowResult::SIR_WINDOW_UNSUPPORTED;
		}
		break;
	case sirWindowMutationKind::SIR_WINDOW_MUTATION_FULLSCREEN_ENTER:
	case sirWindowMutationKind::SIR_WINDOW_MUTATION_FULLSCREEN_EXIT: {
		const Atom hState = XInternAtom( pDisplay, "_NET_WM_STATE", False );
		const Atom hFullscreen = XInternAtom( pDisplay, "_NET_WM_STATE_FULLSCREEN", False );
		if ( hState == None || hFullscreen == None ) {
			return sirWindowResult::SIR_WINDOW_NATIVE_FAILURE;
		}
		XEvent event {};
		event.xclient.type = ClientMessage;
		event.xclient.display = pDisplay;
		event.xclient.window = hWindow;
		event.xclient.message_type = hState;
		event.xclient.format = 32;
		event.xclient.data.l[0] = mutation.eKind == sirWindowMutationKind::SIR_WINDOW_MUTATION_FULLSCREEN_ENTER ? 1 : 0;
		event.xclient.data.l[1] = static_cast<long>(hFullscreen);
		event.xclient.data.l[3] = 1;
		if ( !XSendEvent( pDisplay, DefaultRootWindow( pDisplay ), False, SubstructureNotifyMask | SubstructureRedirectMask, &event ) ) {
			return sirWindowResult::SIR_WINDOW_NATIVE_FAILURE;
		}
		break;
	}
	case sirWindowMutationKind::SIR_WINDOW_MUTATION_MAXIMIZE: {
		const Atom hState = XInternAtom( pDisplay, "_NET_WM_STATE", False );
		const Atom hVertical = XInternAtom( pDisplay, "_NET_WM_STATE_MAXIMIZED_VERT", False );
		const Atom hHorizontal = XInternAtom( pDisplay, "_NET_WM_STATE_MAXIMIZED_HORZ", False );
		if ( hState == None || hVertical == None || hHorizontal == None ) {
			return sirWindowResult::SIR_WINDOW_NATIVE_FAILURE;
		}
		XEvent message {};
		message.xclient.type = ClientMessage;
		message.xclient.display = pDisplay;
		message.xclient.window = hWindow;
		message.xclient.message_type = hState;
		message.xclient.format = 32;
		message.xclient.data.l[0] = 1; // _NET_WM_STATE_ADD
		message.xclient.data.l[1] = static_cast<long>(hVertical);
		message.xclient.data.l[2] = static_cast<long>(hHorizontal);
		message.xclient.data.l[3] = 1; // Client application initiated
		if ( !XSendEvent( pDisplay, DefaultRootWindow( pDisplay ), False, SubstructureNotifyMask | SubstructureRedirectMask, &message ) ) {
			return sirWindowResult::SIR_WINDOW_NATIVE_FAILURE;
		}
		break;
	}
	default:
		return sirWindowResult::SIR_WINDOW_UNSUPPORTED;
	}
	XFlush( pDisplay );
	return sirWindowResult::SIR_WINDOW_OK;
}

sirWindowResult SIRWindowDriverX11::pumpEvents( uint32_t nBudget, sirWindowDriverEventFn_t pCallback, void *pContext ) noexcept
{
	if ( m_bServerLost ) {
		return emitServerLoss( nBudget, pCallback, pContext );
	}
	if ( !transportAlive() ) {
		return emitServerLoss( nBudget, pCallback, pContext );
	}
	sigjmp_buf recovery;
	if ( sigsetjmp( recovery, 1 ) != 0 ) {
		sirX11ClearRecovery();
		markServerLost();
		return emitServerLoss( nBudget, pCallback, pContext );
	}
	s_pX11RecoveryDisplay = static_cast<Display *>(m_pDisplay);
	s_pX11RecoveryPoint = &recovery;
	const sirWindowResult eResult = pumpEventsNative( nBudget, pCallback, pContext );
	sirX11ClearRecovery();
	return eResult;
}

sirWindowResult SIRWindowDriverX11::pumpEventsNative( uint32_t nBudget, sirWindowDriverEventFn_t pCallback, void *pContext ) noexcept
{
	Display *pDisplay = static_cast<Display *>(m_pDisplay);
	if ( pDisplay == nullptr ) {
		return sirWindowResult::SIR_WINDOW_STOPPED;
	}
	if ( nBudget == 0 || pCallback == nullptr ) {
		return sirWindowResult::SIR_WINDOW_INVALID_ARGUMENT;
	}
	uint32_t nEmitted = 0;
	for ( uint32_t i = 0; i < nBudget && XPending( pDisplay ) > 0; ++i ) {
		XEvent event {};
		XNextEvent( pDisplay, &event );
		if ( m_pSink != nullptr ) {
			m_pSink( event, m_pSinkContext );
		}
#ifdef SIR_WINDOW_XRANDR_AVAILABLE
		if ( m_nRandrEventBase >= 0 &&
		     ( event.type == m_nRandrEventBase + RRScreenChangeNotify || event.type == m_nRandrEventBase + RRNotify ) ) {
			if ( event.type == m_nRandrEventBase + RRScreenChangeNotify ) {
				static_cast<void>(XRRUpdateConfiguration( &event ));
			}
			for ( uint32_t j = 0; j < SIR_WINDOW_MAX_WINDOWS; ++j ) {
				if ( m_hWindows[j] != 0 ) {
					m_nDisplayChangePendingMask |= UINT32_C( 1 ) << j;
				}
			}
			continue;
		}
#endif
		if ( event.type == ConfigureNotify && static_cast<uintptr_t>(event.xconfigure.window) == m_nRootWindow ) {
			if ( event.xconfigure.width > 0 && event.xconfigure.height > 0 &&
			     ( static_cast<uint32_t>(event.xconfigure.width) != m_nServerWidth || static_cast<uint32_t>(event.xconfigure.height) != m_nServerHeight ) ) {
				m_nServerWidth = static_cast<uint32_t>(event.xconfigure.width);
				m_nServerHeight = static_cast<uint32_t>(event.xconfigure.height);
				for ( uint32_t j = 0; j < SIR_WINDOW_MAX_WINDOWS; ++j ) {
					if ( m_hWindows[j] != 0 ) {
						m_nDisplayChangePendingMask |= UINT32_C( 1 ) << j;
					}
				}
			}
			continue;
		}
		uint32_t nSlot = SIR_WINDOW_MAX_WINDOWS;
		for ( uint32_t j = 0; j < SIR_WINDOW_MAX_WINDOWS; ++j ) {
			if ( m_hWindows[j] != 0 && m_hWindows[j] == static_cast<uintptr_t>(event.xany.window) ) {
				nSlot = j;
				break;
			}
		}
		if ( nSlot == SIR_WINDOW_MAX_WINDOWS ) {
			continue;
		}
		sirWindowBackendEvent_t emitted = { nSlot, static_cast<uintptr_t>(event.xany.window), sirWindowEventKind::SIR_WINDOW_EVENT_READY, 0, 0 };
		if ( event.type == ConfigureNotify ) {
			emitted.eKind = sirWindowEventKind::SIR_WINDOW_EVENT_RESIZED;
			emitted.nWidth = static_cast<uint32_t>(event.xconfigure.width);
			emitted.nHeight = static_cast<uint32_t>(event.xconfigure.height);
		} else if ( event.type == MapNotify ) {
			emitted.eKind = sirWindowEventKind::SIR_WINDOW_EVENT_SHOWN;
		} else if ( event.type == UnmapNotify ) {
			emitted.eKind = sirWindowEventKind::SIR_WINDOW_EVENT_HIDDEN;
		} else if ( event.type == FocusIn ) {
			emitted.eKind = sirWindowEventKind::SIR_WINDOW_EVENT_FOCUS_GAINED;
		} else if ( event.type == FocusOut ) {
			emitted.eKind = sirWindowEventKind::SIR_WINDOW_EVENT_FOCUS_LOST;
		} else if ( event.type == DestroyNotify ) {
			emitted.eKind = sirWindowEventKind::SIR_WINDOW_EVENT_LOST;
			m_hWindows[nSlot] = 0;
			m_bLost[nSlot] = true;
		} else if ( event.type == ClientMessage ) {
			if ( event.xclient.format != 32 || event.xclient.message_type != static_cast<Atom>(m_nProtocols) ||
			     static_cast<Atom>(event.xclient.data.l[0]) != static_cast<Atom>(m_nDeleteWindow) ) {
				continue;
			}
			emitted.eKind = sirWindowEventKind::SIR_WINDOW_EVENT_CLOSE_REQUESTED;
		} else {
			continue;
		}
		pCallback( pContext, emitted );
		++nEmitted;
	}
	for ( uint32_t j = 0; j < SIR_WINDOW_MAX_WINDOWS && nEmitted < nBudget; ++j ) {
		if ( ( m_nDisplayChangePendingMask & ( UINT32_C( 1 ) << j ) ) == 0 ) {
			continue;
		}
		m_nDisplayChangePendingMask &= ~( UINT32_C( 1 ) << j );
		if ( m_hWindows[j] != 0 ) {
			XWindowAttributes attrs {};
			if ( XGetWindowAttributes( pDisplay, static_cast<Window>(m_hWindows[j]), &attrs ) && attrs.width > 0 && attrs.height > 0 ) {
				const sirWindowBackendEvent_t changed = { j, m_hWindows[j], sirWindowEventKind::SIR_WINDOW_EVENT_DISPLAY_CHANGED, static_cast<uint32_t>(attrs.width),
				                                          static_cast<uint32_t>(attrs.height) };
				pCallback( pContext, changed );
				++nEmitted;
			}
		}
	}
	return sirWindowResult::SIR_WINDOW_OK;
}

void SIRWindowDriverX11::shutdown( void ) noexcept
{
	if ( m_pDisplay == nullptr ) {
		return;
	}
	if ( !m_bServerLost && transportAlive() ) {
		sigjmp_buf recovery;
		if ( sigsetjmp( recovery, 1 ) == 0 ) {
			s_pX11RecoveryDisplay = static_cast<Display *>(m_pDisplay);
			s_pX11RecoveryPoint = &recovery;
			shutdownNative();
		} else {
			markServerLost();
		}
		sirX11ClearRecovery();
	}
	// Once Xlib enters its fatal I/O path it cannot safely be asked to
	// synchronize or close the dead Display. Drop the connection and all
	// public window identities; no further Xlib call may use the old pointer.
	m_pDisplay = nullptr;
	for ( uint32_t i = 0; i < SIR_WINDOW_MAX_WINDOWS; ++i ) {
		m_hWindows[i] = 0;
		m_bLost[i] = false;
		m_nLostNative[i] = 0;
	}
	m_nLossPendingMask = 0;
	m_nDisplayChangePendingMask = 0;
	m_nDeleteWindow = 0;
	m_nProtocols = 0;
	m_nRootWindow = 0;
	m_nRandrEventBase = -1;
	m_bServerLost = false;
	if ( m_bHandlerRegistered ) {
		SIRScopedLock<SIRMutex> lock( s_X11IOHandlerMutex );
		if ( s_nX11HandlerUsers != 0 && --s_nX11HandlerUsers == 0 ) {
			static_cast<void>(XSetIOErrorHandler( s_pPreviousX11IOHandler ));
			s_pPreviousX11IOHandler = nullptr;
		}
		m_bHandlerRegistered = false;
	}
}

void SIRWindowDriverX11::shutdownNative( void ) noexcept
{
	Display *pDisplay = static_cast<Display *>(m_pDisplay);
	if ( pDisplay == nullptr ) {
		return;
	}
	for ( uint32_t i = 0; i < SIR_WINDOW_MAX_WINDOWS; ++i ) {
		if ( m_hWindows[i] != 0 ) {
			static_cast<void>(XDestroyWindow( pDisplay, static_cast<Window>(m_hWindows[i]) ));
			m_hWindows[i] = 0;
			m_bLost[i] = false;
		}
	}
	XCloseDisplay( pDisplay );
	m_pDisplay = nullptr;
	m_pSink = nullptr;
	m_pSinkContext = nullptr;
	m_nDeleteWindow = 0;
	m_nProtocols = 0;
	m_nRootWindow = 0;
	m_nDisplayChangePendingMask = 0;
	m_nServerWidth = 0;
	m_nServerHeight = 0;
	m_nRandrEventBase = -1;
}
