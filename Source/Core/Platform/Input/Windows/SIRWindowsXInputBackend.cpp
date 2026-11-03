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

#include <Core/Platform/Input/Windows/SIRWindowsXInputBackend.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <Xinput.h>

typedef struct sirXInputButtonMapping_s
{
	WORD nNative;
	uint16_t nCode;
} sirXInputButtonMapping_t;
static constexpr sirXInputButtonMapping_t s_ButtonMap[] = { { XINPUT_GAMEPAD_A, SIR_INPUT_PAD_SOUTH },
                                                            { XINPUT_GAMEPAD_B, SIR_INPUT_PAD_EAST },
                                                            { XINPUT_GAMEPAD_X, SIR_INPUT_PAD_WEST },
                                                            { XINPUT_GAMEPAD_Y, SIR_INPUT_PAD_NORTH },
                                                            { XINPUT_GAMEPAD_LEFT_SHOULDER, SIR_INPUT_PAD_LB },
                                                            { XINPUT_GAMEPAD_RIGHT_SHOULDER, SIR_INPUT_PAD_RB },
                                                            { XINPUT_GAMEPAD_BACK, SIR_INPUT_PAD_BACK },
                                                            { XINPUT_GAMEPAD_START, SIR_INPUT_PAD_START },
                                                            { XINPUT_GAMEPAD_LEFT_THUMB, SIR_INPUT_PAD_L3 },
                                                            { XINPUT_GAMEPAD_RIGHT_THUMB, SIR_INPUT_PAD_R3 },
                                                            { XINPUT_GAMEPAD_DPAD_UP, SIR_INPUT_PAD_DPAD_UP },
                                                            { XINPUT_GAMEPAD_DPAD_DOWN, SIR_INPUT_PAD_DPAD_DOWN },
                                                            { XINPUT_GAMEPAD_DPAD_LEFT, SIR_INPUT_PAD_DPAD_LEFT },
                                                            { XINPUT_GAMEPAD_DPAD_RIGHT, SIR_INPUT_PAD_DPAD_RIGHT } };

SIRWindowsXInputBackend::SIRWindowsXInputBackend( void ) noexcept
    : m_pLibrary( nullptr ), m_pGetState( nullptr ), m_pSetState( nullptr ), m_Slots{}
{
}

sirInputBackendId_t SIRWindowsXInputBackend::id( void ) const noexcept
{
	return SIR_INPUT_WIN_XINPUT;
}

bool SIRWindowsXInputBackend::initialize( void ) noexcept
{
	if ( m_pLibrary != nullptr ) {
		return false;
	}
	const wchar_t *szDlls[] = { L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll" };
	for ( uint32_t i = 0; i < 3; ++i ) {
		HMODULE hDLL = LoadLibraryExW( szDlls[i], nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32 );
		if ( hDLL == nullptr ) {
			continue;
		}
		FARPROC pGet = GetProcAddress( hDLL, "XInputGetState" );
		FARPROC pSet = GetProcAddress( hDLL, "XInputSetState" );
		if ( pGet != nullptr && pSet != nullptr ) {
			m_pLibrary = hDLL;
			m_pGetState = ( void * )pGet;
			m_pSetState = ( void * )pSet;
			return true;
		}
		FreeLibrary( hDLL );
	}
	return false;
}

void SIRWindowsXInputBackend::resynchronize( SIRInputService &service ) noexcept
{
	( void )service;
	for ( uint32_t i = 0; i < 4; ++i ) {
		m_Slots[i].bForceSample = true;
	}
}

void SIRWindowsXInputBackend::sample( SIRInputService &service ) noexcept
{
	if ( m_pLibrary == nullptr ) {
		return;
	}
	typedef DWORD( WINAPI * stateFn_t )( DWORD, XINPUT_STATE * );
	const stateFn_t pGet = ( stateFn_t )m_pGetState;
	for ( DWORD nUser = 0; nUser < 4; ++nUser ) {
		XINPUT_STATE state = {};
		const DWORD nStatus = pGet( nUser, &state );
		sirXInputNativeSlot_t &slot = m_Slots[nUser];
		if ( nStatus == ERROR_DEVICE_NOT_CONNECTED ) {
			if ( slot.bConnected ) {
				service.disconnect( id(), slot.id );
				slot.bConnected = false;
			}
			continue;
		}
		if ( nStatus != ERROR_SUCCESS ) {
			continue;
		}
		if ( !slot.bConnected ) {
			const uint64_t nNative = UINT64_C( 0x58494e0000000000 ) | ( uint64_t )( nUser + 1 );
			if ( !service.connect( { 0, nNative, id(), SIR_INPUT_CONTROLLER }, &slot.id ) ) {
				continue;
			}
			slot.bConnected = true;
			slot.nPacket = state.dwPacketNumber - 1;
		}
		if ( slot.nPacket == state.dwPacketNumber && !slot.bForceSample ) {
			continue;
		}
		sirInputControlChange_t changes[24] = {};
		uint32_t nCount = 0;
		for ( uint32_t i = 0; i < sizeof( s_ButtonMap ) / sizeof( s_ButtonMap[0] ); ++i ) {
			changes[nCount++] = { SIR_INPUT_BUTTON, s_ButtonMap[i].nCode,
			                      ( state.Gamepad.wButtons & s_ButtonMap[i].nNative ) != 0 ? 1 : 0 };
		}
		const SHORT nLX = state.Gamepad.sThumbLX, nLY = state.Gamepad.sThumbLY;
		const SHORT nRX = state.Gamepad.sThumbRX, nRY = state.Gamepad.sThumbRY;
		const int32_t axes[SIR_INPUT_AXIS_COUNT] = { nLX == -32768 ? -32767 : nLX,
		                                             nLY == -32768 ? -32767 : nLY,
		                                             nRX == -32768 ? -32767 : nRX,
		                                             nRY == -32768 ? -32767 : nRY,
		                                             ( int32_t )state.Gamepad.bLeftTrigger * 32767 / 255,
		                                             ( int32_t )state.Gamepad.bRightTrigger * 32767 / 255 };
		for ( uint16_t i = 0; i < SIR_INPUT_AXIS_COUNT; ++i ) {
			changes[nCount++] = { SIR_INPUT_AXIS, i, axes[i] };
		}
		if ( service.emitControls( id(), slot.id, changes, nCount ) ) {
			slot.nPacket = state.dwPacketNumber;
			slot.bForceSample = false;
		}
	}
}

bool SIRWindowsXInputBackend::vibrate( sirInputDeviceId_t id, uint16_t nLeft, uint16_t nRight ) noexcept
{
	if ( m_pSetState == nullptr ) {
		return false;
	}
	typedef DWORD( WINAPI * rumbleFn_t )( DWORD, XINPUT_VIBRATION * );
	for ( DWORD i = 0; i < 4; ++i ) {
		if ( m_Slots[i].bConnected && m_Slots[i].id.nSlot == id.nSlot && m_Slots[i].id.nGeneration == id.nGeneration ) {
			XINPUT_VIBRATION vibrations = { nLeft, nRight };
			return ( ( rumbleFn_t )m_pSetState )( i, &vibrations ) == ERROR_SUCCESS;
		}
	}
	return false;
}

void SIRWindowsXInputBackend::stop( SIRInputService &service ) noexcept
{
	for ( uint32_t i = 0; i < 4; ++i ) {
		if ( m_Slots[i].bConnected ) {
			vibrate( m_Slots[i].id, 0, 0 );
			service.disconnect( id(), m_Slots[i].id );
			m_Slots[i].bConnected = false;
		}
	}
	if ( m_pLibrary != nullptr ) {
		FreeLibrary( ( HMODULE )m_pLibrary );
	}
	m_pLibrary = m_pGetState = m_pSetState = nullptr;
}
