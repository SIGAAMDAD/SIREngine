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

#include "SIRNativeInputDriverX11.h"
#include <Core/Platform/Shared/Diagnostics/SIRPlatformTime.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <X11/XF86keysym.h>
#ifdef SIR_INPUT_XINPUT2_AVAILABLE
#include <X11/extensions/XInput2.h>
#endif

struct SIRX11KeyName
{
	char szName[5];
	SIRScancode eCode;
};

// XKB's physical key names stay fixed when a user switches keyboard layouts.
// Row positions below are named by the USB HID key at that physical position.
static constexpr SIRX11KeyName s_szKeyNames[] = {
    { "AE01", SIRScancode::Num1 },
    { "AE02", SIRScancode::Num2 },
    { "AE03", SIRScancode::Num3 },
    { "AE04", SIRScancode::Num4 },
    { "AE05", SIRScancode::Num5 },
    { "AE06", SIRScancode::Num6 },
    { "AE07", SIRScancode::Num7 },
    { "AE08", SIRScancode::Num8 },
    { "AE09", SIRScancode::Num9 },
    { "AE10", SIRScancode::Num0 },
    { "AE11", SIRScancode::Minus },
    { "AE12", SIRScancode::Equals },
    { "AD01", SIRScancode::Q },
    { "AD02", SIRScancode::W },
    { "AD03", SIRScancode::E },
    { "AD04", SIRScancode::R },
    { "AD05", SIRScancode::T },
    { "AD06", SIRScancode::Y },
    { "AD07", SIRScancode::U },
    { "AD08", SIRScancode::I },
    { "AD09", SIRScancode::O },
    { "AD10", SIRScancode::P },
    { "AD11", SIRScancode::LeftBracket },
    { "AD12", SIRScancode::RightBracket },
    { "AC01", SIRScancode::A },
    { "AC02", SIRScancode::S },
    { "AC03", SIRScancode::D },
    { "AC04", SIRScancode::F },
    { "AC05", SIRScancode::G },
    { "AC06", SIRScancode::H },
    { "AC07", SIRScancode::J },
    { "AC08", SIRScancode::K },
    { "AC09", SIRScancode::L },
    { "AC10", SIRScancode::Semicolon },
    { "AC11", SIRScancode::Apostrophe },
    { "AB01", SIRScancode::Z },
    { "AB02", SIRScancode::X },
    { "AB03", SIRScancode::C },
    { "AB04", SIRScancode::V },
    { "AB05", SIRScancode::B },
    { "AB06", SIRScancode::N },
    { "AB07", SIRScancode::M },
    { "AB08", SIRScancode::Comma },
    { "AB09", SIRScancode::Period },
    { "AB10", SIRScancode::Slash },
    { "FK01", SIRScancode::F1 },
    { "FK02", SIRScancode::F2 },
    { "FK03", SIRScancode::F3 },
    { "FK04", SIRScancode::F4 },
    { "FK05", SIRScancode::F5 },
    { "FK06", SIRScancode::F6 },
    { "FK07", SIRScancode::F7 },
    { "FK08", SIRScancode::F8 },
    { "FK09", SIRScancode::F9 },
    { "FK10", SIRScancode::F10 },
    { "FK11", SIRScancode::F11 },
    { "FK12", SIRScancode::F12 },
    { "FK13", SIRScancode::F13 },
    { "FK14", SIRScancode::F14 },
    { "FK15", SIRScancode::F15 },
    { "FK16", SIRScancode::F16 },
    { "FK17", SIRScancode::F17 },
    { "FK18", SIRScancode::F18 },
    { "FK19", SIRScancode::F19 },
    { "FK20", SIRScancode::F20 },
    { "FK21", SIRScancode::F21 },
    { "FK22", SIRScancode::F22 },
    { "FK23", SIRScancode::F23 },
    { "FK24", SIRScancode::F24 },
    { "ESC", SIRScancode::Escape },
    { "TLDE", SIRScancode::Grave },
    { "BKSP", SIRScancode::Backspace },
    { "TAB", SIRScancode::Tab },
    { "RTRN", SIRScancode::Return },
    { "CAPS", SIRScancode::CapsLock },
    { "LFSH", SIRScancode::LeftShift },
    { "RTSH", SIRScancode::RightShift },
    { "LALT", SIRScancode::LeftAlt },
    { "RALT", SIRScancode::RightAlt },
    { "LCTL", SIRScancode::LeftCtrl },
    { "RCTL", SIRScancode::RightCtrl },
    { "LWIN", SIRScancode::LeftGUI },
    { "RWIN", SIRScancode::RightGUI },
    { "MENU", SIRScancode::Application },
    { "COMP", SIRScancode::Application },
    { "SPCE", SIRScancode::Space },
    { "BKSL", SIRScancode::Backslash },
    { "LSGT", SIRScancode::NonUSBackslash },
    { "PRSC", SIRScancode::PrintScreen },
    { "SCLK", SIRScancode::ScrollLock },
    { "PAUS", SIRScancode::Pause },
    { "INS", SIRScancode::Insert },
    { "HOME", SIRScancode::Home },
    { "PGUP", SIRScancode::PageUp },
    { "DELE", SIRScancode::Delete },
    { "END", SIRScancode::End },
    { "PGDN", SIRScancode::PageDown },
    { "RGHT", SIRScancode::Right },
    { "LEFT", SIRScancode::Left },
    { "DOWN", SIRScancode::Down },
    { "UP", SIRScancode::Up },
    { "NMLK", SIRScancode::NumLockClear },
    { "KPDV", SIRScancode::KeypadDivide },
    { "KPMU", SIRScancode::KeypadMultiply },
    { "KPSU", SIRScancode::KeypadMinus },
    { "KPAD", SIRScancode::KeypadPlus },
    { "KPEN", SIRScancode::KeypadEnter },
    { "KPDL", SIRScancode::KeypadPeriod },
    { "KPEQ", SIRScancode::KeypadEquals },
    { "MUTE", SIRScancode::Mute },
    { "VOL-", SIRScancode::VolumeDown },
    { "VOL+", SIRScancode::VolumeUp },
    { "KP0", SIRScancode::Keypad0 },
    { "KP1", SIRScancode::Keypad1 },
    { "KP2", SIRScancode::Keypad2 },
    { "KP3", SIRScancode::Keypad3 },
    { "KP4", SIRScancode::Keypad4 },
    { "KP5", SIRScancode::Keypad5 },
    { "KP6", SIRScancode::Keypad6 },
    { "KP7", SIRScancode::Keypad7 },
    { "KP8", SIRScancode::Keypad8 },
    { "KP9", SIRScancode::Keypad9 },
};

static SIRKeycode logicalKeyNavigation( KeySym nKeysym ) noexcept
{
	switch ( nKeysym ) {
	case XK_Return:
		return SIRKeycode::Return;
	case XK_Escape:
		return SIRKeycode::Escape;
	case XK_BackSpace:
		return SIRKeycode::Backspace;
	case XK_Tab:
		return SIRKeycode::Tab;
	case XK_Delete:
		return SIRKeycode::Delete;
	case XK_Caps_Lock:
		return SIRKeycode::CapsLock;
	case XK_Print:
		return SIRKeycode::PrintScreen;
	case XK_Scroll_Lock:
		return SIRKeycode::ScrollLock;
	case XK_Pause:
		return SIRKeycode::Pause;
	case XK_Insert:
		return SIRKeycode::Insert;
	case XK_Home:
		return SIRKeycode::Home;
	case XK_Prior:
		return SIRKeycode::PageUp;
	case XK_End:
		return SIRKeycode::End;
	case XK_Next:
		return SIRKeycode::PageDown;
	case XK_Right:
		return SIRKeycode::Right;
	case XK_Left:
		return SIRKeycode::Left;
	case XK_Down:
		return SIRKeycode::Down;
	case XK_Up:
		return SIRKeycode::Up;
	case XK_Num_Lock:
		return SIRKeycode::NumLockClear;
	default:
		return SIRKeycode::Unknown;
	}
}

static SIRKeycode logicalKeyModifiers( KeySym nKeysym ) noexcept
{
	switch ( nKeysym ) {
	case XK_Shift_L:
		return SIRKeycode::LeftShift;
	case XK_Shift_R:
		return SIRKeycode::RightShift;
	case XK_Control_L:
		return SIRKeycode::LeftCtrl;
	case XK_Control_R:
		return SIRKeycode::RightCtrl;
	case XK_Alt_L:
		return SIRKeycode::LeftAlt;
	case XK_Alt_R:
		return SIRKeycode::RightAlt;
	case XK_Super_L:
		return SIRKeycode::LeftGUI;
	case XK_Super_R:
		return SIRKeycode::RightGUI;
	case XK_Meta_L:
		return SIRKeycode::LeftMeta;
	case XK_Meta_R:
		return SIRKeycode::RightMeta;
	case XK_Hyper_L:
		return SIRKeycode::LeftHyper;
	case XK_Hyper_R:
		return SIRKeycode::RightHyper;
	case XK_ISO_Level3_Shift:
		return SIRKeycode::Mode;
	case XK_ISO_Level5_Shift:
		return SIRKeycode::Level5Shift;
	case XK_Multi_key:
		return SIRKeycode::MultiKeyCompose;
	case XK_ISO_Left_Tab:
		return SIRKeycode::LeftTab;
	case XK_Menu:
		return SIRKeycode::Application;
	default:
		return SIRKeycode::Unknown;
	}
}

static SIRKeycode logicalKeyKeypad( KeySym nKeysym ) noexcept
{
	switch ( nKeysym ) {
	case XK_KP_Divide:
		return SIRKeycode::KeypadDivide;
	case XK_KP_Multiply:
		return SIRKeycode::KeypadMultiply;
	case XK_KP_Subtract:
		return SIRKeycode::KeypadMinus;
	case XK_KP_Add:
		return SIRKeycode::KeypadPlus;
	case XK_KP_Enter:
		return SIRKeycode::KeypadEnter;
	case XK_KP_Decimal:
		return SIRKeycode::KeypadPeriod;
	case XK_KP_Delete:
		return SIRKeycode::KeypadPeriod;
	case XK_KP_Equal:
		return SIRKeycode::KeypadEquals;
	case XK_KP_Insert:
		return SIRKeycode::Keypad0;
	case XK_KP_End:
		return SIRKeycode::Keypad1;
	case XK_KP_Down:
		return SIRKeycode::Keypad2;
	case XK_KP_Next:
		return SIRKeycode::Keypad3;
	case XK_KP_Left:
		return SIRKeycode::Keypad4;
	case XK_KP_Begin:
		return SIRKeycode::Keypad5;
	case XK_KP_Right:
		return SIRKeycode::Keypad6;
	case XK_KP_Home:
		return SIRKeycode::Keypad7;
	case XK_KP_Up:
		return SIRKeycode::Keypad8;
	case XK_KP_Prior:
		return SIRKeycode::Keypad9;
	case XK_KP_0:
		return SIRKeycode::Keypad0;
	case XK_KP_1:
		return SIRKeycode::Keypad1;
	case XK_KP_2:
		return SIRKeycode::Keypad2;
	case XK_KP_3:
		return SIRKeycode::Keypad3;
	case XK_KP_4:
		return SIRKeycode::Keypad4;
	case XK_KP_5:
		return SIRKeycode::Keypad5;
	case XK_KP_6:
		return SIRKeycode::Keypad6;
	case XK_KP_7:
		return SIRKeycode::Keypad7;
	case XK_KP_8:
		return SIRKeycode::Keypad8;
	case XK_KP_9:
		return SIRKeycode::Keypad9;
	default:
		return SIRKeycode::Unknown;
	}
}

static SIRKeycode logicalKeyMedia( KeySym nKeysym ) noexcept
{
	switch ( nKeysym ) {
	case XF86XK_AudioMute:
		return SIRKeycode::Mute;
	case XF86XK_AudioRaiseVolume:
		return SIRKeycode::VolumeUp;
	case XF86XK_AudioLowerVolume:
		return SIRKeycode::VolumeDown;
	case XF86XK_AudioPlay:
		return SIRKeycode::MediaPlay;
	case XF86XK_AudioPause:
		return SIRKeycode::MediaPause;
	case XF86XK_AudioStop:
		return SIRKeycode::MediaStop;
	case XF86XK_AudioPrev:
		return SIRKeycode::MediaPreviousTrack;
	case XF86XK_AudioNext:
		return SIRKeycode::MediaNextTrack;
	case XF86XK_AudioRecord:
		return SIRKeycode::MediaRecord;
	case XF86XK_AudioRewind:
		return SIRKeycode::MediaRewind;
	case XF86XK_AudioForward:
		return SIRKeycode::MediaFastForward;
	case XF86XK_Eject:
		return SIRKeycode::MediaEject;
	case XF86XK_Back:
		return SIRKeycode::ACBack;
	case XF86XK_Forward:
		return SIRKeycode::ACForward;
	case XF86XK_Refresh:
		return SIRKeycode::ACRefresh;
	case XF86XK_HomePage:
		return SIRKeycode::ACHome;
	case XF86XK_Search:
		return SIRKeycode::ACSearch;
	case XF86XK_Sleep:
		return SIRKeycode::Sleep;
	case XF86XK_WakeUp:
		return SIRKeycode::Wake;
	case XF86XK_PowerOff:
		return SIRKeycode::Power;
	default:
		return SIRKeycode::Unknown;
	}
}

static SIRKeycode logicalKey( KeySym nKeysym ) noexcept
{
	if ( nKeysym >= XK_A && nKeysym <= XK_Z ) {
		return static_cast<SIRKeycode>( nKeysym - XK_A + XK_a );
	}
	const bool bPrintableASCII = nKeysym >= 0x20 && nKeysym <= 0x7e;
	const bool bLatin1 = nKeysym >= 0xa0 && nKeysym <= 0xff;
	if ( bPrintableASCII || bLatin1 ) {
		return static_cast<SIRKeycode>( nKeysym );
	}
	if ( nKeysym >= 0x01000100 && nKeysym <= 0x0110ffff ) {
		return static_cast<SIRKeycode>( nKeysym - 0x01000000 );
	}
	if ( nKeysym >= XK_F1 && nKeysym <= XK_F24 ) {
		const SIRKeycode eFirst = nKeysym <= XK_F12 ? SIRKeycode::F1 : SIRKeycode::F13;
		return static_cast<SIRKeycode>( static_cast<uint32_t>( eFirst ) + nKeysym - ( nKeysym <= XK_F12 ? XK_F1 : XK_F13 ) );
	}
	const SIRKeycode eLogicalKeyNavigation = logicalKeyNavigation( nKeysym );
	if ( eLogicalKeyNavigation != SIRKeycode::Unknown ) {
		return eLogicalKeyNavigation;
	}
	const SIRKeycode eLogicalKeyModifiers = logicalKeyModifiers( nKeysym );
	if ( eLogicalKeyModifiers != SIRKeycode::Unknown ) {
		return eLogicalKeyModifiers;
	}
	const SIRKeycode eLogicalKeyKeypad = logicalKeyKeypad( nKeysym );
	if ( eLogicalKeyKeypad != SIRKeycode::Unknown ) {
		return eLogicalKeyKeypad;
	}
	const SIRKeycode eLogicalKeyMedia = logicalKeyMedia( nKeysym );
	if ( eLogicalKeyMedia != SIRKeycode::Unknown ) {
		return eLogicalKeyMedia;
	}
	return SIRKeycode::Unknown;
}

static uint16_t modifierForKey( SIRKeycode eKey ) noexcept
{
	switch ( eKey ) {
	case SIRKeycode::LeftShift:
		return static_cast<uint16_t>( SIRKeyModifier::LeftShift );
	case SIRKeycode::RightShift:
		return static_cast<uint16_t>( SIRKeyModifier::RightShift );
	case SIRKeycode::LeftCtrl:
		return static_cast<uint16_t>( SIRKeyModifier::LeftCtrl );
	case SIRKeycode::RightCtrl:
		return static_cast<uint16_t>( SIRKeyModifier::RightCtrl );
	case SIRKeycode::LeftAlt:
		return static_cast<uint16_t>( SIRKeyModifier::LeftAlt );
	case SIRKeycode::RightAlt:
		return static_cast<uint16_t>( SIRKeyModifier::RightAlt );
	case SIRKeycode::LeftGUI:
	case SIRKeycode::LeftMeta:
		return static_cast<uint16_t>( SIRKeyModifier::LeftGUI );
	case SIRKeycode::RightGUI:
	case SIRKeycode::RightMeta:
		return static_cast<uint16_t>( SIRKeyModifier::RightGUI );
	default:
		return 0;
	}
}

#if defined( SIR_INPUT_XINPUT2_AVAILABLE ) && defined( XI_TouchBegin )
static float clampUnitInterval( float fValue ) noexcept
{
	if ( fValue < 0.0f ) {
		return 0.0f;
	}
	if ( fValue > 1.0f ) {
		return 1.0f;
	}
	return fValue;
}
#endif

#ifdef SIR_INPUT_XINPUT2_AVAILABLE
// XISelectEvents replaces this client's selection for a device. Preserve other
// users' bits, and remember exactly which bits to remove on detach.
struct SIRX11SelectionResult
{
	bool bSuccess;
	uint32_t nAddedBits;
};

static SIRX11SelectionResult changeXISelection( Display *pDisplay, Window hWindow, uint32_t nBits, bool bAdd ) noexcept
{
	int nCount = 0;
	XIEventMask *pExisting = XIGetSelectedEvents( pDisplay, hWindow, &nCount );
	unsigned char szMask[XIMaskLen( XI_LASTEVENT )]{};
	XIEventMask selection{ XIAllMasterDevices, sizeof( szMask ), szMask };
	for ( int iIndex = 0; iIndex < nCount; ++iIndex ) {
		if ( pExisting[iIndex].deviceid == XIAllMasterDevices ) {
			if ( pExisting[iIndex].mask_len >= selection.mask_len ) {
				selection = pExisting[iIndex];
			} else {
				std::memcpy( szMask, pExisting[iIndex].mask, pExisting[iIndex].mask_len );
			}
			break;
		}
	}
	uint32_t nDelta = 0;
	for ( unsigned iBit = 0; iBit < 32 && iBit < static_cast<unsigned>( selection.mask_len ) * 8; ++iBit ) {
		const uint32_t nFlag = 1u << iBit;
		if ( !( nBits & nFlag ) ) {
			continue;
		}
		if ( bAdd && !XIMaskIsSet( selection.mask, iBit ) ) {
			XISetMask( selection.mask, iBit );
			nDelta |= nFlag;
		} else if ( !bAdd && XIMaskIsSet( selection.mask, iBit ) ) {
			XIClearMask( selection.mask, iBit );
			nDelta |= nFlag;
		}
	}
	const bool bSuccess = !nDelta || XISelectEvents( pDisplay, hWindow, &selection, 1 ) == Success;
	XFree( pExisting );
	return { bSuccess, bSuccess && bAdd ? nDelta : 0 };
}
#endif

SIRNativeInputDriverX11::SIRNativeInputDriverX11( SIRSPSCEventRing<SIREventData>& eventQueue ) noexcept
    : SIRNativeInputDriver( eventQueue ),
      m_Gamepads( m_EventQueue )
{
}

SIRNativeInputDriverX11::~SIRNativeInputDriverX11() noexcept
{
	detachWindow();
}

bool SIRNativeInputDriverX11::attachWindow( Display *pDisplay, Window hWindow, uint32_t nWindowId ) noexcept
{
	XWindowAttributes attributes{};
	if ( m_pDisplay || !pDisplay || !hWindow ) {
		return false;
	}
	if ( !XGetWindowAttributes( pDisplay, hWindow, &attributes ) ) {
		return false;
	}

	m_pDisplay = pDisplay;
	m_hWindow = hWindow;
	m_hRoot = attributes.root;
	m_nWindowId = nWindowId;
	m_nWidth = attributes.width;
	m_nHeight = attributes.height;

	const long nRequiredMask = StructureNotifyMask | FocusChangeMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
	m_nAddedCoreMask = nRequiredMask & ~attributes.your_event_mask;
	XSelectInput( pDisplay, hWindow, attributes.your_event_mask | nRequiredMask );

	Bool bSupported = False;

	const bool bPreviouslyEnabled = XkbGetDetectableAutoRepeat( pDisplay, &bSupported );
	m_bDetectableRepeat = XkbSetDetectableAutoRepeat( pDisplay, True, &bSupported ) && bSupported;
	m_bRestoreRepeat = m_bDetectableRepeat && !bPreviouslyEnabled;

	updateKeyboardMap();

	Window hFocusedWindow = 0;
	int nRevert = 0;

	XGetInputFocus( pDisplay, &hFocusedWindow, &nRevert );
	m_bFocused = hFocusedWindow == hWindow;
#ifdef SIR_INPUT_XINPUT2_AVAILABLE
	int nOpcode = 0;
	int nXExtensionEvent = 0;
	int nError = 0;
	int nMajor = 2;
	int nMinor = 2;
	if ( XQueryExtension( pDisplay, "XInputExtension", &nOpcode, &nXExtensionEvent, &nError ) && XIQueryVersion( pDisplay, &nMajor, &nMinor ) == Success && nMajor >= 2 ) {
		m_nXIOpcode = nOpcode;
		const SIRX11SelectionResult rawSelection = changeXISelection( pDisplay, m_hRoot, 1u << XI_RawMotion, true );
		m_bRawMotion = rawSelection.bSuccess;
		m_nAddedRootXI = rawSelection.nAddedBits;
#ifdef XI_TouchBegin
		if ( nMajor > 2 || nMinor >= 2 ) {
			const uint32_t nTouchMask = ( 1u << XI_TouchBegin ) | ( 1u << XI_TouchUpdate ) | ( 1u << XI_TouchEnd );
			m_nAddedWindowXI = changeXISelection( pDisplay, hWindow, nTouchMask, true ).nAddedBits;
		}
#endif
	}
#endif
	XFlush( pDisplay );
	return true;
}

void SIRNativeInputDriverX11::detachWindow( bool bConnectionLost ) noexcept
{
	if ( !m_pDisplay ) {
		return;
	}
	releaseInput();
	if ( !bConnectionLost ) {
		XWindowAttributes attributes{};
		if ( m_hWindow && m_nAddedCoreMask && XGetWindowAttributes( m_pDisplay, m_hWindow, &attributes ) ) {
			XSelectInput( m_pDisplay, m_hWindow, attributes.your_event_mask & ~m_nAddedCoreMask );
		}
#ifdef SIR_INPUT_XINPUT2_AVAILABLE
		if ( m_nAddedRootXI ) {
			changeXISelection( m_pDisplay, m_hRoot, m_nAddedRootXI, false );
		}
		if ( m_hWindow && m_nAddedWindowXI ) {
			changeXISelection( m_pDisplay, m_hWindow, m_nAddedWindowXI, false );
		}
#endif
		if ( m_bRestoreRepeat ) {
			Bool bSupported = False;
			XkbSetDetectableAutoRepeat( m_pDisplay, False, &bSupported );
		}
		XFlush( m_pDisplay );
	}
	m_pDisplay = nullptr;
	m_hWindow = 0;
	m_nXIOpcode = -1;
	m_nAddedWindowXI = 0;
	m_nAddedRootXI = 0;
	m_bRawMotion = false;
	m_bFocused = false;
}

void SIRNativeInputDriverX11::updateKeyboardMap( void ) noexcept
{
	for ( SIRScancode& eScancode : m_szScancodes ) {
		eScancode = SIRScancode::Unknown;
	}
	XkbDescPtr pKeyboardMap = XkbGetMap( m_pDisplay, 0, XkbUseCoreKbd );
	if ( pKeyboardMap ) {
		if ( XkbGetNames( m_pDisplay, XkbKeyNamesMask, pKeyboardMap ) == Success && pKeyboardMap->names && pKeyboardMap->names->keys ) {
			for ( unsigned iIndex = pKeyboardMap->min_key_code; iIndex <= pKeyboardMap->max_key_code; ++iIndex ) {
				for ( const auto& entry : s_szKeyNames ) {
					if ( std::memcmp( pKeyboardMap->names->keys[iIndex].name, entry.szName, XkbKeyNameLength ) == 0 ) {
						m_szScancodes[iIndex] = entry.eCode;
						break;
					}
				}
			}
		}
		XkbFreeKeyboard( pKeyboardMap, XkbAllComponentsMask, True );
	}
	m_nNumMask = XkbKeysymToModifiers( m_pDisplay, XK_Num_Lock );
	m_nScrollMask = XkbKeysymToModifiers( m_pDisplay, XK_Scroll_Lock );
	m_nModeMask = XkbKeysymToModifiers( m_pDisplay, XK_ISO_Level3_Shift );
	m_nLevel5Mask = XkbKeysymToModifiers( m_pDisplay, XK_ISO_Level5_Shift );
}

SIRNativeInputBackend SIRNativeInputDriverX11::getBackend( void ) const noexcept
{
	return SIRNativeInputBackend::X11;
}

void SIRNativeInputDriverX11::pumpEvents( void ) noexcept
{
	// The window pump forwards X events through eventSink in native order.
	// Only the desktop-independent evdev source needs polling here.
	m_Gamepads.pumpEvents();
}

void SIRNativeInputDriverX11::eventSink( const XEvent& eventData, void *pContext ) noexcept
{
	if ( pContext ) {
		static_cast<SIRNativeInputDriverX11 *>( pContext )->processEvent( eventData );
	}
}

bool SIRNativeInputDriverX11::isKeyPressed( SIRKeycode eKey ) const noexcept
{
	if ( eKey == SIRKeycode::Unknown ) {
		return false;
	}

	for ( const SIRKeyboardEvent& inputState : m_szKeys ) {
		if ( inputState.down && inputState.keycode == static_cast<uint32_t>( eKey ) ) {
			return true;
		}
	}

	return false;
}

bool SIRNativeInputDriverX11::isMouseButtonPressed( SIRMouseButton eButton ) const noexcept
{
	return ( m_nButtons & SIRMouseButtonMask( eButton ) ) != 0;
}

void SIRNativeInputDriverX11::publish( SIREventData& eventData ) noexcept
{
	// X server timestamps use a wrapping millisecond clock with an unspecified
	// origin. The engine requires its own monotonic nanoseconds.
	eventData.timestamp = SIRPlatformTime::getMonotonicNanoseconds();
	if ( m_EventQueue.tryEnqueue( eventData ) != SIREventEnqueueStatus::SIR_EVENT_ENQUEUE_OK && m_nDroppedEvents != UINT32_MAX ) {
		++m_nDroppedEvents;
	}
}

void SIRNativeInputDriverX11::keyEvent( unsigned nCode, bool bDown, unsigned nState ) noexcept
{
	if ( nCode >= m_szKeys.size() ) {
		return;
	}
	auto& keyState = m_szKeys[nCode];
	const bool bHeld = keyState.down;
	if ( !bHeld && !bDown ) {
		return;
	}

	// Keep the identity of a held key across a group switch or keyboard remap.
	if ( !bHeld ) {
		const auto nKeysym = XkbKeycodeToKeysym( m_pDisplay, nCode, XkbGroupForCoreState( nState ), 0 );
		keyState.keycode = static_cast<uint32_t>( logicalKey( nKeysym ) );
		keyState.nativeScancode = static_cast<uint16_t>( nCode );
		keyState.scancode = static_cast<uint32_t>( m_szScancodes[nCode] );
		keyState.windowId = m_nWindowId;
	}
	const auto eLogical = static_cast<SIRKeycode>( keyState.keycode );
	const uint16_t nSideModifier = modifierForKey( eLogical );

	m_nModifiers = bDown ? m_nModifiers | nSideModifier : m_nModifiers & ~nSideModifier;
	keyState.down = bDown;
	keyState.repeat = bHeld && bDown;

	// A core event reports modifier state immediately before this transition.
	// Lock keys toggle on the first press. Level selectors follow their keys.
	if ( bDown && !bHeld ) {
		if ( eLogical == SIRKeycode::NumLockClear ) {
			nState ^= m_nNumMask;
		}
		if ( eLogical == SIRKeycode::CapsLock ) {
			nState ^= LockMask;
		}
		if ( eLogical == SIRKeycode::ScrollLock ) {
			nState ^= m_nScrollMask;
		}
	}
	if ( eLogical == SIRKeycode::Mode ) {
		nState = bDown ? nState | m_nModeMask : nState & ~m_nModeMask;
	}
	if ( eLogical == SIRKeycode::Level5Shift ) {
		nState = bDown ? nState | m_nLevel5Mask : nState & ~m_nLevel5Mask;
	}

	keyState.modifiers = m_nModifiers;
	if ( nState & LockMask ) {
		keyState.modifiers |= static_cast<uint16_t>( SIRKeyModifier::CapsLock );
	}
	if ( nState & m_nNumMask ) {
		keyState.modifiers |= static_cast<uint16_t>( SIRKeyModifier::NumLock );
	}
	if ( nState & m_nScrollMask ) {
		keyState.modifiers |= static_cast<uint16_t>( SIRKeyModifier::ScrollLock );
	}
	if ( nState & m_nModeMask ) {
		keyState.modifiers |= static_cast<uint16_t>( SIRKeyModifier::Mode );
	}
	if ( nState & m_nLevel5Mask ) {
		keyState.modifiers |= static_cast<uint16_t>( SIRKeyModifier::Level5 );
	}

	SIREventData eventData{};
	eventData.type = bDown ? SIREventType::KeyDown : SIREventType::KeyUp;
	eventData.key = keyState;

	publish( eventData );
}

void SIRNativeInputDriverX11::buttonEvent( SIRMouseButton eButton, bool bDown ) noexcept
{
	if ( isMouseButtonPressed( eButton ) == bDown ) {
		return;
	}
	const uint32_t nButtonMask = SIRMouseButtonMask( eButton );
	m_nButtons = bDown ? m_nButtons | nButtonMask : m_nButtons & ~nButtonMask;

	SIREventData eventData{};
	eventData.type = bDown ? SIREventType::MouseButtonDown : SIREventType::MouseButtonUp;
	eventData.button = { m_nWindowId, 0, static_cast<uint8_t>( eButton ), bDown, 1, 0, m_fX, m_fY };

	publish( eventData );
}

void SIRNativeInputDriverX11::releaseInput( void ) noexcept
{
	// Once focus leaves, matching releases can go to another client.
	for ( unsigned iIndex = 0; iIndex < m_szKeys.size(); ++iIndex ) {
		if ( m_szKeys[iIndex].down ) {
			keyEvent( iIndex, false, 0 );
		}
	}
	for ( unsigned iIndex = 1; iIndex <= static_cast<unsigned>( SIRMouseButton::Button16 ); ++iIndex ) {
		buttonEvent( static_cast<SIRMouseButton>( iIndex ), false );
	}
	while ( !m_Contacts.empty() ) {
		SIREventData eventData{};
		eventData.type = SIREventType::FingerCanceled;
		eventData.tfinger = m_Contacts.back();
		eventData.tfinger.deltaX = 0;
		eventData.tfinger.deltaY = 0;
		eventData.tfinger.pressure = 0;
		m_Contacts.pop_back();
		publish( eventData );
	}

	m_bHavePosition = false;
	m_nModifiers = 0;
}

void SIRNativeInputDriverX11::processMouseButtonEvent( const XEvent& nativeEvent ) noexcept
{
	m_fX = static_cast<float>( nativeEvent.xbutton.x );
	m_fY = static_cast<float>( nativeEvent.xbutton.y );
	m_bHavePosition = true;

	const unsigned nNativeButton = nativeEvent.xbutton.button;
	if ( nNativeButton >= 4 && nNativeButton <= 7 ) {
		if ( nativeEvent.type == ButtonPress ) {
			const int nHorizontal = ( nNativeButton == 7 ) - ( nNativeButton == 6 );
			const int nVertical = ( nNativeButton == 4 ) - ( nNativeButton == 5 );

			SIREventData eventData{};
			eventData.type = SIREventType::MouseWheel;
			eventData.wheel = { m_nWindowId, 0, static_cast<float>( nHorizontal ), static_cast<float>( nVertical ), 0, m_fX, m_fY, nHorizontal, nVertical };
			publish( eventData );
		}
	} else if ( nNativeButton > 0 && nNativeButton <= 20 ) {
		const unsigned nMappedButton = nNativeButton <= 3 ? nNativeButton : nNativeButton - 4;
		buttonEvent( static_cast<SIRMouseButton>( nMappedButton ), nativeEvent.type == ButtonPress );
	}
}

void SIRNativeInputDriverX11::processEvent( const XEvent& nativeEvent ) noexcept
{
	if ( !m_pDisplay || nativeEvent.xany.display != m_pDisplay ) {
		return;
	}
	if ( nativeEvent.type == GenericEvent ) {
		extensionEvent( nativeEvent );
		return;
	}
	if ( nativeEvent.type == MappingNotify ) {
		XMappingEvent mapping = nativeEvent.xmapping;
		XRefreshKeyboardMapping( &mapping );
		if ( mapping.request != MappingPointer ) {
			updateKeyboardMap();
		}
		return;
	}
	if ( !m_hWindow || nativeEvent.xany.window != m_hWindow ) {
		return;
	}

	switch ( nativeEvent.type ) {
	case FocusIn:
		m_bFocused = true;
		m_bHavePosition = false;
		break;
	case DestroyNotify:
		m_hWindow = 0; // X no longer accepts requests on this resource.
		[[fallthrough]];
	case FocusOut:
	case UnmapNotify:
		m_bFocused = false;
		releaseInput();
		break;
	case ConfigureNotify:
		m_nWidth = nativeEvent.xconfigure.width;
		m_nHeight = nativeEvent.xconfigure.height;
		break;
	case KeyPress:
	case KeyRelease:
		if ( nativeEvent.type == KeyRelease && !m_bDetectableRepeat && XEventsQueued( m_pDisplay, QueuedAfterReading ) > 0 ) {
			// Legacy servers send a synthetic release immediately before the
			// repeated press. Peek only; the window driver owns both events.
			XEvent nextEvent{};
			XPeekEvent( m_pDisplay, &nextEvent );
			const bool bSameKey = nextEvent.xkey.window == m_hWindow && nextEvent.xkey.keycode == nativeEvent.xkey.keycode;
			if ( nextEvent.type == KeyPress && bSameKey && nextEvent.xkey.time == nativeEvent.xkey.time ) {
				break;
			}
		}
		keyEvent( nativeEvent.xkey.keycode, nativeEvent.type == KeyPress, nativeEvent.xkey.state );
		break;
	case ButtonPress:
	case ButtonRelease:
		processMouseButtonEvent( nativeEvent );
		break;
	case MotionNotify: {
		SIREventData eventData{};

		const float fX = static_cast<float>( nativeEvent.xmotion.x );
		const float fY = static_cast<float>( nativeEvent.xmotion.y );
		const bool bDeriveDelta = m_bHavePosition && !( m_bRawMotion && m_bFocused );

		eventData.type = SIREventType::MouseMotion;
		eventData.motion = { m_nWindowId, 0, m_nButtons, fX, fY, bDeriveDelta ? fX - m_fX : 0, bDeriveDelta ? fY - m_fY : 0 };
		m_fX = fX;
		m_fY = fY;
		m_bHavePosition = true;

		publish( eventData );

		break;
	}
	default:
		break;
	}
}

void SIRNativeInputDriverX11::extensionEvent( const XEvent& nativeEvent ) noexcept
{
#ifdef SIR_INPUT_XINPUT2_AVAILABLE
	XGenericEventCookie cookie = nativeEvent.xcookie;
	if ( cookie.extension != m_nXIOpcode || !m_hWindow ) {
		return;
	}
	const bool bBorrowed = cookie.data != nullptr;
	if ( !bBorrowed && !XGetEventData( m_pDisplay, &cookie ) ) {
		return;
	}
	SIREventData eventData{};
	if ( cookie.evtype == XI_RawMotion && m_bFocused && m_bRawMotion ) {
		const auto& raw = *static_cast<const XIRawEvent *>( cookie.data );
		// Valuator arrays contain only set mask bits, in ascending axis order.
		const double *pValues = raw.raw_values;
		float fDeltaX = 0;
		float fDeltaY = 0;
		if ( raw.valuators.mask_len > 0 ) {
			if ( XIMaskIsSet( raw.valuators.mask, 0 ) ) {
				fDeltaX = static_cast<float>( *pValues++ );
			}
			if ( XIMaskIsSet( raw.valuators.mask, 1 ) ) {
				fDeltaY = static_cast<float>( *pValues );
			}
		}
		if ( fDeltaX || fDeltaY ) {
			eventData.type = SIREventType::MouseMotion;
			eventData.motion = { m_nWindowId, 0, m_nButtons, m_fX, m_fY, fDeltaX, fDeltaY };
			publish( eventData );
		}
	}
#ifdef XI_TouchBegin
	else if ( cookie.evtype == XI_TouchBegin || cookie.evtype == XI_TouchUpdate || cookie.evtype == XI_TouchEnd ) {
		const auto& touch = *static_cast<const XIDeviceEvent *>( cookie.data );
		if ( touch.event == m_hWindow && m_nWidth > 0 && m_nHeight > 0 ) {
			const auto nDeviceId = static_cast<uint64_t>( touch.sourceid );
			const auto nFingerId = static_cast<uint64_t>( static_cast<unsigned>( touch.detail ) );
			const float fX = clampUnitInterval( static_cast<float>( touch.event_x / m_nWidth ) );
			const float fY = clampUnitInterval( static_cast<float>( touch.event_y / m_nHeight ) );
			auto contactIt = m_Contacts.begin();
			for ( ; contactIt != m_Contacts.end(); ++contactIt ) {
				if ( contactIt->touchId == nDeviceId && contactIt->fingerId == nFingerId ) {
					break;
				}
			}
			if ( cookie.evtype == XI_TouchBegin && contactIt == m_Contacts.end() ) {
				eventData.type = SIREventType::FingerDown;
				eventData.tfinger = { nDeviceId, nFingerId, fX, fY, 0, 0, 1, m_nWindowId };
				try {
					m_Contacts.push_back( eventData.tfinger );
					publish( eventData );
				} catch ( const std::bad_alloc& ) {
					// Ignore an untrackable contact and its later updates.
				}
			} else if ( cookie.evtype != XI_TouchBegin && contactIt != m_Contacts.end() ) {
				const bool bEnded = cookie.evtype == XI_TouchEnd;
				eventData.type = bEnded ? SIREventType::FingerUp : SIREventType::FingerMotion;
				eventData.tfinger = { nDeviceId, nFingerId, fX, fY, fX - contactIt->x, fY - contactIt->y, bEnded ? 0.f : 1.f, m_nWindowId };
				if ( bEnded ) {
					m_Contacts.erase( contactIt );
				} else {
					*contactIt = eventData.tfinger;
				}
				publish( eventData );
			}
		}
	}
#endif
	if ( !bBorrowed ) {
		XFreeEventData( cookie.display, &cookie );
	}
#else
	static_cast<void>( nativeEvent );
#endif
}

uint32_t SIRNativeInputDriverX11::droppedEventCount( void ) const noexcept
{
	const uint32_t nGamepadDrops = m_Gamepads.droppedEventCount();
	if ( nGamepadDrops > UINT32_MAX - m_nDroppedEvents ) {
		return UINT32_MAX;
	}

	return m_nDroppedEvents + nGamepadDrops;
}
