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

#ifndef __SIR_INPUT_TYPES_H__
#define __SIR_INPUT_TYPES_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/Shared/Window/SIRInputCodes.h>

// Engine input storage and device identity contracts.
// Native input drivers publish SIREventData; Core/Input consumes it to build
// these device records and frame snapshots. Virtual actions and keybindings
// are resolved by Core/Input and are not part of this physical-input wire ABI.
constexpr uint32_t SIR_INPUT_DEVICE_CAPACITY = 32;
constexpr uint32_t SIR_INPUT_BACKEND_CAPACITY = 8;
constexpr uint32_t SIR_INPUT_EVENT_CAPACITY = 2048;
constexpr uint32_t SIR_INPUT_BUTTON_CAPACITY = 1024;
constexpr uint32_t SIR_INPUT_BUTTON_WORDS = SIR_INPUT_BUTTON_CAPACITY / 32;
constexpr uint32_t SIR_INPUT_AXIS_CAPACITY = 16;
constexpr uint32_t SIR_INPUT_USER_CAPACITY = 8;
constexpr uint32_t SIR_INPUT_NO_USER = UINT32_MAX;

enum class SIRInputDeviceClass : uint8_t
{
	SIR_INPUT_KEYBOARD = 0,
	SIR_INPUT_POINTER = 1,
	SIR_INPUT_CONTROLLER = 2
};

enum class SIRInputSignal : uint8_t
{
	SIR_INPUT_CONNECTED = 0,
	SIR_INPUT_DISCONNECTED = 1,
	SIR_INPUT_BUTTON = 2,
	SIR_INPUT_AXIS = 3,
	SIR_INPUT_MOTION = 4,
	SIR_INPUT_WHEEL = 5,
	SIR_INPUT_RESYNC = 6
};

enum class SIRInputBackendId : uint8_t
{
	SIR_INPUT_WIN_RAW = 1,
	SIR_INPUT_WIN_XINPUT = 2,
	SIR_INPUT_WIN_GAMEINPUT = 3,
	SIR_INPUT_WIN_HID = 4,
	SIR_INPUT_LINUX_X11 = 5,
	SIR_INPUT_LINUX_WAYLAND = 6,
	SIR_INPUT_LINUX_EVDEV = 7,
	SIR_INPUT_LINUX_HIDRAW = 8,
	SIR_INPUT_SDL3 = 9,
	SIR_INPUT_TEST = 250
};

typedef struct sirInputDeviceId_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
} sirInputDeviceId_t;

typedef struct sirInputHardwareKey_s
{
	uint64_t nPhysicalId; // 0: identity unknown; owner+native ID still unique.
	uint64_t nNativeId;
	SIRInputBackendId eBackend;
	SIRInputDeviceClass eClass;
} sirInputHardwareKey_t;

enum class SIRInputClockSource : uint8_t
{
	SIR_INPUT_CLOCK_ENGINE_MONOTONIC = 0,
	SIR_INPUT_CLOCK_EVDEV_MONOTONIC = 1,
	SIR_INPUT_CLOCK_GAMEINPUT = 2
};

constexpr uint8_t SIR_INPUT_TIMESTAMP_NATIVE = 1;

typedef struct sirInputTimestamp_s
{
	uint64_t nSourceNs;
	uint64_t nReceivedNs;
	SIRInputClockSource eClock;
	uint8_t bNative;
} sirInputTimestamp_t;

typedef struct sirInputEvent_s
{
	uint64_t nTimestampNs;
	uint64_t nSequence;
	sirInputDeviceId_t hDevice;
	uint64_t nReceivedNs;
	int32_t nValue;
	int32_t nValueY;
	// Physical control ordinal in the domain selected by eClass/eSignal:
	// keyboard = SIRScancode, pointer button = SIRMouseButton,
	// controller button = SIRGamepadButton, axis = SIRGamepadAxis.
	// SIRKeycode is 32-bit and MUST NOT be truncated into this field.
	uint16_t nControl;
	SIRInputSignal eSignal;
	SIRInputDeviceClass eClass;
	SIRInputClockSource eClock;
	uint8_t nTimestampFlags;
} sirInputEvent_t;

static_assert( sizeof( sirInputEvent_t ) == 48, "Input event must remain 48 bytes" );

typedef struct sirInputControlChange_s
{
	SIRInputSignal eSignal;
	uint16_t nControl;
	int32_t nValue;
} sirInputControlChange_t;

typedef struct sirInputDeviceState_s
{
	uint32_t nHeld[SIR_INPUT_BUTTON_WORDS];
	uint32_t nPressed[SIR_INPUT_BUTTON_WORDS];
	uint32_t nReleased[SIR_INPUT_BUTTON_WORDS];
	int32_t nAxes[SIR_INPUT_AXIS_CAPACITY];
	int32_t nMotionX;
	int32_t nMotionY;
} sirInputDeviceState_t;

typedef struct sirInputFrame_s
{
	uint64_t nTick;
	uint32_t nCount;
	uint32_t nResyncCount;
	uint64_t nCutoffNs;
	uint64_t nOldestReceiptAgeNs;
	uint64_t nNewestReceiptAgeNs;
	sirInputEvent_t events[SIR_INPUT_EVENT_CAPACITY];
	sirInputDeviceId_t devices[SIR_INPUT_DEVICE_CAPACITY];
	sirInputDeviceState_t state[SIR_INPUT_DEVICE_CAPACITY];
	uint8_t nConnected[SIR_INPUT_DEVICE_CAPACITY];
	uint32_t nUsers[SIR_INPUT_DEVICE_CAPACITY];
} sirInputFrame_t;

static_assert( sizeof( sirInputDeviceId_t ) == 8, "Input device handle ABI changed" );
static_assert( sizeof( sirInputHardwareKey_t ) == 24, "Hardware key ABI changed" );
static_assert( sizeof( sirInputTimestamp_t ) == 24, "Input timestamp ABI changed" );
static_assert( SIR_INPUT_BUTTON_CAPACITY % 32 == 0, "Button bitmap capacity must be word-aligned" );
static_assert( SIR_INPUT_BUTTON_CAPACITY >= static_cast<uint32_t>( SIRScancode::Count ), "Physical key bitmap is too small" );

#endif // __SIR_INPUT_TYPES_H__
