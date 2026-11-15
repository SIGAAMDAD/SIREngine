#include <Core/Platform/Window/SIRNativeWindowDrivers.h>
#include <Core/Platform/Input/Linux/SIRLinuxX11Backend.h>
#include <Core/Platform/Input/SIRInputService.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

SIRX11WindowDriver::SIRX11WindowDriver( void ) noexcept
    : m_pSystem( nullptr ), m_pDisplay( nullptr ), m_hInputMethod( nullptr ), m_hDelete( 0 ), m_hProtocols( 0 ), m_szSlots{},
      m_pInput( nullptr ), m_pDesktop( nullptr ), m_hInputWindow{}, m_bInputAttached( false )
{
}

bool SIRX11WindowDriver::initialize( SIRWindowSystem& system ) noexcept
{
	if ( m_pDisplay != nullptr ) {
		return false;
	}
	Display *pDisplay = XOpenDisplay( nullptr );
	if ( pDisplay == nullptr ) {
		return false;
	}
	m_pDisplay = pDisplay;
	m_pSystem = &system;
	m_hDelete = XInternAtom( pDisplay, "WM_DELETE_WINDOW", False );
	m_hProtocols = XInternAtom( pDisplay, "WM_PROTOCOLS", False );
	if ( XSupportsLocale() ) {
		( void )XSetLocaleModifiers( "" );
		m_hInputMethod = XOpenIM( pDisplay, nullptr, nullptr, nullptr );
	}
	( void )SIRLinuxX11Backend::enableDetectableRepeat( pDisplay );
	return true;
}

void SIRX11WindowDriver::shutdown( void ) noexcept
{
	if ( m_pDisplay == nullptr ) {
		return;
	}
	for ( uint32_t i = 0; i < SIR_WINDOW_MAX_WINDOWS; ++i ) {
		if ( m_szSlots[i].hInputContext != nullptr ) {
			XDestroyIC( m_szSlots[i].hInputContext );
		}
		if ( m_szSlots[i].hNative != 0 ) {
			XDestroyWindow( m_pDisplay, m_szSlots[i].hNative );
		}
		m_szSlots[i] = {};
	}
	if ( m_hInputMethod != nullptr ) {
		XCloseIM( m_hInputMethod );
	}
	XCloseDisplay( m_pDisplay );
	m_pDisplay = nullptr;
	m_hInputMethod = nullptr;
	m_pSystem = nullptr;
	m_pInput = nullptr;
	m_pDesktop = nullptr;
	m_bInputAttached = false;
}

bool SIRX11WindowDriver::create( sirWindowHandle_t hWindow, const sirWindowDesc_t& desc, sirWindowNativeView_t *pView ) noexcept
{
	if ( m_pDisplay == nullptr || pView == nullptr || hWindow.nSlot >= SIR_WINDOW_MAX_WINDOWS ) {
		return false;
	}
	sirX11WindowSlot_t& slot = m_szSlots[hWindow.nSlot];
	if ( slot.hNative != 0 ) {
		return false;
	}
	const Window hNative = XCreateSimpleWindow( m_pDisplay, DefaultRootWindow( m_pDisplay ), 0, 0, desc.nWidth, desc.nHeight, 0, 0, 0 );
	if ( hNative == 0 ) {
		return false;
	}
	const long nMask = SIRLinuxX11Backend::requiredEventMask() | StructureNotifyMask | ExposureMask;
	XSelectInput( m_pDisplay, hNative, nMask );
	( void )XSetWMProtocols( m_pDisplay, hNative, &m_hDelete, 1 );
	XStoreName( m_pDisplay, hNative, desc.pszTitle );
	if ( m_hInputMethod != nullptr ) {
		slot.hInputContext = XCreateIC( m_hInputMethod, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow, hNative,
		                                XNFocusWindow, hNative, nullptr );
	}
	slot.hWindow = hWindow;
	slot.hNative = hNative;
	if ( desc.bVisible ) {
		XMapWindow( m_pDisplay, hNative );
	}
	XFlush( m_pDisplay );
	*pView = { SIR_WINDOW_PLATFORM_X11, ( void * )( uintptr_t )hNative, m_pDisplay, 0 };
	return true;
}

void SIRX11WindowDriver::destroy( sirWindowHandle_t hWindow, const sirWindowNativeView_t& view ) noexcept
{
	( void )view;
	if ( m_pDisplay == nullptr || hWindow.nSlot >= SIR_WINDOW_MAX_WINDOWS ) {
		return;
	}
	sirX11WindowSlot_t& slot = m_szSlots[hWindow.nSlot];
	if ( slot.hWindow.nGeneration != hWindow.nGeneration ) {
		return;
	}
	if ( slot.hInputContext != nullptr ) {
		XDestroyIC( slot.hInputContext );
	}
	if ( slot.hNative != 0 ) {
		XDestroyWindow( m_pDisplay, slot.hNative );
	}
	slot = {};
	XFlush( m_pDisplay );
}

bool SIRX11WindowDriver::bindInput( sirWindowHandle_t hWindow, SIRInputService& input, SIRInputBackend& backend ) noexcept
{
	if ( m_pInput != nullptr || backend.id() != SIR_INPUT_LINUX_X11 || hWindow.nSlot >= SIR_WINDOW_MAX_WINDOWS ) {
		return false;
	}
	if ( m_szSlots[hWindow.nSlot].hWindow.nGeneration != hWindow.nGeneration ) {
		return false;
	}
	if ( !input.attach( backend ) ) {
		return false;
	}
	m_pInput = &input;
	m_pDesktop = &backend;
	m_hInputWindow = hWindow;
	return true;
}

void SIRX11WindowDriver::processInput( sirX11WindowSlot_t& slot, XEvent& event ) noexcept
{
	if ( m_pInput == nullptr || m_pDesktop == nullptr ) {
		return;
	}
	if ( slot.hWindow.nSlot != m_hInputWindow.nSlot || slot.hWindow.nGeneration != m_hInputWindow.nGeneration ) {
		return;
	}
	if ( event.type == FocusIn || event.type == FocusOut ) {
		const bool bFocused = event.type == FocusIn;
		m_pInput->text().setFocused( bFocused );
		m_pInput->setHapticsPaused( !bFocused );
		if ( slot.hInputContext != nullptr ) {
			if ( bFocused && m_pInput->text().snapshot().bEnabled ) {
				XSetICFocus( slot.hInputContext );
			} else {
				XUnsetICFocus( slot.hInputContext );
			}
		}
	}
	const bool bTextKeyEvent = event.type == KeyPress || event.type == KeyRelease;
	if ( slot.hInputContext != nullptr && m_pInput->text().snapshot().bEnabled && bTextKeyEvent ) {
		if ( XFilterEvent( &event, slot.hNative ) ) {
			return;
		}
		if ( event.type == KeyPress ) {
			char szUtf8[1024] = {};
			KeySym eKey = NoSymbol;
			Status eStatus = XLookupNone;
			const int nBytes = Xutf8LookupString( slot.hInputContext, &event.xkey, szUtf8, sizeof( szUtf8 ), &eKey, &eStatus );
			const bool bTextResult = eStatus == XLookupChars || eStatus == XLookupBoth;
			if ( nBytes > 0 && nBytes <= ( int )sizeof( szUtf8 ) && bTextResult ) {
				( void )m_pInput->text().appendUtf8( szUtf8, ( uint32_t )nBytes );
			}
		}
	}
	( void )( ( SIRLinuxX11Backend * )m_pDesktop )->processEvent( *m_pInput, event );
}

void SIRX11WindowDriver::processEvent( XEvent& event ) noexcept
{
	if ( event.type == GenericEvent ) {
		if ( m_pInput != nullptr && m_bInputAttached ) {
			( void )( ( SIRLinuxX11Backend * )m_pDesktop )->processEvent( *m_pInput, event );
		}
		return;
	}
	for ( uint32_t i = 0; i < SIR_WINDOW_MAX_WINDOWS; ++i ) {
		sirX11WindowSlot_t& slot = m_szSlots[i];
		if ( slot.hNative == 0 || slot.hNative != event.xany.window ) {
			continue;
		}
		if ( event.type == ClientMessage ) {
			if ( event.xclient.format == 32 && event.xclient.message_type == m_hProtocols &&
			     ( Atom )event.xclient.data.l[0] == m_hDelete ) {
				( void )m_pSystem->notifyClose( slot.hWindow );
			}
			return;
		}
		if ( event.type == DestroyNotify ) {
			( void )m_pSystem->notifyLost( slot.hWindow );
			if ( slot.hInputContext != nullptr ) {
				XDestroyIC( slot.hInputContext );
			}
			slot = {};
			return;
		}
		if ( event.type == ConfigureNotify ) {
			( void )m_pSystem->notifyResize( slot.hWindow, ( uint32_t )event.xconfigure.width, ( uint32_t )event.xconfigure.height );
		}
		if ( event.type == FocusIn || event.type == FocusOut ) {
			( void )m_pSystem->notifyFocus( slot.hWindow, event.type == FocusIn );
		}
		processInput( slot, event );
		return;
	}
}

bool SIRX11WindowDriver::pump( void ) noexcept
{
	if ( m_pDisplay == nullptr ) {
		return false;
	}
	if ( m_pInput != nullptr && !m_bInputAttached ) {
		const sirX11WindowSlot_t& slot = m_szSlots[m_hInputWindow.nSlot];
		if ( !( ( SIRLinuxX11Backend * )m_pDesktop )->attachWindow( *m_pInput, slot.hNative ) ) {
			return false;
		}
		m_bInputAttached = true;
		( void )( ( SIRLinuxX11Backend * )m_pDesktop )->enableRawPointer( m_pDisplay );
	}
	while ( XPending( m_pDisplay ) > 0 ) {
		XEvent event = {};
		XNextEvent( m_pDisplay, &event );
		processEvent( event );
	}
	return true;
}
