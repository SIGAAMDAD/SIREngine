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

#include <Core/Platform/Input/Windows/SIRWindowsRawBackend.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

SIRWindowsRawBackend::SIRWindowsRawBackend( void ) noexcept : m_pWindow( nullptr )
{
}
sirInputBackendId_t SIRWindowsRawBackend::id( void ) const noexcept
{
	return SIR_INPUT_WIN_RAW;
}
void SIRWindowsRawBackend::sample( SIRInputService &service ) noexcept
{
	( void )service;
}
void SIRWindowsRawBackend::resynchronize( SIRInputService &service ) noexcept
{
	( void )service;
}
bool SIRWindowsRawBackend::vibrate( sirInputDeviceId_t id, uint16_t nL, uint16_t nR ) noexcept
{
	( void )id;
	( void )nL;
	( void )nR;
	return false;
}

bool SIRWindowsRawBackend::attachWindow( void *pWindow ) noexcept
{
	if ( m_pWindow != nullptr || pWindow == nullptr ) {
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

bool SIRWindowsRawBackend::processRaw( SIRInputService &service, void *pHRawInput ) noexcept
{
	UINT nBytes = 0;
	if ( GetRawInputData( ( HRAWINPUT )pHRawInput, RID_INPUT, nullptr, &nBytes, sizeof( RAWINPUTHEADER ) ) != 0 ||
	     nBytes > sizeof( RAWINPUT ) ) {
		return false;
	}
	RAWINPUT input = {};
	if ( GetRawInputData( ( HRAWINPUT )pHRawInput, RID_INPUT, &input, &nBytes, sizeof( RAWINPUTHEADER ) ) == ( UINT )-1 ) {
		return false;
	}
	if ( input.header.dwType != RIM_TYPEKEYBOARD && input.header.dwType != RIM_TYPEMOUSE ) {
		return false;
	}
	const sirInputDeviceClass_t eClass = input.header.dwType == RIM_TYPEKEYBOARD ? SIR_INPUT_KEYBOARD : SIR_INPUT_POINTER;
	const uint64_t nNative = ( uint64_t )( uintptr_t )input.header.hDevice;
	if ( nNative == 0 ) {
		return false;
	}
	sirInputDeviceId_t hDevice = {};
	if ( !service.findDevice( id(), nNative, &hDevice ) && !service.connect( { 0, nNative, id(), eClass }, &hDevice ) ) {
		return false;
	}
	if ( eClass == SIR_INPUT_KEYBOARD ) {
		const RAWKEYBOARD &key = input.data.keyboard;
		if ( key.MakeCode == 0 ) {
			return true;
		}
		const uint16_t nScan =
		    ( uint16_t )( key.MakeCode | ( ( key.Flags & RI_KEY_E0 ) ? 0x100 : 0 ) | ( ( key.Flags & RI_KEY_E1 ) ? 0x200 : 0 ) );
		const sirInputControlChange_t change = { SIR_INPUT_BUTTON, nScan, ( key.Flags & RI_KEY_BREAK ) ? 0 : 1 };
		return service.emitControls( id(), hDevice, &change, 1 );
	}
	const RAWMOUSE &mouse = input.data.mouse;
	sirInputControlChange_t changes[10] = {};
	uint32_t nCount = 0;
	const uint16_t nDown[5] = { RI_MOUSE_LEFT_BUTTON_DOWN, RI_MOUSE_RIGHT_BUTTON_DOWN, RI_MOUSE_MIDDLE_BUTTON_DOWN, RI_MOUSE_BUTTON_4_DOWN,
	                            RI_MOUSE_BUTTON_5_DOWN };
	const uint16_t nUp[5] = { RI_MOUSE_LEFT_BUTTON_UP, RI_MOUSE_RIGHT_BUTTON_UP, RI_MOUSE_MIDDLE_BUTTON_UP, RI_MOUSE_BUTTON_4_UP,
	                          RI_MOUSE_BUTTON_5_UP };
	for ( uint16_t i = 0; i < 5; ++i ) {
		if ( ( mouse.usButtonFlags & nDown[i] ) != 0 ) {
			changes[nCount++] = { SIR_INPUT_BUTTON, i, 1 };
		}
		if ( ( mouse.usButtonFlags & nUp[i] ) != 0 ) {
			changes[nCount++] = { SIR_INPUT_BUTTON, i, 0 };
		}
	}
	if ( nCount && !service.emitControls( id(), hDevice, changes, nCount ) ) {
		return false;
	}
	if ( ( mouse.usFlags & MOUSE_MOVE_ABSOLUTE ) == 0 && ( mouse.lLastX || mouse.lLastY ) &&
	     !service.emitPointer( id(), hDevice, SIR_INPUT_MOTION, 0, mouse.lLastX, mouse.lLastY ) ) {
		return false;
	}
	if ( ( mouse.usButtonFlags & RI_MOUSE_WHEEL ) &&
	     !service.emitPointer( id(), hDevice, SIR_INPUT_WHEEL, 0, ( SHORT )mouse.usButtonData, 0 ) ) {
		return false;
	}
	if ( ( mouse.usButtonFlags & RI_MOUSE_HWHEEL ) &&
	     !service.emitPointer( id(), hDevice, SIR_INPUT_WHEEL, 1, ( SHORT )mouse.usButtonData, 0 ) ) {
		return false;
	}
	return true;
}

bool SIRWindowsRawBackend::processMessage( SIRInputService &service, uint32_t nMessage, uintptr_t nWParam, intptr_t nLParam ) noexcept
{
	if ( m_pWindow == nullptr ) {
		return false;
	}
	if ( nMessage == WM_INPUT ) {
		return processRaw( service, ( void * )nLParam );
	}
	if ( nMessage == WM_INPUT_DEVICE_CHANGE && nWParam == GIDC_REMOVAL ) {
		sirInputDeviceId_t h = {};
		const uint64_t nNative = ( uint64_t )nLParam;
		bool bRemoved = false;
		if ( service.findDevice( id(), nNative, &h ) ) {
			bRemoved = service.disconnect( id(), h );
		}
		return bRemoved;
	}
	return false;
}

void SIRWindowsRawBackend::stop( SIRInputService &service ) noexcept
{
	if ( m_pWindow == nullptr ) {
		return;
	}
	RAWINPUTDEVICE devices[2] = { { HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_KEYBOARD, RIDEV_REMOVE, nullptr },
	                              { HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_MOUSE, RIDEV_REMOVE, nullptr } };
	RegisterRawInputDevices( devices, 2, sizeof( devices[0] ) );
	m_pWindow = nullptr;
	( void )service;
}
