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

SIRInputWindow::SIRInputWindow( void ) noexcept
    : m_pInput( nullptr ), m_pDesktop( nullptr ), m_pDisplay( nullptr ), m_nWindow( 0 ), m_bOpen( false ), m_bAttached( false ),
      m_bRegistered( false ), m_bFocusLost( false )
{
}

SIRInputWindow::~SIRInputWindow( void )
{
	close();
}

bool SIRInputWindow::open( SIRInputService &input, SIRInputBackend &desktop ) noexcept
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

bool SIRInputWindow::pump( SIRInputService &input ) noexcept
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
	}

	Display *pDisplay = ( Display * )m_pDisplay;
	while ( m_bOpen && XPending( pDisplay ) > 0 ) {
		XEvent event = {};
		XNextEvent( pDisplay, &event );
		if ( event.xany.window != ( Window )m_nWindow ) {
			continue;
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

bool SIRInputWindow::isOpen( void ) const noexcept
{
	return m_bOpen;
}

void SIRInputWindow::close( void ) noexcept
{
	Display *pDisplay = ( Display * )m_pDisplay;
	if ( pDisplay != nullptr ) {
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
}
