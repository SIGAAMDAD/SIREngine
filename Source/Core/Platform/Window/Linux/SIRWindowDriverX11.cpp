#include <Core/Platform/Window/Linux/SIRWindowDriverX11.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <string.h>

// Set the ICCCM name and UTF-8 EWMH name for window managers and IME users.
static sirWindowResult_t sirSetX11Title( Display *pDisplay, Window hWindow, const char *pszTitle ) noexcept
{
	if ( pszTitle == nullptr || strlen( pszTitle ) > 0x7fffffffU ) {
		return SIR_WINDOW_INVALID_ARGUMENT;
	}
	if ( XStoreName( pDisplay, hWindow, pszTitle ) == 0 ) {
		return SIR_WINDOW_NATIVE_FAILURE;
	}
	const Atom hName = XInternAtom( pDisplay, "_NET_WM_NAME", False );
	const Atom hUtf8 = XInternAtom( pDisplay, "UTF8_STRING", False );
	if ( hName != None && hUtf8 != None ) {
		XChangeProperty( pDisplay, hWindow, hName, hUtf8, 8, PropModeReplace, ( const unsigned char * )pszTitle,
		                 ( int )strlen( pszTitle ) );
	}
	return SIR_WINDOW_OK;
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

sirWindowResult_t SIRWindowDriverX11::initialize( void ) noexcept
{
	if ( m_pDisplay != nullptr ) {
		return SIR_WINDOW_BUSY;
	}
	Display *pDisplay = XOpenDisplay( nullptr );
	if ( pDisplay == nullptr ) {
		return SIR_WINDOW_NOT_READY;
	}
	m_nDeleteWindow = ( uintptr_t )XInternAtom( pDisplay, "WM_DELETE_WINDOW", False );
	m_nProtocols = ( uintptr_t )XInternAtom( pDisplay, "WM_PROTOCOLS", False );
	if ( m_nDeleteWindow == 0 || m_nProtocols == 0 ) {
		XCloseDisplay( pDisplay );
		return SIR_WINDOW_NATIVE_FAILURE;
	}
	m_pDisplay = pDisplay;
	return SIR_WINDOW_OK;
}

sirWindowResult_t SIRWindowDriverX11::createWindow( const sirWindowCreateDesc_t& desc, uint32_t nSlot,
                                                    sirWindowNativeView_t *pView ) noexcept
{
	if ( nSlot >= SIR_WINDOW_MAX_WINDOWS || pView == nullptr || desc.pszTitle == nullptr || desc.nWidth == 0 || desc.nHeight == 0 ) {
		return SIR_WINDOW_INVALID_ARGUMENT;
	}
	Display *pDisplay = ( Display * )m_pDisplay;
	if ( pDisplay == nullptr ) {
		return SIR_WINDOW_STOPPED;
	}
	if ( m_hWindows[nSlot] != 0 ) {
		return SIR_WINDOW_BUSY;
	}
	const int nScreen = DefaultScreen( pDisplay );
	Window hWindow = XCreateSimpleWindow( pDisplay, RootWindow( pDisplay, nScreen ), 0, 0, desc.nWidth, desc.nHeight, 0, 0, 0 );
	if ( hWindow == 0 ) {
		return SIR_WINDOW_NATIVE_FAILURE;
	}
	XSelectInput( pDisplay, hWindow, StructureNotifyMask | FocusChangeMask );
	Atom hDelete = ( Atom )m_nDeleteWindow;
	if ( !XSetWMProtocols( pDisplay, hWindow, &hDelete, 1 ) || sirSetX11Title( pDisplay, hWindow, desc.pszTitle ) != SIR_WINDOW_OK ) {
		( void )XDestroyWindow( pDisplay, hWindow );
		return SIR_WINDOW_NATIVE_FAILURE;
	}
	( void )XMapWindow( pDisplay, hWindow );
	( void )XFlush( pDisplay );
	m_hWindows[nSlot] = ( uintptr_t )hWindow;
	m_bLost[nSlot] = false;
	*pView = { SIR_WINDOW_PLATFORM_X11, ( uintptr_t )hWindow, ( uintptr_t )pDisplay, 0 };
	return SIR_WINDOW_OK;
}

sirWindowResult_t SIRWindowDriverX11::destroyWindow( uint32_t nSlot ) noexcept
{
	if ( m_pDisplay == nullptr ) {
		return SIR_WINDOW_STOPPED;
	}
	if ( nSlot >= SIR_WINDOW_MAX_WINDOWS || ( m_hWindows[nSlot] == 0 && !m_bLost[nSlot] ) ) {
		return SIR_WINDOW_INVALID_HANDLE;
	}
	const Window hWindow = ( Window )m_hWindows[nSlot];
	m_hWindows[nSlot] = 0;
	m_bLost[nSlot] = false;
	if ( hWindow != 0 ) {
		( void )XDestroyWindow( ( Display * )m_pDisplay, hWindow );
	}
	( void )XFlush( ( Display * )m_pDisplay );
	return SIR_WINDOW_OK;
}

sirWindowResult_t SIRWindowDriverX11::mutateWindow( uint32_t nSlot, const sirWindowMutation_t& mutation ) noexcept
{
	Display *pDisplay = ( Display * )m_pDisplay;
	if ( pDisplay == nullptr ) {
		return SIR_WINDOW_STOPPED;
	}
	if ( nSlot >= SIR_WINDOW_MAX_WINDOWS || m_hWindows[nSlot] == 0 ) {
		return SIR_WINDOW_INVALID_HANDLE;
	}
	const Window hWindow = ( Window )m_hWindows[nSlot];
	switch ( mutation.eKind ) {
	case SIR_WINDOW_MUTATION_TITLE: {
		if ( mutation.pszTitle == nullptr ) {
			return SIR_WINDOW_INVALID_ARGUMENT;
		}
		const sirWindowResult_t eTitle = sirSetX11Title( pDisplay, hWindow, mutation.pszTitle );
		if ( eTitle != SIR_WINDOW_OK ) {
			return eTitle;
		}
		break;
	}
	case SIR_WINDOW_MUTATION_RESIZE:
		if ( mutation.nWidth == 0 || mutation.nHeight == 0 ) {
			return SIR_WINDOW_INVALID_ARGUMENT;
		}
		( void )XResizeWindow( pDisplay, hWindow, mutation.nWidth, mutation.nHeight );
		break;
	case SIR_WINDOW_MUTATION_SHOW:
	case SIR_WINDOW_MUTATION_RESTORE:
		( void )XMapRaised( pDisplay, hWindow );
		break;
	case SIR_WINDOW_MUTATION_HIDE:
		( void )XUnmapWindow( pDisplay, hWindow );
		break;
	case SIR_WINDOW_MUTATION_MINIMIZE:
		if ( !XIconifyWindow( pDisplay, hWindow, DefaultScreen( pDisplay ) ) ) {
			return SIR_WINDOW_UNSUPPORTED;
		}
		break;
	case SIR_WINDOW_MUTATION_MAXIMIZE: {
		const Atom hState = XInternAtom( pDisplay, "_NET_WM_STATE", False );
		const Atom hVertical = XInternAtom( pDisplay, "_NET_WM_STATE_MAXIMIZED_VERT", False );
		const Atom hHorizontal = XInternAtom( pDisplay, "_NET_WM_STATE_MAXIMIZED_HORZ", False );
		if ( hState == None || hVertical == None || hHorizontal == None ) {
			return SIR_WINDOW_NATIVE_FAILURE;
		}
		XEvent message = {};
		message.xclient.type = ClientMessage;
		message.xclient.display = pDisplay;
		message.xclient.window = hWindow;
		message.xclient.message_type = hState;
		message.xclient.format = 32;
		message.xclient.data.l[0] = 1; // _NET_WM_STATE_ADD
		message.xclient.data.l[1] = ( long )hVertical;
		message.xclient.data.l[2] = ( long )hHorizontal;
		message.xclient.data.l[3] = 1; // Client application initiated
		if ( !XSendEvent( pDisplay, DefaultRootWindow( pDisplay ), False, SubstructureNotifyMask | SubstructureRedirectMask,
		                  &message ) ) {
			return SIR_WINDOW_NATIVE_FAILURE;
		}
		break;
	}
	default:
		return SIR_WINDOW_UNSUPPORTED;
	}
	XFlush( pDisplay );
	return SIR_WINDOW_OK;
}

sirWindowResult_t SIRWindowDriverX11::pumpEvents( uint32_t nBudget, sirWindowDriverEventFn_t pCallback, void *pContext ) noexcept
{
	Display *pDisplay = ( Display * )m_pDisplay;
	if ( pDisplay == nullptr ) {
		return SIR_WINDOW_STOPPED;
	}
	if ( nBudget == 0 || pCallback == nullptr ) {
		return SIR_WINDOW_INVALID_ARGUMENT;
	}
	for ( uint32_t i = 0; i < nBudget && XPending( pDisplay ) > 0; ++i ) {
		XEvent event = {};
		XNextEvent( pDisplay, &event );
		if ( m_pSink != nullptr ) {
			m_pSink( event, m_pSinkContext );
		}
		uint32_t nSlot = SIR_WINDOW_MAX_WINDOWS;
		for ( uint32_t j = 0; j < SIR_WINDOW_MAX_WINDOWS; ++j ) {
			if ( m_hWindows[j] != 0 && m_hWindows[j] == ( uintptr_t )event.xany.window ) {
				nSlot = j;
				break;
			}
		}
		if ( nSlot == SIR_WINDOW_MAX_WINDOWS ) {
			continue;
		}
		sirWindowBackendEvent_t emitted = { nSlot, ( uintptr_t )event.xany.window, SIR_WINDOW_EVENT_READY, 0, 0 };
		if ( event.type == ConfigureNotify ) {
			emitted.eKind = SIR_WINDOW_EVENT_RESIZED;
			emitted.nWidth = ( uint32_t )event.xconfigure.width;
			emitted.nHeight = ( uint32_t )event.xconfigure.height;
		} else if ( event.type == MapNotify ) {
			emitted.eKind = SIR_WINDOW_EVENT_SHOWN;
		} else if ( event.type == UnmapNotify ) {
			emitted.eKind = SIR_WINDOW_EVENT_HIDDEN;
		} else if ( event.type == FocusIn ) {
			emitted.eKind = SIR_WINDOW_EVENT_FOCUS_GAINED;
		} else if ( event.type == FocusOut ) {
			emitted.eKind = SIR_WINDOW_EVENT_FOCUS_LOST;
		} else if ( event.type == DestroyNotify ) {
			emitted.eKind = SIR_WINDOW_EVENT_LOST;
			m_hWindows[nSlot] = 0;
			m_bLost[nSlot] = true;
		} else if ( event.type == ClientMessage ) {
			if ( event.xclient.format != 32 || event.xclient.message_type != ( Atom )m_nProtocols ||
			     ( Atom )event.xclient.data.l[0] != ( Atom )m_nDeleteWindow ) {
				continue;
			}
			emitted.eKind = SIR_WINDOW_EVENT_CLOSE_REQUESTED;
		} else {
			continue;
		}
		pCallback( pContext, emitted );
	}
	return SIR_WINDOW_OK;
}

void SIRWindowDriverX11::shutdown( void ) noexcept
{
	Display *pDisplay = ( Display * )m_pDisplay;
	if ( pDisplay == nullptr ) {
		return;
	}
	for ( uint32_t i = 0; i < SIR_WINDOW_MAX_WINDOWS; ++i ) {
		if ( m_hWindows[i] != 0 ) {
			( void )XDestroyWindow( pDisplay, ( Window )m_hWindows[i] );
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
}
