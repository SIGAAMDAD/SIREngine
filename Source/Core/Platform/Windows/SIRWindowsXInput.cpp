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

#include <Core/Platform/Windows/SIRWindowsXInput.h>
#include <Core/Platform/SIRPlatformTime.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <Xinput.h>

typedef struct sirXInputButtonMap_s
{
	WORD nXInput;
	sirInputGamepadButton_t eButton;
} sirXInputButtonMap_t;

static constexpr sirXInputButtonMap_t s_GamepadButtonMap[] = { { XINPUT_GAMEPAD_A, SIR_GAMEPAD_BUTTON_SOUTH },
                                                               { XINPUT_GAMEPAD_B, SIR_GAMEPAD_BUTTON_EAST },
                                                               { XINPUT_GAMEPAD_X, SIR_GAMEPAD_BUTTON_WEST },
                                                               { XINPUT_GAMEPAD_Y, SIR_GAMEPAD_BUTTON_NORTH },
                                                               { XINPUT_GAMEPAD_LEFT_SHOULDER, SIR_GAMEPAD_BUTTON_LEFT_SHOULDER },
                                                               { XINPUT_GAMEPAD_RIGHT_SHOULDER, SIR_GAMEPAD_BUTTON_RIGHT_SHOULDER },
                                                               { XINPUT_GAMEPAD_BACK, SIR_GAMEPAD_BUTTON_BACK },
                                                               { XINPUT_GAMEPAD_START, SIR_GAMEPAD_BUTTON_START },
                                                               { XINPUT_GAMEPAD_LEFT_THUMB, SIR_GAMEPAD_BUTTON_LEFT_THUMB },
                                                               { XINPUT_GAMEPAD_RIGHT_THUMB, SIR_GAMEPAD_BUTTON_RIGHT_THUMB },
                                                               { XINPUT_GAMEPAD_DPAD_UP, SIR_GAMEPAD_BUTTON_DPAD_UP },
                                                               { XINPUT_GAMEPAD_DPAD_DOWN, SIR_GAMEPAD_BUTTON_DPAD_DOWN },
                                                               { XINPUT_GAMEPAD_DPAD_LEFT, SIR_GAMEPAD_BUTTON_DPAD_LEFT },
                                                               { XINPUT_GAMEPAD_DPAD_RIGHT, SIR_GAMEPAD_BUTTON_DPAD_RIGHT } };

// XInput is dynamically loaded so SIRCore does not link a legacy import lib.
// The first compatible runtime is preferred, without downloading any SDK.
SIRWindowsXInput::SIRWindowsXInput( SIRInputSystem &input ) noexcept
    : m_Input( input ), m_pLibrary( nullptr ), m_pGetState( nullptr ), m_pSetState( nullptr ), m_Devices{}
{
}

SIRWindowsXInput::~SIRWindowsXInput()
{
	shutdown();
}

bool SIRWindowsXInput::initialize( void ) noexcept
{
	if ( m_pLibrary != nullptr ) {
		return false;
	}
	const wchar_t *szLibraries[] = { L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll" };
	for ( uint32_t iLibrary = 0; iLibrary < 3; ++iLibrary ) {
		HMODULE hModule = LoadLibraryExW( szLibraries[iLibrary], nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32 );
		if ( hModule == nullptr ) {
			continue;
		}
		FARPROC pGet = GetProcAddress( hModule, "XInputGetState" );
		FARPROC pSet = GetProcAddress( hModule, "XInputSetState" );
		if ( pGet != nullptr && pSet != nullptr ) {
			m_pLibrary = hModule;
			m_pGetState = ( void * )pGet;
			m_pSetState = ( void * )pSet;
			return true;
		}
		FreeLibrary( hModule );
	}
	return false;
}

uint32_t SIRWindowsXInput::poll( void ) noexcept
{
	if ( m_pLibrary == nullptr ) {
		return 0;
	}
	typedef DWORD( WINAPI * getState_t )( DWORD, XINPUT_STATE * );
	const getState_t pGet = ( getState_t )m_pGetState;
	uint32_t nProcessed = 0;
	for ( DWORD iUser = 0; iUser < 4; ++iUser ) {
		XINPUT_STATE state = {};
		const DWORD nResult = pGet( iUser, &state );
		sirXInputSlot_t &slot = m_Devices[iUser];
		if ( slot.bActive && !m_Input.isDeviceActive( slot.hDevice ) ) {
			slot.bActive = false;
		}
		if ( nResult == ERROR_DEVICE_NOT_CONNECTED ) {
			if ( slot.bActive ) {
				m_Input.removeDevice( slot.hDevice );
				slot.bActive = false;
			}
			continue;
		}
		if ( nResult != ERROR_SUCCESS ) {
			continue;
		}
		if ( !slot.bActive ) {
			const uint64_t nNativeId = UINT64_C( 0x58494e5000000000 ) | ( uint64_t )iUser;
			if ( !m_Input.addDevice( SIR_INPUT_DEVICE_GAMEPAD, nNativeId, &slot.hDevice ) ) {
				continue;
			}
			slot.bActive = true;
			slot.nPacket = state.dwPacketNumber - 1;
		}
		const WORD nButtons = state.Gamepad.wButtons;
		sirInputGamepadSample_t sample = {};
		for ( uint32_t iButton = 0; iButton < sizeof( s_GamepadButtonMap ) / sizeof( s_GamepadButtonMap[0] ); ++iButton ) {
			if ( ( nButtons & s_GamepadButtonMap[iButton].nXInput ) != 0 ) {
				sample.nButtons |= UINT32_C( 1 ) << s_GamepadButtonMap[iButton].eButton;
			}
		}
		sample.nAxes[SIR_GAMEPAD_AXIS_LEFT_X] = state.Gamepad.sThumbLX == -32768 ? -32767 : state.Gamepad.sThumbLX;
		sample.nAxes[SIR_GAMEPAD_AXIS_LEFT_Y] = state.Gamepad.sThumbLY == -32768 ? -32767 : state.Gamepad.sThumbLY;
		sample.nAxes[SIR_GAMEPAD_AXIS_RIGHT_X] = state.Gamepad.sThumbRX == -32768 ? -32767 : state.Gamepad.sThumbRX;
		sample.nAxes[SIR_GAMEPAD_AXIS_RIGHT_Y] = state.Gamepad.sThumbRY == -32768 ? -32767 : state.Gamepad.sThumbRY;
		sample.nAxes[SIR_GAMEPAD_AXIS_LEFT_TRIGGER] = ( uint32_t )state.Gamepad.bLeftTrigger * 32767 / 255;
		sample.nAxes[SIR_GAMEPAD_AXIS_RIGHT_TRIGGER] = ( uint32_t )state.Gamepad.bRightTrigger * 32767 / 255;
		if ( !m_Input.submitGamepadSample( slot.hDevice, sample, SIRPlatformTime::getMonotonicNanoseconds() ) ) {
			// Retry after the input owner has recovered the bounded queue.
			continue;
		}
		if ( slot.nPacket != state.dwPacketNumber ) {
			++nProcessed;
		}
		slot.nPacket = state.dwPacketNumber;
	}
	return nProcessed;
}

bool SIRWindowsXInput::setVibration( sirInputDeviceHandle_t hDevice, uint16_t nLeftMotor, uint16_t nRightMotor ) noexcept
{
	if ( m_pLibrary == nullptr ) {
		return false;
	}
	typedef DWORD( WINAPI * setState_t )( DWORD, XINPUT_VIBRATION * );
	const setState_t pSet = ( setState_t )m_pSetState;
	for ( DWORD iUser = 0; iUser < 4; ++iUser ) {
		const sirXInputSlot_t &slot = m_Devices[iUser];
		if ( slot.bActive && slot.hDevice.nSlot == hDevice.nSlot && slot.hDevice.nGeneration == hDevice.nGeneration ) {
			XINPUT_VIBRATION effect = { nLeftMotor, nRightMotor };
			return pSet( iUser, &effect ) == ERROR_SUCCESS;
		}
	}
	return false;
}

void SIRWindowsXInput::shutdown( void ) noexcept
{
	if ( m_pLibrary == nullptr ) {
		return;
	}
	typedef DWORD( WINAPI * setState_t )( DWORD, XINPUT_VIBRATION * );
	const setState_t pSet = ( setState_t )m_pSetState;
	for ( DWORD iUser = 0; iUser < 4; ++iUser ) {
		sirXInputSlot_t &slot = m_Devices[iUser];
		if ( slot.bActive ) {
			XINPUT_VIBRATION zero = {};
			pSet( iUser, &zero );
			m_Input.removeDevice( slot.hDevice );
			slot.bActive = false;
		}
	}
	FreeLibrary( ( HMODULE )m_pLibrary );
	m_pLibrary = nullptr;
	m_pGetState = m_pSetState = nullptr;
}

bool SIRWindowsXInput::ownsDevice( sirInputDeviceHandle_t hDevice ) const noexcept
{
	for ( uint32_t i = 0; i < 4; ++i ) {
		const sirXInputSlot_t &slot = m_Devices[i];
		if ( slot.bActive && slot.hDevice.nSlot == hDevice.nSlot && slot.hDevice.nGeneration == hDevice.nGeneration ) {
			return true;
		}
	}
	return false;
}

bool SIRWindowsXInput::setVibration( sirInputDeviceHandle_t hDevice, uint16_t nStrong, uint16_t nWeak, uint16_t nDurationMs ) noexcept
{
	// XInput cannot guarantee finite effect expiration; require the caller to
	// issue an explicit zero-motor stop rather than inventing a timer here.
	return nDurationMs == 0 && setVibration( hDevice, nStrong, nWeak );
}
