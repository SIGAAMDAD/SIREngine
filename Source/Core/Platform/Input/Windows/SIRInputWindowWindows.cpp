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
#include <Core/Platform/Input/Windows/SIRWindowsRawBackend.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <imm.h>

static constexpr wchar_t s_szInputClass[] = L"SIREngineNativeInputWindow";

static bool sirInputUpdateWinCursor( HWND hWindow, bool bCapture, bool *pCaptured ) noexcept
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


static LRESULT CALLBACK sirInputNativeWindowProc( HWND hWindow, UINT nMessage, WPARAM nWParam, LPARAM nLParam )
{
	if ( nMessage == WM_NCCREATE ) {
		const CREATESTRUCTW *pCreate = ( const CREATESTRUCTW * )nLParam;
		SetWindowLongPtrW( hWindow, GWLP_USERDATA, ( LONG_PTR )pCreate->lpCreateParams );
		( ( SIRInputWindow * )pCreate->lpCreateParams )->adoptNativeWindow( ( uintptr_t )hWindow );
	}

	SIRInputWindow *pWindow = ( SIRInputWindow * )GetWindowLongPtrW( hWindow, GWLP_USERDATA );
	if ( pWindow != nullptr ) {
		return ( LRESULT )pWindow->dispatchNativeMessage( nMessage, ( uintptr_t )nWParam, ( intptr_t )nLParam );
	}
	return DefWindowProcW( hWindow, nMessage, nWParam, nLParam );
}

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

bool SIRInputWindow::open( SIRInputService &input, SIRInputBackend &desktop ) noexcept
{
	if ( m_pInput != nullptr || m_nWindow != 0 || desktop.id() != SIR_INPUT_WIN_RAW ) {
		return false;
	}

	WNDCLASSW windowClass = {};
	windowClass.lpfnWndProc = sirInputNativeWindowProc;
	windowClass.hCursor = LoadCursorW( nullptr, IDC_ARROW );
	windowClass.hInstance = GetModuleHandleW( nullptr );
	windowClass.lpszClassName = s_szInputClass;
	const ATOM nClass = RegisterClassW( &windowClass );
	if ( nClass == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS ) {
		return false;
	}
	m_bRegistered = nClass != 0;

	HWND hWindow = CreateWindowExW( 0, s_szInputClass, L"SIREngine native input", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800,
	                                450, nullptr, nullptr, windowClass.hInstance, this );
	if ( hWindow == nullptr ) {
		close();
		return false;
	}

	m_nWindow = ( uintptr_t )hWindow;
	SIRWindowsRawBackend &raw = ( SIRWindowsRawBackend & )desktop;
	if ( !raw.attachWindow( hWindow ) ) {
		close();
		return false;
	}
	if ( !input.attach( desktop ) ) {
		raw.stop( input );
		close();
		return false;
	}

	m_pInput = &input;
	m_pDesktop = &desktop;
	m_bOpen = true;
	m_bAttached = true;
	ShowWindow( hWindow, SW_SHOWNORMAL );
	return true;
}

void SIRInputWindow::adoptNativeWindow( uintptr_t nWindow ) noexcept
{
	m_nWindow = nWindow;
}

intptr_t SIRInputWindow::dispatchNativeMessage( uint32_t nMessage, uintptr_t nWParam, intptr_t nLParam ) noexcept
{
	HWND hWindow = ( HWND )m_nWindow;
	if ( nMessage == WM_CLOSE ) {
		if ( hWindow != nullptr ) {
			DestroyWindow( hWindow );
		}
		return 0;
	}
	if ( nMessage == WM_DESTROY ) {
		( void )sirInputUpdateWinCursor( hWindow, false, &m_bCursorCaptured );
		m_bFocused = false;
		m_bOpen = false;
		return 0;
	}
	if ( nMessage == WM_NCDESTROY ) {
		SetWindowLongPtrW( hWindow, GWLP_USERDATA, 0 );
		m_nWindow = 0;
		return ( intptr_t )DefWindowProcW( hWindow, nMessage, ( WPARAM )nWParam, ( LPARAM )nLParam );
	}
	if ( nMessage == WM_SETCURSOR && m_eCursorMode != SIR_INPUT_CURSOR_FREE &&
	     LOWORD( nLParam ) == HTCLIENT ) {
		SetCursor( nullptr );
		return TRUE;
	}
	if ( ( nMessage == WM_SIZE || nMessage == WM_MOVE ) && m_bCursorCaptured ) {
		( void )sirInputUpdateWinCursor( hWindow, true, &m_bCursorCaptured );
	}
	if ( m_pInput != nullptr && m_pDesktop != nullptr ) {
		if ( nMessage == WM_SETFOCUS ) {
			m_bFocused = true;
			m_pInput->text().setFocused( true );
			m_pInput->setHapticsPaused( false );
			if ( m_eCursorMode == SIR_INPUT_CURSOR_CAPTURED ) {
				( void )sirInputUpdateWinCursor( hWindow, true, &m_bCursorCaptured );
			}
		}
		if ( nMessage == WM_KILLFOCUS ) {
			m_bFocused = false;
			m_pInput->text().setFocused( false );
			m_pInput->setHapticsPaused( true );
			( void )sirInputUpdateWinCursor( hWindow, false, &m_bCursorCaptured );
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
			return 0;
		}
		if ( nMessage == WM_UNICHAR ) {
			if ( nWParam == UNICODE_NOCHAR ) {
				return TRUE;
			}
			( void )m_pInput->text().appendCodepoint( ( uint32_t )nWParam );
			return 0;
		}
		if ( nMessage == WM_INPUT || nMessage == WM_INPUT_DEVICE_CHANGE ) {
			( ( SIRWindowsRawBackend * )m_pDesktop )->processMessage( *m_pInput, nMessage, nWParam, nLParam );
			if ( nMessage == WM_INPUT_DEVICE_CHANGE ) {
				return 0;
			}
		}

	}
	return ( intptr_t )DefWindowProcW( hWindow, nMessage, ( WPARAM )nWParam, ( LPARAM )nLParam );
}

bool SIRInputWindow::pump( SIRInputService &input ) noexcept
{
	if ( &input != m_pInput || !m_bOpen ) {
		return false;
	}

	MSG message = {};
	while ( m_bOpen && PeekMessageW( &message, ( HWND )m_nWindow, 0, 0, PM_REMOVE ) ) {
		TranslateMessage( &message );
		DispatchMessageW( &message );
	}
	if ( m_bFocusLost ) {
		m_bFocusLost = false;
		( void )input.loseBackendState( SIR_INPUT_WIN_RAW );
	}
	return m_bOpen;
}

bool SIRInputWindow::setCursorMode( sirInputCursorMode_t eMode ) noexcept
{
	if ( eMode > SIR_INPUT_CURSOR_CAPTURED || !m_bOpen ) {
		return false;
	}
	const bool bCapture = m_bFocused && eMode == SIR_INPUT_CURSOR_CAPTURED;
	if ( !sirInputUpdateWinCursor( ( HWND )m_nWindow, bCapture, &m_bCursorCaptured ) ) {
		return false;
	}
	m_eCursorMode = eMode;
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
	if ( m_pInput != nullptr ) {
		m_pInput->text().setEnabled( bEnabled );
	}
}

bool SIRInputWindow::isOpen( void ) const noexcept
{
	return m_bOpen;
}

void SIRInputWindow::close( void ) noexcept
{
	( void )sirInputUpdateWinCursor( ( HWND )m_nWindow, false, &m_bCursorCaptured );
	if ( m_nWindow != 0 ) {
		DestroyWindow( ( HWND )m_nWindow );
		m_nWindow = 0;
	}
	if ( m_bRegistered ) {
		UnregisterClassW( s_szInputClass, GetModuleHandleW( nullptr ) );
	}
	m_bRegistered = false;
	m_bFocusLost = false;
	m_bFocused = false;
	m_eCursorMode = SIR_INPUT_CURSOR_FREE;
	m_bOpen = false;
	m_bAttached = false;
	m_pInput = nullptr;
	m_pDesktop = nullptr;
	m_pDisplay = nullptr;
}
