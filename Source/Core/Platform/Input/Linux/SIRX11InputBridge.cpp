#include <Core/Platform/Input/Linux/SIRX11InputBridge.h>
#include <Core/Platform/SIRPlatformNativeWindow.h>

SIRX11InputBridge::~SIRX11InputBridge( void ) noexcept
{
	unbind();
}

bool SIRX11InputBridge::bind( SIRInputService& input, SIRLinuxX11Backend& backend, const sirWindowNativeView_t& view ) noexcept
{
	sirLinuxNativeWindow_t native = {};
	if ( m_pInput != nullptr || !sirPlatformDecodeLinuxWindow( view, &native ) || native.eDisplayType != SIR_LINUX_DISPLAY_X11 ||
	     !input.attach( backend ) ) {
		return false;
	}
	const Window hWindow = ( Window )native.nSurface;
	Display *pDisplay = ( Display * )native.nDisplay;
	m_pInput = &input;
	m_pBackend = &backend;
	m_pDisplay = pDisplay;
	m_hWindow = hWindow;
	( void )SIRLinuxX11Backend::enableDetectableRepeat( pDisplay );
	if ( XSupportsLocale() ) {
		( void )XSetLocaleModifiers( "" );
		m_hInputMethod = XOpenIM( pDisplay, nullptr, nullptr, nullptr );
	}
	if ( !registerWindow( view ) ) {
		unbind();
		return false;
	}
	XFlush( pDisplay );
	return true;
}

bool SIRX11InputBridge::beginFrame( void ) noexcept
{
	if ( m_pInput == nullptr || m_pBackend == nullptr ) {
		return false;
	}
	if ( m_bDeferredResync ) {
		if ( !m_pInput->loseBackendState( SIR_INPUT_LINUX_X11 ) ) {
			return false;
		}
		m_bDeferredResync = false;
	}
	if ( !m_bAttached ) {
		if ( !m_pBackend->attachWindow( *m_pInput, m_hWindow ) ) {
			return false;
		}
		m_bAttached = true;
		( void )m_pBackend->enableRawPointer( m_pDisplay );
	}
	return true;
}

bool SIRX11InputBridge::isBound( void ) const noexcept
{
	return m_pInput != nullptr;
}

bool SIRX11InputBridge::registerWindow( const sirWindowNativeView_t& view ) noexcept
{
	sirLinuxNativeWindow_t native = {};
	if ( m_pInput == nullptr || !sirPlatformDecodeLinuxWindow( view, &native ) || native.eDisplayType != SIR_LINUX_DISPLAY_X11 ||
	     native.nDisplay != ( uintptr_t )m_pDisplay || m_nWindowCount >= SIR_WINDOW_MAX_WINDOWS ) {
		return false;
	}
	const Window hWindow = ( Window )native.nSurface;
	for ( uint32_t i = 0; i < m_nWindowCount; ++i ) {
		if ( m_hRegisteredWindows[i] == hWindow ) {
			return false;
		}
	}
	XIC hContext = nullptr;
	if ( m_hInputMethod != nullptr ) {
		hContext = XCreateIC( m_hInputMethod, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow, hWindow, XNFocusWindow,
		                      hWindow, nullptr );
	}
	XSelectInput( m_pDisplay, hWindow, SIRLinuxX11Backend::requiredEventMask() | StructureNotifyMask );
	m_hRegisteredWindows[m_nWindowCount] = hWindow;
	m_hRegisteredContexts[m_nWindowCount] = hContext;
	if ( m_nWindowCount == 0 ) {
		m_hInputContext = hContext;
	}
	++m_nWindowCount;
	return true;
}

bool SIRX11InputBridge::unregisterWindow( uintptr_t nNativeWindow ) noexcept
{
	if ( m_pInput == nullptr || nNativeWindow == 0 ) {
		return false;
	}
	for ( uint32_t i = 0; i < m_nWindowCount; ++i ) {
		if ( m_hRegisteredWindows[i] != ( Window )nNativeWindow ) {
			continue;
		}
		if ( m_hRegisteredContexts[i] != nullptr ) {
			if ( m_hRegisteredContexts[i] == m_hInputContext ) {
				XUnsetICFocus( m_hInputContext );
				m_hInputContext = nullptr;
			}
			XDestroyIC( m_hRegisteredContexts[i] );
		}
		if ( m_hWindow == ( Window )nNativeWindow ) {
			// unregisterWindow may run after publish() in the application
			// lifecycle. Keep the reset pending until the next beginFrame.
			m_bDeferredResync = true;
			m_bFocused = false;
			m_pInput->text().setFocused( false );
			m_pInput->setHapticsPaused( true );
			if ( m_bCursorCaptured ) {
				XUngrabPointer( m_pDisplay, CurrentTime );
				m_bCursorCaptured = false;
			}
			m_hWindow = 0;
		}
		--m_nWindowCount;
		m_hRegisteredWindows[i] = m_hRegisteredWindows[m_nWindowCount];
		m_hRegisteredContexts[i] = m_hRegisteredContexts[m_nWindowCount];
		m_hRegisteredWindows[m_nWindowCount] = 0;
		m_hRegisteredContexts[m_nWindowCount] = nullptr;
		if ( m_hWindow == 0 && m_nWindowCount != 0 ) {
			m_hWindow = m_hRegisteredWindows[0];
			m_hInputContext = m_hRegisteredContexts[0];
			if ( m_bAttached && !m_pBackend->focusWindow( *m_pInput, m_hWindow ) ) {
				return false;
			}
		}
		return true;
	}
	return false;
}

bool SIRX11InputBridge::setCursorMode( sirInputCursorMode_t eMode ) noexcept
{
	if ( eMode > SIR_INPUT_CURSOR_CAPTURED || m_pInput == nullptr || m_pDisplay == nullptr ) {
		return false;
	}
	Display *pDisplay = m_pDisplay;
	const Window hWindow = ( Window )m_hWindow;
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
			XSetICFocus( ( XIC )m_hInputContext );
		} else {
			XUnsetICFocus( ( XIC )m_hInputContext );
		}
	}
}

void SIRX11InputBridge::processEvent( const XEvent& event ) noexcept
{
	if ( m_pInput == nullptr || m_pBackend == nullptr ) {
		return;
	}
	SIRInputService& input = *m_pInput;
	SIRLinuxX11Backend *pX11 = m_pBackend;
	if ( event.type == GenericEvent ) {
		( void )pX11->processEvent( input, event );
		return;
	}
	uint32_t nIndex = m_nWindowCount;
	for ( uint32_t i = 0; i < m_nWindowCount; ++i ) {
		if ( m_hRegisteredWindows[i] == event.xany.window ) {
			nIndex = i;
			break;
		}
	}
	if ( nIndex == m_nWindowCount ) {
		return;
	}
	if ( event.type == DestroyNotify ) {
		( void )unregisterWindow( ( uintptr_t )event.xany.window );
		return;
	}
	if ( event.type == FocusIn && event.xany.window != m_hWindow ) {
		if ( m_bAttached && !m_pBackend->focusWindow( input, event.xany.window ) ) {
			return;
		}
		if ( m_hInputContext != nullptr ) {
			XUnsetICFocus( m_hInputContext );
		}
		m_hWindow = event.xany.window;
		m_hInputContext = m_hRegisteredContexts[nIndex];
		m_bFocused = false;
	}
	if ( event.xany.window != m_hWindow ) {
		return;
	}
	XEvent nativeEvent = event;
	if ( event.type == FocusIn || event.type == FocusOut ) {
		m_bFocused = event.type == FocusIn;
		input.text().setFocused( m_bFocused );
		input.setHapticsPaused( !m_bFocused );
		if ( m_hInputContext != nullptr ) {
			if ( m_bFocused && input.text().snapshot().bEnabled ) {
				XSetICFocus( ( XIC )m_hInputContext );
			} else {
				XUnsetICFocus( ( XIC )m_hInputContext );
			}
		}
		if ( m_eCursorMode == SIR_INPUT_CURSOR_CAPTURED ) {
			( void )setCursorMode( m_eCursorMode );
		}
	}
	if ( m_bFocused && input.text().snapshot().bEnabled && m_hInputContext != nullptr &&
	     ( event.type == KeyPress || event.type == KeyRelease ) ) {
		if ( XFilterEvent( &nativeEvent, ( Window )m_hWindow ) ) {
			return;
		}
		if ( event.type == KeyPress ) {
			char szUtf8[1024] = {};
			KeySym eKey = NoSymbol;
			Status eStatus = XLookupNone;
			const int nLength = Xutf8LookupString( ( XIC )m_hInputContext, &nativeEvent.xkey, szUtf8, sizeof( szUtf8 ), &eKey, &eStatus );
			if ( nLength > 0 && nLength <= ( int )sizeof( szUtf8 ) && ( eStatus == XLookupChars || eStatus == XLookupBoth ) ) {
				( void )input.text().appendUtf8( szUtf8, ( uint32_t )nLength );
			}
		}
	}
	( void )pX11->processEvent( input, event );
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
	for ( uint32_t i = 0; i < m_nWindowCount; ++i ) {
		if ( m_hRegisteredContexts[i] != nullptr ) {
			XDestroyIC( m_hRegisteredContexts[i] );
		}
		m_hRegisteredContexts[i] = nullptr;
		m_hRegisteredWindows[i] = 0;
	}
	m_nWindowCount = 0;
	if ( m_hInputMethod != nullptr ) {
		XCloseIM( m_hInputMethod );
	}
	m_pBackend->stop( *m_pInput );
	m_pInput = nullptr;
	m_pBackend = nullptr;
	m_bAttached = false;
	m_pDisplay = nullptr;
	m_hWindow = 0;
	m_hInputContext = nullptr;
	m_hInputMethod = nullptr;
	m_bFocused = false;
	m_bDeferredResync = false;
	m_bCursorCaptured = false;
	m_eCursorMode = SIR_INPUT_CURSOR_FREE;
	m_nInvisibleCursor = 0;
}
