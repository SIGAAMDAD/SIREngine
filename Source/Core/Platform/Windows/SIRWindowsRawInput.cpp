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

#include <Core/Platform/Windows/SIRWindowsRawInput.h>
#include <Core/Platform/SIRPlatformTime.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <hidusage.h>

SIRWindowsRawInput::SIRWindowsRawInput( SIRInputSystem &input ) noexcept : m_Input( input ), m_pWindow( nullptr )
{
}

SIRWindowsRawInput::~SIRWindowsRawInput()
{
	shutdown();
}

bool SIRWindowsRawInput::initialize( void *pWindow ) noexcept
{
	if ( pWindow == nullptr || m_pWindow != nullptr ) {
		return false;
	}
	RAWINPUTDEVICE devices[2] = { { HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_KEYBOARD, RIDEV_DEVNOTIFY, ( HWND )pWindow },
	                              { HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_MOUSE, RIDEV_DEVNOTIFY, ( HWND )pWindow } };
	if ( !RegisterRawInputDevices( devices, 2, sizeof( devices[0] ) ) ) {
		return false;
	}
	m_pWindow = pWindow;
	return true;
}

bool SIRWindowsRawInput::processRawInput( void *pRawInput ) noexcept
{
	UINT nBytes = 0;
	if ( GetRawInputData( ( HRAWINPUT )pRawInput, RID_INPUT, nullptr, &nBytes, sizeof( RAWINPUTHEADER ) ) != 0 ||
	     nBytes < sizeof( RAWINPUTHEADER ) || nBytes > sizeof( RAWINPUT ) ) {
		return false;
	}
	RAWINPUT input = {};
	if ( GetRawInputData( ( HRAWINPUT )pRawInput, RID_INPUT, &input, &nBytes, sizeof( RAWINPUTHEADER ) ) != nBytes ) {
		return false;
	}
	const sirInputDeviceKind_t eKind = input.header.dwType == RIM_TYPEKEYBOARD ? SIR_INPUT_DEVICE_KEYBOARD : SIR_INPUT_DEVICE_MOUSE;
	if ( input.header.dwType != RIM_TYPEKEYBOARD && input.header.dwType != RIM_TYPEMOUSE ) {
		return false;
	}
	const uint64_t nId = ( uint64_t )( uintptr_t )input.header.hDevice;
	sirInputDeviceHandle_t hDevice = {};
	if ( !m_Input.findDevice( eKind, nId, &hDevice ) && !m_Input.addDevice( eKind, nId, &hDevice ) ) {
		return false;
	}
	const uint64_t nNow = SIRPlatformTime::getMonotonicNanoseconds();
	if ( eKind == SIR_INPUT_DEVICE_KEYBOARD ) {
		const RAWKEYBOARD &key = input.data.keyboard;
		const uint16_t nCode =
		    ( uint16_t )( key.MakeCode | ( ( key.Flags & RI_KEY_E0 ) ? 0x100 : 0 ) | ( ( key.Flags & RI_KEY_E1 ) ? 0x200 : 0 ) );
		if ( nCode == 0 ) {
			return true;
		}
		return m_Input.pushButton( hDevice, nCode, ( key.Flags & RI_KEY_BREAK ) == 0, nNow );
	}
	const RAWMOUSE &mouse = input.data.mouse;
	if ( ( mouse.usFlags & MOUSE_MOVE_ABSOLUTE ) == 0 && ( mouse.lLastX != 0 || mouse.lLastY != 0 ) &&
	     !m_Input.pushPointer( hDevice, mouse.lLastX, mouse.lLastY, nNow ) ) {
		return false;
	}
	const uint16_t nDown[] = { RI_MOUSE_LEFT_BUTTON_DOWN, RI_MOUSE_RIGHT_BUTTON_DOWN, RI_MOUSE_MIDDLE_BUTTON_DOWN, RI_MOUSE_BUTTON_4_DOWN,
	                           RI_MOUSE_BUTTON_5_DOWN };
	const uint16_t nUp[] = { RI_MOUSE_LEFT_BUTTON_UP, RI_MOUSE_RIGHT_BUTTON_UP, RI_MOUSE_MIDDLE_BUTTON_UP, RI_MOUSE_BUTTON_4_UP,
	                         RI_MOUSE_BUTTON_5_UP };
	for ( uint16_t nCode = 0; nCode < 5; ++nCode ) {
		if ( ( mouse.usButtonFlags & nDown[nCode] ) && !m_Input.pushButton( hDevice, nCode, true, nNow ) ) {
			return false;
		}
		if ( ( mouse.usButtonFlags & nUp[nCode] ) && !m_Input.pushButton( hDevice, nCode, false, nNow ) ) {
			return false;
		}
	}
	// Wheel data is retained as raw Windows WHEEL_DELTA units.
	if ( ( mouse.usButtonFlags & RI_MOUSE_WHEEL ) && !m_Input.pushWheel( hDevice, 0, ( SHORT )mouse.usButtonData, nNow ) ) {
		return false;
	}
	if ( ( mouse.usButtonFlags & RI_MOUSE_HWHEEL ) && !m_Input.pushWheel( hDevice, 1, ( SHORT )mouse.usButtonData, nNow ) ) {
		return false;
	}
	return true;
}

bool SIRWindowsRawInput::forgetDevice( uint64_t nNativeId ) noexcept
{
	bool bRemoved = false;
	for ( uint8_t eKind = SIR_INPUT_DEVICE_KEYBOARD; eKind <= SIR_INPUT_DEVICE_MOUSE; ++eKind ) {
		sirInputDeviceHandle_t hDevice = {};
		if ( m_Input.findDevice( ( sirInputDeviceKind_t )eKind, nNativeId, &hDevice ) ) {
			bRemoved |= m_Input.removeDevice( hDevice );
		}
	}
	return bRemoved;
}

bool SIRWindowsRawInput::processMessage( uint32_t nMessage, uintptr_t nWParam, intptr_t nLParam ) noexcept
{
	if ( m_pWindow == nullptr ) {
		return false;
	}
	if ( nMessage == WM_INPUT ) {
		return processRawInput( ( void * )nLParam );
	}
	if ( nMessage == WM_INPUT_DEVICE_CHANGE && nWParam == GIDC_REMOVAL ) {
		return forgetDevice( ( uint64_t )nLParam );
	}
	return false;
}

void SIRWindowsRawInput::shutdown( void ) noexcept
{
	if ( m_pWindow == nullptr ) {
		return;
	}
	RAWINPUTDEVICE devices[2] = { { HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_KEYBOARD, RIDEV_REMOVE, nullptr },
	                              { HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_MOUSE, RIDEV_REMOVE, nullptr } };
	RegisterRawInputDevices( devices, 2, sizeof( devices[0] ) );
	m_pWindow = nullptr;
}

bool SIRWindowsRawInput::ownsDevice( sirInputDeviceHandle_t hDevice ) const noexcept
{
	if ( m_pWindow == nullptr ) {
		return false;
	}
	sirInputDeviceState_t state = {};
	return m_Input.getState( hDevice, &state ) && ( state.eKind == SIR_INPUT_DEVICE_KEYBOARD || state.eKind == SIR_INPUT_DEVICE_MOUSE );
}
