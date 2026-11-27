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

#ifndef __SIR_NATIVE_INPUT_EVDEV_GAMEPAD_DRIVER_H__
#define __SIR_NATIVE_INPUT_EVDEV_GAMEPAD_DRIVER_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Events/SIRSPSCEventRing.h>
#include <Core/Input/SIRInputControllerProfiles.h>
#include <Core/Platform/Shared/Window/SIREventTypes.h>

struct input_event;
struct input_id;

// Owned and pumped by the selected desktop input driver. The engine event
// queue and optional controller profiles must outlive this object.
class SIRNativeInputEvdevGamepadDriver final
{
public:
	explicit SIRNativeInputEvdevGamepadDriver(
		SIRSPSCEventRing<SIREventData>& eventQueue, const SIRInputControllerProfiles *pProfiles = nullptr ) noexcept;
	~SIRNativeInputEvdevGamepadDriver() noexcept;
	SIRNativeInputEvdevGamepadDriver( const SIRNativeInputEvdevGamepadDriver& ) = delete;
	SIRNativeInputEvdevGamepadDriver& operator=( const SIRNativeInputEvdevGamepadDriver& ) = delete;

	void pumpEvents( void ) noexcept;
	uint32_t droppedEventCount( void ) const noexcept;

private:
	static constexpr size_t BUTTON_COUNT = static_cast<size_t>( SIRInputControllerControl::SIR_INPUT_PAD_BUTTON_COUNT );
	static constexpr size_t AXIS_COUNT = static_cast<size_t>( SIRGamepadAxis::Count );
	static constexpr size_t REPORT_CAPACITY = 96;

	struct Axis
	{
		uint16_t nCode;
		int32_t nMinimum;
		int32_t nMaximum;
		bool bInvert;
	};

	struct Change
	{
		uint64_t nTime;
		int32_t nValue;
		uint16_t nCode;
		uint16_t nType;
	};

	struct Device
	{
		int nFd = -1;
		uint32_t nSlot{};
		uint32_t nEventId{};
		uint64_t nNativeId{};
		uint64_t nPhysicalId{};
		char szPath[288]{};
		uint16_t szKeys[BUTTON_COUNT]{};
		Axis szAxes[AXIS_COUNT]{};
		bool bHat[2]{};
		bool bMonotonic{};
		bool bDropped{};
		Change szPending[REPORT_CAPACITY]{};
		size_t nPendingCount{};
		bool bChanged{};
		uint32_t nButtons{};
		int16_t szValues[AXIS_COUNT]{};
	};

	void discover( void ) noexcept;
	void pollHotplug( void ) noexcept;
	void openDevice( const char *szPath ) noexcept;
	void configureDeviceControls( Device& device, const input_id& identity, bool bIdentified, const unsigned long *pKeyBits ) noexcept;
	void closeDevice( Device& device ) noexcept;
	bool snapshot( Device& device, bool bInitial ) noexcept;
	bool process( Device& device, const input_event& eventData ) noexcept;
	void applyChange( Device& device, const Change& change ) noexcept;
	void applyKey( Device& device, size_t iIndex, bool bDown, uint64_t nTimestamp ) noexcept;
	void applyAxis( Device& device, size_t iIndex, int32_t nValue, uint64_t nTimestamp ) noexcept;
	void applyHat( Device& device, size_t iIndex, int32_t nValue, uint64_t nTimestamp ) noexcept;
	void setButton( Device& device, SIRGamepadButton eButton, bool bDown, uint64_t nTimestamp ) noexcept;
	void setAxis( Device& device, size_t iIndex, int16_t nValue, uint64_t nTimestamp ) noexcept;
	void publishDevice( SIREventType eType, const Device& device, uint64_t nTimestamp ) noexcept;
	void publish( const SIREventData& eventData ) noexcept;

	SIRSPSCEventRing<SIREventData>& m_EventQueue;
	uint32_t m_nDroppedEvents = 0;
	Device m_Devices[SIR_INPUT_DEVICE_CAPACITY];
	uint32_t m_szDeviceGenerations[SIR_INPUT_DEVICE_CAPACITY] = {};
	const SIRInputControllerProfiles *m_pProfiles;
	int m_nWatchFd = -1;
	uint64_t m_nNextScan = 0;
	bool m_bPumping = false;
};

#endif // __SIR_NATIVE_INPUT_EVDEV_GAMEPAD_DRIVER_H__
