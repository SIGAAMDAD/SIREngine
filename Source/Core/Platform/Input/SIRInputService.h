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

#ifndef __SIRENGINE_SIRINPUTSERVICE_H__
#define __SIRENGINE_SIRINPUTSERVICE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/Input/SIRInputActions.h>
#include <Core/Platform/Input/SIRInputBackend.h>
#include <Core/Platform/Input/SIRInputDeviceRegistry.h>
#include <Core/Platform/Input/SIRInputFrameState.h>

// Single input owner: UE's device/backend ownership and Source's tick snapshot.
// No native window/input handles or third-party library types in this contract.
class SIRInputService
{
public:
	SIRInputService( void ) noexcept;
	SIRInputService( const SIRInputService & ) = delete;
	SIRInputService &operator=( const SIRInputService & ) = delete;

	bool attach( SIRInputBackend &backend ) noexcept;
	bool begin( uint64_t nTick ) noexcept;
	void sample( void ) noexcept;
	bool publish( void ) noexcept;
	void setTimingMetricsEnabled( bool bEnabled ) noexcept;
	bool recover( void ) noexcept;
	const sirInputFrame_t *frame( void ) const noexcept;
	SIRInputActions &actions( void ) noexcept;
	const SIRInputActions &actions( void ) const noexcept;
	bool connect( const sirInputHardwareKey_t &key, sirInputDeviceId_t *pOut ) noexcept;
	bool disconnect( sirInputBackendId_t eSource, sirInputDeviceId_t hDevice ) noexcept;
	bool pairUser( sirInputDeviceId_t hDevice, uint32_t nUser ) noexcept;
	bool describe( sirInputDeviceId_t hDevice, sirInputDeviceDescriptor_t *pOut ) const noexcept;
	bool findDevice( sirInputBackendId_t eSource, uint64_t nNative, sirInputDeviceId_t *pOut ) const noexcept;
	bool emitControls( sirInputBackendId_t eSource, sirInputDeviceId_t hDevice, const sirInputControlChange_t *pChanges, uint32_t nCount,
	                   uint64_t nTime = 0, sirInputClockSource_t eClock = SIR_INPUT_CLOCK_ENGINE_MONOTONIC ) noexcept;
	bool emitPointer( sirInputBackendId_t eSource, sirInputDeviceId_t hDevice, sirInputSignal_t eSignal, uint16_t nControl, int32_t nX,
	                  int32_t nY, uint64_t nTime = 0, sirInputClockSource_t eClock = SIR_INPUT_CLOCK_ENGINE_MONOTONIC ) noexcept;
	bool loseState( sirInputBackendId_t eSource, sirInputDeviceId_t hDevice ) noexcept;
	bool loseBackendState( sirInputBackendId_t eSource ) noexcept;
	bool vibrate( sirInputDeviceId_t hDevice, uint16_t nLeft, uint16_t nRight ) noexcept;
	uint32_t activeDeviceCount( void ) const noexcept;
	void shutdown( void ) noexcept;

private:
	bool verifyOwner( sirInputBackendId_t eSource, sirInputDeviceId_t id, sirInputDeviceDescriptor_t *pOut ) const noexcept;
	bool hasBackend( sirInputBackendId_t eSource ) const noexcept;
	sirInputTimestamp_t timestamp( uint64_t nNative, sirInputClockSource_t eClock ) const noexcept;
	SIRInputActions m_Actions;
	SIRInputDeviceRegistry m_Registry;
	SIRInputFrameState m_State;
	SIRInputBackend *m_pBackends[SIR_INPUT_BACKEND_CAPACITY];
	uint32_t m_nBackendCount;
	bool m_bStopping;
};

#endif // __SIRENGINE_SIRINPUTSERVICE_H__
