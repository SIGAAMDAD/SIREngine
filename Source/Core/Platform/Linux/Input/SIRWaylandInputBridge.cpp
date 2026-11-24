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
#include <Core/Platform/Linux/Input/SIRWaylandInputBridge.h>
#include <Core/Platform/Shared/Window/SIRPlatformNativeWindow.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#ifdef SIR_WAYLAND_POINTER_PROTOCOLS_AVAILABLE
#include <pointer-constraints-unstable-v1-client-protocol.h>
#include <relative-pointer-unstable-v1-client-protocol.h>
#endif
#ifdef SIR_WAYLAND_XKB_AVAILABLE
#include <xkbcommon/xkbcommon.h>
#endif

SIRWaylandInputBridge::~SIRWaylandInputBridge( void ) noexcept
{
	unbind();
}

SIRInputBackendId SIRWaylandInputBridge::id( void ) const noexcept
{
	return SIRInputBackendId::SIR_INPUT_LINUX_WAYLAND;
}

void SIRWaylandInputBridge::sample( SIRInputService& service ) noexcept
{
	static_cast<void>(service);
}

void SIRWaylandInputBridge::resynchronize( SIRInputService& service ) noexcept
{
	if ( m_bKeyboardDevice ) {
		static_cast<void>(service.loseState( id(), m_hKeyboard ));
	}
	if ( m_bPointerDevice ) {
		static_cast<void>(service.loseState( id(), m_hPointer ));
	}
	service.text().clearPreedit();
	service.text().setFocused( m_pKeyboardFocus != nullptr );
	service.setHapticsPaused( m_pKeyboardFocus == nullptr );
	endPointerCapture();
	m_bHavePointerPosition = false;
	m_nRemainderX = 0;
	m_nRemainderY = 0;
	m_nWheelRemainder[0] = 0;
	m_nWheelRemainder[1] = 0;
	m_nRelativeRemainderX = 0;
	m_nRelativeRemainderY = 0;
}

bool SIRWaylandInputBridge::vibrate( sirInputDeviceId_t hDevice, uint16_t nLeft, uint16_t nRight ) noexcept
{
	static_cast<void>(hDevice);
	static_cast<void>(nLeft);
	static_cast<void>(nRight);
	return false;
}

bool SIRWaylandInputBridge::bind( SIRInputService& input, SIRWindowDriverWayland& driver, const sirWindowNativeView_t& view ) noexcept
{
	sirLinuxNativeWindow_t native {};
	wl_seat *pSeat = driver.borrowSeat();
	if ( m_pInput != nullptr || !SIRPlatformNativeWindow::decodeLinux( view, &native ) || native.eDisplayType != sirLinuxDisplayKind::SIR_LINUX_DISPLAY_WAYLAND ||
	     pSeat == nullptr ) {
		return false;
	}
	m_pInput = &input;
	m_pDriver = &driver;
	m_pDisplay = reinterpret_cast<wl_display *>(native.nDisplay);
	m_pSeat = pSeat;
	m_Surfaces[0] = reinterpret_cast<wl_surface *>(native.nSurface);
	m_nWindowCount = 1;
	static const wl_seat_listener s_SeatListener = { seatCapabilities, seatName };
	if ( !driver.setSeatLostSink( seatLost, this ) || !driver.setPointerProtocolLostSink( pointerProtocolsLost, this ) ||
	     wl_seat_add_listener( pSeat, &s_SeatListener, this ) != 0 ) {
		unbind();
		return false;
	}
	if ( !input.attach( *this ) ) {
		unbind();
		return false;
	}
	return true;
}

bool SIRWaylandInputBridge::registerWindow( const sirWindowNativeView_t& view ) noexcept
{
	sirLinuxNativeWindow_t native {};
	if ( m_pInput == nullptr || m_bSeatLost || !SIRPlatformNativeWindow::decodeLinux( view, &native ) ||
	     native.eDisplayType != sirLinuxDisplayKind::SIR_LINUX_DISPLAY_WAYLAND || native.nDisplay != reinterpret_cast<uintptr_t>(m_pDisplay) ) {
		return false;
	}
	wl_surface *pSurface = reinterpret_cast<wl_surface *>(native.nSurface);
	for ( uint32_t i = 0; i < m_nWindowCount; ++i ) {
		if ( m_Surfaces[i] == pSurface ) {
			return false;
		}
	}
	if ( m_nWindowCount == SIR_WINDOW_MAX_WINDOWS ) {
		return false;
	}
	m_Surfaces[m_nWindowCount++] = pSurface;
	return true;
}

void SIRWaylandInputBridge::unregisterWindow( uintptr_t nNativeSurface ) noexcept
{
	wl_surface *pSurface = reinterpret_cast<wl_surface *>(nNativeSurface);
	for ( uint32_t i = 0; i < m_nWindowCount; ++i ) {
		if ( m_Surfaces[i] != pSurface ) {
			continue;
		}
		if ( m_pKeyboardFocus == pSurface ) {
			m_pKeyboardFocus = nullptr;
			if ( m_pInput != nullptr ) {
				m_pInput->text().setFocused( false );
				m_pInput->setHapticsPaused( true );
				if ( m_bKeyboardDevice ) {
					m_bKeyboardResyncPending = true;
				}
			}
		}
		if ( m_pPointerFocus == pSurface ) {
			endPointerCapture();
			m_pPointerFocus = nullptr;
			m_bHavePointerPosition = false;
			m_nWheelRemainder[0] = 0;
			m_nWheelRemainder[1] = 0;
			if ( m_bPointerDevice && m_pInput != nullptr ) {
				m_bPointerResyncPending = true;
			}
		}
		for ( uint32_t j = i + 1; j < m_nWindowCount; ++j ) {
			m_Surfaces[j - 1] = m_Surfaces[j];
		}
		m_Surfaces[--m_nWindowCount] = nullptr;
		return;
	}
}

bool SIRWaylandInputBridge::beginFrame( void ) noexcept
{
	if ( m_pInput == nullptr ) {
		return false;
	}
	// Platform native window teardown can happen after the input frame was
	// published. Deliver loss events at the next collecting frame boundary.
	if ( m_bKeyboardResyncPending ) {
		if ( m_bKeyboardDevice && !m_pInput->loseState( id(), m_hKeyboard ) ) {
			return false;
		}
		m_bKeyboardResyncPending = false;
	}
	if ( m_bPointerResyncPending ) {
		if ( m_bPointerDevice && !m_pInput->loseState( id(), m_hPointer ) ) {
			return false;
		}
		m_bPointerResyncPending = false;
	}
	if ( m_bSeatLost && m_pDriver != nullptr ) {
		wl_seat *pReplacement = m_pDriver->borrowSeat();
		if ( pReplacement != nullptr ) {
			static const wl_seat_listener s_SeatListener = { seatCapabilities, seatName };
			if ( wl_seat_add_listener( pReplacement, &s_SeatListener, this ) != 0 ) {
				return false;
			}
			m_pSeat = pReplacement;
			m_bSeatLost = false;
		}
	}
	if ( !m_bSeatLost ) {
		ensureDevices();
		if ( m_eCursorMode == SIRInputCursorMode::SIR_INPUT_CURSOR_CAPTURED && m_pLockedPointer == nullptr ) {
			static_cast<void>(beginPointerCapture());
		}
	}
	return true;
}

bool SIRWaylandInputBridge::isBound( void ) const noexcept
{
	return m_pInput != nullptr;
}

void SIRWaylandInputBridge::setTextEnabled( bool bEnabled ) noexcept
{
	if ( m_pInput != nullptr ) {
		m_pInput->text().setEnabled( bEnabled );
	}
}

bool SIRWaylandInputBridge::setCursorMode( SIRInputCursorMode eMode ) noexcept
{
	if ( m_pInput == nullptr || eMode == SIRInputCursorMode::SIR_INPUT_CURSOR_HIDDEN || eMode > SIRInputCursorMode::SIR_INPUT_CURSOR_CAPTURED ) {
		return false;
	}
	if ( eMode == SIRInputCursorMode::SIR_INPUT_CURSOR_FREE ) {
		endPointerCapture();
		m_eCursorMode = SIRInputCursorMode::SIR_INPUT_CURSOR_FREE;
		return true;
	}
	if ( !beginPointerCapture() ) {
		return false;
	}
	m_eCursorMode = SIRInputCursorMode::SIR_INPUT_CURSOR_CAPTURED;
	return true;
}

SIRInputCursorMode SIRWaylandInputBridge::cursorMode( void ) const noexcept
{
	return m_eCursorMode;
}

bool SIRWaylandInputBridge::beginPointerCapture( void ) noexcept
{
#ifdef SIR_WAYLAND_POINTER_PROTOCOLS_AVAILABLE
	if ( m_pLockedPointer != nullptr ) {
		return true;
	}
	if ( m_pDriver == nullptr || m_pPointer == nullptr || m_pPointerFocus == nullptr || !m_bPointerDevice || m_bSeatLost ) {
		return false;
	}
	zwp_pointer_constraints_v1 *pConstraints = m_pDriver->borrowPointerConstraints();
	zwp_relative_pointer_manager_v1 *pRelativeManager = m_pDriver->borrowRelativePointerManager();
	if ( pConstraints == nullptr || pRelativeManager == nullptr ) {
		return false;
	}
	static const zwp_relative_pointer_v1_listener s_RelativeListener = { relativeMotion };
	static const zwp_locked_pointer_v1_listener s_LockListener = { pointerLocked, pointerUnlocked };
	zwp_relative_pointer_v1 *pRelative = zwp_relative_pointer_manager_v1_get_relative_pointer( pRelativeManager, m_pPointer );
	if ( pRelative == nullptr ) {
		return false;
	}
	if ( zwp_relative_pointer_v1_add_listener( pRelative, &s_RelativeListener, this ) != 0 ) {
		zwp_relative_pointer_v1_destroy( pRelative );
		return false;
	}
	zwp_locked_pointer_v1 *pLock = zwp_pointer_constraints_v1_lock_pointer( pConstraints, m_pPointerFocus, m_pPointer, nullptr,
	                                                                        ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT );
	if ( pLock == nullptr ) {
		zwp_relative_pointer_v1_destroy( pRelative );
		return false;
	}
	if ( zwp_locked_pointer_v1_add_listener( pLock, &s_LockListener, this ) != 0 ) {
		zwp_locked_pointer_v1_destroy( pLock );
		zwp_relative_pointer_v1_destroy( pRelative );
		return false;
	}
	m_pRelativePointer = pRelative;
	m_pLockedPointer = pLock;
	m_bPointerLocked = false;
	m_nRelativeRemainderX = 0;
	m_nRelativeRemainderY = 0;
	return true;
#else
	return false;
#endif
}

void SIRWaylandInputBridge::endPointerCapture( void ) noexcept
{
#ifdef SIR_WAYLAND_POINTER_PROTOCOLS_AVAILABLE
	if ( m_pLockedPointer != nullptr ) {
		zwp_locked_pointer_v1_destroy( m_pLockedPointer );
		m_pLockedPointer = nullptr;
	}
	if ( m_pRelativePointer != nullptr ) {
		zwp_relative_pointer_v1_destroy( m_pRelativePointer );
		m_pRelativePointer = nullptr;
	}
#endif
	m_bPointerLocked = false;
	m_nRelativeRemainderX = 0;
	m_nRelativeRemainderY = 0;
}

void SIRWaylandInputBridge::pointerProtocolsLost( void *pContext ) noexcept
{
	static_cast<SIRWaylandInputBridge *>(pContext)->endPointerCapture();
}

void SIRWaylandInputBridge::relativeMotion( void *pContext, zwp_relative_pointer_v1 *pRelative, uint32_t nTimeHi, uint32_t nTimeLo,
                                            wl_fixed_t nDx, wl_fixed_t nDy, wl_fixed_t nDxUnaccel, wl_fixed_t nDyUnaccel ) noexcept
{
	static_cast<void>(nTimeHi);
	static_cast<void>(nTimeLo);
	static_cast<void>(nDx);
	static_cast<void>(nDy);
	SIRWaylandInputBridge *pBridge = static_cast<SIRWaylandInputBridge *>(pContext);
	if ( pBridge->m_pRelativePointer != pRelative || !pBridge->m_bPointerLocked || pBridge->m_pPointerFocus == nullptr ||
	     !pBridge->m_bPointerDevice ) {
		return;
	}
	const int64_t nX = static_cast<int64_t>(nDxUnaccel) + pBridge->m_nRelativeRemainderX;
	const int64_t nY = static_cast<int64_t>(nDyUnaccel) + pBridge->m_nRelativeRemainderY;
	const int32_t nMoveX = static_cast<int32_t>( nX / 256 );
	const int32_t nMoveY = static_cast<int32_t>( nY / 256 );
	pBridge->m_nRelativeRemainderX = static_cast<int32_t>( nX % 256 );
	pBridge->m_nRelativeRemainderY = static_cast<int32_t>( nY % 256 );
	if ( nMoveX != 0 || nMoveY != 0 ) {
		static_cast<void>(pBridge->m_pInput->emitPointer( pBridge->id(), pBridge->m_hPointer, SIRInputSignal::SIR_INPUT_MOTION, 0, nMoveX, nMoveY ));
	}
}

void SIRWaylandInputBridge::pointerLocked( void *pContext, zwp_locked_pointer_v1 *pLock ) noexcept
{
	SIRWaylandInputBridge *pBridge = static_cast<SIRWaylandInputBridge *>(pContext);
	if ( pBridge->m_pLockedPointer == pLock ) {
		pBridge->m_bPointerLocked = true;
		pBridge->m_bHavePointerPosition = false;
	}
}

void SIRWaylandInputBridge::pointerUnlocked( void *pContext, zwp_locked_pointer_v1 *pLock ) noexcept
{
	SIRWaylandInputBridge *pBridge = static_cast<SIRWaylandInputBridge *>(pContext);
	if ( pBridge->m_pLockedPointer == pLock ) {
		pBridge->m_bPointerLocked = false;
		pBridge->m_bHavePointerPosition = false;
	}
}

void SIRWaylandInputBridge::ensureDevices( void ) noexcept
{
	if ( m_pInput == nullptr || m_bSeatLost || m_pSeat == nullptr ) {
		return;
	}
	const uint64_t nSeatId = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(m_pSeat));
	if ( m_pKeyboard != nullptr && !m_bKeyboardDevice ) {
		m_bKeyboardDevice = m_pInput->connect( { 0, nSeatId, id(), SIRInputDeviceClass::SIR_INPUT_KEYBOARD }, &m_hKeyboard );
	}
	if ( m_pPointer != nullptr && !m_bPointerDevice ) {
		m_bPointerDevice = m_pInput->connect( { 0, nSeatId ^ UINT64_C( 1 ), id(), SIRInputDeviceClass::SIR_INPUT_POINTER }, &m_hPointer );
	}
}

bool SIRWaylandInputBridge::validSurface( wl_surface *pSurface ) const noexcept
{
	if ( pSurface == nullptr ) {
		return false;
	}
	for ( uint32_t i = 0; i < m_nWindowCount; ++i ) {
		if ( m_Surfaces[i] == pSurface ) {
			return true;
		}
	}
	return false;
}

void SIRWaylandInputBridge::clearKeymap( void ) noexcept
{
#ifdef SIR_WAYLAND_XKB_AVAILABLE
	if ( m_pKeyState != nullptr ) {
		xkb_state_unref( m_pKeyState );
	}
	if ( m_pKeymap != nullptr ) {
		xkb_keymap_unref( m_pKeymap );
	}
	if ( m_pXkbContext != nullptr ) {
		xkb_context_unref( m_pXkbContext );
	}
#endif
	m_pKeyState = nullptr;
	m_pKeymap = nullptr;
	m_pXkbContext = nullptr;
}

void SIRWaylandInputBridge::clearKeyboard( void ) noexcept
{
	if ( m_pInput != nullptr && m_bKeyboardDevice ) {
		static_cast<void>(m_pInput->disconnect( id(), m_hKeyboard ));
	}
	m_bKeyboardDevice = false;
	m_bKeyboardResyncPending = false;
	m_hKeyboard = {};
	m_pKeyboardFocus = nullptr;
	clearKeymap();
	if ( m_pKeyboard != nullptr ) {
		if ( wl_proxy_get_version( reinterpret_cast<wl_proxy *>(m_pKeyboard) ) >= 3 ) {
			wl_keyboard_release( m_pKeyboard );
		} else {
			wl_keyboard_destroy( m_pKeyboard );
		}
		m_pKeyboard = nullptr;
	}
	if ( m_pInput != nullptr ) {
		m_pInput->text().setFocused( false );
		m_pInput->setHapticsPaused( true );
	}
}

void SIRWaylandInputBridge::clearPointer( void ) noexcept
{
	if ( m_pInput != nullptr && m_bPointerDevice ) {
		static_cast<void>(m_pInput->disconnect( id(), m_hPointer ));
	}
	m_bPointerDevice = false;
	m_bPointerResyncPending = false;
	m_hPointer = {};
	m_pPointerFocus = nullptr;
	m_bHavePointerPosition = false;
	m_nWheelRemainder[0] = 0;
	m_nWheelRemainder[1] = 0;
	endPointerCapture();
	if ( m_pPointer != nullptr ) {
		if ( wl_proxy_get_version( reinterpret_cast<wl_proxy *>(m_pPointer) ) >= 3 ) {
			wl_pointer_release( m_pPointer );
		} else {
			wl_pointer_destroy( m_pPointer );
		}
		m_pPointer = nullptr;
	}
}

void SIRWaylandInputBridge::releaseSeatObjects( void ) noexcept
{
	clearKeyboard();
	clearPointer();
}

void SIRWaylandInputBridge::seatLost( void *pContext ) noexcept
{
	SIRWaylandInputBridge *pBridge = static_cast<SIRWaylandInputBridge *>(pContext);
	pBridge->releaseSeatObjects();
	pBridge->m_bSeatLost = true;
	pBridge->m_pSeat = nullptr;
}

void SIRWaylandInputBridge::seatCapabilities( void *pContext, wl_seat *pSeat, uint32_t nCapabilities ) noexcept
{
	SIRWaylandInputBridge *pBridge = static_cast<SIRWaylandInputBridge *>(pContext);
	if ( pBridge->m_pSeat != pSeat || pBridge->m_bSeatLost ) {
		return;
	}
	static const wl_keyboard_listener s_KeyboardListener = { keyboardKeymap, keyboardEnter,     keyboardLeave,
	                                                         keyboardKey,    keyboardModifiers, keyboardRepeat };
	static const wl_pointer_listener s_PointerListener = { pointerEnter,      pointerLeave,    pointerMotion,
	                                                       pointerButton,     pointerAxis,     pointerFrame,
	                                                       pointerAxisSource, pointerAxisStop, pointerAxisDiscrete };
	if ( ( nCapabilities & WL_SEAT_CAPABILITY_KEYBOARD ) == 0 ) {
		pBridge->clearKeyboard();
	} else if ( pBridge->m_pKeyboard == nullptr ) {
		pBridge->m_pKeyboard = wl_seat_get_keyboard( pSeat );
		if ( pBridge->m_pKeyboard != nullptr && wl_keyboard_add_listener( pBridge->m_pKeyboard, &s_KeyboardListener, pBridge ) != 0 ) {
			pBridge->clearKeyboard();
		}
	}
	if ( ( nCapabilities & WL_SEAT_CAPABILITY_POINTER ) == 0 ) {
		pBridge->clearPointer();
	} else if ( pBridge->m_pPointer == nullptr ) {
		pBridge->m_pPointer = wl_seat_get_pointer( pSeat );
		if ( pBridge->m_pPointer != nullptr && wl_pointer_add_listener( pBridge->m_pPointer, &s_PointerListener, pBridge ) != 0 ) {
			pBridge->clearPointer();
		}
	}
	pBridge->ensureDevices();
}

void SIRWaylandInputBridge::seatName( void *pContext, wl_seat *pSeat, const char *pszName ) noexcept
{
	static_cast<void>(pContext);
	static_cast<void>(pSeat);
	static_cast<void>(pszName);
}

void SIRWaylandInputBridge::keyboardKeymap( void *pContext, wl_keyboard *pKeyboard, uint32_t nFormat, int32_t nFd, uint32_t nSize ) noexcept
{
	SIRWaylandInputBridge *pBridge = static_cast<SIRWaylandInputBridge *>(pContext);
	if ( pBridge->m_pKeyboard != pKeyboard ) {
		if ( nFd >= 0 ) {
			close( nFd );
		}
		return;
	}
	pBridge->clearKeymap();
#ifdef SIR_WAYLAND_XKB_AVAILABLE
	if ( nFormat == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 && nFd >= 0 && nSize > 0 && nSize <= UINT32_C( 1 ) << 20 ) {
		struct stat info {};
		const bool bSized = fstat( nFd, &info ) == 0 && info.st_size >= static_cast<int64_t>(nSize);
		const void *pData = bSized ? mmap( nullptr, nSize, PROT_READ, MAP_PRIVATE, nFd, 0 ) : MAP_FAILED;
		if ( pData != MAP_FAILED ) {
			const char *pszKeymap = static_cast<const char *>(pData);
			// The protocol specifies a NUL-terminated keymap, but a malicious
			// compositor can provide an unterminated or truncated fd mapping.
			if ( pszKeymap[nSize - 1] == '\0' ) {
				pBridge->m_pXkbContext = xkb_context_new( XKB_CONTEXT_NO_FLAGS );
				if ( pBridge->m_pXkbContext != nullptr ) {
					pBridge->m_pKeymap = xkb_keymap_new_from_string( pBridge->m_pXkbContext, pszKeymap, XKB_KEYMAP_FORMAT_TEXT_V1,
					                                                 XKB_KEYMAP_COMPILE_NO_FLAGS );
					if ( pBridge->m_pKeymap != nullptr ) {
						pBridge->m_pKeyState = xkb_state_new( pBridge->m_pKeymap );
					}
				}
			}
			munmap( const_cast<void *>(pData), nSize );
		}
	}
#else
	static_cast<void>(nFormat);
	static_cast<void>(nSize);
#endif
	if ( nFd >= 0 ) {
		close( nFd );
	}
	if ( pBridge->m_bKeyboardDevice && pBridge->m_pInput != nullptr ) {
		static_cast<void>(pBridge->m_pInput->loseState( pBridge->id(), pBridge->m_hKeyboard ));
	}
}

void SIRWaylandInputBridge::keyboardEnter( void *pContext, wl_keyboard *pKeyboard, uint32_t nSerial, wl_surface *pSurface,
                                           wl_array *pKeys ) noexcept
{
	static_cast<void>(nSerial);
	SIRWaylandInputBridge *pBridge = static_cast<SIRWaylandInputBridge *>(pContext);
	if ( pBridge->m_pKeyboard != pKeyboard || !pBridge->validSurface( pSurface ) ) {
		return;
	}
	if ( pBridge->m_pKeyboardFocus != nullptr && pBridge->m_pKeyboardFocus != pSurface && pBridge->m_bKeyboardDevice ) {
		static_cast<void>(pBridge->m_pInput->loseState( pBridge->id(), pBridge->m_hKeyboard ));
	}
	pBridge->m_pKeyboardFocus = pSurface;
	pBridge->ensureDevices();
	pBridge->m_pInput->text().setFocused( true );
	pBridge->m_pInput->setHapticsPaused( false );
	if ( pBridge->m_bKeyboardDevice && pKeys != nullptr ) {
		const uint32_t nCount = pKeys->size / sizeof( uint32_t );
		const uint32_t *pScanCodes = static_cast<const uint32_t *>(pKeys->data);
		for ( uint32_t i = 0; i < nCount && i < SIR_INPUT_BUTTON_CAPACITY; ++i ) {
			if ( pScanCodes[i] < SIR_INPUT_BUTTON_CAPACITY - 8 ) {
				const sirInputControlChange_t change = { SIRInputSignal::SIR_INPUT_BUTTON, static_cast<uint16_t>( pScanCodes[i] + 8 ), 1 };
				static_cast<void>(pBridge->m_pInput->emitControls( pBridge->id(), pBridge->m_hKeyboard, &change, 1 ));
			}
		}
	}
}

void SIRWaylandInputBridge::keyboardLeave( void *pContext, wl_keyboard *pKeyboard, uint32_t nSerial, wl_surface *pSurface ) noexcept
{
	static_cast<void>(nSerial);
	SIRWaylandInputBridge *pBridge = static_cast<SIRWaylandInputBridge *>(pContext);
	if ( pBridge->m_pKeyboard != pKeyboard || pBridge->m_pKeyboardFocus != pSurface ) {
		return;
	}
	pBridge->m_pKeyboardFocus = nullptr;
	pBridge->m_pInput->text().setFocused( false );
	pBridge->m_pInput->setHapticsPaused( true );
	if ( pBridge->m_bKeyboardDevice ) {
		static_cast<void>(pBridge->m_pInput->loseState( pBridge->id(), pBridge->m_hKeyboard ));
	}
}

void SIRWaylandInputBridge::keyboardKey( void *pContext, wl_keyboard *pKeyboard, uint32_t nSerial, uint32_t nTime, uint32_t nKey,
                                         uint32_t nState ) noexcept
{
	static_cast<void>(nSerial);
	static_cast<void>(nTime);
	SIRWaylandInputBridge *pBridge = static_cast<SIRWaylandInputBridge *>(pContext);
	if ( pBridge->m_pKeyboard != pKeyboard || pBridge->m_pKeyboardFocus == nullptr || !pBridge->m_bKeyboardDevice ||
	     nKey >= SIR_INPUT_BUTTON_CAPACITY - 8 ||
	     ( nState != WL_KEYBOARD_KEY_STATE_PRESSED && nState != WL_KEYBOARD_KEY_STATE_RELEASED ) ) {
		return;
	}
	const bool bPressed = nState == WL_KEYBOARD_KEY_STATE_PRESSED;
	const sirInputControlChange_t change = { SIRInputSignal::SIR_INPUT_BUTTON, static_cast<uint16_t>( nKey + 8 ), bPressed ? 1 : 0 };
	static_cast<void>(pBridge->m_pInput->emitControls( pBridge->id(), pBridge->m_hKeyboard, &change, 1 ));
#ifdef SIR_WAYLAND_XKB_AVAILABLE
	if ( pBridge->m_pKeyState != nullptr && bPressed && pBridge->m_pInput->text().snapshot().bEnabled ) {
		const uint32_t nCodepoint = xkb_state_key_get_utf32( pBridge->m_pKeyState, nKey + 8 );
		if ( nCodepoint >= 32 ) {
			static_cast<void>(pBridge->m_pInput->text().appendCodepoint( nCodepoint ));
		}
	}
#endif
}

void SIRWaylandInputBridge::keyboardModifiers( void *pContext, wl_keyboard *pKeyboard, uint32_t nSerial, uint32_t nDepressed,
                                               uint32_t nLatched, uint32_t nLocked, uint32_t nGroup ) noexcept
{
	static_cast<void>(nSerial);
	SIRWaylandInputBridge *pBridge = static_cast<SIRWaylandInputBridge *>(pContext);
#ifdef SIR_WAYLAND_XKB_AVAILABLE
	// Modifier events belong to the compositor seat and can precede enter.
	// Keep the XKB state synchronized even without a focused surface.
	if ( pBridge->m_pKeyboard == pKeyboard && pBridge->m_pKeyState != nullptr ) {
		static_cast<void>(xkb_state_update_mask( pBridge->m_pKeyState, nDepressed, nLatched, nLocked, 0, 0, nGroup ));
	}
#else
	static_cast<void>(pBridge);
	static_cast<void>(pKeyboard);
	static_cast<void>(nDepressed);
	static_cast<void>(nLatched);
	static_cast<void>(nLocked);
	static_cast<void>(nGroup);
#endif
}

void SIRWaylandInputBridge::keyboardRepeat( void *pContext, wl_keyboard *pKeyboard, int32_t nRate, int32_t nDelay ) noexcept
{
	static_cast<void>(pContext);
	static_cast<void>(pKeyboard);
	static_cast<void>(nRate);
	static_cast<void>(nDelay);
}

void SIRWaylandInputBridge::pointerEnter( void *pContext, wl_pointer *pPointer, uint32_t nSerial, wl_surface *pSurface, wl_fixed_t nX,
                                          wl_fixed_t nY ) noexcept
{
	static_cast<void>(nSerial);
	SIRWaylandInputBridge *pBridge = static_cast<SIRWaylandInputBridge *>(pContext);
	if ( pBridge->m_pPointer != pPointer || !pBridge->validSurface( pSurface ) ) {
		return;
	}
	if ( pBridge->m_pPointerFocus != nullptr && pBridge->m_pPointerFocus != pSurface ) {
		pBridge->endPointerCapture();
		if ( pBridge->m_bPointerDevice ) {
			static_cast<void>(pBridge->m_pInput->loseState( pBridge->id(), pBridge->m_hPointer ));
		}
	}
	pBridge->m_pPointerFocus = pSurface;
	pBridge->m_nRemainderX = 0;
	pBridge->m_nRemainderY = 0;
	pBridge->m_nWheelRemainder[0] = 0;
	pBridge->m_nWheelRemainder[1] = 0;
	pBridge->m_nLastX = nX;
	pBridge->m_nLastY = nY;
	pBridge->m_bHavePointerPosition = true;
	pBridge->ensureDevices();
	if ( pBridge->m_eCursorMode == SIRInputCursorMode::SIR_INPUT_CURSOR_CAPTURED ) {
		static_cast<void>(pBridge->beginPointerCapture());
	}
}

void SIRWaylandInputBridge::pointerLeave( void *pContext, wl_pointer *pPointer, uint32_t nSerial, wl_surface *pSurface ) noexcept
{
	static_cast<void>(nSerial);
	SIRWaylandInputBridge *pBridge = static_cast<SIRWaylandInputBridge *>(pContext);
	if ( pBridge->m_pPointer != pPointer || pBridge->m_pPointerFocus != pSurface ) {
		return;
	}
	pBridge->endPointerCapture();
	pBridge->m_pPointerFocus = nullptr;
	pBridge->m_bHavePointerPosition = false;
	pBridge->m_nRemainderX = 0;
	pBridge->m_nRemainderY = 0;
	pBridge->m_nWheelRemainder[0] = 0;
	pBridge->m_nWheelRemainder[1] = 0;
	if ( pBridge->m_bPointerDevice ) {
		static_cast<void>(pBridge->m_pInput->loseState( pBridge->id(), pBridge->m_hPointer ));
	}
}

void SIRWaylandInputBridge::pointerMotion( void *pContext, wl_pointer *pPointer, uint32_t nTime, wl_fixed_t nX, wl_fixed_t nY ) noexcept
{
	static_cast<void>(nTime);
	SIRWaylandInputBridge *pBridge = static_cast<SIRWaylandInputBridge *>(pContext);
	if ( pBridge->m_pPointer != pPointer || pBridge->m_pPointerFocus == nullptr || !pBridge->m_bPointerDevice ||
	     pBridge->m_bPointerLocked ) {
		return;
	}
	if ( !pBridge->m_bHavePointerPosition ) {
		pBridge->m_bHavePointerPosition = true;
		pBridge->m_nLastX = nX;
		pBridge->m_nLastY = nY;
		return;
	}
	const int64_t nDx = static_cast<int64_t>(nX) - pBridge->m_nLastX + pBridge->m_nRemainderX;
	const int64_t nDy = static_cast<int64_t>(nY) - pBridge->m_nLastY + pBridge->m_nRemainderY;
	pBridge->m_nLastX = nX;
	pBridge->m_nLastY = nY;
	const int32_t nMoveX = static_cast<int32_t>( nDx / 256 );
	const int32_t nMoveY = static_cast<int32_t>( nDy / 256 );
	pBridge->m_nRemainderX = static_cast<int32_t>( nDx % 256 );
	pBridge->m_nRemainderY = static_cast<int32_t>( nDy % 256 );
	if ( nMoveX != 0 || nMoveY != 0 ) {
		static_cast<void>(pBridge->m_pInput->emitPointer( pBridge->id(), pBridge->m_hPointer, SIRInputSignal::SIR_INPUT_MOTION, 0, nMoveX, nMoveY ));
	}
}

void SIRWaylandInputBridge::pointerButton( void *pContext, wl_pointer *pPointer, uint32_t nSerial, uint32_t nTime, uint32_t nButton,
                                           uint32_t nState ) noexcept
{
	static_cast<void>(nSerial);
	static_cast<void>(nTime);
	SIRWaylandInputBridge *pBridge = static_cast<SIRWaylandInputBridge *>(pContext);
	if ( pBridge->m_pPointer != pPointer || pBridge->m_pPointerFocus == nullptr || !pBridge->m_bPointerDevice || nButton < 0x110 ||
	     nButton > 0x112 ) {
		return;
	}
	const uint16_t nCode = nButton == 0x110 ? 0 : ( nButton == 0x111 ? 1 : 2 );
	const sirInputControlChange_t change = { SIRInputSignal::SIR_INPUT_BUTTON, nCode, nState == WL_POINTER_BUTTON_STATE_PRESSED ? 1 : 0 };
	static_cast<void>(pBridge->m_pInput->emitControls( pBridge->id(), pBridge->m_hPointer, &change, 1 ));
}

void SIRWaylandInputBridge::pointerAxis( void *pContext, wl_pointer *pPointer, uint32_t nTime, uint32_t nAxis, wl_fixed_t nValue ) noexcept
{
	static_cast<void>(nTime);
	SIRWaylandInputBridge *pBridge = static_cast<SIRWaylandInputBridge *>(pContext);
	if ( pBridge->m_pPointer != pPointer || pBridge->m_pPointerFocus == nullptr || !pBridge->m_bPointerDevice ||
	     nAxis > WL_POINTER_AXIS_HORIZONTAL_SCROLL ) {
		return;
	}
	const uint32_t nIndex = nAxis == WL_POINTER_AXIS_VERTICAL_SCROLL ? 0 : 1;
	const int64_t nScaled = -static_cast<int64_t>(nValue) * 12 + pBridge->m_nWheelRemainder[nIndex];
	const int32_t nDelta = static_cast<int32_t>( nScaled / 256 );
	pBridge->m_nWheelRemainder[nIndex] = static_cast<int32_t>( nScaled % 256 );
	if ( nDelta != 0 ) {
		static_cast<void>(pBridge->m_pInput->emitPointer( pBridge->id(), pBridge->m_hPointer, SIRInputSignal::SIR_INPUT_WHEEL, nIndex, nDelta, 0 ));
	}
}

void SIRWaylandInputBridge::pointerFrame( void *pContext, wl_pointer *pPointer ) noexcept
{
	static_cast<void>(pContext);
	static_cast<void>(pPointer);
}

void SIRWaylandInputBridge::pointerAxisSource( void *pContext, wl_pointer *pPointer, uint32_t nSource ) noexcept
{
	static_cast<void>(pContext);
	static_cast<void>(pPointer);
	static_cast<void>(nSource);
}

void SIRWaylandInputBridge::pointerAxisStop( void *pContext, wl_pointer *pPointer, uint32_t nTime, uint32_t nAxis ) noexcept
{
	static_cast<void>(pContext);
	static_cast<void>(pPointer);
	static_cast<void>(nTime);
	static_cast<void>(nAxis);
}

void SIRWaylandInputBridge::pointerAxisDiscrete( void *pContext, wl_pointer *pPointer, uint32_t nAxis, int32_t nDiscrete ) noexcept
{
	static_cast<void>(pContext);
	static_cast<void>(pPointer);
	static_cast<void>(nAxis);
	static_cast<void>(nDiscrete);
}

void SIRWaylandInputBridge::stop( SIRInputService& service ) noexcept
{
	static_cast<void>(service);
	unbind();
}

void SIRWaylandInputBridge::unbind( void ) noexcept
{
	if ( m_pInput == nullptr ) {
		return;
	}
	if ( m_pDriver != nullptr ) {
		static_cast<void>(m_pDriver->setSeatLostSink( nullptr, nullptr ));
		static_cast<void>(m_pDriver->setPointerProtocolLostSink( nullptr, nullptr ));
	}
	releaseSeatObjects();
	m_pInput = nullptr;
	m_pDriver = nullptr;
	m_pDisplay = nullptr;
	m_pSeat = nullptr;
	m_nWindowCount = 0;
	for ( uint32_t i = 0; i < SIR_WINDOW_MAX_WINDOWS; ++i ) {
		m_Surfaces[i] = nullptr;
	}
	m_bSeatLost = false;
	m_eCursorMode = SIRInputCursorMode::SIR_INPUT_CURSOR_FREE;
}
