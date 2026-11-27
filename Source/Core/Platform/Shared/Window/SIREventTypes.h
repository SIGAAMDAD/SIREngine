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

#ifndef __SIR_EVENT_TYPES_H__
#define __SIR_EVENT_TYPES_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include "SIRInputCodes.h"

// Engine-owned event identifiers. The category ranges intentionally follow
// SDL3's event grouping, but this is not an SDL_Event binary ABI.
enum class SIREventType : uint32_t
{
	None = 0,

	// Application lifecycle.
	Quit = 0x100,
	Terminating,
	LowMemory,
	WillEnterBackground,
	DidEnterBackground,
	WillEnterForeground,
	DidEnterForeground,
	LocaleChanged,
	SystemThemeChanged,

	// Display changes.
	DisplayOrientation = 0x151,
	DisplayAdded,
	DisplayRemoved,
	DisplayMoved,
	DisplayDesktopModeChanged,
	DisplayCurrentModeChanged,
	DisplayContentScaleChanged,
	DisplayUsableBoundsChanged,

	// Window lifecycle, focus, geometry, presentation properties.
	WindowShown = 0x202,
	WindowHidden,
	WindowExposed,
	WindowMoved,
	WindowResized,
	WindowPixelSizeChanged,
	WindowMetalViewResized,
	WindowMinimized,
	WindowMaximized,
	WindowRestored,
	WindowMouseEnter,
	WindowMouseLeave,
	WindowFocusGained,
	WindowFocusLost,
	WindowCloseRequested,
	WindowHitTest,
	WindowICCProfileChanged,
	WindowDisplayChanged,
	WindowDisplayScaleChanged,
	WindowSafeAreaChanged,
	WindowOccluded,
	WindowEnterFullscreen,
	WindowLeaveFullscreen,
	WindowDestroyed,
	WindowHDRStateChanged,
	WindowSettingsChanged,

	// Physical keyboard, text composition and virtual keyboard.
	KeyDown = 0x300,
	KeyUp,
	TextEditing,
	TextInput,
	KeymapChanged,
	KeyboardAdded,
	KeyboardRemoved,
	TextEditingCandidates,
	ScreenKeyboardShown,
	ScreenKeyboardHidden,

	// Mouse.
	MouseMotion = 0x400,
	MouseButtonDown,
	MouseButtonUp,
	MouseWheel,
	MouseAdded,
	MouseRemoved,

	// Unmapped joysticks and other generic controllers.
	JoystickAxisMotion = 0x600,
	JoystickBallMotion,
	JoystickHatMotion,
	JoystickButtonDown,
	JoystickButtonUp,
	JoystickAdded,
	JoystickRemoved,
	JoystickBatteryUpdated,
	JoystickUpdateComplete,

	// Mapped gamepads.
	GamepadAxisMotion = 0x650,
	GamepadButtonDown,
	GamepadButtonUp,
	GamepadAdded,
	GamepadRemoved,
	GamepadRemapped,
	GamepadTouchpadDown,
	GamepadTouchpadMotion,
	GamepadTouchpadUp,
	GamepadSensorUpdate,
	GamepadUpdateComplete,
	GamepadSteamHandleUpdated,
	GamepadCapsenseTouch,
	GamepadCapsenseRelease,

	// Touchscreens and pinch gestures.
	FingerDown = 0x700,
	FingerUp,
	FingerMotion,
	FingerCanceled,
	PinchBegin = 0x710,
	PinchUpdate,
	PinchEnd,

	ClipboardUpdate = 0x900,

	// Drag and drop.
	DropFile = 0x1000,
	DropText,
	DropBegin,
	DropComplete,
	DropPosition,

	// Pressure-sensitive pen / stylus input.
	PenProximityIn = 0x1300,
	PenProximityOut,
	PenDown,
	PenUp,
	PenButtonDown,
	PenButtonUp,
	PenMotion,
	PenAxis,

	// Custom engine/application events may use IDs starting here.
	User = 0x8000
};

struct SIRDisplayEvent
{
	uint32_t displayId;
	int32_t data1;
	int32_t data2;
};

// data1/data2 are type-dependent, e.g. x/y for Moved, w/h for Resized.
struct SIRWindowEvent
{
	uint32_t windowId;
	int32_t data1;
	int32_t data2;
};

// Used by keyboard/mouse/joystick/gamepad attach and detach events.
struct SIRDeviceEvent
{
	uint32_t deviceId;
};

struct SIRKeyboardEvent
{
	uint32_t windowId;
	uint32_t deviceId;
	SIRScancode scancode; // Engine physical-key code.
	SIRKeycode keycode; // Engine layout-dependent key code.
	SIRKeyModifier modifiers; // Engine-defined modifier bitfield.
	uint16_t nativeScancode; // Optional backend scancode.
	bool down;
	bool repeat;
};

// Text pointers and arrays are borrowed; event-queue storage must keep them
// alive until dispatch completes (or deep-copy into a queue-owned arena).
struct SIRTextInputEvent
{
	uint32_t windowId;
	const char *text; // UTF-8.
};

struct SIRTextEditingEvent
{
	uint32_t windowId;
	const char *text; // UTF-8 composition text.
	int32_t start;
	int32_t length;
};

struct SIRTextCandidatesEvent
{
	uint32_t windowId;
	const char *const *candidates;
	int32_t candidateCount;
	int32_t selectedCandidate;
	bool horizontal;
};

struct SIRMouseMotionEvent
{
	uint32_t windowId;
	uint32_t deviceId;
	uint32_t buttonState;
	float x;
	float y;
	float deltaX;
	float deltaY;
};

struct SIRMouseButtonEvent
{
	uint32_t windowId;
	uint32_t deviceId;
	SIRMouseButton button;
	bool down;
	uint8_t clicks;
	uint8_t padding;
	float x;
	float y;
};

struct SIRMouseWheelEvent
{
	uint32_t windowId;
	uint32_t deviceId;
	float x; // Horizontal scroll amount.
	float y; // Vertical scroll amount.
	SIRMouseWheelDirection direction; // 0 normal, 1 flipped.
	float mouseX;
	float mouseY;
	int32_t integerX; // Accumulated whole wheel ticks, if available.
	int32_t integerY;
};

struct SIRJoystickAxisEvent
{
	uint32_t deviceId;
	uint8_t axis;
	int16_t value; // -32768 .. 32767.
};

struct SIRJoystickBallEvent
{
	uint32_t deviceId;
	uint8_t ball;
	int16_t deltaX;
	int16_t deltaY;
};

struct SIRJoystickHatEvent
{
	uint32_t deviceId;
	uint8_t hat;
	uint8_t value; // Direction bitfield, 0 centered.
};

struct SIRJoystickButtonEvent
{
	uint32_t deviceId;
	uint8_t button;
	bool down;
};

struct SIRJoystickBatteryEvent
{
	uint32_t deviceId;
	uint32_t powerState;
	int32_t percent; // -1 if unknown.
};

struct SIRGamepadAxisEvent
{
	uint32_t deviceId;
	uint8_t axis; // Engine-mapped axis index.
	int16_t value;
};

struct SIRGamepadButtonEvent
{
	uint32_t deviceId;
	uint8_t button; // Engine-mapped button index.
	bool down;
};

struct SIRGamepadTouchpadEvent
{
	uint32_t deviceId;
	int32_t touchpad;
	int32_t finger;
	float x; // Normalized touchpad position.
	float y;
	float pressure; // Normalized 0..1.
};

struct SIRGamepadSensorEvent
{
	uint32_t deviceId;
	int32_t sensorType;
	float data[3];
	uint64_t sensorTimestamp;
};

struct SIRGamepadCapsenseEvent
{
	uint32_t deviceId;
	uint8_t sensor;
	bool down;
};

struct SIRTouchFingerEvent
{
	uint64_t touchId;
	uint64_t fingerId;
	float x; // Normalized window coordinates.
	float y;
	float deltaX;
	float deltaY;
	float pressure;
	uint32_t windowId;
};

struct SIRPinchEvent
{
	float scale; // Scale change since previous update.
	uint32_t windowId;
	float spanX;
	float spanY;
	float focusX;
	float focusY;
};

struct SIRPenEvent
{
	uint32_t windowId;
	uint32_t penId;
	uint32_t penState;
	uint32_t deviceType;
	float x; // Window-relative coordinates.
	float y;
	uint32_t axis; // Used for PenAxis.
	float value; // Used for PenAxis.
	uint8_t button;
	bool down;
	bool eraser;
};

struct SIRDropEvent
{
	uint32_t windowId;
	float x;
	float y;
	const char *source; // Optional, borrowed.
	const char *data; // UTF-8 text/path, borrowed.
};

struct SIRClipboardEvent
{
	bool owner;
	int32_t mimeTypeCount;
	const char *const *mimeTypes; // Borrowed.
};

struct SIRUserEvent
{
	uint32_t windowId;
	int32_t code;
	void *data1;
	void *data2;
};

// One event header followed by a variant payload. All event instances are
// copyable and have the same 128-byte size. This is NOT an SDL binary ABI.
struct SIREventData
{
	SIREventType type;
	uint32_t reserved;
	uint64_t timestamp; // Nanoseconds from the engine monotonic clock.

	union
	{
		SIRDisplayEvent display;
		SIRWindowEvent window;
		SIRDeviceEvent kdevice;
		SIRDeviceEvent mdevice;
		SIRDeviceEvent jdevice;
		SIRDeviceEvent gdevice;
		SIRKeyboardEvent key;
		SIRTextInputEvent text;
		SIRTextEditingEvent edit;
		SIRTextCandidatesEvent editCandidates;
		SIRMouseMotionEvent motion;
		SIRMouseButtonEvent button;
		SIRMouseWheelEvent wheel;
		SIRJoystickAxisEvent jaxis;
		SIRJoystickBallEvent jball;
		SIRJoystickHatEvent jhat;
		SIRJoystickButtonEvent jbutton;
		SIRJoystickBatteryEvent jbattery;
		SIRGamepadAxisEvent gaxis;
		SIRGamepadButtonEvent gbutton;
		SIRGamepadTouchpadEvent gtouchpad;
		SIRGamepadSensorEvent gsensor;
		SIRGamepadCapsenseEvent gcapsense;
		SIRTouchFingerEvent tfinger;
		SIRPinchEvent pinch;
		SIRPenEvent pen;
		SIRDropEvent drop;
		SIRClipboardEvent clipboard;
		SIRUserEvent user;
		uint8_t padding[112];
	};
};

static_assert( sizeof( SIREventType ) == sizeof( uint32_t ), "SIREventType must be 32 bits" );
static_assert( sizeof( SIREventData ) == 128, "SIREventData must fit the fixed 128-byte event slot" );
static_assert( std::is_trivially_copyable<SIREventData>::value, "SIREventData must be trivially copyable" );
static_assert( std::is_standard_layout<SIREventData>::value, "SIREventData must have standard layout" );
static_assert( offsetof( SIREventData, timestamp ) == 8, "SIREventData timestamp must follow the type and reserved field" );
static_assert( offsetof( SIREventData, key ) == 16, "SIREventData payload must follow the common header" );

#endif // __SIR_EVENT_TYPES_H__
