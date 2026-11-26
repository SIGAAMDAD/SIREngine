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

#include <Core/Platform/Windows/Input/SIRWin32InputBridge.h>
#include <Core/Platform/Shared/Window/SIRPlatformNativeWindow.h>
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
	RECT bounds {};
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
	sirWindowsNativeWindow_t native {};
	if ( m_pInput != nullptr || !SIRPlatformNativeWindow::decodeWindows( view, &native ) || !backend.attachWindow( static_cast<void *>(native.nHWND) ) ) {
		return false;
	}
	if ( !input.attach( backend ) ) {
		backend.stop( input );
		return false;
	}
	m_pInput = &input;
	m_pBackend = &backend;
	m_nWindow = native.nHWND;
	m_nInstance = native.nHInstance;
	m_hRegisteredWindows[0] = native.nHWND;
	m_nWindowCount = 1;
	m_bFocused = GetFocus() == static_cast<HWND>(m_nWindow);
	input.text().setFocused( m_bFocused );
	input.setHapticsPaused( !m_bFocused );
	return true;
}

bool SIRWin32InputBridge::isBound( void ) const noexcept
{
	return m_pInput != nullptr;
}

bool SIRWin32InputBridge::registerWindow( const sirWindowNativeView_t& view ) noexcept
{
	sirWindowsNativeWindow_t native {};
	if ( m_pInput == nullptr || m_nWindowCount >= SIR_WINDOW_MAX_WINDOWS || !SIRPlatformNativeWindow::decodeWindows( view, &native ) ||
	     native.nHInstance != m_nInstance ) {
		return false;
	}
	for ( uint32_t i = 0; i < m_nWindowCount; ++i ) {
		if ( m_hRegisteredWindows[i] == native.nHWND ) {
			return false;
		}
	}
	m_hRegisteredWindows[m_nWindowCount++] = native.nHWND;
	return true;
}

bool SIRWin32InputBridge::unregisterWindow( uintptr_t nNativeWindow ) noexcept
{
	if ( m_pInput == nullptr || nNativeWindow == 0 ) {
		return false;
	}
	for ( uint32_t i = 0; i < m_nWindowCount; ++i ) {
		if ( m_hRegisteredWindows[i] != nNativeWindow ) {
			continue;
		}
		const bool bCurrent = m_nWindow == nNativeWindow;
		if ( bCurrent ) {
			static_cast<void>(sirWin32UpdateCursorCapture( static_cast<HWND>(m_nWindow), false, &m_bCursorCaptured ));
			m_pInput->text().setFocused( false );
			m_pInput->text().clearPreedit();
			m_pInput->setHapticsPaused( true );
			m_bFocused = false;
			m_bFocusLost = true;
		}
		--m_nWindowCount;
		for ( uint32_t j = i; j < m_nWindowCount; ++j ) {
			m_hRegisteredWindows[j] = m_hRegisteredWindows[j + 1];
		}
		m_hRegisteredWindows[m_nWindowCount] = 0;
		if ( bCurrent ) {
			const uintptr_t nReplacement = m_nWindowCount != 0 ? m_hRegisteredWindows[0] : 0;
			if ( nReplacement != 0 && !m_pBackend->retargetWindow( static_cast<void *>(nReplacement) ) ) {
				m_nWindow = 0;
				return false;
			}
			m_nWindow = nReplacement;
		}
		return true;
	}
	return false;
}

bool SIRWin32InputBridge::acceptsWindow( uintptr_t nWindow ) const noexcept
{
	if ( m_pInput == nullptr || nWindow == 0 ) {
		return false;
	}
	for ( uint32_t i = 0; i < m_nWindowCount; ++i ) {
		if ( m_hRegisteredWindows[i] == nWindow ) {
			return true;
		}
	}
	return false;
}

intptr_t SIRWin32InputBridge::processWindowMessage( const sirWindowWin32Message_t& message, bool *pHandled ) noexcept
{
	if ( pHandled == nullptr ) {
		return 0;
	}
	*pHandled = false;
	if ( !acceptsWindow( message.nWindow ) ) {
		return 0;
	}
	if ( message.nMessage == WM_SETFOCUS ) {
		if ( m_bFocusLost ) {
			finishPump();
		}
		if ( m_nWindow != message.nWindow ) {
			static_cast<void>(sirWin32UpdateCursorCapture( static_cast<HWND>(m_nWindow), false, &m_bCursorCaptured ));
			if ( m_pBackend == nullptr || !m_pBackend->retargetWindow( static_cast<void *>(message.nWindow) ) ) {
				return 0;
			}
			m_pInput->text().clearPreedit();
			m_nWindow = message.nWindow;
		}
	} else if ( message.nWindow != m_nWindow ) {
		// Old HWND messages must not change the new focus owner's devices.
		return 0;
	}
	return processMessage( message.nMessage, message.nWParam, message.nLParam, pHandled );
}

bool SIRWin32InputBridge::setCursorMode( SIRInputCursorMode eMode ) noexcept
{
	if ( m_pInput == nullptr || eMode > SIRInputCursorMode::SIR_INPUT_CURSOR_CAPTURED ) {
		return false;
	}
	if ( !sirWin32UpdateCursorCapture( static_cast<HWND>(m_nWindow), m_bFocused && eMode == SIRInputCursorMode::SIR_INPUT_CURSOR_CAPTURED, &m_bCursorCaptured ) ) {
		return false;
	}
	m_eCursorMode = eMode;
	if ( m_bFocused && eMode != SIRInputCursorMode::SIR_INPUT_CURSOR_FREE ) {
		SetCursor( nullptr );
	}
	return true;
}

SIRInputCursorMode SIRWin32InputBridge::cursorMode( void ) const noexcept
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
		static_cast<void>(m_pInput->loseBackendState( SIRInputBackendId::SIR_INPUT_WIN_RAW ));
	}
}

intptr_t SIRWin32InputBridge::processMessage( uint32_t nMessage, uintptr_t nWParam, intptr_t nLParam, bool *pHandled ) noexcept
{
	if ( pHandled == nullptr ) {
		return 0;
	}
	*pHandled = false;
	HWND hWindow = static_cast<HWND>(m_nWindow);
	if ( hWindow == nullptr ) {
		return 0;
	}
	if ( nMessage == WM_SETCURSOR && m_eCursorMode != SIRInputCursorMode::SIR_INPUT_CURSOR_FREE && LOWORD( nLParam ) == HTCLIENT ) {
		SetCursor( nullptr );
		*pHandled = true;
		return TRUE;
	}
	if ( ( nMessage == WM_SIZE || nMessage == WM_MOVE ) && m_bCursorCaptured ) {
		static_cast<void>(sirWin32UpdateCursorCapture( hWindow, true, &m_bCursorCaptured ));
	}
	if ( m_pInput != nullptr && m_pBackend != nullptr ) {
		if ( nMessage == WM_SETFOCUS ) {
			m_bFocused = true;
			m_pInput->text().setFocused( true );
			m_pInput->setHapticsPaused( false );
			if ( m_eCursorMode == SIRInputCursorMode::SIR_INPUT_CURSOR_CAPTURED ) {
				static_cast<void>(sirWin32UpdateCursorCapture( hWindow, true, &m_bCursorCaptured ));
			}
		}
		if ( nMessage == WM_KILLFOCUS ) {
			m_bFocused = false;
			m_pInput->text().setFocused( false );
			m_pInput->setHapticsPaused( true );
			static_cast<void>(sirWin32UpdateCursorCapture( hWindow, false, &m_bCursorCaptured ));
			m_bFocusLost = true;
		}
		if ( nMessage == WM_IME_STARTCOMPOSITION || nMessage == WM_IME_ENDCOMPOSITION ) {
			m_pInput->text().clearPreedit();
		}
		if ( nMessage == WM_IME_COMPOSITION && ( nLParam & GCS_COMPSTR ) != 0 ) {
			HIMC hContext = ImmGetContext( hWindow );
			if ( hContext != nullptr ) {
				const LONG nLength = ImmGetCompositionStringW( hContext, GCS_COMPSTR, nullptr, 0 );
				if ( nLength >= 0 && static_cast<uint32_t>(nLength) <= 2 * SIR_INPUT_PREEDIT_CAPACITY &&
				     ( nLength % static_cast<LONG>(sizeof( uint16_t ) ) == 0 )) {
					uint16_t szUnits[SIR_INPUT_PREEDIT_CAPACITY];
					memset( szUnits, 0, sizeof( szUnits ) );
					const LONG nRead = nLength != 0 ? ImmGetCompositionStringW( hContext, GCS_COMPSTR, szUnits, nLength ) : 0;
					const LONG nCaretUnits = ImmGetCompositionStringW( hContext, GCS_CURSORPOS, nullptr, 0 );
					uint32_t nCaret = 0;
					const uint32_t nUnits = static_cast<uint32_t>(nLength) / sizeof( uint16_t );
					const uint32_t nLimit = nCaretUnits < 0 ? 0 : static_cast<uint32_t>(nCaretUnits) > nUnits ? nUnits : static_cast<uint32_t>(nCaretUnits);
					for ( uint32_t i = 0; i < nLimit; ++i ) {
						if ( szUnits[i] < 0xdc00 || szUnits[i] > 0xdfff ) {
							++nCaret;
						}
					}
					if ( nRead == nLength ) {
						static_cast<void>(m_pInput->text().setPreeditUtf16( szUnits, nUnits, nCaret, 0 ));
					}
				}
				ImmReleaseContext( hWindow, hContext );
			}
		}
		if ( nMessage == WM_CHAR ) {
			const uint16_t nUnit = static_cast<uint16_t>(nWParam);
			if ( nUnit >= 32 || nUnit == 10 || nUnit == 13 || nUnit == 9 ) {
				static_cast<void>(m_pInput->text().appendUtf16( nUnit ));
			}
			*pHandled = true;
			return 0;
		}
		if ( nMessage == WM_UNICHAR ) {
			*pHandled = true;
			if ( nWParam == UNICODE_NOCHAR ) {
				return TRUE;
			}
			static_cast<void>(m_pInput->text().appendCodepoint( static_cast<uint32_t>(nWParam) ));
			return 0;
		}
		if ( nMessage == WM_INPUT || nMessage == WM_INPUT_DEVICE_CHANGE ) {
			static_cast<SIRWindowsRawBackend *>(m_pBackend)->processMessage( *m_pInput, nMessage, nWParam, nLParam );
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
		static_cast<void>(sirWin32UpdateCursorCapture( static_cast<HWND>(m_nWindow), false, &m_bCursorCaptured ));
		m_pInput->text().setFocused( false );
		m_pInput->setHapticsPaused( true );
		m_pBackend->stop( *m_pInput );
	}
	m_pInput = nullptr;
	m_pBackend = nullptr;
	m_nWindow = 0;
	m_nInstance = 0;
	m_nWindowCount = 0;
	for ( uint32_t i = 0; i < SIR_WINDOW_MAX_WINDOWS; ++i ) {
		m_hRegisteredWindows[i] = 0;
	}
	m_bFocused = false;
	m_bFocusLost = false;
	m_bCursorCaptured = false;
	m_eCursorMode = SIRInputCursorMode::SIR_INPUT_CURSOR_FREE;
}
