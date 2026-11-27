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

/*
 * Window driver's wl_display_dispatch() -> this driver's wl_seat listeners
 *       |                                     |
 *       |                             keyboard / pointer / touch
 *       |                                     |
 *       |                         SIRSPSCEventRing::tryEnqueue()
 *       |
 *       +-- window lifecycle remains entirely with the window driver
 *
 * pumpEvents() only pumps shared evdev gamepads and local key repeat.
 * It must never dispatch the borrowed wl_display a second time.
 */
#include "SIRNativeInputDriverWayland.h"

#include <Core/Platform/Shared/Diagnostics/SIRPlatformTime.h>
#include <linux/input-event-codes.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <cstring>

struct SIRWaylandScanEntry
{
	uint16_t nLinuxKey;
	SIRScancode eScancode;
};

static SIRScancode scancodeForLinuxKey( uint32_t nKey ) noexcept
{
	static constexpr SIRWaylandScanEntry s_szMap[] = {
		{ KEY_A, SIRScancode::A }, { KEY_B, SIRScancode::B }, { KEY_C, SIRScancode::C },
		{ KEY_D, SIRScancode::D }, { KEY_E, SIRScancode::E }, { KEY_F, SIRScancode::F },
		{ KEY_G, SIRScancode::G }, { KEY_H, SIRScancode::H }, { KEY_I, SIRScancode::I },
		{ KEY_J, SIRScancode::J }, { KEY_K, SIRScancode::K }, { KEY_L, SIRScancode::L },
		{ KEY_M, SIRScancode::M }, { KEY_N, SIRScancode::N }, { KEY_O, SIRScancode::O },
		{ KEY_P, SIRScancode::P }, { KEY_Q, SIRScancode::Q }, { KEY_R, SIRScancode::R },
		{ KEY_S, SIRScancode::S }, { KEY_T, SIRScancode::T }, { KEY_U, SIRScancode::U },
		{ KEY_V, SIRScancode::V }, { KEY_W, SIRScancode::W }, { KEY_X, SIRScancode::X },
		{ KEY_Y, SIRScancode::Y }, { KEY_Z, SIRScancode::Z },
		{ KEY_1, SIRScancode::Num1 }, { KEY_2, SIRScancode::Num2 }, { KEY_3, SIRScancode::Num3 },
		{ KEY_4, SIRScancode::Num4 }, { KEY_5, SIRScancode::Num5 }, { KEY_6, SIRScancode::Num6 },
		{ KEY_7, SIRScancode::Num7 }, { KEY_8, SIRScancode::Num8 }, { KEY_9, SIRScancode::Num9 },
		{ KEY_0, SIRScancode::Num0 }, { KEY_MINUS, SIRScancode::Minus }, { KEY_EQUAL, SIRScancode::Equals },
		{ KEY_LEFTBRACE, SIRScancode::LeftBracket }, { KEY_RIGHTBRACE, SIRScancode::RightBracket },
		{ KEY_BACKSLASH, SIRScancode::Backslash }, { KEY_102ND, SIRScancode::NonUSBackslash },
		{ KEY_SEMICOLON, SIRScancode::Semicolon }, { KEY_APOSTROPHE, SIRScancode::Apostrophe },
		{ KEY_GRAVE, SIRScancode::Grave }, { KEY_COMMA, SIRScancode::Comma },
		{ KEY_DOT, SIRScancode::Period }, { KEY_SLASH, SIRScancode::Slash },
		{ KEY_ESC, SIRScancode::Escape }, { KEY_BACKSPACE, SIRScancode::Backspace },
		{ KEY_TAB, SIRScancode::Tab }, { KEY_ENTER, SIRScancode::Return },
		{ KEY_SPACE, SIRScancode::Space }, { KEY_CAPSLOCK, SIRScancode::CapsLock },
		{ KEY_LEFTSHIFT, SIRScancode::LeftShift }, { KEY_RIGHTSHIFT, SIRScancode::RightShift },
		{ KEY_LEFTCTRL, SIRScancode::LeftCtrl }, { KEY_RIGHTCTRL, SIRScancode::RightCtrl },
		{ KEY_LEFTALT, SIRScancode::LeftAlt }, { KEY_RIGHTALT, SIRScancode::RightAlt },
		{ KEY_LEFTMETA, SIRScancode::LeftGUI }, { KEY_RIGHTMETA, SIRScancode::RightGUI },
		{ KEY_COMPOSE, SIRScancode::Application }, { KEY_MENU, SIRScancode::Application },
		{ KEY_F1, SIRScancode::F1 }, { KEY_F2, SIRScancode::F2 }, { KEY_F3, SIRScancode::F3 },
		{ KEY_F4, SIRScancode::F4 }, { KEY_F5, SIRScancode::F5 }, { KEY_F6, SIRScancode::F6 },
		{ KEY_F7, SIRScancode::F7 }, { KEY_F8, SIRScancode::F8 }, { KEY_F9, SIRScancode::F9 },
		{ KEY_F10, SIRScancode::F10 }, { KEY_F11, SIRScancode::F11 }, { KEY_F12, SIRScancode::F12 },
		{ KEY_F13, SIRScancode::F13 }, { KEY_F14, SIRScancode::F14 }, { KEY_F15, SIRScancode::F15 },
		{ KEY_F16, SIRScancode::F16 }, { KEY_F17, SIRScancode::F17 }, { KEY_F18, SIRScancode::F18 },
		{ KEY_F19, SIRScancode::F19 }, { KEY_F20, SIRScancode::F20 }, { KEY_F21, SIRScancode::F21 },
		{ KEY_F22, SIRScancode::F22 }, { KEY_F23, SIRScancode::F23 }, { KEY_F24, SIRScancode::F24 },
		{ KEY_SYSRQ, SIRScancode::PrintScreen }, { KEY_SCROLLLOCK, SIRScancode::ScrollLock },
		{ KEY_PAUSE, SIRScancode::Pause }, { KEY_INSERT, SIRScancode::Insert },
		{ KEY_HOME, SIRScancode::Home }, { KEY_PAGEUP, SIRScancode::PageUp },
		{ KEY_DELETE, SIRScancode::Delete }, { KEY_END, SIRScancode::End },
		{ KEY_PAGEDOWN, SIRScancode::PageDown }, { KEY_RIGHT, SIRScancode::Right },
		{ KEY_LEFT, SIRScancode::Left }, { KEY_DOWN, SIRScancode::Down },
		{ KEY_UP, SIRScancode::Up }, { KEY_NUMLOCK, SIRScancode::NumLockClear },
		{ KEY_KPSLASH, SIRScancode::KeypadDivide }, { KEY_KPASTERISK, SIRScancode::KeypadMultiply },
		{ KEY_KPMINUS, SIRScancode::KeypadMinus }, { KEY_KPPLUS, SIRScancode::KeypadPlus },
		{ KEY_KPENTER, SIRScancode::KeypadEnter }, { KEY_KPDOT, SIRScancode::KeypadPeriod },
		{ KEY_KPEQUAL, SIRScancode::KeypadEquals }, { KEY_KP0, SIRScancode::Keypad0 },
		{ KEY_KP1, SIRScancode::Keypad1 }, { KEY_KP2, SIRScancode::Keypad2 },
		{ KEY_KP3, SIRScancode::Keypad3 }, { KEY_KP4, SIRScancode::Keypad4 },
		{ KEY_KP5, SIRScancode::Keypad5 }, { KEY_KP6, SIRScancode::Keypad6 },
		{ KEY_KP7, SIRScancode::Keypad7 }, { KEY_KP8, SIRScancode::Keypad8 },
		{ KEY_KP9, SIRScancode::Keypad9 },
		{ KEY_MUTE, SIRScancode::Mute }, { KEY_VOLUMEDOWN, SIRScancode::VolumeDown },
		{ KEY_VOLUMEUP, SIRScancode::VolumeUp }
	};

	for ( const SIRWaylandScanEntry& entry : s_szMap ) {
		if ( entry.nLinuxKey == nKey ) {
			return entry.eScancode;
		}
	}

	return SIRScancode::Unknown;
}

static SIRKeycode navigationKey( xkb_keysym_t nSymbol ) noexcept
{
	switch ( nSymbol ) {
	case XKB_KEY_Return: return SIRKeycode::Return;
	case XKB_KEY_Escape: return SIRKeycode::Escape;
	case XKB_KEY_BackSpace: return SIRKeycode::Backspace;
	case XKB_KEY_Tab: return SIRKeycode::Tab;
	case XKB_KEY_ISO_Left_Tab: return SIRKeycode::LeftTab;
	case XKB_KEY_Delete: return SIRKeycode::Delete;
	case XKB_KEY_Caps_Lock: return SIRKeycode::CapsLock;
	case XKB_KEY_Print: return SIRKeycode::PrintScreen;
	case XKB_KEY_Scroll_Lock: return SIRKeycode::ScrollLock;
	case XKB_KEY_Pause: return SIRKeycode::Pause;
	case XKB_KEY_Insert: return SIRKeycode::Insert;
	case XKB_KEY_Home: return SIRKeycode::Home;
	case XKB_KEY_Prior: return SIRKeycode::PageUp;
	case XKB_KEY_End: return SIRKeycode::End;
	case XKB_KEY_Next: return SIRKeycode::PageDown;
	case XKB_KEY_Right: return SIRKeycode::Right;
	case XKB_KEY_Left: return SIRKeycode::Left;
	case XKB_KEY_Down: return SIRKeycode::Down;
	case XKB_KEY_Up: return SIRKeycode::Up;
	case XKB_KEY_Num_Lock: return SIRKeycode::NumLockClear;
	default: return SIRKeycode::Unknown;
	}
}

static SIRKeycode modifierKey( xkb_keysym_t nSymbol ) noexcept
{
	switch ( nSymbol ) {
	case XKB_KEY_Shift_L: return SIRKeycode::LeftShift;
	case XKB_KEY_Shift_R: return SIRKeycode::RightShift;
	case XKB_KEY_Control_L: return SIRKeycode::LeftCtrl;
	case XKB_KEY_Control_R: return SIRKeycode::RightCtrl;
	case XKB_KEY_Alt_L: return SIRKeycode::LeftAlt;
	case XKB_KEY_Alt_R: return SIRKeycode::RightAlt;
	case XKB_KEY_Super_L: return SIRKeycode::LeftGUI;
	case XKB_KEY_Super_R: return SIRKeycode::RightGUI;
	case XKB_KEY_Meta_L: return SIRKeycode::LeftMeta;
	case XKB_KEY_Meta_R: return SIRKeycode::RightMeta;
	case XKB_KEY_Hyper_L: return SIRKeycode::LeftHyper;
	case XKB_KEY_Hyper_R: return SIRKeycode::RightHyper;
	case XKB_KEY_ISO_Level3_Shift: return SIRKeycode::Mode;
	case XKB_KEY_ISO_Level5_Shift: return SIRKeycode::Level5Shift;
	case XKB_KEY_Multi_key: return SIRKeycode::MultiKeyCompose;
	case XKB_KEY_Menu: return SIRKeycode::Application;
	default: return SIRKeycode::Unknown;
	}
}

static SIRKeycode keypadKey( xkb_keysym_t nSymbol ) noexcept
{
	switch ( nSymbol ) {
	case XKB_KEY_KP_0: return SIRKeycode::Keypad0;
	case XKB_KEY_KP_1: return SIRKeycode::Keypad1;
	case XKB_KEY_KP_2: return SIRKeycode::Keypad2;
	case XKB_KEY_KP_3: return SIRKeycode::Keypad3;
	case XKB_KEY_KP_4: return SIRKeycode::Keypad4;
	case XKB_KEY_KP_5: return SIRKeycode::Keypad5;
	case XKB_KEY_KP_6: return SIRKeycode::Keypad6;
	case XKB_KEY_KP_7: return SIRKeycode::Keypad7;
	case XKB_KEY_KP_8: return SIRKeycode::Keypad8;
	case XKB_KEY_KP_9: return SIRKeycode::Keypad9;
	case XKB_KEY_KP_Divide: return SIRKeycode::KeypadDivide;
	case XKB_KEY_KP_Multiply: return SIRKeycode::KeypadMultiply;
	case XKB_KEY_KP_Subtract: return SIRKeycode::KeypadMinus;
	case XKB_KEY_KP_Add: return SIRKeycode::KeypadPlus;
	case XKB_KEY_KP_Enter: return SIRKeycode::KeypadEnter;
	case XKB_KEY_KP_Decimal: return SIRKeycode::KeypadPeriod;
	case XKB_KEY_KP_Equal: return SIRKeycode::KeypadEquals;
	case XKB_KEY_KP_Delete: return SIRKeycode::KeypadPeriod;
	case XKB_KEY_KP_Insert: return SIRKeycode::Keypad0;
	case XKB_KEY_KP_End: return SIRKeycode::Keypad1;
	case XKB_KEY_KP_Down: return SIRKeycode::Keypad2;
	case XKB_KEY_KP_Next: return SIRKeycode::Keypad3;
	case XKB_KEY_KP_Left: return SIRKeycode::Keypad4;
	case XKB_KEY_KP_Begin: return SIRKeycode::Keypad5;
	case XKB_KEY_KP_Right: return SIRKeycode::Keypad6;
	case XKB_KEY_KP_Home: return SIRKeycode::Keypad7;
	case XKB_KEY_KP_Up: return SIRKeycode::Keypad8;
	case XKB_KEY_KP_Prior: return SIRKeycode::Keypad9;
	default: return SIRKeycode::Unknown;
	}
}

static SIRKeycode logicalKey( xkb_keysym_t nSymbol ) noexcept
{
	if ( nSymbol >= XKB_KEY_F1 && nSymbol <= XKB_KEY_F24 ) {
		const SIRKeycode eFirst = nSymbol <= XKB_KEY_F12 ? SIRKeycode::F1 : SIRKeycode::F13;
		const uint32_t nFirstSymbol = nSymbol <= XKB_KEY_F12 ? XKB_KEY_F1 : XKB_KEY_F13;
		return static_cast<SIRKeycode>( static_cast<uint32_t>( eFirst ) + nSymbol - nFirstSymbol );
	}

	SIRKeycode eResult = navigationKey( nSymbol );
	if ( eResult == SIRKeycode::Unknown ) {
		eResult = modifierKey( nSymbol );
	}
	if ( eResult == SIRKeycode::Unknown ) {
		eResult = keypadKey( nSymbol );
	}
	if ( eResult != SIRKeycode::Unknown ) {
		return eResult;
	}

	uint32_t nUnicode = xkb_keysym_to_utf32( nSymbol );
	if ( nUnicode >= 'A' && nUnicode <= 'Z' ) {
		nUnicode += 'a' - 'A';
	}
	if ( nUnicode >= 0x20 && nUnicode <= 0x10ffff ) {
		return static_cast<SIRKeycode>( nUnicode );
	}

	return SIRKeycode::Unknown;
}

static SIRMouseButton mouseButton( uint32_t nLinuxCode ) noexcept
{
	switch ( nLinuxCode ) {
	case BTN_LEFT: return static_cast<SIRMouseButton>( 1 );
	case BTN_MIDDLE: return static_cast<SIRMouseButton>( 2 );
	case BTN_RIGHT: return static_cast<SIRMouseButton>( 3 );
	case BTN_SIDE: return static_cast<SIRMouseButton>( 4 );
	case BTN_EXTRA: return static_cast<SIRMouseButton>( 5 );
	case BTN_FORWARD: return static_cast<SIRMouseButton>( 6 );
	case BTN_BACK: return static_cast<SIRMouseButton>( 7 );
	case BTN_TASK: return static_cast<SIRMouseButton>( 8 );
	default: return static_cast<SIRMouseButton>( 0 );
	}
}

static float clampNormalized( float fValue ) noexcept
{
	if ( fValue < 0.f ) {
		return 0.f;
	}
	if ( fValue > 1.f ) {
		return 1.f;
	}
	return fValue;
}

const wl_registry_listener SIRNativeInputDriverWayland::s_RegistryListener = {
	&SIRNativeInputDriverWayland::registryGlobal,
	&SIRNativeInputDriverWayland::registryRemove
};

const wl_seat_listener SIRNativeInputDriverWayland::s_SeatListener = {
	&SIRNativeInputDriverWayland::seatCapabilities,
	&SIRNativeInputDriverWayland::seatName
};

const wl_keyboard_listener SIRNativeInputDriverWayland::s_KeyboardListener = {
	&SIRNativeInputDriverWayland::keyboardKeymap,
	&SIRNativeInputDriverWayland::keyboardEnter,
	&SIRNativeInputDriverWayland::keyboardLeave,
	&SIRNativeInputDriverWayland::keyboardKey,
	&SIRNativeInputDriverWayland::keyboardModifiers,
	&SIRNativeInputDriverWayland::keyboardRepeatInfo
};

const wl_pointer_listener SIRNativeInputDriverWayland::s_PointerListener = {
	&SIRNativeInputDriverWayland::pointerEnter,
	&SIRNativeInputDriverWayland::pointerLeave,
	&SIRNativeInputDriverWayland::pointerMotion,
	&SIRNativeInputDriverWayland::pointerButton,
	&SIRNativeInputDriverWayland::pointerAxis,
	&SIRNativeInputDriverWayland::pointerFrame,
	&SIRNativeInputDriverWayland::pointerAxisSource,
	&SIRNativeInputDriverWayland::pointerAxisStop,
	&SIRNativeInputDriverWayland::pointerAxisDiscrete
#ifdef WL_POINTER_AXIS_VALUE120_SINCE_VERSION
	, nullptr
#endif
#ifdef WL_POINTER_AXIS_RELATIVE_DIRECTION_SINCE_VERSION
	, nullptr
#endif
};

const wl_touch_listener SIRNativeInputDriverWayland::s_TouchListener = {
	&SIRNativeInputDriverWayland::touchDown,
	&SIRNativeInputDriverWayland::touchUp,
	&SIRNativeInputDriverWayland::touchMotion,
	&SIRNativeInputDriverWayland::touchFrame,
	&SIRNativeInputDriverWayland::touchCancel
#ifdef WL_TOUCH_SHAPE_SINCE_VERSION
	, nullptr
#endif
#ifdef WL_TOUCH_ORIENTATION_SINCE_VERSION
	, nullptr
#endif
};

SIRNativeInputDriverWayland::SIRNativeInputDriverWayland(
	SIRSPSCEventRing<SIREventData>& eventQueue, const SIRInputControllerProfiles *pProfiles ) noexcept
	: SIRNativeInputDriver( eventQueue ), m_Gamepads( m_EventQueue, pProfiles )
{
}

SIRNativeInputDriverWayland::~SIRNativeInputDriverWayland() noexcept
{
	detachWindow();
}

bool SIRNativeInputDriverWayland::attachWindow( wl_display *pDisplay, wl_surface *pSurface, uint32_t nWindowId ) noexcept
{
	if ( m_pDisplay || !pDisplay || !pSurface ) {
		return false;
	}

	m_pXKBContext = xkb_context_new( XKB_CONTEXT_NO_FLAGS );
	if ( !m_pXKBContext ) {
		return false;
	}

	m_pDisplay = pDisplay;
	m_pSurface = pSurface;
	m_nWindowId = nWindowId;
	m_pRegistry = wl_display_get_registry( pDisplay );
	if ( !m_pRegistry || wl_registry_add_listener( m_pRegistry, &s_RegistryListener, this ) != 0 ) {
		detachWindow();
		return false;
	}

	// The first roundtrip enumerates globals; the second delivers capabilities
	// for the seat proxy bound by registryGlobal. Run before the event loop.
	if ( wl_display_roundtrip( pDisplay ) < 0 || wl_display_roundtrip( pDisplay ) < 0 ) {
		detachWindow( true );
		return false;
	}

	return true;
}

void SIRNativeInputDriverWayland::setSurfaceSize( uint32_t nWidth, uint32_t nHeight ) noexcept
{
	m_nWidth = nWidth;
	m_nHeight = nHeight;
}

void SIRNativeInputDriverWayland::detachWindow( bool bConnectionLost ) noexcept
{
	destroySeat( bConnectionLost );

	if ( m_pRegistry ) {
		wl_registry_destroy( m_pRegistry );
		m_pRegistry = nullptr;
	}
	clearKeymap();
	if ( m_pXKBContext ) {
		xkb_context_unref( m_pXKBContext );
		m_pXKBContext = nullptr;
	}

	m_pDisplay = nullptr;
	m_pSurface = nullptr;
	m_nWindowId = 0;
	m_nWidth = 0;
	m_nHeight = 0;
}

void SIRNativeInputDriverWayland::clearKeymap( void ) noexcept
{
	if ( m_pXKBState ) {
		xkb_state_unref( m_pXKBState );
		m_pXKBState = nullptr;
	}
	if ( m_pXKBKeymap ) {
		xkb_keymap_unref( m_pXKBKeymap );
		m_pXKBKeymap = nullptr;
	}
}

void SIRNativeInputDriverWayland::registryGlobal(
	void *pData, wl_registry *pRegistry, uint32_t nName, const char *szInterface, uint32_t nVersion ) noexcept
{
	SIRNativeInputDriverWayland *pDriver = static_cast<SIRNativeInputDriverWayland *>( pData );
	if ( pDriver->m_pSeat || std::strcmp( szInterface, wl_seat_interface.name ) != 0 ) {
		return;
	}

	// A separate seat proxy carries this input driver's listeners. It does not
	// steal the registry or seat listeners already owned by the window driver.
	const uint32_t nBoundVersion = nVersion < 5 ? nVersion : 5;
	if ( nBoundVersion < 3 ) {
		return;
	}

	pDriver->m_pSeat = static_cast<wl_seat *>( wl_registry_bind( pRegistry, nName, &wl_seat_interface, nBoundVersion ) );
	if ( pDriver->m_pSeat && wl_seat_add_listener( pDriver->m_pSeat, &s_SeatListener, pDriver ) == 0 ) {
		pDriver->m_nSeatGlobal = nName;
	} else {
		pDriver->destroySeat( false );
	}
}

void SIRNativeInputDriverWayland::registryRemove( void *pData, wl_registry *, uint32_t nName ) noexcept
{
	SIRNativeInputDriverWayland *pDriver = static_cast<SIRNativeInputDriverWayland *>( pData );
	if ( pDriver->m_nSeatGlobal == nName ) {
		pDriver->destroySeat( false );
	}
}

void SIRNativeInputDriverWayland::seatCapabilities( void *pData, wl_seat *, uint32_t nCapabilities ) noexcept
{
	static_cast<SIRNativeInputDriverWayland *>( pData )->createSeatDevices( nCapabilities );
}

void SIRNativeInputDriverWayland::seatName( void *, wl_seat *, const char * ) noexcept
{
}

void SIRNativeInputDriverWayland::createSeatDevices( uint32_t nCapabilities ) noexcept
{
	if ( ( nCapabilities & WL_SEAT_CAPABILITY_KEYBOARD ) && !m_pKeyboard ) {
		m_pKeyboard = wl_seat_get_keyboard( m_pSeat );
		if ( m_pKeyboard && wl_keyboard_add_listener( m_pKeyboard, &s_KeyboardListener, this ) != 0 ) {
			wl_keyboard_destroy( m_pKeyboard );
			m_pKeyboard = nullptr;
		}
	} else if ( !( nCapabilities & WL_SEAT_CAPABILITY_KEYBOARD ) && m_pKeyboard ) {
		releaseKeyboard();
		wl_keyboard_release( m_pKeyboard );
		m_pKeyboard = nullptr;
		clearKeymap();
	}

	if ( ( nCapabilities & WL_SEAT_CAPABILITY_POINTER ) && !m_pPointer ) {
		m_pPointer = wl_seat_get_pointer( m_pSeat );
		if ( m_pPointer && wl_pointer_add_listener( m_pPointer, &s_PointerListener, this ) != 0 ) {
			wl_pointer_destroy( m_pPointer );
			m_pPointer = nullptr;
		}
	} else if ( !( nCapabilities & WL_SEAT_CAPABILITY_POINTER ) && m_pPointer ) {
		releasePointer();
		wl_pointer_release( m_pPointer );
		m_pPointer = nullptr;
	}

	if ( ( nCapabilities & WL_SEAT_CAPABILITY_TOUCH ) && !m_pTouch ) {
		m_pTouch = wl_seat_get_touch( m_pSeat );
		if ( m_pTouch && wl_touch_add_listener( m_pTouch, &s_TouchListener, this ) != 0 ) {
			wl_touch_destroy( m_pTouch );
			m_pTouch = nullptr;
		}
	} else if ( !( nCapabilities & WL_SEAT_CAPABILITY_TOUCH ) && m_pTouch ) {
		cancelContacts();
		wl_touch_release( m_pTouch );
		m_pTouch = nullptr;
	}
}

void SIRNativeInputDriverWayland::destroySeatDevices( bool bConnectionLost ) noexcept
{
	releaseKeyboard();
	releasePointer();
	cancelContacts();

	if ( m_pKeyboard ) {
		if ( bConnectionLost ) {
			wl_keyboard_destroy( m_pKeyboard );
		} else {
			wl_keyboard_release( m_pKeyboard );
		}
		m_pKeyboard = nullptr;
	}
	if ( m_pPointer ) {
		if ( bConnectionLost ) {
			wl_pointer_destroy( m_pPointer );
		} else {
			wl_pointer_release( m_pPointer );
		}
		m_pPointer = nullptr;
	}
	if ( m_pTouch ) {
		if ( bConnectionLost ) {
			wl_touch_destroy( m_pTouch );
		} else {
			wl_touch_release( m_pTouch );
		}
		m_pTouch = nullptr;
	}
	clearKeymap();
}

void SIRNativeInputDriverWayland::destroySeat( bool bConnectionLost ) noexcept
{
	destroySeatDevices( bConnectionLost );

	if ( m_pSeat ) {
		if ( !bConnectionLost && wl_seat_get_version( m_pSeat ) >= WL_SEAT_RELEASE_SINCE_VERSION ) {
			wl_seat_release( m_pSeat );
		} else {
			wl_seat_destroy( m_pSeat );
		}
		m_pSeat = nullptr;
	}
	m_nSeatGlobal = UINT32_MAX;
}

void SIRNativeInputDriverWayland::keyboardKeymap( void *pData, wl_keyboard *, uint32_t nFormat, int32_t nFd, uint32_t nSize ) noexcept
{
	SIRNativeInputDriverWayland *pDriver = static_cast<SIRNativeInputDriverWayland *>( pData );
	if ( nFormat != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 || nSize == 0 || nFd < 0 ) {
		if ( nFd >= 0 ) {
			close( nFd );
		}
		return;
	}

	void *pMemory = mmap( nullptr, nSize, PROT_READ, MAP_PRIVATE, nFd, 0 );
	close( nFd );
	if ( pMemory == MAP_FAILED ) {
		return;
	}

	const char *szKeymap = static_cast<const char *>( pMemory );
	xkb_keymap *pMap = nullptr;
	if ( szKeymap[nSize - 1] == '\0' ) {
		pMap = xkb_keymap_new_from_string( pDriver->m_pXKBContext, szKeymap, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS );
	}
	munmap( pMemory, nSize );
	if ( !pMap ) {
		return;
	}

	xkb_state *pState = xkb_state_new( pMap );
	if ( !pState ) {
		xkb_keymap_unref( pMap );
		return;
	}

	const bool bHadFocus = pDriver->m_bKeyboardFocus;
	pDriver->releaseKeyboard();
	pDriver->clearKeymap();
	pDriver->m_bKeyboardFocus = bHadFocus;
	pDriver->m_pXKBKeymap = pMap;
	pDriver->m_pXKBState = pState;
}

void SIRNativeInputDriverWayland::keyboardEnter( void *pData, wl_keyboard *, uint32_t, wl_surface *pSurface, wl_array *pKeys ) noexcept
{
	SIRNativeInputDriverWayland *pDriver = static_cast<SIRNativeInputDriverWayland *>( pData );
	if ( pSurface != pDriver->m_pSurface ) {
		return;
	}

	pDriver->m_bKeyboardFocus = true;
	if ( pKeys ) {
		const uint32_t *pCodes = static_cast<const uint32_t *>( pKeys->data );
		const size_t nCount = pKeys->size / sizeof( uint32_t );
		for ( size_t iKey = 0; iKey < nCount; ++iKey ) {
			pDriver->keyEvent( pCodes[iKey], true );
		}
	}
}

void SIRNativeInputDriverWayland::keyboardLeave( void *pData, wl_keyboard *, uint32_t, wl_surface *pSurface ) noexcept
{
	SIRNativeInputDriverWayland *pDriver = static_cast<SIRNativeInputDriverWayland *>( pData );
	if ( pSurface == pDriver->m_pSurface ) {
		pDriver->releaseKeyboard();
	}
}

void SIRNativeInputDriverWayland::keyboardKey(
	void *pData, wl_keyboard *, uint32_t, uint32_t, uint32_t nKey, uint32_t nState ) noexcept
{
	SIRNativeInputDriverWayland *pDriver = static_cast<SIRNativeInputDriverWayland *>( pData );
	if ( pDriver->m_bKeyboardFocus ) {
		pDriver->keyEvent( nKey, nState == WL_KEYBOARD_KEY_STATE_PRESSED );
	}
}

void SIRNativeInputDriverWayland::keyboardModifiers( void *pData, wl_keyboard *, uint32_t, uint32_t nDepressed,
	uint32_t nLatched, uint32_t nLocked, uint32_t nGroup ) noexcept
{
	SIRNativeInputDriverWayland *pDriver = static_cast<SIRNativeInputDriverWayland *>( pData );
	if ( pDriver->m_pXKBState ) {
		// In Wayland, the compositor supplies authoritative modifier masks.
		// Updating xkb_state per key would apply them a second time.
		xkb_state_update_mask( pDriver->m_pXKBState, nDepressed, nLatched, nLocked, 0, 0, nGroup );
		pDriver->updateModifiers();
	}
}

void SIRNativeInputDriverWayland::keyboardRepeatInfo( void *pData, wl_keyboard *, int32_t nRate, int32_t nDelay ) noexcept
{
	SIRNativeInputDriverWayland *pDriver = static_cast<SIRNativeInputDriverWayland *>( pData );
	pDriver->m_nRepeatRate = nRate > 0 ? nRate : 0;
	pDriver->m_nRepeatDelay = nDelay > 0 ? nDelay : 0;
	pDriver->m_nRepeatInterval = nRate > 0 ? UINT64_C( 1000000000 ) / static_cast<uint32_t>( nRate ) : 0;
	if ( !pDriver->m_nRepeatInterval ) {
		pDriver->m_nRepeatKey = UINT32_MAX;
	}
}

void SIRNativeInputDriverWayland::updateModifiers( void ) noexcept
{
	uint16_t nModifiers = 0;
	if ( m_szKeys[KEY_LEFTSHIFT].down ) {
		nModifiers |= static_cast<uint16_t>( SIRKeyModifier::LeftShift );
	}
	if ( m_szKeys[KEY_RIGHTSHIFT].down ) {
		nModifiers |= static_cast<uint16_t>( SIRKeyModifier::RightShift );
	}
	if ( m_szKeys[KEY_LEFTCTRL].down ) {
		nModifiers |= static_cast<uint16_t>( SIRKeyModifier::LeftCtrl );
	}
	if ( m_szKeys[KEY_RIGHTCTRL].down ) {
		nModifiers |= static_cast<uint16_t>( SIRKeyModifier::RightCtrl );
	}
	if ( m_szKeys[KEY_LEFTALT].down ) {
		nModifiers |= static_cast<uint16_t>( SIRKeyModifier::LeftAlt );
	}
	if ( m_szKeys[KEY_RIGHTALT].down ) {
		nModifiers |= static_cast<uint16_t>( SIRKeyModifier::RightAlt );
	}
	if ( m_szKeys[KEY_LEFTMETA].down ) {
		nModifiers |= static_cast<uint16_t>( SIRKeyModifier::LeftGUI );
	}
	if ( m_szKeys[KEY_RIGHTMETA].down ) {
		nModifiers |= static_cast<uint16_t>( SIRKeyModifier::RightGUI );
	}
	if ( m_pXKBState ) {
		const xkb_state_component eLocked = XKB_STATE_MODS_LOCKED;
		if ( xkb_state_mod_name_is_active( m_pXKBState, XKB_MOD_NAME_CAPS, eLocked ) > 0 ) {
			nModifiers |= static_cast<uint16_t>( SIRKeyModifier::CapsLock );
		}
		if ( xkb_state_mod_name_is_active( m_pXKBState, XKB_MOD_NAME_NUM, eLocked ) > 0 ) {
			nModifiers |= static_cast<uint16_t>( SIRKeyModifier::NumLock );
		}
	}
	m_nModifiers = nModifiers;
}

void SIRNativeInputDriverWayland::keyEvent( uint32_t nKey, bool bDown, bool bRepeat ) noexcept
{
	if ( nKey >= m_szKeys.size() ) {
		return;
	}

	SIRKeyboardEvent& key = m_szKeys[nKey];
	const bool bHeld = key.down;
	if ( bDown && bHeld && !bRepeat ) {
		return;
	}
	if ( !bDown && !bHeld ) {
		return;
	}

	// Wayland sends evdev key numbers. xkbcommon expects evdev + 8, while
	// SIREngine's physical scancode stays tied to the Linux physical key.
	if ( !bHeld ) {
		const xkb_keycode_t nXKBCode = nKey + 8;
		const xkb_keysym_t nSymbol = m_pXKBState ? xkb_state_key_get_one_sym( m_pXKBState, nXKBCode ) : XKB_KEY_NoSymbol;
		key.keycode = static_cast<uint32_t>( logicalKey( nSymbol ) );
		key.nativeScancode = static_cast<uint16_t>( nKey );
		key.scancode = static_cast<uint32_t>( scancodeForLinuxKey( nKey ) );
		key.windowId = m_nWindowId;
	}

	key.down = bDown;
	key.repeat = bRepeat;
	updateModifiers();
	key.modifiers = m_nModifiers;

	SIREventData eventData{};
	eventData.type = bDown ? SIREventType::KeyDown : SIREventType::KeyUp;
	eventData.key = key;
	publish( eventData );

	if ( !bDown && m_nRepeatKey == nKey ) {
		m_nRepeatKey = UINT32_MAX;
	}
	if ( bDown && !bRepeat ) {
		m_nRepeatKey = UINT32_MAX;
		if ( m_nRepeatRate && m_pXKBKeymap && xkb_keymap_key_repeats( m_pXKBKeymap, nKey + 8 ) ) {
			m_nRepeatKey = nKey;
			m_nNextRepeat = SIRPlatformTime::getMonotonicNanoseconds() + static_cast<uint64_t>( m_nRepeatDelay ) * UINT64_C( 1000000 );
		}
	}
}

void SIRNativeInputDriverWayland::releaseKeyboard( void ) noexcept
{
	m_bKeyboardFocus = false;
	m_nRepeatKey = UINT32_MAX;
	for ( size_t iKey = 0; iKey < m_szKeys.size(); ++iKey ) {
		if ( m_szKeys[iKey].down ) {
			keyEvent( static_cast<uint32_t>( iKey ), false );
		}
	}
	m_nModifiers = 0;
}

void SIRNativeInputDriverWayland::repeatKeys( void ) noexcept
{
	if ( !m_bKeyboardFocus || m_nRepeatKey >= m_szKeys.size() || !m_nRepeatInterval ) {
		return;
	}

	const uint64_t nNow = SIRPlatformTime::getMonotonicNanoseconds();
	if ( nNow < m_nNextRepeat ) {
		return;
	}

	// A stalled frame must not turn into an unbounded burst of repeated keys.
	for ( uint32_t iRepeat = 0; iRepeat < 8 && nNow >= m_nNextRepeat; ++iRepeat ) {
		keyEvent( m_nRepeatKey, true, true );
		m_nNextRepeat += m_nRepeatInterval;
	}
	if ( nNow >= m_nNextRepeat ) {
		m_nNextRepeat = nNow + m_nRepeatInterval;
	}
}

void SIRNativeInputDriverWayland::pointerEnter( void *pData, wl_pointer *, uint32_t, wl_surface *pSurface, int32_t nX, int32_t nY ) noexcept
{
	SIRNativeInputDriverWayland *pDriver = static_cast<SIRNativeInputDriverWayland *>( pData );
	if ( pSurface != pDriver->m_pSurface ) {
		return;
	}
	pDriver->m_bPointerFocus = true;
	pDriver->m_fX = static_cast<float>( wl_fixed_to_double( nX ) );
	pDriver->m_fY = static_cast<float>( wl_fixed_to_double( nY ) );
	pDriver->m_bHavePointer = true;
	SIREventData eventData{};
	eventData.type = SIREventType::MouseMotion;
	eventData.motion = { pDriver->m_nWindowId, 0, pDriver->m_nButtons, pDriver->m_fX, pDriver->m_fY, 0.f, 0.f };
	pDriver->publish( eventData );
}

void SIRNativeInputDriverWayland::pointerLeave( void *pData, wl_pointer *, uint32_t, wl_surface *pSurface ) noexcept
{
	SIRNativeInputDriverWayland *pDriver = static_cast<SIRNativeInputDriverWayland *>( pData );
	if ( pSurface == pDriver->m_pSurface ) {
		pDriver->releasePointer();
	}
}

void SIRNativeInputDriverWayland::pointerMotion( void *pData, wl_pointer *, uint32_t, int32_t nX, int32_t nY ) noexcept
{
	SIRNativeInputDriverWayland *pDriver = static_cast<SIRNativeInputDriverWayland *>( pData );
	if ( !pDriver->m_bPointerFocus ) {
		return;
	}

	const float fX = static_cast<float>( wl_fixed_to_double( nX ) );
	const float fY = static_cast<float>( wl_fixed_to_double( nY ) );
	const float fDX = pDriver->m_bHavePointer ? fX - pDriver->m_fX : 0.f;
	const float fDY = pDriver->m_bHavePointer ? fY - pDriver->m_fY : 0.f;
	SIREventData eventData{};
	eventData.type = SIREventType::MouseMotion;
	eventData.motion = { pDriver->m_nWindowId, 0, pDriver->m_nButtons, fX, fY, fDX, fDY };
	pDriver->m_fX = fX;
	pDriver->m_fY = fY;
	pDriver->m_bHavePointer = true;
	pDriver->publish( eventData );
}

void SIRNativeInputDriverWayland::buttonEvent( SIRMouseButton eButton, bool bDown ) noexcept
{
	const uint32_t nMask = SIRMouseButtonMask( eButton );
	if ( ( ( m_nButtons & nMask ) != 0 ) == bDown ) {
		return;
	}

	m_nButtons = bDown ? m_nButtons | nMask : m_nButtons & ~nMask;
	SIREventData eventData{};
	eventData.type = bDown ? SIREventType::MouseButtonDown : SIREventType::MouseButtonUp;
	eventData.button = { m_nWindowId, 0, static_cast<uint8_t>( eButton ), bDown, 1, 0, m_fX, m_fY };
	publish( eventData );
}

void SIRNativeInputDriverWayland::pointerButton( void *pData, wl_pointer *, uint32_t, uint32_t, uint32_t nButton, uint32_t nState ) noexcept
{
	SIRNativeInputDriverWayland *pDriver = static_cast<SIRNativeInputDriverWayland *>( pData );
	const SIRMouseButton eButton = mouseButton( nButton );
	if ( pDriver->m_bPointerFocus && eButton != static_cast<SIRMouseButton>( 0 ) ) {
		pDriver->buttonEvent( eButton, nState == WL_POINTER_BUTTON_STATE_PRESSED );
	}
}

void SIRNativeInputDriverWayland::pointerAxis( void *pData, wl_pointer *, uint32_t, uint32_t nAxis, int32_t nValue ) noexcept
{
	SIRNativeInputDriverWayland *pDriver = static_cast<SIRNativeInputDriverWayland *>( pData );
	if ( !pDriver->m_bPointerFocus ) {
		return;
	}
	if ( nAxis != WL_POINTER_AXIS_HORIZONTAL_SCROLL && nAxis != WL_POINTER_AXIS_VERTICAL_SCROLL ) {
		return;
	}

	// wl_pointer.axis is continuous surface-coordinate distance, not wheel
	// detents. Preserve its fractional magnitude; axis_discrete is ignored.
	const float fValue = static_cast<float>( wl_fixed_to_double( nValue ) );
	const float fHorizontal = nAxis == WL_POINTER_AXIS_HORIZONTAL_SCROLL ? fValue : 0.f;
	const float fVertical = nAxis == WL_POINTER_AXIS_VERTICAL_SCROLL ? -fValue : 0.f;
	SIREventData eventData{};
	eventData.type = SIREventType::MouseWheel;
	eventData.wheel = { pDriver->m_nWindowId, 0, fHorizontal, fVertical, 0, pDriver->m_fX, pDriver->m_fY, 0, 0 };
	pDriver->publish( eventData );
}

void SIRNativeInputDriverWayland::pointerFrame( void *, wl_pointer * ) noexcept
{
}

void SIRNativeInputDriverWayland::pointerAxisSource( void *, wl_pointer *, uint32_t ) noexcept
{
}

void SIRNativeInputDriverWayland::pointerAxisStop( void *, wl_pointer *, uint32_t, uint32_t ) noexcept
{
}

void SIRNativeInputDriverWayland::pointerAxisDiscrete( void *, wl_pointer *, uint32_t, int32_t ) noexcept
{
	// Discrete steps accompany wl_pointer.axis. Publishing both would duplicate scrolling.
}

void SIRNativeInputDriverWayland::releasePointer( void ) noexcept
{
	m_bPointerFocus = false;
	m_bHavePointer = false;
	for ( unsigned iButton = 1; iButton <= static_cast<unsigned>( SIRMouseButton::Button16 ); ++iButton ) {
		buttonEvent( static_cast<SIRMouseButton>( iButton ), false );
	}
}

SIRNativeInputDriverWayland::Contact *SIRNativeInputDriverWayland::findContact( int32_t nId ) noexcept
{
	for ( Contact& contact : m_szContacts ) {
		if ( contact.nId == nId ) {
			return &contact;
		}
	}
	return nullptr;
}

void SIRNativeInputDriverWayland::emitContact( Contact& contact, SIREventType eType, float fX, float fY ) noexcept
{
	SIREventData eventData{};
	eventData.type = eType;
	eventData.tfinger = contact.eventData;
	eventData.tfinger.deltaX = fX - contact.eventData.x;
	eventData.tfinger.deltaY = fY - contact.eventData.y;
	eventData.tfinger.x = fX;
	eventData.tfinger.y = fY;
	eventData.tfinger.pressure = eType == SIREventType::FingerMotion ? 1.f : 0.f;
	contact.eventData = eventData.tfinger;
	publish( eventData );
}

void SIRNativeInputDriverWayland::touchDown(
	void *pData, wl_touch *, uint32_t, uint32_t, wl_surface *pSurface, int32_t nId, int32_t nX, int32_t nY ) noexcept
{
	SIRNativeInputDriverWayland *pDriver = static_cast<SIRNativeInputDriverWayland *>( pData );
	if ( pSurface != pDriver->m_pSurface || !pDriver->m_nWidth || !pDriver->m_nHeight ) {
		return;
	}
	if ( pDriver->findContact( nId ) ) {
		return;
	}

	for ( Contact& contact : pDriver->m_szContacts ) {
		if ( contact.nId == -1 ) {
			const float fX = clampNormalized( static_cast<float>( wl_fixed_to_double( nX ) ) / pDriver->m_nWidth );
			const float fY = clampNormalized( static_cast<float>( wl_fixed_to_double( nY ) ) / pDriver->m_nHeight );
			contact.nId = nId;
			contact.eventData = { 0, static_cast<uint64_t>( static_cast<uint32_t>( nId ) ), fX, fY, 0, 0, 1.f, pDriver->m_nWindowId };
			SIREventData eventData{};
			eventData.type = SIREventType::FingerDown;
			eventData.tfinger = contact.eventData;
			pDriver->publish( eventData );
			return;
		}
	}
}

void SIRNativeInputDriverWayland::touchUp( void *pData, wl_touch *, uint32_t, uint32_t, int32_t nId ) noexcept
{
	SIRNativeInputDriverWayland *pDriver = static_cast<SIRNativeInputDriverWayland *>( pData );
	Contact *pContact = pDriver->findContact( nId );
	if ( pContact ) {
		pDriver->emitContact( *pContact, SIREventType::FingerUp, pContact->eventData.x, pContact->eventData.y );
		pContact->nId = -1;
	}
}

void SIRNativeInputDriverWayland::touchMotion( void *pData, wl_touch *, uint32_t, int32_t nId, int32_t nX, int32_t nY ) noexcept
{
	SIRNativeInputDriverWayland *pDriver = static_cast<SIRNativeInputDriverWayland *>( pData );
	Contact *pContact = pDriver->findContact( nId );
	if ( pContact && pDriver->m_nWidth && pDriver->m_nHeight ) {
		const float fX = clampNormalized( static_cast<float>( wl_fixed_to_double( nX ) ) / pDriver->m_nWidth );
		const float fY = clampNormalized( static_cast<float>( wl_fixed_to_double( nY ) ) / pDriver->m_nHeight );
		pDriver->emitContact( *pContact, SIREventType::FingerMotion, fX, fY );
	}
}

void SIRNativeInputDriverWayland::touchFrame( void *, wl_touch * ) noexcept
{
}

void SIRNativeInputDriverWayland::touchCancel( void *pData, wl_touch * ) noexcept
{
	static_cast<SIRNativeInputDriverWayland *>( pData )->cancelContacts();
}

void SIRNativeInputDriverWayland::cancelContacts( void ) noexcept
{
	for ( Contact& contact : m_szContacts ) {
		if ( contact.nId != -1 ) {
			emitContact( contact, SIREventType::FingerCanceled, contact.eventData.x, contact.eventData.y );
			contact.nId = -1;
		}
	}
}

SIRNativeInputBackend SIRNativeInputDriverWayland::getBackend( void ) const noexcept
{
	return SIRNativeInputBackend::Wayland;
}

void SIRNativeInputDriverWayland::pumpEvents( void ) noexcept
{
	// The window driver dispatches Wayland callbacks on its existing thread.
	repeatKeys();
	m_Gamepads.pumpEvents();
}

bool SIRNativeInputDriverWayland::isKeyPressed( SIRKeycode eKey ) const noexcept
{
	if ( eKey == SIRKeycode::Unknown ) {
		return false;
	}
	for ( const SIRKeyboardEvent& key : m_szKeys ) {
		if ( key.down && key.keycode == static_cast<uint32_t>( eKey ) ) {
			return true;
		}
	}
	return false;
}

bool SIRNativeInputDriverWayland::isMouseButtonPressed( SIRMouseButton eButton ) const noexcept
{
	return ( m_nButtons & SIRMouseButtonMask( eButton ) ) != 0;
}

void SIRNativeInputDriverWayland::publish( SIREventData& eventData ) noexcept
{
	// Wayland's uint32 millisecond timestamps wrap and have an unspecified epoch.
	// Match the existing input event ABI's monotonic nanosecond clock.
	eventData.timestamp = SIRPlatformTime::getMonotonicNanoseconds();
	if ( m_EventQueue.tryEnqueue( eventData ) != SIREventEnqueueStatus::SIR_EVENT_ENQUEUE_OK && m_nDroppedEvents != UINT32_MAX ) {
		++m_nDroppedEvents;
	}
}

uint32_t SIRNativeInputDriverWayland::droppedEventCount( void ) const noexcept
{
	const uint32_t nGamepadDrops = m_Gamepads.droppedEventCount();
	if ( nGamepadDrops > UINT32_MAX - m_nDroppedEvents ) {
		return UINT32_MAX;
	}

	return m_nDroppedEvents + nGamepadDrops;
}
