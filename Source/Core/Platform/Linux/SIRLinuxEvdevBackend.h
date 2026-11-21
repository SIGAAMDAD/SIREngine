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

#ifndef __SIRENGINE_SIRLINUXEVDEVBACKEND_H__
#define __SIRENGINE_SIRLINUXEVDEVBACKEND_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Input/SIRInputBackend.h>
#include <Core/Input/SIRInputService.h>
#include <Core/Input/SIRInputControllerProfiles.h>

constexpr uint32_t SIR_INPUT_EVDEV_CAPACITY = 16;
constexpr uint32_t SIR_INPUT_EVDEV_REPORT_CAPACITY = 96;

typedef struct sirInputEvdevAxis_s
{
	int32_t nMinimum;
	int32_t nMaximum;
	uint16_t nCode;
	bool bTrigger;
	bool bValid;
	bool bInvert;
} sirInputEvdevAxis_t;

typedef struct sirInputEvdevSlot_s
{
	sirInputDeviceId_t id;
	int nFd;
	int nRumbleEffect;
	uint32_t nPending;
	uint64_t nNative;
	bool bClockMonotonic;
	bool bCanRumble;
	bool bHatDPad;
	int8_t nHatX;
	int8_t nHatY;
	bool bDesynchronized;
	bool bOpen;
	bool bHasProfile;
	char szPath[512];
	sirInputControllerProfile_t profile;
	sirInputControlChange_t pending[SIR_INPUT_EVDEV_REPORT_CAPACITY];
	sirInputEvdevAxis_t axes[SIR_INPUT_AXIS_COUNT];
} sirInputEvdevSlot_t;

// Linux gamepads only. Wayland/X11 own keyboard and mouse focus separately.
class SIRLinuxEvdevBackend final : public SIRInputBackend
{
public:
	SIRLinuxEvdevBackend( void ) noexcept;
	sirInputBackendId_t id( void ) const noexcept override;
	void sample( SIRInputService& service ) noexcept override;
	void resynchronize( SIRInputService& service ) noexcept override;
	bool vibrate( sirInputDeviceId_t id, uint16_t nLeft, uint16_t nRight ) noexcept override;
	uint32_t hapticCapabilities( sirInputDeviceId_t hDevice ) const noexcept override;
	void stop( SIRInputService& service ) noexcept override;
	bool discover( SIRInputService& service, const char *pszDir = "/dev/input" ) noexcept;
	bool addProfile( const sirInputControllerProfile_t& profile ) noexcept;
	bool importCookedProfiles( const uint8_t *pBytes, uint32_t nBytes ) noexcept;
	bool loadCookedProfilesFile( const char *pszPath ) noexcept;
	static uint64_t physicalKey( const char *pszPhys, uint16_t nVendor, uint16_t nProduct ) noexcept;
	static int16_t mapButton( uint16_t nNative ) noexcept;
	static int16_t mapAxis( uint16_t nNative ) noexcept;
	static int32_t normalize( int32_t nValue, int32_t nMinimum, int32_t nMaximum, bool bTrigger ) noexcept;

private:
	friend class SIRInputEvdevBatchTestPeer;
	bool openDevice( SIRInputService& service, const char *pszPath ) noexcept;
	bool requery( SIRInputService& service, sirInputEvdevSlot_t& slot, bool bReset ) noexcept;
	void closeDevice( SIRInputService& service, sirInputEvdevSlot_t& slot ) noexcept;
	void pollHotplug( SIRInputService& service ) noexcept;
	bool queueChange( sirInputEvdevSlot_t& slot, sirInputSignal_t eSignal, uint16_t nCode, int32_t nValue ) noexcept;
	static int16_t resolveButton( const sirInputEvdevSlot_t& slot, uint16_t nCode ) noexcept;
	sirInputEvdevSlot_t m_Slots[SIR_INPUT_EVDEV_CAPACITY];
	SIRInputControllerProfiles m_Profiles;
	bool m_bProfilesFrozen;
	char m_szDirectory[512];
	int m_nWatchFd;
	int m_nWatch;
};

#endif // __SIRENGINE_SIRLINUXEVDEVBACKEND_H__
