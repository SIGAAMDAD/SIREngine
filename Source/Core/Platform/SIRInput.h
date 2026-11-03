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

#ifndef __SIRENGINE_SIRINPUT_H__
#define __SIRENGINE_SIRINPUT_H__

#include <SIRPCH.hpp>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif

constexpr uint32_t SIR_INPUT_MAX_DEVICES = 32;
constexpr uint32_t SIR_INPUT_MAX_EVENTS = 1024;
constexpr uint32_t SIR_INPUT_MAX_BUTTONS = 1024;
constexpr uint32_t SIR_INPUT_MAX_AXES = 64;
constexpr uint32_t SIR_INPUT_BUTTON_WORDS = SIR_INPUT_MAX_BUTTONS / 32;

typedef enum sirInputDeviceKind_e : uint8_t
{
	SIR_INPUT_DEVICE_KEYBOARD = 0,
	SIR_INPUT_DEVICE_MOUSE = 1,
	SIR_INPUT_DEVICE_GAMEPAD = 2
} sirInputDeviceKind_t;

typedef enum sirInputEventKind_e : uint8_t
{
	SIR_INPUT_EVENT_CONNECTED = 0,
	SIR_INPUT_EVENT_DISCONNECTED = 1,
	SIR_INPUT_EVENT_BUTTON = 2,
	SIR_INPUT_EVENT_AXIS = 3,
	SIR_INPUT_EVENT_POINTER = 4,
	SIR_INPUT_EVENT_WHEEL = 5,
	SIR_INPUT_EVENT_RESYNC = 6
} sirInputEventKind_t;

typedef struct sirInputDeviceHandle_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
} sirInputDeviceHandle_t;

typedef struct sirInputEvent_s
{
	uint64_t nTimestampNs;
	uint64_t nSequence;
	sirInputDeviceHandle_t hDevice;
	int32_t nValue;
	int32_t nValueY;
	uint16_t nCode;
	sirInputEventKind_t eKind;
	sirInputDeviceKind_t eDeviceKind;
} sirInputEvent_t;

typedef struct sirInputDeviceState_s
{
	uint64_t nNativeId;
	uint32_t nButtons[SIR_INPUT_BUTTON_WORDS];
	uint32_t nPressed[SIR_INPUT_BUTTON_WORDS];
	uint32_t nReleased[SIR_INPUT_BUTTON_WORDS];
	int32_t nAxes[SIR_INPUT_MAX_AXES];
	int32_t nMouseDeltaX;
	int32_t nMouseDeltaY;
	sirInputDeviceKind_t eKind;
} sirInputDeviceState_t;

// The owner thread calls all methods. Native collectors must not call this
// concurrently: threaded backends need a separately synchronized ingress.
class SIRInputSystem
{
public:
	SIRInputSystem( void ) noexcept;
	SIRInputSystem( const SIRInputSystem & ) = delete;
	SIRInputSystem &operator=( const SIRInputSystem & ) = delete;

	bool addDevice( sirInputDeviceKind_t eKind, uint64_t nNativeId, sirInputDeviceHandle_t *pHandle ) noexcept;
	bool findDevice( sirInputDeviceKind_t eKind, uint64_t nNativeId, sirInputDeviceHandle_t *pHandle ) const noexcept;
	bool removeDevice( sirInputDeviceHandle_t hDevice ) noexcept;
	bool pushButton( sirInputDeviceHandle_t hDevice, uint16_t nCode, bool bPressed, uint64_t nTimestampNs ) noexcept;
	bool pushAxis( sirInputDeviceHandle_t hDevice, uint16_t nAxis, int32_t nValue, uint64_t nTimestampNs ) noexcept;
	bool pushPointer( sirInputDeviceHandle_t hDevice, int32_t nDeltaX, int32_t nDeltaY, uint64_t nTimestampNs ) noexcept;
	bool pushWheel( sirInputDeviceHandle_t hDevice, uint16_t nAxis, int32_t nDelta, uint64_t nTimestampNs ) noexcept;
	bool pollEvent( sirInputEvent_t *pEvent ) noexcept;
	bool getState( sirInputDeviceHandle_t hDevice, sirInputDeviceState_t *pState ) const noexcept;
	bool isOverflowed( void ) const noexcept;
	bool resynchronizeDevice( sirInputDeviceHandle_t hDevice ) noexcept;
	uint32_t getActiveDeviceCount( void ) const noexcept;
	uint32_t enumerateDevices( sirInputDeviceHandle_t *pOutput, uint32_t nCapacity ) const noexcept;
	void beginTick( void ) noexcept;
	void shutdown( void ) noexcept;

private:
	typedef struct sirDeviceSlot_s
	{
		sirInputDeviceState_t state;
		uint32_t nGeneration;
		bool bActive;
	} sirDeviceSlot_t;

	bool isValid( sirInputDeviceHandle_t hDevice ) const noexcept;
	bool enqueue( const sirInputEvent_t &event ) noexcept;
	uint64_t resolveTime( uint64_t nTimestampNs ) const noexcept;

	sirDeviceSlot_t m_Devices[SIR_INPUT_MAX_DEVICES];
	sirInputEvent_t m_Events[SIR_INPUT_MAX_EVENTS];
	uint64_t m_nSequence;
	uint32_t m_nRead;
	uint32_t m_nWrite;
	uint32_t m_nCount;
	uint32_t m_nActiveDevices;
	bool m_bOverflowed;
};

#endif // __SIRENGINE_SIRINPUT_H__
