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

#include <Core/Platform/Input/Windows/SIRWindowsRawDecode.h>
#include <Core/Platform/Input/SIRInputService.h>
#include <Core/Platform/SIRPlatformTime.h>

bool sirInputDecodeWindowsRawRecord( SIRInputService& service, const RAWINPUT& input ) noexcept
{
	if ( input.header.dwType != RIM_TYPEKEYBOARD && input.header.dwType != RIM_TYPEMOUSE ) {
		return true;
	}
	const uint64_t nReceivedNs = SIRPlatformTime::getMonotonicNanoseconds();
	const sirInputDeviceClass_t eClass = input.header.dwType == RIM_TYPEKEYBOARD ? SIR_INPUT_KEYBOARD : SIR_INPUT_POINTER;
	const uint64_t nNative = ( uint64_t )( uintptr_t )input.header.hDevice;
	if ( nNative == 0 ) {
		return false;
	}
	sirInputDeviceId_t hDevice = {};
	if ( !service.findDevice( SIR_INPUT_WIN_RAW, nNative, &hDevice ) &&
	     !service.connect( { 0, nNative, SIR_INPUT_WIN_RAW, eClass }, &hDevice ) ) {
		return false;
	}
	if ( eClass == SIR_INPUT_KEYBOARD ) {
		const RAWKEYBOARD& key = input.data.keyboard;
		if ( key.MakeCode == 0 ) {
			return true;
		}
		const uint16_t nScan =
		    ( uint16_t )( key.MakeCode | ( ( key.Flags & RI_KEY_E0 ) ? 0x100 : 0 ) | ( ( key.Flags & RI_KEY_E1 ) ? 0x200 : 0 ) );
		const sirInputControlChange_t change = { SIR_INPUT_BUTTON, nScan, ( key.Flags & RI_KEY_BREAK ) ? 0 : 1 };
		return service.emitControls( SIR_INPUT_WIN_RAW, hDevice, &change, 1, nReceivedNs );
	}
	const RAWMOUSE& mouse = input.data.mouse;
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
	if ( nCount && !service.emitControls( SIR_INPUT_WIN_RAW, hDevice, changes, nCount, nReceivedNs ) ) {
		return false;
	}
	if ( ( mouse.usFlags & MOUSE_MOVE_ABSOLUTE ) == 0 && ( mouse.lLastX || mouse.lLastY ) &&
	     !service.emitPointer( SIR_INPUT_WIN_RAW, hDevice, SIR_INPUT_MOTION, 0, mouse.lLastX, mouse.lLastY, nReceivedNs ) ) {
		return false;
	}
	if ( ( mouse.usButtonFlags & RI_MOUSE_WHEEL ) &&
	     !service.emitPointer( SIR_INPUT_WIN_RAW, hDevice, SIR_INPUT_WHEEL, 0, ( SHORT )mouse.usButtonData, 0, nReceivedNs ) ) {
		return false;
	}
	if ( ( mouse.usButtonFlags & RI_MOUSE_HWHEEL ) &&
	     !service.emitPointer( SIR_INPUT_WIN_RAW, hDevice, SIR_INPUT_WHEEL, 1, ( SHORT )mouse.usButtonData, 0, nReceivedNs ) ) {
		return false;
	}
	return true;
}

// Validate every variable-length native record before following its aligned stride.
// The kernel owns the buffer; the input service owns event/order publication.
bool sirInputDecodeWindowsRawBatch( SIRInputService& service, const RAWINPUT *pRecords, uint32_t nCount, uint32_t nCapacityBytes ) noexcept
{
	if ( pRecords == nullptr || nCount == 0 || nCapacityBytes < sizeof( RAWINPUTHEADER ) ) {
		return false;
	}

	const uint8_t *pCursor = ( const uint8_t * )pRecords;
	uint32_t nRemaining = nCapacityBytes;
	for ( uint32_t iRecord = 0; iRecord < nCount; ++iRecord ) {
		if ( nRemaining < sizeof( RAWINPUTHEADER ) ) {
			return false;
		}

		const RAWINPUT *pCurrent = ( const RAWINPUT * )pCursor;
		const uint32_t nPayloadBytes = pCurrent->header.dwType == RIM_TYPEKEYBOARD ? sizeof( RAWKEYBOARD )
		                               : pCurrent->header.dwType == RIM_TYPEMOUSE  ? sizeof( RAWMOUSE )
		                                                                           : sizeof( RAWHID );
		const uint32_t nRequiredBytes = ( uint32_t )offsetof( RAWINPUT, data ) + nPayloadBytes;
		if ( pCurrent->header.dwSize < nRequiredBytes || pCurrent->header.dwSize > nRemaining ) {
			return false;
		}

		if ( iRecord + 1 < nCount ) {
			constexpr uint32_t ALIGNMENT = sizeof( void * );
			const uint32_t nStride = ( pCurrent->header.dwSize + ALIGNMENT - 1 ) & ~( ALIGNMENT - 1 );
			if ( nStride > nRemaining ) {
				return false;
			}

			pCursor += nStride;
			nRemaining -= nStride;
		}
	}

	// Publish only after every buffer boundary has been validated.
	pCursor = ( const uint8_t * )pRecords;
	for ( uint32_t iRecord = 0; iRecord < nCount; ++iRecord ) {
		const RAWINPUT *pCurrent = ( const RAWINPUT * )pCursor;
		if ( !sirInputDecodeWindowsRawRecord( service, *pCurrent ) ) {
			return false;
		}
		if ( iRecord + 1 < nCount ) {
			constexpr uint32_t ALIGNMENT = sizeof( void * );
			const uint32_t nStride = ( pCurrent->header.dwSize + ALIGNMENT - 1 ) & ~( ALIGNMENT - 1 );
			pCursor += nStride;
		}
	}
	return true;
}
