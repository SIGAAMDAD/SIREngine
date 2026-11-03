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

#ifndef __SIRENGINE_SIRINPUTCOORDINATOR_H__
#define __SIRENGINE_SIRINPUTCOORDINATOR_H__

#include <SIRPCH.hpp>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#include <Core/Platform/SIRInputBackend.h>

constexpr uint32_t SIR_INPUT_MAX_BACKENDS = 8;
constexpr uint32_t SIR_INPUT_MAX_USERS = 8;
constexpr uint32_t SIR_INPUT_UNPAIRED_USER = UINT32_MAX;

typedef enum sirInputCoordinatorResult_e : uint8_t
{
	SIR_INPUT_COORD_OK = 0,
	SIR_INPUT_COORD_INVALID_TICK = 1,
	SIR_INPUT_COORD_NOT_COLLECTING = 2,
	SIR_INPUT_COORD_QUEUE_OVERFLOW = 3,
	SIR_INPUT_COORD_UNOWNED_DEVICE = 4,
	SIR_INPUT_COORD_DUPLICATE_OWNER = 5,
	SIR_INPUT_COORD_DUPLICATE_PHYSICAL_DEVICE = 6
} sirInputCoordinatorResult_t;

typedef struct sirInputDeviceRoute_s
{
	sirInputDeviceHandle_t hDevice;
	uint64_t nPhysicalKey;
	uint32_t nUserId;
	sirInputBackendKind_t eBackend;
	bool bActive;
} sirInputDeviceRoute_t;

// UE-style backend/device ownership above the Source-style polled input state.
// No native window/HID handles, allocator or dynamic collections cross this API.
// All calls run on the input owner thread. Backends are borrowed and must outlive
// the coordinator's explicit shutdown() call; registrations cannot mutate mid-tick.
class SIRInputCoordinator
{
public:
	explicit SIRInputCoordinator( SIRInputSystem &input ) noexcept;
	SIRInputCoordinator( const SIRInputCoordinator & ) = delete;
	SIRInputCoordinator &operator=( const SIRInputCoordinator & ) = delete;

	bool attachBackend( SIRInputBackend &backend ) noexcept;
	bool beginTick( uint64_t nTick ) noexcept;
	bool publishTick( void ) noexcept;
	bool getDeviceRoute( sirInputDeviceHandle_t hDevice, sirInputDeviceRoute_t *pRoute ) const noexcept;
	bool assignUser( sirInputDeviceHandle_t hDevice, uint32_t nUserId ) noexcept;
	bool setVibration( sirInputDeviceHandle_t hDevice, uint16_t nStrong, uint16_t nWeak, uint16_t nDurationMs ) noexcept;
	sirInputCoordinatorResult_t getLastResult( void ) const noexcept;
	uint32_t getBackendCount( void ) const noexcept;
	void shutdown( void ) noexcept;

private:
	bool reconcileDeviceOwners( void ) noexcept;
	static bool isDesktopBackend( sirInputBackendKind_t eKind ) noexcept;
	SIRInputBackend *findBackend( sirInputBackendKind_t eKind ) const noexcept;

	SIRInputSystem &m_Input;
	SIRInputBackend *m_pBackends[SIR_INPUT_MAX_BACKENDS];
	sirInputDeviceRoute_t m_Routes[SIR_INPUT_MAX_DEVICES];
	uint32_t m_nBackendCount;
	uint64_t m_nPendingTick;
	sirInputCoordinatorResult_t m_eLastResult;
	bool m_bCollecting;
	bool m_bStopped;
};

#endif // __SIRENGINE_SIRINPUTCOORDINATOR_H__
