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

#include <Core/Platform/Input/SIRInputWindow.h>
#include <Core/Platform/Input/SIRInputService.h>
#include <Core/Platform/Input/Linux/SIRLinuxX11Backend.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

SIRInputWindow::SIRInputWindow( void ) noexcept
    : m_pInput( nullptr ), m_pDesktop( nullptr ), m_pDisplay( nullptr ), m_nWindow( 0 ), m_bOpen( false ), m_bAttached( false ),
      m_bRegistered( false ), m_bFocusLost( false ), m_bFocused( false ), m_bCursorCaptured( false ),
      m_eCursorMode( SIR_INPUT_CURSOR_FREE ), m_pInputMethod( nullptr ), m_pInputContext( nullptr ), m_nInvisibleCursor( 0 )
{
}

SIRInputWindow::~SIRInputWindow( void )
{
	close();
}

bool SIRInputWindow::open( SIRInputService& input, SIRInputBackend& desktop ) noexcept
{
	if ( m_pInput != nullptr || m_pDisplay != nullptr || desktop.id() != SIR_INPUT_LINUX_X11 ) {
		return false;
	}

	Display *pDisplay = XOpenDisplay( nullptr );
	if ( pDisplay == nullptr ) {
		return false;
	}

	const int nScreen = DefaultScreen( pDisplay );
	Window hWindow = XCreateSimpleWindow( pDisplay, RootWindow( pDisplay, nScreen ), 0, 0, 800, 450, 0, 0, 0 );
	if ( hWindow == 0 ) {
		XCloseDisplay( pDisplay );
		return false;
	}

	XSelectInput( pDisplay, hWindow, SIRLinuxX11Backend::requiredEventMask() | StructureNotifyMask );
	if ( XSupportsLocale() ) {
		( void )XSetLocaleModifiers( "" );
		XIM hInputMethod = XOpenIM( pDisplay, nullptr, nullptr, nullptr );
		if ( hInputMethod != nullptr ) {
			XIC hInputContext = XCreateIC( hInputMethod, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow, hWindow,
			                               XNFocusWindow, hWindow, nullptr );
			if ( hInputContext != nullptr ) {
				m_pInputMethod = hInputMethod;
				m_pInputContext = hInputContext;
			} else {
				XCloseIM( hInputMethod );
			}
		}
	}
	( void )SIRLinuxX11Backend::enableDetectableRepeat( pDisplay );
	Atom nDelete = XInternAtom( pDisplay, "WM_DELETE_WINDOW", False );
	XSetWMProtocols( pDisplay, hWindow, &nDelete, 1 );

	m_pDisplay = pDisplay;
	m_nWindow = ( uintptr_t )hWindow;
	if ( !input.attach( desktop ) ) {
		close();
		return false;
	}

	m_pInput = &input;
	m_pDesktop = &desktop;
	m_bOpen = true;
	XStoreName( pDisplay, hWindow, "SIREngine native input" );
	XMapWindow( pDisplay, hWindow );
	XFlush( pDisplay );
	return true;
}

bool SIRInputWindow::pump( SIRInputService& input ) noexcept
{
	if ( !m_bOpen || &input != m_pInput ) {
		return false;
	}

	SIRLinuxX11Backend *pX11 = ( SIRLinuxX11Backend * )m_pDesktop;
	if ( !m_bAttached ) {
		if ( !pX11->attachWindow( input, ( Window )m_nWindow ) ) {
			return false;
		}
		m_bAttached = true;
		( void )pX11->enableRawPointer( ( Display * )m_pDisplay );
	}

	Display *pDisplay = ( Display * )m_pDisplay;
	while ( m_bOpen && XPending( pDisplay ) > 0 ) {
		XEvent event = {};
		XNextEvent( pDisplay, &event );
		if ( event.type == GenericEvent ) {
			( void )pX11->processEvent( input, event );
			continue;
		}
		if ( event.xany.window != ( Window )m_nWindow ) {
			continue;
		}
		if ( event.type == FocusIn || event.type == FocusOut ) {
			m_bFocused = event.type == FocusIn;
			input.text().setFocused( m_bFocused );
			input.setHapticsPaused( !m_bFocused );
			if ( m_pInputContext != nullptr ) {
				if ( m_bFocused && input.text().snapshot().bEnabled ) {
					XSetICFocus( ( XIC )m_pInputContext );
				} else {
					XUnsetICFocus( ( XIC )m_pInputContext );
				}
			}
			if ( m_eCursorMode == SIR_INPUT_CURSOR_CAPTURED ) {
				( void )setCursorMode( m_eCursorMode );
			}
		}
		if ( m_bFocused && input.text().snapshot().bEnabled && m_pInputContext != nullptr &&
		     ( event.type == KeyPress || event.type == KeyRelease ) ) {
			if ( XFilterEvent( &event, ( Window )m_nWindow ) ) {
				continue;
			}
			if ( event.type == KeyPress ) {
				char szUtf8[1024] = {};
				KeySym eKey = NoSymbol;
				Status eStatus = XLookupNone;
				const int nLength = Xutf8LookupString( ( XIC )m_pInputContext, &event.xkey, szUtf8, sizeof( szUtf8 ), &eKey, &eStatus );
				if ( nLength > 0 && nLength <= ( int )sizeof( szUtf8 ) && ( eStatus == XLookupChars || eStatus == XLookupBoth ) ) {
					( void )input.text().appendUtf8( szUtf8, ( uint32_t )nLength );
				}
			}
		}
		if ( event.type == ClientMessage ) {
			const Atom hProtocols = XInternAtom( pDisplay, "WM_PROTOCOLS", True );
			const Atom hDelete = XInternAtom( pDisplay, "WM_DELETE_WINDOW", True );
			if ( event.xclient.format != 32 || event.xclient.message_type != hProtocols || ( Atom )event.xclient.data.l[0] != hDelete ) {
				continue;
			}
		}
		if ( event.type == DestroyNotify || event.type == ClientMessage ) {
			m_bOpen = false;
			if ( event.type == DestroyNotify ) {
				m_nWindow = 0;
			}
			break;
		}
		( void )pX11->processEvent( input, event );
	}
	return m_bOpen;
}

bool SIRInputWindow::setCursorMode( sirInputCursorMode_t eMode ) noexcept
{
	if ( eMode > SIR_INPUT_CURSOR_CAPTURED || !m_bOpen || m_pDisplay == nullptr ) {
		return false;
	}
	Display *pDisplay = ( Display * )m_pDisplay;
	const Window hWindow = ( Window )m_nWindow;
	if ( hWindow == 0 ) {
		return false;
	}
	if ( m_bCursorCaptured ) {
		XUngrabPointer( pDisplay, CurrentTime );
		m_bCursorCaptured = false;
	}
	if ( eMode != SIR_INPUT_CURSOR_FREE && m_nInvisibleCursor == 0 ) {
		const char szBlank[1] = { 0 };
		const Pixmap nBlank = XCreateBitmapFromData( pDisplay, hWindow, szBlank, 1, 1 );
		if ( nBlank == 0 ) {
			return false;
		}
		XColor black = {};
		m_nInvisibleCursor = ( uintptr_t )XCreatePixmapCursor( pDisplay, nBlank, nBlank, &black, &black, 0, 0 );
		XFreePixmap( pDisplay, nBlank );
		if ( m_nInvisibleCursor == 0 ) {
			return false;
		}
	}
	if ( eMode == SIR_INPUT_CURSOR_FREE ) {
		XUndefineCursor( pDisplay, hWindow );
	} else {
		XDefineCursor( pDisplay, hWindow, ( Cursor )m_nInvisibleCursor );
	}
	if ( eMode == SIR_INPUT_CURSOR_CAPTURED && m_bFocused ) {
		const int nResult = XGrabPointer( pDisplay, hWindow, True, ButtonPressMask | ButtonReleaseMask | PointerMotionMask, GrabModeAsync,
		                                  GrabModeAsync, hWindow, ( Cursor )m_nInvisibleCursor, CurrentTime );
		if ( nResult != GrabSuccess ) {
			XFlush( pDisplay );
			return false;
		}
		m_bCursorCaptured = true;
	}
	m_eCursorMode = eMode;
	XFlush( pDisplay );
	return true;
}

sirInputCursorMode_t SIRInputWindow::cursorMode( void ) const noexcept
{
	return m_eCursorMode;
}

bool SIRInputWindow::focused( void ) const noexcept
{
	return m_bFocused;
}

void SIRInputWindow::setTextEnabled( bool bEnabled ) noexcept
{
	if ( m_pInput == nullptr ) {
		return;
	}
	m_pInput->text().setEnabled( bEnabled );
	if ( m_pInputContext != nullptr && m_bFocused ) {
		if ( bEnabled ) {
			XSetICFocus( ( XIC )m_pInputContext );
		} else {
			XUnsetICFocus( ( XIC )m_pInputContext );
		}
	}
}

bool SIRInputWindow::isOpen( void ) const noexcept
{
	return m_bOpen;
}

void SIRInputWindow::close( void ) noexcept
{
	Display *pDisplay = ( Display * )m_pDisplay;
	if ( pDisplay != nullptr ) {
		if ( m_bCursorCaptured ) {
			XUngrabPointer( pDisplay, CurrentTime );
		}
		if ( m_nInvisibleCursor != 0 ) {
			XFreeCursor( pDisplay, ( Cursor )m_nInvisibleCursor );
		}
		if ( m_pInputContext != nullptr ) {
			XDestroyIC( ( XIC )m_pInputContext );
		}
		if ( m_pInputMethod != nullptr ) {
			XCloseIM( ( XIM )m_pInputMethod );
		}
		if ( m_nWindow != 0 ) {
			XDestroyWindow( pDisplay, ( Window )m_nWindow );
		}
		XCloseDisplay( pDisplay );
	}
	m_pInput = nullptr;
	m_pDesktop = nullptr;
	m_pDisplay = nullptr;
	m_nWindow = 0;
	m_bOpen = false;
	m_bAttached = false;
	m_bRegistered = false;
	m_bFocusLost = false;
	m_bFocused = false;
	m_bCursorCaptured = false;
	m_eCursorMode = SIR_INPUT_CURSOR_FREE;
	m_pInputContext = nullptr;
	m_pInputMethod = nullptr;
	m_nInvisibleCursor = 0;
}
