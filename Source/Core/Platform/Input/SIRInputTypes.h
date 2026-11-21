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

#ifndef __SIRENGINE_SIRINPUTTYPES_H__
#define __SIRENGINE_SIRINPUTTYPES_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

// The platform-independent wire contract. Native device handles never cross here.
constexpr uint32_t SIR_INPUT_DEVICE_CAPACITY = 32;
constexpr uint32_t SIR_INPUT_BACKEND_CAPACITY = 8;
constexpr uint32_t SIR_INPUT_EVENT_CAPACITY = 2048;
constexpr uint32_t SIR_INPUT_BUTTON_CAPACITY = 1024;
constexpr uint32_t SIR_INPUT_BUTTON_WORDS = SIR_INPUT_BUTTON_CAPACITY / 32;
constexpr uint32_t SIR_INPUT_AXIS_CAPACITY = 16;
constexpr uint32_t SIR_INPUT_USER_CAPACITY = 8;
constexpr uint32_t SIR_INPUT_NO_USER = UINT32_MAX;

typedef enum sirInputDeviceClass_e : uint8_t
{
	SIR_INPUT_KEYBOARD = 0,
	SIR_INPUT_POINTER = 1,
	SIR_INPUT_CONTROLLER = 2
} sirInputDeviceClass_t;

typedef enum sirInputSignal_e : uint8_t
{
	SIR_INPUT_CONNECTED = 0,
	SIR_INPUT_DISCONNECTED = 1,
	SIR_INPUT_BUTTON = 2,
	SIR_INPUT_AXIS = 3,
	SIR_INPUT_MOTION = 4,
	SIR_INPUT_WHEEL = 5,
	SIR_INPUT_RESYNC = 6
} sirInputSignal_t;

typedef enum sirInputBackendId_e : uint8_t
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
} sirInputBackendId_t;

typedef enum sirInputControllerControl_e : uint16_t
{
	SIR_INPUT_PAD_SOUTH = 0,
	SIR_INPUT_PAD_EAST = 1,
	SIR_INPUT_PAD_WEST = 2,
	SIR_INPUT_PAD_NORTH = 3,
	SIR_INPUT_PAD_LB = 4,
	SIR_INPUT_PAD_RB = 5,
	SIR_INPUT_PAD_BACK = 6,
	SIR_INPUT_PAD_START = 7,
	SIR_INPUT_PAD_GUIDE = 8,
	SIR_INPUT_PAD_L3 = 9,
	SIR_INPUT_PAD_R3 = 10,
	SIR_INPUT_PAD_DPAD_UP = 11,
	SIR_INPUT_PAD_DPAD_DOWN = 12,
	SIR_INPUT_PAD_DPAD_LEFT = 13,
	SIR_INPUT_PAD_DPAD_RIGHT = 14,
	SIR_INPUT_PAD_LT_BUTTON = 15,
	SIR_INPUT_PAD_RT_BUTTON = 16,
	SIR_INPUT_PAD_BUTTON_COUNT = 17
} sirInputControllerControl_t;

typedef enum sirInputAxisCode_e : uint16_t
{
	SIR_INPUT_AXIS_LX = 0,
	SIR_INPUT_AXIS_LY = 1,
	SIR_INPUT_AXIS_RX = 2,
	SIR_INPUT_AXIS_RY = 3,
	SIR_INPUT_AXIS_LT = 4,
	SIR_INPUT_AXIS_RT = 5,
	SIR_INPUT_AXIS_COUNT = 6
} sirInputAxisCode_t;

typedef struct sirInputDeviceId_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
} sirInputDeviceId_t;

typedef struct sirInputHardwareKey_s
{
	uint64_t nPhysicalId; // 0: identity unknown; owner+native ID still unique.
	uint64_t nNativeId;
	sirInputBackendId_t eBackend;
	sirInputDeviceClass_t eClass;
} sirInputHardwareKey_t;

typedef enum sirInputClockSource_e : uint8_t
{
	SIR_INPUT_CLOCK_ENGINE_MONOTONIC = 0,
	SIR_INPUT_CLOCK_EVDEV_MONOTONIC = 1,
	SIR_INPUT_CLOCK_GAMEINPUT = 2
} sirInputClockSource_t;

constexpr uint8_t SIR_INPUT_TIMESTAMP_NATIVE = 1;

typedef struct sirInputTimestamp_s
{
	uint64_t nSourceNs;
	uint64_t nReceivedNs;
	sirInputClockSource_t eClock;
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
	uint16_t nControl;
	sirInputSignal_t eSignal;
	sirInputDeviceClass_t eClass;
	sirInputClockSource_t eClock;
	uint8_t nTimestampFlags;
} sirInputEvent_t;

static_assert( sizeof( sirInputEvent_t ) == 48, "Input event must remain 48 bytes" );

typedef struct sirInputControlChange_s
{
	sirInputSignal_t eSignal;
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

#endif // __SIRENGINE_SIRINPUTTYPES_H__
