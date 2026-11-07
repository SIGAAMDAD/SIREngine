#include <Core/Platform/Input/Linux/SIRX11InputBridge.h>

SIRX11InputBridge::~SIRX11InputBridge( void ) noexcept
{
	unbind();
}

bool SIRX11InputBridge::bind( SIRInputService& input, SIRLinuxX11Backend& backend, const sirWindowNativeView_t& view ) noexcept
{
	if ( m_pInput != nullptr || view.ePlatform != SIR_WINDOW_PLATFORM_X11 || view.nNativeWindow == 0 || view.nDisplayOrInstance == 0 ||
	     !input.attach( backend ) ) {
		return false;
	}
	const Window hWindow = ( Window )view.nNativeWindow;
	if ( !backend.attachWindow( input, hWindow ) ) {
		backend.stop( input );
		return false;
	}
	Display *pDisplay = ( Display * )view.nDisplayOrInstance;
	m_pInput = &input;
	m_pBackend = &backend;
	m_pDisplay = pDisplay;
	m_hWindow = hWindow;
	XSelectInput( pDisplay, hWindow, SIRLinuxX11Backend::requiredEventMask() | StructureNotifyMask );
	( void )SIRLinuxX11Backend::enableDetectableRepeat( pDisplay );
	( void )backend.enableRawPointer( pDisplay );
	if ( XSupportsLocale() ) {
		( void )XSetLocaleModifiers( "" );
		XIM hMethod = XOpenIM( pDisplay, nullptr, nullptr, nullptr );
		if ( hMethod != nullptr ) {
			XIC hContext = XCreateIC( hMethod, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow, hWindow, XNFocusWindow,
			                          hWindow, nullptr );
			if ( hContext != nullptr ) {
				m_hInputMethod = hMethod;
				m_hInputContext = hContext;
			} else {
				XCloseIM( hMethod );
			}
		}
	}
	XFlush( pDisplay );
	return true;
}

bool SIRX11InputBridge::isBound( void ) const noexcept
{
	return m_pInput != nullptr;
}

bool SIRX11InputBridge::setCursorMode( sirInputCursorMode_t eMode ) noexcept
{
	if ( m_pDisplay == nullptr || m_hWindow == 0 || eMode > SIR_INPUT_CURSOR_CAPTURED ) {
		return false;
	}
	if ( m_bCursorCaptured ) {
		XUngrabPointer( m_pDisplay, CurrentTime );
		m_bCursorCaptured = false;
	}
	if ( eMode != SIR_INPUT_CURSOR_FREE && m_nInvisibleCursor == 0 ) {
		const char szBlank[1] = { 0 };
		const Pixmap hBlank = XCreateBitmapFromData( m_pDisplay, m_hWindow, szBlank, 1, 1 );
		if ( hBlank == 0 ) {
			return false;
		}
		XColor black = {};
		m_nInvisibleCursor = ( uintptr_t )XCreatePixmapCursor( m_pDisplay, hBlank, hBlank, &black, &black, 0, 0 );
		XFreePixmap( m_pDisplay, hBlank );
		if ( m_nInvisibleCursor == 0 ) {
			return false;
		}
	}
	if ( eMode == SIR_INPUT_CURSOR_FREE ) {
		XUndefineCursor( m_pDisplay, m_hWindow );
	} else {
		XDefineCursor( m_pDisplay, m_hWindow, ( Cursor )m_nInvisibleCursor );
	}
	if ( eMode == SIR_INPUT_CURSOR_CAPTURED && m_bFocused ) {
		const int nResult = XGrabPointer( m_pDisplay, m_hWindow, True, ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
		                                  GrabModeAsync, GrabModeAsync, m_hWindow, ( Cursor )m_nInvisibleCursor, CurrentTime );
		if ( nResult != GrabSuccess ) {
			XFlush( m_pDisplay );
			return false;
		}
		m_bCursorCaptured = true;
	}
	m_eCursorMode = eMode;
	XFlush( m_pDisplay );
	return true;
}

sirInputCursorMode_t SIRX11InputBridge::cursorMode( void ) const noexcept
{
	return m_eCursorMode;
}

void SIRX11InputBridge::setTextEnabled( bool bEnabled ) noexcept
{
	if ( m_pInput == nullptr ) {
		return;
	}
	m_pInput->text().setEnabled( bEnabled );
	if ( m_hInputContext != nullptr && m_bFocused ) {
		if ( bEnabled ) {
			XSetICFocus( m_hInputContext );
		} else {
			XUnsetICFocus( m_hInputContext );
		}
	}
}

void SIRX11InputBridge::processEvent( const XEvent& event ) noexcept
{
	if ( m_pInput == nullptr || m_pBackend == nullptr ) {
		return;
	}
	if ( event.type == GenericEvent ) {
		( void )m_pBackend->processEvent( *m_pInput, event );
		return;
	}
	if ( event.xany.window != m_hWindow ) {
		return;
	}
	if ( event.type == FocusIn || event.type == FocusOut ) {
		m_bFocused = event.type == FocusIn;
		m_pInput->text().setFocused( m_bFocused );
		m_pInput->setHapticsPaused( !m_bFocused );
		if ( m_eCursorMode == SIR_INPUT_CURSOR_CAPTURED ) {
			( void )setCursorMode( m_eCursorMode );
		}
		if ( m_hInputContext != nullptr ) {
			if ( m_bFocused && m_pInput->text().snapshot().bEnabled ) {
				XSetICFocus( m_hInputContext );
			} else {
				XUnsetICFocus( m_hInputContext );
			}
		}
	}
	if ( m_bFocused && m_pInput->text().snapshot().bEnabled && m_hInputContext != nullptr &&
	     ( event.type == KeyPress || event.type == KeyRelease ) ) {
		XEvent filtered = event;
		if ( XFilterEvent( &filtered, m_hWindow ) ) {
			return;
		}
		if ( event.type == KeyPress ) {
			char szUtf8[1024] = {};
			KeySym eKey = NoSymbol;
			Status eStatus = XLookupNone;
			const int nLength = Xutf8LookupString( m_hInputContext, &filtered.xkey, szUtf8, sizeof( szUtf8 ), &eKey, &eStatus );
			if ( nLength > 0 && nLength <= ( int )sizeof( szUtf8 ) && ( eStatus == XLookupChars || eStatus == XLookupBoth ) ) {
				( void )m_pInput->text().appendUtf8( szUtf8, ( uint32_t )nLength );
			}
		}
	}
	( void )m_pBackend->processEvent( *m_pInput, event );
}

void SIRX11InputBridge::unbind( void ) noexcept
{
	if ( m_pInput == nullptr ) {
		return;
	}
	m_pInput->text().setFocused( false );
	m_pInput->setHapticsPaused( true );
	if ( m_bCursorCaptured ) {
		XUngrabPointer( m_pDisplay, CurrentTime );
	}
	if ( m_nInvisibleCursor != 0 ) {
		XFreeCursor( m_pDisplay, ( Cursor )m_nInvisibleCursor );
	}
	if ( m_hInputContext != nullptr ) {
		XDestroyIC( m_hInputContext );
	}
	if ( m_hInputMethod != nullptr ) {
		XCloseIM( m_hInputMethod );
	}
	m_pBackend->stop( *m_pInput );
	m_pInput = nullptr;
	m_pBackend = nullptr;
	m_pDisplay = nullptr;
	m_hWindow = 0;
	m_hInputContext = nullptr;
	m_hInputMethod = nullptr;
	m_bFocused = false;
	m_bCursorCaptured = false;
	m_eCursorMode = SIR_INPUT_CURSOR_FREE;
	m_nInvisibleCursor = 0;
}
