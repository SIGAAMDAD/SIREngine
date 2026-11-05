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
#include <Core/Platform/Input/Windows/SIRWindowsRawBackend.h>
#include <Core/Platform/Input/SIRInputService.h>
#include <stdio.h>

static bool testKeyboardTransitions( void )
{
	SIRInputService service;
	SIRWindowsRawBackend backend;
	if ( !service.attach( backend ) || !service.begin( 10 ) ) {
		return false;
	}

	RAWINPUT packet = {};
	packet.header.dwType = RIM_TYPEKEYBOARD;
	packet.header.dwSize = sizeof( RAWINPUT );
	packet.header.hDevice = ( HANDLE )( uintptr_t )11;
	packet.data.keyboard.MakeCode = 30;
	packet.data.keyboard.Flags = RI_KEY_E0;
	if ( !sirInputDecodeWindowsRawRecord( service, packet ) ) {
		return false;
	}
	packet.data.keyboard.Flags = RI_KEY_E0 | RI_KEY_BREAK;
	if ( !sirInputDecodeWindowsRawRecord( service, packet ) || !service.publish() ) {
		return false;
	}

	const sirInputFrame_t *pFrame = service.frame();
	const uint32_t nScan = 30 | 0x100;
	const uint32_t nMask = 1U << ( nScan % 32 );
	const bool bValid = pFrame != nullptr && pFrame->nCount == 3 && pFrame->events[1].nControl == nScan && pFrame->events[1].nValue == 1 &&
	                    pFrame->events[2].nValue == 0 && pFrame->events[1].nReceivedNs != 0 &&
	                    pFrame->events[1].eClock == SIR_INPUT_CLOCK_ENGINE_MONOTONIC &&
	                    ( pFrame->state[0].nPressed[nScan / 32] & nMask ) != 0 && ( pFrame->state[0].nReleased[nScan / 32] & nMask ) != 0 &&
	                    ( pFrame->state[0].nHeld[nScan / 32] & nMask ) == 0;
	service.shutdown();
	return bValid;
}

static bool testHighRateMouseBatch( void )
{
	SIRInputService service;
	SIRWindowsRawBackend backend;
	if ( !service.attach( backend ) || !service.begin( 11 ) ) {
		return false;
	}

	RAWINPUT szRecords[128] = {};
	for ( uint32_t iRecord = 0; iRecord < 128; ++iRecord ) {
		RAWINPUT& packet = szRecords[iRecord];
		packet.header.dwType = RIM_TYPEMOUSE;
		packet.header.dwSize = sizeof( RAWINPUT );
		packet.header.hDevice = ( HANDLE )( uintptr_t )12;
		packet.data.mouse.lLastX = 1;
		packet.data.mouse.lLastY = -1;
	}
	szRecords[0].data.mouse.usButtonFlags = RI_MOUSE_LEFT_BUTTON_DOWN | RI_MOUSE_WHEEL;
	szRecords[0].data.mouse.usButtonData = 120;
	szRecords[127].data.mouse.usButtonFlags = RI_MOUSE_LEFT_BUTTON_UP | RI_MOUSE_HWHEEL;
	szRecords[127].data.mouse.usButtonData = ( USHORT )-120;

	if ( !sirInputDecodeWindowsRawBatch( service, szRecords, 128, sizeof( szRecords ) ) || !service.publish() ) {
		return false;
	}
	const sirInputFrame_t *pFrame = service.frame();
	const uint32_t nMask = 1;
	const bool bValid = pFrame != nullptr && pFrame->nCount == 133 && pFrame->state[0].nMotionX == 128 &&
	                    pFrame->state[0].nMotionY == -128 && ( pFrame->state[0].nPressed[0] & nMask ) != 0 &&
	                    ( pFrame->state[0].nReleased[0] & nMask ) != 0 && ( pFrame->state[0].nHeld[0] & nMask ) == 0 &&
	                    pFrame->events[1].nReceivedNs == pFrame->events[2].nReceivedNs &&
	                    pFrame->events[2].nReceivedNs == pFrame->events[3].nReceivedNs;
	service.shutdown();
	return bValid;
}

static bool testRejectMalformedBatchWithoutMutation( void )
{
	SIRInputService service;
	SIRWindowsRawBackend backend;
	if ( !service.attach( backend ) || !service.begin( 12 ) ) {
		return false;
	}
	RAWINPUT szRecords[2] = {};
	for ( uint32_t i = 0; i < 2; ++i ) {
		szRecords[i].header.dwType = RIM_TYPEMOUSE;
		szRecords[i].header.dwSize = sizeof( RAWINPUT );
		szRecords[i].header.hDevice = ( HANDLE )( uintptr_t )13;
		szRecords[i].data.mouse.lLastX = 2;
	}
	if ( sirInputDecodeWindowsRawBatch( service, szRecords, 2, sizeof( szRecords ) - 1 ) || service.activeDeviceCount() != 0 ) {
		return false;
	}
	szRecords[1].header.dwSize = 8;
	if ( sirInputDecodeWindowsRawBatch( service, szRecords, 2, sizeof( szRecords ) ) || service.activeDeviceCount() != 0 ) {
		return false;
	}
	if ( !service.publish() || service.frame()->nCount != 0 ) {
		return false;
	}
	service.shutdown();
	return true;
}

int main( void )
{
	if ( !testKeyboardTransitions() || !testHighRateMouseBatch() || !testRejectMalformedBatchWithoutMutation() ) {
		fputs( "SIRWindowsRawBatchTests FAILED\n", stderr );
		return 1;
	}
	puts( "SIRWindowsRawBatchTests PASS" );
	return 0;
}
