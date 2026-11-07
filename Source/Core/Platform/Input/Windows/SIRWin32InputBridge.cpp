#include <Core/Platform/Input/Windows/SIRWin32InputBridge.h>
#include <Core/Platform/SIRPlatformNativeWindow.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <imm.h>

static bool sirWin32UpdateCursorCapture( HWND hWindow, bool bCapture, bool *pCaptured ) noexcept
{
	if ( !bCapture ) {
		if ( *pCaptured ) {
			ClipCursor( nullptr );
			*pCaptured = false;
		}
		return true;
	}
	if ( hWindow == nullptr ) {
		return false;
	}
	RECT bounds = {};
	POINT origin = { 0, 0 };
	if ( !GetClientRect( hWindow, &bounds ) || !ClientToScreen( hWindow, &origin ) ) {
		return false;
	}
	OffsetRect( &bounds, origin.x, origin.y );
	if ( !ClipCursor( &bounds ) ) {
		return false;
	}
	*pCaptured = true;
	return true;
}

SIRWin32InputBridge::~SIRWin32InputBridge( void ) noexcept
{
	unbind();
}

bool SIRWin32InputBridge::bind( SIRInputService& input, SIRWindowsRawBackend& backend, const sirWindowNativeView_t& view ) noexcept
{
	sirWindowsNativeWindow_t native = {};
	if ( m_pInput != nullptr || !sirPlatformDecodeWindowsWindow( view, &native ) || !backend.attachWindow( ( void * )native.nHWND ) ) {
		return false;
	}
	if ( !input.attach( backend ) ) {
		backend.stop( input );
		return false;
	}
	m_pInput = &input;
	m_pBackend = &backend;
	m_nWindow = native.nHWND;
	m_bFocused = GetFocus() == ( HWND )m_nWindow;
	input.text().setFocused( m_bFocused );
	input.setHapticsPaused( !m_bFocused );
	return true;
}

bool SIRWin32InputBridge::isBound( void ) const noexcept
{
	return m_pInput != nullptr;
}

bool SIRWin32InputBridge::acceptsWindow( uintptr_t nWindow ) const noexcept
{
	return m_pInput != nullptr && m_nWindow != 0 && m_nWindow == nWindow;
}

bool SIRWin32InputBridge::setCursorMode( sirInputCursorMode_t eMode ) noexcept
{
	if ( m_pInput == nullptr || eMode > SIR_INPUT_CURSOR_CAPTURED ) {
		return false;
	}
	if ( !sirWin32UpdateCursorCapture( ( HWND )m_nWindow, m_bFocused && eMode == SIR_INPUT_CURSOR_CAPTURED, &m_bCursorCaptured ) ) {
		return false;
	}
	m_eCursorMode = eMode;
	if ( m_bFocused && eMode != SIR_INPUT_CURSOR_FREE ) {
		SetCursor( nullptr );
	}
	return true;
}

sirInputCursorMode_t SIRWin32InputBridge::cursorMode( void ) const noexcept
{
	return m_eCursorMode;
}

void SIRWin32InputBridge::setTextEnabled( bool bEnabled ) noexcept
{
	if ( m_pInput != nullptr ) {
		m_pInput->text().setEnabled( bEnabled );
	}
}

void SIRWin32InputBridge::finishPump( void ) noexcept
{
	if ( m_bFocusLost && m_pInput != nullptr ) {
		m_bFocusLost = false;
		( void )m_pInput->loseBackendState( SIR_INPUT_WIN_RAW );
	}
}

intptr_t SIRWin32InputBridge::processMessage( uint32_t nMessage, uintptr_t nWParam, intptr_t nLParam, bool *pHandled ) noexcept
{
	if ( pHandled == nullptr ) {
		return 0;
	}
	*pHandled = false;
	HWND hWindow = ( HWND )m_nWindow;
	if ( hWindow == nullptr ) {
		return 0;
	}
	if ( nMessage == WM_SETCURSOR && m_eCursorMode != SIR_INPUT_CURSOR_FREE && LOWORD( nLParam ) == HTCLIENT ) {
		SetCursor( nullptr );
		*pHandled = true;
		return TRUE;
	}
	if ( ( nMessage == WM_SIZE || nMessage == WM_MOVE ) && m_bCursorCaptured ) {
		( void )sirWin32UpdateCursorCapture( hWindow, true, &m_bCursorCaptured );
	}
	if ( m_pInput != nullptr && m_pBackend != nullptr ) {
		if ( nMessage == WM_SETFOCUS ) {
			m_bFocused = true;
			m_pInput->text().setFocused( true );
			m_pInput->setHapticsPaused( false );
			if ( m_eCursorMode == SIR_INPUT_CURSOR_CAPTURED ) {
				( void )sirWin32UpdateCursorCapture( hWindow, true, &m_bCursorCaptured );
			}
		}
		if ( nMessage == WM_KILLFOCUS ) {
			m_bFocused = false;
			m_pInput->text().setFocused( false );
			m_pInput->setHapticsPaused( true );
			( void )sirWin32UpdateCursorCapture( hWindow, false, &m_bCursorCaptured );
			m_bFocusLost = true;
		}
		if ( nMessage == WM_IME_STARTCOMPOSITION || nMessage == WM_IME_ENDCOMPOSITION ) {
			m_pInput->text().clearPreedit();
		}
		if ( nMessage == WM_IME_COMPOSITION && ( nLParam & GCS_COMPSTR ) != 0 ) {
			HIMC hContext = ImmGetContext( hWindow );
			if ( hContext != nullptr ) {
				const LONG nLength = ImmGetCompositionStringW( hContext, GCS_COMPSTR, nullptr, 0 );
				if ( nLength >= 0 && ( uint32_t )nLength <= 2 * SIR_INPUT_PREEDIT_CAPACITY &&
				     ( nLength % ( LONG )sizeof( uint16_t ) ) == 0 ) {
					uint16_t szUnits[SIR_INPUT_PREEDIT_CAPACITY] = {};
					const LONG nRead = nLength != 0 ? ImmGetCompositionStringW( hContext, GCS_COMPSTR, szUnits, nLength ) : 0;
					const LONG nCaretUnits = ImmGetCompositionStringW( hContext, GCS_CURSORPOS, nullptr, 0 );
					uint32_t nCaret = 0;
					const uint32_t nUnits = ( uint32_t )nLength / sizeof( uint16_t );
					const uint32_t nLimit = nCaretUnits < 0 ? 0 : ( uint32_t )nCaretUnits > nUnits ? nUnits : ( uint32_t )nCaretUnits;
					for ( uint32_t i = 0; i < nLimit; ++i ) {
						if ( szUnits[i] < 0xdc00 || szUnits[i] > 0xdfff ) {
							++nCaret;
						}
					}
					if ( nRead == nLength ) {
						( void )m_pInput->text().setPreeditUtf16( szUnits, nUnits, nCaret, 0 );
					}
				}
				ImmReleaseContext( hWindow, hContext );
			}
		}
		if ( nMessage == WM_CHAR ) {
			const uint16_t nUnit = ( uint16_t )nWParam;
			if ( nUnit >= 32 || nUnit == 10 || nUnit == 13 || nUnit == 9 ) {
				( void )m_pInput->text().appendUtf16( nUnit );
			}
			*pHandled = true;
			return 0;
		}
		if ( nMessage == WM_UNICHAR ) {
			*pHandled = true;
			if ( nWParam == UNICODE_NOCHAR ) {
				return TRUE;
			}
			( void )m_pInput->text().appendCodepoint( ( uint32_t )nWParam );
			return 0;
		}
		if ( nMessage == WM_INPUT || nMessage == WM_INPUT_DEVICE_CHANGE ) {
			( ( SIRWindowsRawBackend * )m_pBackend )->processMessage( *m_pInput, nMessage, nWParam, nLParam );
			if ( nMessage == WM_INPUT_DEVICE_CHANGE ) {
				*pHandled = true;
				return 0;
			}
		}
	}
	// Native DefWindowProc belongs to SIRWindowDriverWin32 after the message sink.
	return 0;
}

void SIRWin32InputBridge::unbind( void ) noexcept
{
	if ( m_pInput != nullptr ) {
		( void )sirWin32UpdateCursorCapture( ( HWND )m_nWindow, false, &m_bCursorCaptured );
		m_pInput->text().setFocused( false );
		m_pInput->setHapticsPaused( true );
		m_pBackend->stop( *m_pInput );
	}
	m_pInput = nullptr;
	m_pBackend = nullptr;
	m_nWindow = 0;
	m_bFocused = false;
	m_bFocusLost = false;
	m_bCursorCaptured = false;
	m_eCursorMode = SIR_INPUT_CURSOR_FREE;
}
