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
#include <Core/Platform/Input/Linux/SIRWaylandInputBridge.h>
#include <Core/Platform/SIRPlatformNativeWindow.h>
#include <sys/mman.h>
#include <unistd.h>
#include <limits.h>
#ifdef SIR_WAYLAND_XKB_AVAILABLE
#include <xkbcommon/xkbcommon.h>
#endif

SIRWaylandInputBridge::~SIRWaylandInputBridge( void ) noexcept
{
	unbind();
}

sirInputBackendId_t SIRWaylandInputBridge::id( void ) const noexcept
{
	return SIR_INPUT_LINUX_WAYLAND;
}

void SIRWaylandInputBridge::sample( SIRInputService& service ) noexcept
{
	( void )service;
}

void SIRWaylandInputBridge::resynchronize( SIRInputService& service ) noexcept
{
	( void )service;
	m_bHavePointerPosition = false;
	m_nRemainderX = 0;
	m_nRemainderY = 0;
	m_nWheelRemainder[0] = 0;
	m_nWheelRemainder[1] = 0;
}

bool SIRWaylandInputBridge::vibrate( sirInputDeviceId_t hDevice, uint16_t nLeft, uint16_t nRight ) noexcept
{
	( void )hDevice;
	( void )nLeft;
	( void )nRight;
	return false;
}

bool SIRWaylandInputBridge::bind( SIRInputService& input, SIRWindowDriverWayland& driver, const sirWindowNativeView_t& view ) noexcept
{
	sirLinuxNativeWindow_t native = {};
	wl_seat *pSeat = driver.borrowSeat();
	if ( m_pInput != nullptr || !sirPlatformDecodeLinuxWindow( view, &native ) || native.eDisplayType != SIR_LINUX_DISPLAY_WAYLAND ||
	     pSeat == nullptr ) {
		return false;
	}
	m_pInput = &input;
	m_pDriver = &driver;
	m_pDisplay = ( wl_display * )native.nDisplay;
	m_pSeat = pSeat;
	m_Surfaces[0] = ( wl_surface * )native.nSurface;
	m_nWindowCount = 1;
	static const wl_seat_listener s_SeatListener = { seatCapabilities, seatName };
	if ( !driver.setSeatLostSink( seatLost, this ) || wl_seat_add_listener( pSeat, &s_SeatListener, this ) != 0 ) {
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
	sirLinuxNativeWindow_t native = {};
	if ( m_pInput == nullptr || m_bSeatLost || !sirPlatformDecodeLinuxWindow( view, &native ) ||
	     native.eDisplayType != SIR_LINUX_DISPLAY_WAYLAND || native.nDisplay != ( uintptr_t )m_pDisplay ) {
		return false;
	}
	wl_surface *pSurface = ( wl_surface * )native.nSurface;
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
	wl_surface *pSurface = ( wl_surface * )nNativeSurface;
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
					( void )m_pInput->loseState( id(), m_hKeyboard );
				}
			}
		}
		if ( m_pPointerFocus == pSurface ) {
			m_pPointerFocus = nullptr;
			m_bHavePointerPosition = false;
			m_nWheelRemainder[0] = 0;
			m_nWheelRemainder[1] = 0;
			if ( m_bPointerDevice && m_pInput != nullptr ) {
				( void )m_pInput->loseState( id(), m_hPointer );
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

void SIRWaylandInputBridge::ensureDevices( void ) noexcept
{
	if ( m_pInput == nullptr || m_bSeatLost || m_pSeat == nullptr ) {
		return;
	}
	const uint64_t nSeatId = ( uint64_t )( uintptr_t )m_pSeat;
	if ( m_pKeyboard != nullptr && !m_bKeyboardDevice ) {
		m_bKeyboardDevice = m_pInput->connect( { 0, nSeatId, id(), SIR_INPUT_KEYBOARD }, &m_hKeyboard );
	}
	if ( m_pPointer != nullptr && !m_bPointerDevice ) {
		m_bPointerDevice = m_pInput->connect( { 0, nSeatId ^ UINT64_C( 1 ), id(), SIR_INPUT_POINTER }, &m_hPointer );
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
		( void )m_pInput->disconnect( id(), m_hKeyboard );
	}
	m_bKeyboardDevice = false;
	m_hKeyboard = {};
	m_pKeyboardFocus = nullptr;
	clearKeymap();
	if ( m_pKeyboard != nullptr ) {
		if ( wl_proxy_get_version( ( wl_proxy * )m_pKeyboard ) >= 3 ) {
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
		( void )m_pInput->disconnect( id(), m_hPointer );
	}
	m_bPointerDevice = false;
	m_hPointer = {};
	m_pPointerFocus = nullptr;
	m_bHavePointerPosition = false;
	m_nWheelRemainder[0] = 0;
	m_nWheelRemainder[1] = 0;
	if ( m_pPointer != nullptr ) {
		if ( wl_proxy_get_version( ( wl_proxy * )m_pPointer ) >= 3 ) {
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
	SIRWaylandInputBridge *pBridge = ( SIRWaylandInputBridge * )pContext;
	pBridge->releaseSeatObjects();
	pBridge->m_bSeatLost = true;
	pBridge->m_pSeat = nullptr;
}

void SIRWaylandInputBridge::seatCapabilities( void *pContext, wl_seat *pSeat, uint32_t nCapabilities ) noexcept
{
	SIRWaylandInputBridge *pBridge = ( SIRWaylandInputBridge * )pContext;
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
	( void )pContext;
	( void )pSeat;
	( void )pszName;
}

void SIRWaylandInputBridge::keyboardKeymap( void *pContext, wl_keyboard *pKeyboard, uint32_t nFormat, int32_t nFd, uint32_t nSize ) noexcept
{
	SIRWaylandInputBridge *pBridge = ( SIRWaylandInputBridge * )pContext;
	if ( pBridge->m_pKeyboard != pKeyboard ) {
		if ( nFd >= 0 ) {
			close( nFd );
		}
		return;
	}
	pBridge->clearKeymap();
#ifdef SIR_WAYLAND_XKB_AVAILABLE
	if ( nFormat == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 && nFd >= 0 && nSize > 0 && nSize <= UINT32_C( 1 ) << 20 ) {
		const void *pData = mmap( nullptr, nSize, PROT_READ, MAP_PRIVATE, nFd, 0 );
		if ( pData != MAP_FAILED ) {
			pBridge->m_pXkbContext = xkb_context_new( XKB_CONTEXT_NO_FLAGS );
			if ( pBridge->m_pXkbContext != nullptr ) {
				pBridge->m_pKeymap = xkb_keymap_new_from_string( pBridge->m_pXkbContext, ( const char * )pData, XKB_KEYMAP_FORMAT_TEXT_V1,
				                                                 XKB_KEYMAP_COMPILE_NO_FLAGS );
				if ( pBridge->m_pKeymap != nullptr ) {
					pBridge->m_pKeyState = xkb_state_new( pBridge->m_pKeymap );
				}
			}
			munmap( ( void * )pData, nSize );
		}
	}
#else
	( void )nFormat;
	( void )nSize;
#endif
	if ( nFd >= 0 ) {
		close( nFd );
	}
	if ( pBridge->m_bKeyboardDevice && pBridge->m_pInput != nullptr ) {
		( void )pBridge->m_pInput->loseState( pBridge->id(), pBridge->m_hKeyboard );
	}
}

void SIRWaylandInputBridge::keyboardEnter( void *pContext, wl_keyboard *pKeyboard, uint32_t nSerial, wl_surface *pSurface,
                                           wl_array *pKeys ) noexcept
{
	( void )nSerial;
	SIRWaylandInputBridge *pBridge = ( SIRWaylandInputBridge * )pContext;
	if ( pBridge->m_pKeyboard != pKeyboard || !pBridge->validSurface( pSurface ) ) {
		return;
	}
	if ( pBridge->m_pKeyboardFocus != nullptr && pBridge->m_pKeyboardFocus != pSurface && pBridge->m_bKeyboardDevice ) {
		( void )pBridge->m_pInput->loseState( pBridge->id(), pBridge->m_hKeyboard );
	}
	pBridge->m_pKeyboardFocus = pSurface;
	pBridge->ensureDevices();
	pBridge->m_pInput->text().setFocused( true );
	pBridge->m_pInput->setHapticsPaused( false );
	if ( pBridge->m_bKeyboardDevice && pKeys != nullptr ) {
		const uint32_t nCount = pKeys->size / sizeof( uint32_t );
		const uint32_t *pScanCodes = ( const uint32_t * )pKeys->data;
		for ( uint32_t i = 0; i < nCount && i < SIR_INPUT_BUTTON_CAPACITY; ++i ) {
			if ( pScanCodes[i] < SIR_INPUT_BUTTON_CAPACITY - 8 ) {
				const sirInputControlChange_t change = { SIR_INPUT_BUTTON, ( uint16_t )( pScanCodes[i] + 8 ), 1 };
				( void )pBridge->m_pInput->emitControls( pBridge->id(), pBridge->m_hKeyboard, &change, 1 );
			}
		}
	}
}

void SIRWaylandInputBridge::keyboardLeave( void *pContext, wl_keyboard *pKeyboard, uint32_t nSerial, wl_surface *pSurface ) noexcept
{
	( void )nSerial;
	SIRWaylandInputBridge *pBridge = ( SIRWaylandInputBridge * )pContext;
	if ( pBridge->m_pKeyboard != pKeyboard || pBridge->m_pKeyboardFocus != pSurface ) {
		return;
	}
	pBridge->m_pKeyboardFocus = nullptr;
	pBridge->m_pInput->text().setFocused( false );
	pBridge->m_pInput->setHapticsPaused( true );
	if ( pBridge->m_bKeyboardDevice ) {
		( void )pBridge->m_pInput->loseState( pBridge->id(), pBridge->m_hKeyboard );
	}
}

void SIRWaylandInputBridge::keyboardKey( void *pContext, wl_keyboard *pKeyboard, uint32_t nSerial, uint32_t nTime, uint32_t nKey,
                                         uint32_t nState ) noexcept
{
	( void )nSerial;
	( void )nTime;
	SIRWaylandInputBridge *pBridge = ( SIRWaylandInputBridge * )pContext;
	if ( pBridge->m_pKeyboard != pKeyboard || pBridge->m_pKeyboardFocus == nullptr || !pBridge->m_bKeyboardDevice ||
	     nKey >= SIR_INPUT_BUTTON_CAPACITY - 8 ) {
		return;
	}
	const bool bPressed = nState == WL_KEYBOARD_KEY_STATE_PRESSED;
	const sirInputControlChange_t change = { SIR_INPUT_BUTTON, ( uint16_t )( nKey + 8 ), bPressed ? 1 : 0 };
	( void )pBridge->m_pInput->emitControls( pBridge->id(), pBridge->m_hKeyboard, &change, 1 );
#ifdef SIR_WAYLAND_XKB_AVAILABLE
	if ( pBridge->m_pKeyState != nullptr && bPressed && pBridge->m_pInput->text().snapshot().bEnabled ) {
		const uint32_t nCodepoint = xkb_state_key_get_utf32( pBridge->m_pKeyState, nKey + 8 );
		if ( nCodepoint >= 32 ) {
			( void )pBridge->m_pInput->text().appendCodepoint( nCodepoint );
		}
	}
#endif
}

void SIRWaylandInputBridge::keyboardModifiers( void *pContext, wl_keyboard *pKeyboard, uint32_t nSerial, uint32_t nDepressed,
                                               uint32_t nLatched, uint32_t nLocked, uint32_t nGroup ) noexcept
{
	( void )nSerial;
	SIRWaylandInputBridge *pBridge = ( SIRWaylandInputBridge * )pContext;
#ifdef SIR_WAYLAND_XKB_AVAILABLE
	if ( pBridge->m_pKeyboard == pKeyboard && pBridge->m_pKeyState != nullptr ) {
		( void )xkb_state_update_mask( pBridge->m_pKeyState, nDepressed, nLatched, nLocked, 0, 0, nGroup );
	}
#else
	( void )pBridge;
	( void )pKeyboard;
	( void )nDepressed;
	( void )nLatched;
	( void )nLocked;
	( void )nGroup;
#endif
}

void SIRWaylandInputBridge::keyboardRepeat( void *pContext, wl_keyboard *pKeyboard, int32_t nRate, int32_t nDelay ) noexcept
{
	( void )pContext;
	( void )pKeyboard;
	( void )nRate;
	( void )nDelay;
}

void SIRWaylandInputBridge::pointerEnter( void *pContext, wl_pointer *pPointer, uint32_t nSerial, wl_surface *pSurface, wl_fixed_t nX,
                                          wl_fixed_t nY ) noexcept
{
	( void )nSerial;
	SIRWaylandInputBridge *pBridge = ( SIRWaylandInputBridge * )pContext;
	if ( pBridge->m_pPointer != pPointer || !pBridge->validSurface( pSurface ) ) {
		return;
	}
	if ( pBridge->m_pPointerFocus != nullptr && pBridge->m_pPointerFocus != pSurface && pBridge->m_bPointerDevice ) {
		( void )pBridge->m_pInput->loseState( pBridge->id(), pBridge->m_hPointer );
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
}

void SIRWaylandInputBridge::pointerLeave( void *pContext, wl_pointer *pPointer, uint32_t nSerial, wl_surface *pSurface ) noexcept
{
	( void )nSerial;
	SIRWaylandInputBridge *pBridge = ( SIRWaylandInputBridge * )pContext;
	if ( pBridge->m_pPointer != pPointer || pBridge->m_pPointerFocus != pSurface ) {
		return;
	}
	pBridge->m_pPointerFocus = nullptr;
	pBridge->m_bHavePointerPosition = false;
	pBridge->m_nRemainderX = 0;
	pBridge->m_nRemainderY = 0;
	pBridge->m_nWheelRemainder[0] = 0;
	pBridge->m_nWheelRemainder[1] = 0;
	if ( pBridge->m_bPointerDevice ) {
		( void )pBridge->m_pInput->loseState( pBridge->id(), pBridge->m_hPointer );
	}
}

void SIRWaylandInputBridge::pointerMotion( void *pContext, wl_pointer *pPointer, uint32_t nTime, wl_fixed_t nX, wl_fixed_t nY ) noexcept
{
	( void )nTime;
	SIRWaylandInputBridge *pBridge = ( SIRWaylandInputBridge * )pContext;
	if ( pBridge->m_pPointer != pPointer || pBridge->m_pPointerFocus == nullptr || !pBridge->m_bPointerDevice ) {
		return;
	}
	if ( !pBridge->m_bHavePointerPosition ) {
		pBridge->m_bHavePointerPosition = true;
		pBridge->m_nLastX = nX;
		pBridge->m_nLastY = nY;
		return;
	}
	const int64_t nDx = ( int64_t )nX - pBridge->m_nLastX + pBridge->m_nRemainderX;
	const int64_t nDy = ( int64_t )nY - pBridge->m_nLastY + pBridge->m_nRemainderY;
	pBridge->m_nLastX = nX;
	pBridge->m_nLastY = nY;
	const int32_t nMoveX = ( int32_t )( nDx / 256 );
	const int32_t nMoveY = ( int32_t )( nDy / 256 );
	pBridge->m_nRemainderX = ( int32_t )( nDx % 256 );
	pBridge->m_nRemainderY = ( int32_t )( nDy % 256 );
	if ( nMoveX != 0 || nMoveY != 0 ) {
		( void )pBridge->m_pInput->emitPointer( pBridge->id(), pBridge->m_hPointer, SIR_INPUT_MOTION, 0, nMoveX, nMoveY );
	}
}

void SIRWaylandInputBridge::pointerButton( void *pContext, wl_pointer *pPointer, uint32_t nSerial, uint32_t nTime, uint32_t nButton,
                                           uint32_t nState ) noexcept
{
	( void )nSerial;
	( void )nTime;
	SIRWaylandInputBridge *pBridge = ( SIRWaylandInputBridge * )pContext;
	if ( pBridge->m_pPointer != pPointer || pBridge->m_pPointerFocus == nullptr || !pBridge->m_bPointerDevice || nButton < 0x110 ||
	     nButton > 0x112 ) {
		return;
	}
	const uint16_t nCode = nButton == 0x110 ? 0 : ( nButton == 0x111 ? 1 : 2 );
	const sirInputControlChange_t change = { SIR_INPUT_BUTTON, nCode, nState == WL_POINTER_BUTTON_STATE_PRESSED ? 1 : 0 };
	( void )pBridge->m_pInput->emitControls( pBridge->id(), pBridge->m_hPointer, &change, 1 );
}

void SIRWaylandInputBridge::pointerAxis( void *pContext, wl_pointer *pPointer, uint32_t nTime, uint32_t nAxis, wl_fixed_t nValue ) noexcept
{
	( void )nTime;
	SIRWaylandInputBridge *pBridge = ( SIRWaylandInputBridge * )pContext;
	if ( pBridge->m_pPointer != pPointer || pBridge->m_pPointerFocus == nullptr || !pBridge->m_bPointerDevice ||
	     nAxis > WL_POINTER_AXIS_HORIZONTAL_SCROLL ) {
		return;
	}
	const uint32_t nIndex = nAxis == WL_POINTER_AXIS_VERTICAL_SCROLL ? 0 : 1;
	const int64_t nScaled = -( int64_t )nValue * 12 + pBridge->m_nWheelRemainder[nIndex];
	const int32_t nDelta = ( int32_t )( nScaled / 256 );
	pBridge->m_nWheelRemainder[nIndex] = ( int32_t )( nScaled % 256 );
	if ( nDelta != 0 ) {
		( void )pBridge->m_pInput->emitPointer( pBridge->id(), pBridge->m_hPointer, SIR_INPUT_WHEEL, nIndex, nDelta, 0 );
	}
}

void SIRWaylandInputBridge::pointerFrame( void *pContext, wl_pointer *pPointer ) noexcept
{
	( void )pContext;
	( void )pPointer;
}

void SIRWaylandInputBridge::pointerAxisSource( void *pContext, wl_pointer *pPointer, uint32_t nSource ) noexcept
{
	( void )pContext;
	( void )pPointer;
	( void )nSource;
}

void SIRWaylandInputBridge::pointerAxisStop( void *pContext, wl_pointer *pPointer, uint32_t nTime, uint32_t nAxis ) noexcept
{
	( void )pContext;
	( void )pPointer;
	( void )nTime;
	( void )nAxis;
}

void SIRWaylandInputBridge::pointerAxisDiscrete( void *pContext, wl_pointer *pPointer, uint32_t nAxis, int32_t nDiscrete ) noexcept
{
	( void )pContext;
	( void )pPointer;
	( void )nAxis;
	( void )nDiscrete;
}

void SIRWaylandInputBridge::stop( SIRInputService& service ) noexcept
{
	( void )service;
	unbind();
}

void SIRWaylandInputBridge::unbind( void ) noexcept
{
	if ( m_pInput == nullptr ) {
		return;
	}
	if ( m_pDriver != nullptr ) {
		( void )m_pDriver->setSeatLostSink( nullptr, nullptr );
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
}
