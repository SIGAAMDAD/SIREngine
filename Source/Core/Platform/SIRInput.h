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

// Canonical gamepad controls shared by all native backends. Gamepad axis
// values use signed [-32767, 32767] sticks and [0, 32767] triggers.
typedef enum sirInputGamepadButton_e : uint8_t
{
	SIR_GAMEPAD_BUTTON_SOUTH = 0,
	SIR_GAMEPAD_BUTTON_EAST = 1,
	SIR_GAMEPAD_BUTTON_WEST = 2,
	SIR_GAMEPAD_BUTTON_NORTH = 3,
	SIR_GAMEPAD_BUTTON_LEFT_SHOULDER = 4,
	SIR_GAMEPAD_BUTTON_RIGHT_SHOULDER = 5,
	SIR_GAMEPAD_BUTTON_BACK = 6,
	SIR_GAMEPAD_BUTTON_START = 7,
	SIR_GAMEPAD_BUTTON_GUIDE = 8,
	SIR_GAMEPAD_BUTTON_LEFT_THUMB = 9,
	SIR_GAMEPAD_BUTTON_RIGHT_THUMB = 10,
	SIR_GAMEPAD_BUTTON_DPAD_UP = 11,
	SIR_GAMEPAD_BUTTON_DPAD_DOWN = 12,
	SIR_GAMEPAD_BUTTON_DPAD_LEFT = 13,
	SIR_GAMEPAD_BUTTON_DPAD_RIGHT = 14,
	SIR_GAMEPAD_BUTTON_LEFT_TRIGGER_DIGITAL = 15,
	SIR_GAMEPAD_BUTTON_RIGHT_TRIGGER_DIGITAL = 16,
	SIR_GAMEPAD_BUTTON_COUNT = 17
} sirInputGamepadButton_t;

typedef enum sirInputGamepadAxis_e : uint8_t
{
	SIR_GAMEPAD_AXIS_LEFT_X = 0,
	SIR_GAMEPAD_AXIS_LEFT_Y = 1,
	SIR_GAMEPAD_AXIS_RIGHT_X = 2,
	SIR_GAMEPAD_AXIS_RIGHT_Y = 3,
	SIR_GAMEPAD_AXIS_LEFT_TRIGGER = 4,
	SIR_GAMEPAD_AXIS_RIGHT_TRIGGER = 5,
	SIR_GAMEPAD_AXIS_COUNT = 6
} sirInputGamepadAxis_t;

typedef struct sirInputGamepadSample_s
{
	uint32_t nButtons;
	int32_t nAxes[SIR_GAMEPAD_AXIS_COUNT];
} sirInputGamepadSample_t;

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

// Runtime-only descriptor; the input owner stores these in one fixed table.
typedef struct sirInputDeviceSlot_s
{
	sirInputDeviceState_t state;
	uint32_t nGeneration;
	bool bActive;
} sirInputDeviceSlot_t;

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
	bool submitGamepadSample( sirInputDeviceHandle_t hDevice, const sirInputGamepadSample_t &sample, uint64_t nTimestampNs ) noexcept;
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
	bool isValid( sirInputDeviceHandle_t hDevice ) const noexcept;
	bool enqueue( const sirInputEvent_t &event ) noexcept;
	uint64_t resolveTime( uint64_t nTimestampNs ) const noexcept;

	sirInputDeviceSlot_t m_Devices[SIR_INPUT_MAX_DEVICES];
	sirInputEvent_t m_Events[SIR_INPUT_MAX_EVENTS];
	uint64_t m_nSequence;
	uint32_t m_nRead;
	uint32_t m_nWrite;
	uint32_t m_nCount;
	uint32_t m_nActiveDevices;
	bool m_bOverflowed;
};

#endif // __SIRENGINE_SIRINPUT_H__
