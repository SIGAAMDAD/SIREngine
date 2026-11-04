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
#include <hidusage.h>
#include <Core/Platform/Input/Windows/SIRWindowsRawDecode.h>

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
	     nBytes > sizeof( RAWINPUT ) || nBytes < sizeof( RAWINPUTHEADER ) ) {
		return false;
	}

	RAWINPUT input = {};
	const UINT nRead = GetRawInputData( ( HRAWINPUT )pHRawInput, RID_INPUT, &input, &nBytes, sizeof( RAWINPUTHEADER ) );
	if ( nRead == ( UINT )-1 || nRead != nBytes ) {
		return false;
	}
	return sirInputDecodeWindowsRawRecord( service, input );
}

// WM_INPUT's lParam is no longer in the raw queue once dispatched. Process it
// individually, then drain later queued WM_INPUT reports in bounded batches.
bool SIRWindowsRawBackend::drainBuffered( SIRInputService &service ) noexcept
{
	if ( ( HIWORD( GetQueueStatus( QS_RAWINPUT ) ) & QS_RAWINPUT ) == 0 ) {
		return true;
	}

	constexpr uint32_t BUFFER_BYTES = 16 * 1024;
	constexpr uint32_t MAX_BATCHES = 8;
	alignas( 8 ) uint8_t szBuffer[BUFFER_BYTES] = {};

	for ( uint32_t iBatch = 0; iBatch < MAX_BATCHES; ++iBatch ) {
		UINT nCapacityBytes = BUFFER_BYTES;
		const UINT nCount = GetRawInputBuffer( ( RAWINPUT * )szBuffer, &nCapacityBytes, sizeof( RAWINPUTHEADER ) );
		if ( nCount == 0 ) {
			return true;
		}
		if ( nCount == ( UINT )-1 ) {
			// An oversized device report stays queued for the ordinary WM_INPUT
			// dispatch path; do not allocate on the input hot path.
			return GetLastError() == ERROR_INSUFFICIENT_BUFFER;
		}
		if ( !sirInputDecodeWindowsRawBatch( service, ( RAWINPUT * )szBuffer, nCount, BUFFER_BYTES ) ) {
			return false;
		}
	}
	return true;
}

bool SIRWindowsRawBackend::processMessage( SIRInputService &service, uint32_t nMessage, uintptr_t nWParam, intptr_t nLParam ) noexcept
{
	if ( m_pWindow == nullptr ) {
		return false;
	}
	if ( nMessage == WM_INPUT ) {
		const bool bCurrent = processRaw( service, ( void * )nLParam );
		const bool bBuffered = drainBuffered( service );
		if ( !bCurrent || !bBuffered ) {
			// Recover held input when a capacity failure invalidates a batch.
			( void )service.recover();
			return false;
		}
		return true;
	}
	if ( nMessage == WM_INPUT_DEVICE_CHANGE && nWParam == GIDC_ARRIVAL ) {
		RID_DEVICE_INFO info = {};
		info.cbSize = sizeof( info );
		UINT nSize = sizeof( info );
		const HANDLE hNative = ( HANDLE )nLParam;
		if ( GetRawInputDeviceInfoW( hNative, RIDI_DEVICEINFO, &info, &nSize ) == ( UINT )-1 ) {
			return false;
		}
		const sirInputDeviceClass_t eClass = info.dwType == RIM_TYPEKEYBOARD ? SIR_INPUT_KEYBOARD : SIR_INPUT_POINTER;
		if ( info.dwType != RIM_TYPEKEYBOARD && info.dwType != RIM_TYPEMOUSE ) {
			return false;
		}
		sirInputDeviceId_t hDevice = {};
		const uint64_t nNative = ( uint64_t )( uintptr_t )hNative;
		return service.findDevice( id(), nNative, &hDevice ) || service.connect( { 0, nNative, id(), eClass }, &hDevice );
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
