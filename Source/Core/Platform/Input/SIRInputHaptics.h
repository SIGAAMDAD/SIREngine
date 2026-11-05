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

#ifndef __SIRENGINE_SIRINPUTHAPTICS_H__
#define __SIRENGINE_SIRINPUTHAPTICS_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/Input/SIRInputTypes.h>

constexpr uint32_t SIR_INPUT_HAPTIC_EFFECT_CAPACITY = 64;
constexpr uint32_t SIR_INPUT_HAPTIC_MAX_DURATION_MS = 60000;

typedef enum sirInputHapticCapability_e : uint32_t
{
	SIR_INPUT_HAPTIC_CAP_NONE = 0,
	SIR_INPUT_HAPTIC_CAP_RUMBLE = 1,
	SIR_INPUT_HAPTIC_CAP_TRIGGER_RUMBLE = 2,
	SIR_INPUT_HAPTIC_CAP_LED = 4,
	SIR_INPUT_HAPTIC_CAP_ADAPTIVE_TRIGGER = 8,
	SIR_INPUT_HAPTIC_CAP_HD_HAPTICS = 16
} sirInputHapticCapability_t;

typedef struct sirInputHapticHandle_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
} sirInputHapticHandle_t;

typedef struct sirInputHapticRequest_s
{
	sirInputDeviceId_t hDevice;
	uint16_t nLeft;
	uint16_t nRight;
	uint32_t nAttackMs;
	uint32_t nSustainMs;
	uint32_t nReleaseMs;
	uint8_t nPriority;
} sirInputHapticRequest_t;

typedef bool ( *sirInputRumbleSink_t )( void *pContext, sirInputDeviceId_t hDevice, uint16_t nLeft, uint16_t nRight );

typedef struct sirInputHapticEffect_s
{
	sirInputHapticRequest_t request;
	uint64_t nStartNs;
	uint32_t nGeneration;
	bool bActive;
} sirInputHapticEffect_t;

typedef struct sirInputHapticMotorState_s
{
	sirInputDeviceId_t hDevice;
	uint16_t nLeft;
	uint16_t nRight;
	bool bActive;
} sirInputHapticMotorState_t;

// Fixed-capacity, monotonic-time motor envelope mixer. Highest-priority
// active layer wins; layers of equal priority use per-motor maximum.
class SIRInputHaptics
{
public:
	SIRInputHaptics( void ) noexcept;
	bool schedule( const sirInputHapticRequest_t& request, uint64_t nNowNs, sirInputHapticHandle_t *pHandle ) noexcept;
	bool cancel( sirInputHapticHandle_t hEffect ) noexcept;
	bool advance( uint64_t nNowNs, bool bPaused, sirInputRumbleSink_t pSink, void *pContext ) noexcept;
	void forgetDevice( sirInputDeviceId_t hDevice, sirInputRumbleSink_t pSink, void *pContext ) noexcept;
	void stopAll( sirInputRumbleSink_t pSink, void *pContext ) noexcept;
	uint32_t activeCount( void ) const noexcept;
	bool hasWork( void ) const noexcept;

private:
	static uint32_t gain( const sirInputHapticEffect_t& effect, uint64_t nNowNs ) noexcept;
	sirInputHapticEffect_t m_Effects[SIR_INPUT_HAPTIC_EFFECT_CAPACITY];
	sirInputHapticMotorState_t m_Motors[SIR_INPUT_DEVICE_CAPACITY];
	uint32_t m_nGeneration;
	uint32_t m_nActive;
	uint32_t m_nOutput;
};

#endif // __SIRENGINE_SIRINPUTHAPTICS_H__
