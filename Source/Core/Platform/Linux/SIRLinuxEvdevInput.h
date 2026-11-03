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

#ifndef __SIRENGINE_SIRLINUXEVDEVINPUT_H__
#define __SIRENGINE_SIRLINUXEVDEVINPUT_H__

#include <SIRPCH.hpp>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#include <Core/Platform/SIRInputBackend.h>
#include <Core/Platform/SIRInputControllerProfiles.h>

typedef struct sirEvdevAxisCalibration_s
{
	int32_t nMin;
	int32_t nMax;
	uint16_t nNativeCode;
	bool bInvert;
	bool bValid;
} sirEvdevAxisCalibration_t;

// Oversized or truncated evdev reports trigger a full hardware resynchronization.
constexpr uint32_t SIR_EVDEV_MAX_FRAME_EVENTS = 64;

typedef struct sirEvdevSlot_s
{
	sirInputDeviceHandle_t hDevice;
	int nFd;
	uint64_t nDeviceId;
	const sirInputControllerProfile_t *pProfile;
	bool bDesynchronized;
	bool bHatDPad;
	bool bCanRumble;
	uint32_t nFrameCount;
	sirInputGamepadFrameEvent_t frame[SIR_EVDEV_MAX_FRAME_EVENTS];
	int nEffectId;
	char szPath[512];
	sirEvdevAxisCalibration_t axes[SIR_GAMEPAD_AXIS_COUNT];
} sirEvdevSlot_t;

// Nonblocking evdev controller backend. The caller owns discovery timing and
// calls pump() from the SIRInputSystem owner thread.
class SIRLinuxEvdevInput : public SIRInputBackend
{
public:
	bool ownsDevice( sirInputDeviceHandle_t hDevice ) const noexcept override;
	sirInputBackendKind_t getBackendKind( void ) const noexcept override
	{
		return SIR_INPUT_BACKEND_LINUX_EVDEV;
	}
	explicit SIRLinuxEvdevInput( SIRInputSystem &input ) noexcept;
	SIRLinuxEvdevInput( const SIRLinuxEvdevInput & ) = delete;
	SIRLinuxEvdevInput &operator=( const SIRLinuxEvdevInput & ) = delete;
	~SIRLinuxEvdevInput();

	bool openDevice( const char *pszPath ) noexcept;
	bool registerProfile( const sirInputControllerProfile_t &profile ) noexcept;
	uint32_t discover( const char *pszDirectory = "/dev/input" ) noexcept;
	bool watchDirectory( const char *pszDirectory = "/dev/input" ) noexcept;
	bool isWatching( void ) const noexcept;
	uint32_t pump( void ) noexcept override;
	static int16_t mapButton( uint16_t nNativeCode ) noexcept;
	static int16_t mapAxis( uint16_t nNativeCode ) noexcept;
	static int32_t normalizeAxis( int32_t nValue, int32_t nMinimum, int32_t nMaximum, bool bTrigger ) noexcept;
	uint32_t getDeviceCount( void ) const noexcept;
	bool setVibration( sirInputDeviceHandle_t hDevice, uint16_t nStrong, uint16_t nWeak, uint16_t nDurationMs ) noexcept override;
	void shutdown( void ) noexcept override;

private:
	bool resynchronize( sirEvdevSlot_t &slot ) noexcept;
	int16_t mapDeviceButton( const sirEvdevSlot_t &slot, uint16_t nCode ) const noexcept;
	int16_t mapDeviceAxis( const sirEvdevSlot_t &slot, uint16_t nCode ) const noexcept;
	bool appendFrameEvent( sirEvdevSlot_t &slot, sirInputEventKind_t eKind, uint16_t nCode, int32_t nValue ) noexcept;
	bool stageHat( sirEvdevSlot_t &slot, uint16_t nCode, int32_t nValue ) noexcept;
	void closeSlot( sirEvdevSlot_t &slot ) noexcept;
	void pumpHotplug( void ) noexcept;

	SIRInputSystem &m_Input;
	SIRInputControllerProfiles m_Profiles;
	sirEvdevSlot_t m_Devices[SIR_INPUT_MAX_DEVICES];
	uint32_t m_nDeviceCount;
	int m_nNotifyFd;
	int m_nNotifyWatch;
	char m_szWatchDirectory[512];
};

#endif // __SIRENGINE_SIRLINUXEVDEVINPUT_H__
