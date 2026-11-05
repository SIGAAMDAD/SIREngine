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

#ifndef __SIRENGINE_SIRWINDOWSGAMEINPUTBACKEND_H__
#define __SIRENGINE_SIRWINDOWSGAMEINPUTBACKEND_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/Input/SIRInputBackend.h>
#include <Core/Platform/Input/SIRInputService.h>
#include <Core/Platform/Input/Windows/SIRWindowsGameInputCodec.h>
#include <atomic>
#include <GameInput.h>

typedef struct sirGameInputNativeDevice_s
{
	void *pDevice;
	sirInputDeviceId_t id;
	sirGameInputDecodedGamepad_t decoded;
	uint16_t nMotorLeft;
	uint16_t nMotorRight;
	uint16_t nTriggerLeft;
	uint16_t nTriggerRight;
} sirGameInputNativeDevice_t;

// GameInput owns the input history and device endpoints. The callback only
// publishes retained COM device pointers; SIRInputService stays owner-thread.
class SIRWindowsGameInputBackend final : public SIRInputBackend
{
public:
	SIRWindowsGameInputBackend( void ) noexcept;
	SIRWindowsGameInputBackend( const SIRWindowsGameInputBackend& ) = delete;
	SIRWindowsGameInputBackend& operator=( const SIRWindowsGameInputBackend& ) = delete;

	sirInputBackendId_t id( void ) const noexcept override;
	bool initialize( void ) noexcept;
	void sample( SIRInputService& service ) noexcept override;
	void resynchronize( SIRInputService& service ) noexcept override;
	bool vibrate( sirInputDeviceId_t hDevice, uint16_t nLeft, uint16_t nRight ) noexcept override;
	uint32_t hapticCapabilities( sirInputDeviceId_t hDevice ) const noexcept override;
	bool vibrateTriggers( sirInputDeviceId_t hDevice, uint16_t nLeft, uint16_t nRight ) noexcept override;
	void stop( SIRInputService& service ) noexcept override;
	bool initialized( void ) const noexcept;

private:
	static constexpr uint32_t PENDING_CAPACITY = 64;
	static constexpr uint32_t HISTORY_BUDGET = 64;

	static void CALLBACK onDeviceChanged( GameInput::v1::GameInputCallbackToken nToken, void *pContext,
	                                      GameInput::v1::IGameInputDevice *pDevice, uint64_t nTimestamp,
	                                      GameInput::v1::GameInputDeviceStatus eCurrent,
	                                      GameInput::v1::GameInputDeviceStatus ePrevious ) noexcept;
	void enqueueDevice( void *pDevice ) noexcept;
	void consumeDeviceChanges( SIRInputService& service ) noexcept;
	int32_t findDevice( const void *pDevice ) const noexcept;
	bool addDevice( SIRInputService& service, void *pDevice ) noexcept;
	void dropDevice( SIRInputService& service, uint32_t iSlot ) noexcept;
	bool processReading( SIRInputService& service, void *pReading ) noexcept;
	void invalidateHistory( SIRInputService& service ) noexcept;
	void refreshEnumeration( void ) noexcept;

	void *m_pLibrary;
	void *m_pInput;
	void *m_pCursor;
	uint64_t m_nDeviceCallback;
	sirGameInputNativeDevice_t m_Devices[SIR_INPUT_DEVICE_CAPACITY];
	std::atomic<void *> m_Pending[PENDING_CAPACITY];
	std::atomic<bool> m_bOverflow;
};

#endif // __SIRENGINE_SIRWINDOWSGAMEINPUTBACKEND_H__
