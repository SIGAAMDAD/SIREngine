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

#ifndef __SIR_INPUT_CODES_H__
#define __SIR_INPUT_CODES_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <cstdint>
#include <type_traits>

// Platform-independent control codes used by input drivers and Core/Input.
// These are canonical device controls, not gameplay actions or virtual bindings.
// SDL3-compatible numeric values are retained where documented; SDL is not required.
// Keyboard scancodes identify physical positions; keycodes identify logical keys.
// Gameplay-facing virtual input and bindings belong in Source/Core/Input.

enum class SIRScancode : uint32_t
{
	Unknown = 0,
	A = 4,
	B = 5,
	C = 6,
	D = 7,
	E = 8,
	F = 9,
	G = 10,
	H = 11,
	I = 12,
	J = 13,
	K = 14,
	L = 15,
	M = 16,
	N = 17,
	O = 18,
	P = 19,
	Q = 20,
	R = 21,
	S = 22,
	T = 23,
	U = 24,
	V = 25,
	W = 26,
	X = 27,
	Y = 28,
	Z = 29,
	Num1 = 30,
	Num2 = 31,
	Num3 = 32,
	Num4 = 33,
	Num5 = 34,
	Num6 = 35,
	Num7 = 36,
	Num8 = 37,
	Num9 = 38,
	Num0 = 39,
	Return = 40,
	Escape = 41,
	Backspace = 42,
	Tab = 43,
	Space = 44,
	Minus = 45,
	Equals = 46,
	LeftBracket = 47,
	RightBracket = 48,
	Backslash = 49,
	NonUSHash = 50,
	Semicolon = 51,
	Apostrophe = 52,
	Grave = 53,
	Comma = 54,
	Period = 55,
	Slash = 56,
	CapsLock = 57,
	F1 = 58,
	F2 = 59,
	F3 = 60,
	F4 = 61,
	F5 = 62,
	F6 = 63,
	F7 = 64,
	F8 = 65,
	F9 = 66,
	F10 = 67,
	F11 = 68,
	F12 = 69,
	PrintScreen = 70,
	ScrollLock = 71,
	Pause = 72,
	Insert = 73,
	Home = 74,
	PageUp = 75,
	Delete = 76,
	End = 77,
	PageDown = 78,
	Right = 79,
	Left = 80,
	Down = 81,
	Up = 82,
	NumLockClear = 83,
	KeypadDivide = 84,
	KeypadMultiply = 85,
	KeypadMinus = 86,
	KeypadPlus = 87,
	KeypadEnter = 88,
	Keypad1 = 89,
	Keypad2 = 90,
	Keypad3 = 91,
	Keypad4 = 92,
	Keypad5 = 93,
	Keypad6 = 94,
	Keypad7 = 95,
	Keypad8 = 96,
	Keypad9 = 97,
	Keypad0 = 98,
	KeypadPeriod = 99,
	NonUSBackslash = 100,
	Application = 101,
	Power = 102,
	KeypadEquals = 103,
	F13 = 104,
	F14 = 105,
	F15 = 106,
	F16 = 107,
	F17 = 108,
	F18 = 109,
	F19 = 110,
	F20 = 111,
	F21 = 112,
	F22 = 113,
	F23 = 114,
	F24 = 115,
	Execute = 116,
	Help = 117,
	Menu = 118,
	Select = 119,
	Stop = 120,
	Again = 121,
	Undo = 122,
	Cut = 123,
	Copy = 124,
	Paste = 125,
	Find = 126,
	Mute = 127,
	VolumeUp = 128,
	VolumeDown = 129,
	KeypadComma = 133,
	KeypadEqualsAS400 = 134,
	International1 = 135,
	International2 = 136,
	International3 = 137,
	International4 = 138,
	International5 = 139,
	International6 = 140,
	International7 = 141,
	International8 = 142,
	International9 = 143,
	Lang1 = 144,
	Lang2 = 145,
	Lang3 = 146,
	Lang4 = 147,
	Lang5 = 148,
	Lang6 = 149,
	Lang7 = 150,
	Lang8 = 151,
	Lang9 = 152,
	AltErase = 153,
	SysReq = 154,
	Cancel = 155,
	Clear = 156,
	Prior = 157,
	Return2 = 158,
	Separator = 159,
	Out = 160,
	Oper = 161,
	ClearAgain = 162,
	CrSel = 163,
	ExSel = 164,
	Front = 165,
	Keypad00 = 176,
	Keypad000 = 177,
	ThousandsSeparator = 178,
	DecimalSeparator = 179,
	CurrencyUnit = 180,
	CurrencySubUnit = 181,
	KeypadLeftParen = 182,
	KeypadRightParen = 183,
	KeypadLeftBrace = 184,
	KeypadRightBrace = 185,
	KeypadTab = 186,
	KeypadBackspace = 187,
	KeypadA = 188,
	KeypadB = 189,
	KeypadC = 190,
	KeypadD = 191,
	KeypadE = 192,
	KeypadF = 193,
	KeypadXor = 194,
	KeypadPower = 195,
	KeypadPercent = 196,
	KeypadLess = 197,
	KeypadGreater = 198,
	KeypadAmpersand = 199,
	KeypadDoubleAmpersand = 200,
	KeypadVerticalBar = 201,
	KeypadDoubleVerticalBar = 202,
	KeypadColon = 203,
	KeypadHash = 204,
	KeypadSpace = 205,
	KeypadAt = 206,
	KeypadExclam = 207,
	KeypadMemStore = 208,
	KeypadMemRecall = 209,
	KeypadMemClear = 210,
	KeypadMemAdd = 211,
	KeypadMemSubtract = 212,
	KeypadMemMultiply = 213,
	KeypadMemDivide = 214,
	KeypadPlusMinus = 215,
	KeypadClear = 216,
	KeypadClearEntry = 217,
	KeypadBinary = 218,
	KeypadOctal = 219,
	KeypadDecimal = 220,
	KeypadHexadecimal = 221,
	LeftCtrl = 224,
	LeftShift = 225,
	LeftAlt = 226,
	LeftGUI = 227,
	RightCtrl = 228,
	RightShift = 229,
	RightAlt = 230,
	RightGUI = 231,
	Mode = 257,
	Sleep = 258,
	Wake = 259,
	ChannelIncrement = 260,
	ChannelDecrement = 261,
	MediaPlay = 262,
	MediaPause = 263,
	MediaRecord = 264,
	MediaFastForward = 265,
	MediaRewind = 266,
	MediaNextTrack = 267,
	MediaPreviousTrack = 268,
	MediaStop = 269,
	MediaEject = 270,
	MediaPlayPause = 271,
	MediaSelect = 272,
	ACNew = 273,
	ACOpen = 274,
	ACClose = 275,
	ACExit = 276,
	ACSave = 277,
	ACPrint = 278,
	ACProperties = 279,
	ACSearch = 280,
	ACHome = 281,
	ACBack = 282,
	ACForward = 283,
	ACStop = 284,
	ACRefresh = 285,
	ACBookmarks = 286,
	SoftLeft = 287,
	SoftRight = 288,
	Call = 289,
	EndCall = 290,
	Reserved = 400,
	Count = 512,
};

constexpr uint32_t SIRKeycodeScancodeMask = 1u << 30;
constexpr uint32_t SIRKeycodeExtendedMask = 1u << 29;

// Logical key (layout-dependent Unicode or nonprinting special code).
enum class SIRKeycode : uint32_t
{
	Unknown = 0,
	Return = 0x0D,
	Escape = 0x1B,
	Backspace = 0x08,
	Tab = 0x09,
	Space = 0x20,
	Exclaim = 0x21,
	DoubleApostrophe = 0x22,
	Hash = 0x23,
	Dollar = 0x24,
	Percent = 0x25,
	Ampersand = 0x26,
	Apostrophe = 0x27,
	LeftParen = 0x28,
	RightParen = 0x29,
	Asterisk = 0x2A,
	Plus = 0x2B,
	Comma = 0x2C,
	Minus = 0x2D,
	Period = 0x2E,
	Slash = 0x2F,
	Num0 = 0x30,
	Num1 = 0x31,
	Num2 = 0x32,
	Num3 = 0x33,
	Num4 = 0x34,
	Num5 = 0x35,
	Num6 = 0x36,
	Num7 = 0x37,
	Num8 = 0x38,
	Num9 = 0x39,
	Colon = 0x3A,
	Semicolon = 0x3B,
	Less = 0x3C,
	Equals = 0x3D,
	Greater = 0x3E,
	Question = 0x3F,
	At = 0x40,
	LeftBracket = 0x5B,
	Backslash = 0x5C,
	RightBracket = 0x5D,
	Caret = 0x5E,
	Underscore = 0x5F,
	Grave = 0x60,
	A = 0x61,
	B = 0x62,
	C = 0x63,
	D = 0x64,
	E = 0x65,
	F = 0x66,
	G = 0x67,
	H = 0x68,
	I = 0x69,
	J = 0x6a,
	K = 0x6b,
	L = 0x6c,
	M = 0x6d,
	N = 0x6e,
	O = 0x6f,
	P = 0x70,
	Q = 0x71,
	R = 0x72,
	S = 0x73,
	T = 0x74,
	U = 0x75,
	V = 0x76,
	W = 0x77,
	X = 0x78,
	Y = 0x79,
	Z = 0x7a,
	LeftBrace = 0x7B,
	Pipe = 0x7C,
	RightBrace = 0x7D,
	Tilde = 0x7E,
	Delete = 0x7F,
	PlusMinus = 0xB1,
	CapsLock = ( SIRKeycodeScancodeMask | 57u ),
	PrintScreen = ( SIRKeycodeScancodeMask | 70u ),
	ScrollLock = ( SIRKeycodeScancodeMask | 71u ),
	Pause = ( SIRKeycodeScancodeMask | 72u ),
	Insert = ( SIRKeycodeScancodeMask | 73u ),
	Home = ( SIRKeycodeScancodeMask | 74u ),
	PageUp = ( SIRKeycodeScancodeMask | 75u ),
	End = ( SIRKeycodeScancodeMask | 77u ),
	PageDown = ( SIRKeycodeScancodeMask | 78u ),
	Right = ( SIRKeycodeScancodeMask | 79u ),
	Left = ( SIRKeycodeScancodeMask | 80u ),
	Down = ( SIRKeycodeScancodeMask | 81u ),
	Up = ( SIRKeycodeScancodeMask | 82u ),
	NumLockClear = ( SIRKeycodeScancodeMask | 83u ),
	KeypadDivide = ( SIRKeycodeScancodeMask | 84u ),
	KeypadMultiply = ( SIRKeycodeScancodeMask | 85u ),
	KeypadMinus = ( SIRKeycodeScancodeMask | 86u ),
	KeypadPlus = ( SIRKeycodeScancodeMask | 87u ),
	KeypadEnter = ( SIRKeycodeScancodeMask | 88u ),
	KeypadPeriod = ( SIRKeycodeScancodeMask | 99u ),
	Application = ( SIRKeycodeScancodeMask | 101u ),
	Power = ( SIRKeycodeScancodeMask | 102u ),
	KeypadEquals = ( SIRKeycodeScancodeMask | 103u ),
	Execute = ( SIRKeycodeScancodeMask | 116u ),
	Help = ( SIRKeycodeScancodeMask | 117u ),
	Menu = ( SIRKeycodeScancodeMask | 118u ),
	Select = ( SIRKeycodeScancodeMask | 119u ),
	Stop = ( SIRKeycodeScancodeMask | 120u ),
	Again = ( SIRKeycodeScancodeMask | 121u ),
	Undo = ( SIRKeycodeScancodeMask | 122u ),
	Cut = ( SIRKeycodeScancodeMask | 123u ),
	Copy = ( SIRKeycodeScancodeMask | 124u ),
	Paste = ( SIRKeycodeScancodeMask | 125u ),
	Find = ( SIRKeycodeScancodeMask | 126u ),
	Mute = ( SIRKeycodeScancodeMask | 127u ),
	VolumeUp = ( SIRKeycodeScancodeMask | 128u ),
	VolumeDown = ( SIRKeycodeScancodeMask | 129u ),
	KeypadComma = ( SIRKeycodeScancodeMask | 133u ),
	KeypadEqualsAS400 = ( SIRKeycodeScancodeMask | 134u ),
	AltErase = ( SIRKeycodeScancodeMask | 153u ),
	SysReq = ( SIRKeycodeScancodeMask | 154u ),
	Cancel = ( SIRKeycodeScancodeMask | 155u ),
	Clear = ( SIRKeycodeScancodeMask | 156u ),
	Prior = ( SIRKeycodeScancodeMask | 157u ),
	Return2 = ( SIRKeycodeScancodeMask | 158u ),
	Separator = ( SIRKeycodeScancodeMask | 159u ),
	Out = ( SIRKeycodeScancodeMask | 160u ),
	Oper = ( SIRKeycodeScancodeMask | 161u ),
	ClearAgain = ( SIRKeycodeScancodeMask | 162u ),
	CrSel = ( SIRKeycodeScancodeMask | 163u ),
	ExSel = ( SIRKeycodeScancodeMask | 164u ),
	Front = ( SIRKeycodeScancodeMask | 165u ),
	Keypad00 = ( SIRKeycodeScancodeMask | 176u ),
	Keypad000 = ( SIRKeycodeScancodeMask | 177u ),
	ThousandsSeparator = ( SIRKeycodeScancodeMask | 178u ),
	DecimalSeparator = ( SIRKeycodeScancodeMask | 179u ),
	CurrencyUnit = ( SIRKeycodeScancodeMask | 180u ),
	CurrencySubUnit = ( SIRKeycodeScancodeMask | 181u ),
	KeypadLeftParen = ( SIRKeycodeScancodeMask | 182u ),
	KeypadRightParen = ( SIRKeycodeScancodeMask | 183u ),
	KeypadLeftBrace = ( SIRKeycodeScancodeMask | 184u ),
	KeypadRightBrace = ( SIRKeycodeScancodeMask | 185u ),
	KeypadTab = ( SIRKeycodeScancodeMask | 186u ),
	KeypadBackspace = ( SIRKeycodeScancodeMask | 187u ),
	KeypadXor = ( SIRKeycodeScancodeMask | 194u ),
	KeypadPower = ( SIRKeycodeScancodeMask | 195u ),
	KeypadPercent = ( SIRKeycodeScancodeMask | 196u ),
	KeypadLess = ( SIRKeycodeScancodeMask | 197u ),
	KeypadGreater = ( SIRKeycodeScancodeMask | 198u ),
	KeypadAmpersand = ( SIRKeycodeScancodeMask | 199u ),
	KeypadDoubleAmpersand = ( SIRKeycodeScancodeMask | 200u ),
	KeypadVerticalBar = ( SIRKeycodeScancodeMask | 201u ),
	KeypadDoubleVerticalBar = ( SIRKeycodeScancodeMask | 202u ),
	KeypadColon = ( SIRKeycodeScancodeMask | 203u ),
	KeypadHash = ( SIRKeycodeScancodeMask | 204u ),
	KeypadSpace = ( SIRKeycodeScancodeMask | 205u ),
	KeypadAt = ( SIRKeycodeScancodeMask | 206u ),
	KeypadExclam = ( SIRKeycodeScancodeMask | 207u ),
	KeypadMemStore = ( SIRKeycodeScancodeMask | 208u ),
	KeypadMemRecall = ( SIRKeycodeScancodeMask | 209u ),
	KeypadMemClear = ( SIRKeycodeScancodeMask | 210u ),
	KeypadMemAdd = ( SIRKeycodeScancodeMask | 211u ),
	KeypadMemSubtract = ( SIRKeycodeScancodeMask | 212u ),
	KeypadMemMultiply = ( SIRKeycodeScancodeMask | 213u ),
	KeypadMemDivide = ( SIRKeycodeScancodeMask | 214u ),
	KeypadPlusMinus = ( SIRKeycodeScancodeMask | 215u ),
	KeypadClear = ( SIRKeycodeScancodeMask | 216u ),
	KeypadClearEntry = ( SIRKeycodeScancodeMask | 217u ),
	KeypadBinary = ( SIRKeycodeScancodeMask | 218u ),
	KeypadOctal = ( SIRKeycodeScancodeMask | 219u ),
	KeypadDecimal = ( SIRKeycodeScancodeMask | 220u ),
	KeypadHexadecimal = ( SIRKeycodeScancodeMask | 221u ),
	LeftCtrl = ( SIRKeycodeScancodeMask | 224u ),
	LeftShift = ( SIRKeycodeScancodeMask | 225u ),
	LeftAlt = ( SIRKeycodeScancodeMask | 226u ),
	LeftGUI = ( SIRKeycodeScancodeMask | 227u ),
	RightCtrl = ( SIRKeycodeScancodeMask | 228u ),
	RightShift = ( SIRKeycodeScancodeMask | 229u ),
	RightAlt = ( SIRKeycodeScancodeMask | 230u ),
	RightGUI = ( SIRKeycodeScancodeMask | 231u ),
	Mode = ( SIRKeycodeScancodeMask | 257u ),
	Sleep = ( SIRKeycodeScancodeMask | 258u ),
	Wake = ( SIRKeycodeScancodeMask | 259u ),
	ChannelIncrement = ( SIRKeycodeScancodeMask | 260u ),
	ChannelDecrement = ( SIRKeycodeScancodeMask | 261u ),
	MediaPlay = ( SIRKeycodeScancodeMask | 262u ),
	MediaPause = ( SIRKeycodeScancodeMask | 263u ),
	MediaRecord = ( SIRKeycodeScancodeMask | 264u ),
	MediaFastForward = ( SIRKeycodeScancodeMask | 265u ),
	MediaRewind = ( SIRKeycodeScancodeMask | 266u ),
	MediaNextTrack = ( SIRKeycodeScancodeMask | 267u ),
	MediaPreviousTrack = ( SIRKeycodeScancodeMask | 268u ),
	MediaStop = ( SIRKeycodeScancodeMask | 269u ),
	MediaEject = ( SIRKeycodeScancodeMask | 270u ),
	MediaPlayPause = ( SIRKeycodeScancodeMask | 271u ),
	MediaSelect = ( SIRKeycodeScancodeMask | 272u ),
	ACNew = ( SIRKeycodeScancodeMask | 273u ),
	ACOpen = ( SIRKeycodeScancodeMask | 274u ),
	ACClose = ( SIRKeycodeScancodeMask | 275u ),
	ACExit = ( SIRKeycodeScancodeMask | 276u ),
	ACSave = ( SIRKeycodeScancodeMask | 277u ),
	ACPrint = ( SIRKeycodeScancodeMask | 278u ),
	ACProperties = ( SIRKeycodeScancodeMask | 279u ),
	ACSearch = ( SIRKeycodeScancodeMask | 280u ),
	ACHome = ( SIRKeycodeScancodeMask | 281u ),
	ACBack = ( SIRKeycodeScancodeMask | 282u ),
	ACForward = ( SIRKeycodeScancodeMask | 283u ),
	ACStop = ( SIRKeycodeScancodeMask | 284u ),
	ACRefresh = ( SIRKeycodeScancodeMask | 285u ),
	ACBookmarks = ( SIRKeycodeScancodeMask | 286u ),
	SoftLeft = ( SIRKeycodeScancodeMask | 287u ),
	SoftRight = ( SIRKeycodeScancodeMask | 288u ),
	Call = ( SIRKeycodeScancodeMask | 289u ),
	EndCall = ( SIRKeycodeScancodeMask | 290u ),
	F1 = ( SIRKeycodeScancodeMask | 58u ),
	F2 = ( SIRKeycodeScancodeMask | 59u ),
	F3 = ( SIRKeycodeScancodeMask | 60u ),
	F4 = ( SIRKeycodeScancodeMask | 61u ),
	F5 = ( SIRKeycodeScancodeMask | 62u ),
	F6 = ( SIRKeycodeScancodeMask | 63u ),
	F7 = ( SIRKeycodeScancodeMask | 64u ),
	F8 = ( SIRKeycodeScancodeMask | 65u ),
	F9 = ( SIRKeycodeScancodeMask | 66u ),
	F10 = ( SIRKeycodeScancodeMask | 67u ),
	F11 = ( SIRKeycodeScancodeMask | 68u ),
	F12 = ( SIRKeycodeScancodeMask | 69u ),
	F13 = ( SIRKeycodeScancodeMask | 104u ),
	F14 = ( SIRKeycodeScancodeMask | 105u ),
	F15 = ( SIRKeycodeScancodeMask | 106u ),
	F16 = ( SIRKeycodeScancodeMask | 107u ),
	F17 = ( SIRKeycodeScancodeMask | 108u ),
	F18 = ( SIRKeycodeScancodeMask | 109u ),
	F19 = ( SIRKeycodeScancodeMask | 110u ),
	F20 = ( SIRKeycodeScancodeMask | 111u ),
	F21 = ( SIRKeycodeScancodeMask | 112u ),
	F22 = ( SIRKeycodeScancodeMask | 113u ),
	F23 = ( SIRKeycodeScancodeMask | 114u ),
	F24 = ( SIRKeycodeScancodeMask | 115u ),
	Keypad1 = ( SIRKeycodeScancodeMask | 89u ),
	Keypad2 = ( SIRKeycodeScancodeMask | 90u ),
	Keypad3 = ( SIRKeycodeScancodeMask | 91u ),
	Keypad4 = ( SIRKeycodeScancodeMask | 92u ),
	Keypad5 = ( SIRKeycodeScancodeMask | 93u ),
	Keypad6 = ( SIRKeycodeScancodeMask | 94u ),
	Keypad7 = ( SIRKeycodeScancodeMask | 95u ),
	Keypad8 = ( SIRKeycodeScancodeMask | 96u ),
	Keypad9 = ( SIRKeycodeScancodeMask | 97u ),
	Keypad0 = ( SIRKeycodeScancodeMask | 98u ),
	KeypadA = ( SIRKeycodeScancodeMask | 188u ),
	KeypadB = ( SIRKeycodeScancodeMask | 189u ),
	KeypadC = ( SIRKeycodeScancodeMask | 190u ),
	KeypadD = ( SIRKeycodeScancodeMask | 191u ),
	KeypadE = ( SIRKeycodeScancodeMask | 192u ),
	KeypadF = ( SIRKeycodeScancodeMask | 193u ),
	LeftTab = ( SIRKeycodeExtendedMask | 1u ),
	Level5Shift = ( SIRKeycodeExtendedMask | 2u ),
	MultiKeyCompose = ( SIRKeycodeExtendedMask | 3u ),
	LeftMeta = ( SIRKeycodeExtendedMask | 4u ),
	RightMeta = ( SIRKeycodeExtendedMask | 5u ),
	LeftHyper = ( SIRKeycodeExtendedMask | 6u ),
	RightHyper = ( SIRKeycodeExtendedMask | 7u ),
};

// Only for keys that SDL3 represents with the scancode bit set.
constexpr SIRKeycode SIRKeycodeFromScancode( SIRScancode eScancode ) noexcept
{
	return static_cast<SIRKeycode>( SIRKeycodeScancodeMask | static_cast<uint32_t>( eScancode ) );
}

// Bitmask of active modifiers; values match SDL3 Keymod.
enum class SIRKeyModifier : uint16_t
{
	None = 0x0000,
	LeftShift = 0x0001,
	RightShift = 0x0002,
	Level5 = 0x0004,
	LeftCtrl = 0x0040,
	RightCtrl = 0x0080,
	LeftAlt = 0x0100,
	RightAlt = 0x0200,
	LeftGUI = 0x0400,
	RightGUI = 0x0800,
	NumLock = 0x1000,
	CapsLock = 0x2000,
	Mode = 0x4000,
	ScrollLock = 0x8000,
	Ctrl = 0x00C0,
	Shift = 0x0003,
	Alt = 0x0300,
	GUI = 0x0C00,
};

constexpr SIRKeyModifier operator|( SIRKeyModifier eLeft, SIRKeyModifier eRight ) noexcept
{
	return static_cast<SIRKeyModifier>( static_cast<uint16_t>( eLeft ) | static_cast<uint16_t>( eRight ) );
}

constexpr SIRKeyModifier operator&( SIRKeyModifier eLeft, SIRKeyModifier eRight ) noexcept
{
	return static_cast<SIRKeyModifier>( static_cast<uint16_t>( eLeft ) & static_cast<uint16_t>( eRight ) );
}

constexpr bool SIRHasKeyModifier( SIRKeyModifier eFlags, SIRKeyModifier eFlag ) noexcept
{
	return ( static_cast<uint16_t>( eFlags ) & static_cast<uint16_t>( eFlag ) ) != 0;
}

// Mouse buttons; matches SDL3 for buttons 1..5. Higher indices are backend-defined.
enum class SIRMouseButton : uint8_t
{
	Unknown = 0,
	Left = 1,
	Middle = 2,
	Right = 3,
	X1 = 4,
	X2 = 5,
	Button6 = 6,
	Button7 = 7,
	Button8 = 8,
	Button9 = 9,
	Button10 = 10,
	Button11 = 11,
	Button12 = 12,
	Button13 = 13,
	Button14 = 14,
	Button15 = 15,
	Button16 = 16,
};

// Bit i represents mouse button i + 1. Zero/unsupported values produce zero.
constexpr uint32_t SIRMouseButtonMask( SIRMouseButton eButton ) noexcept
{
const uint32_t code = static_cast<uint32_t>( eButton );
	return ( code >= 1u && code <= 32u ) ? ( 1u << ( code - 1u ) ) : 0u;
}

// Scroll direction; SDL3-compatible values.
enum class SIRMouseWheelDirection : uint32_t
{
	Normal = 0,
	Flipped = 1,
};

// Mapped gamepad buttons; positions match SDL3 rather than controller-specific labels.
enum class SIRGamepadButton : uint8_t
{
	South = 0,
	East = 1,
	West = 2,
	North = 3,
	Back = 4,
	Guide = 5,
	Start = 6,
	LeftStick = 7,
	RightStick = 8,
	LeftShoulder = 9,
	RightShoulder = 10,
	DPadUp = 11,
	DPadDown = 12,
	DPadLeft = 13,
	DPadRight = 14,
	Misc1 = 15,
	RightPaddle1 = 16,
	LeftPaddle1 = 17,
	RightPaddle2 = 18,
	LeftPaddle2 = 19,
	Touchpad = 20,
	Misc2 = 21,
	Misc3 = 22,
	Misc4 = 23,
	Misc5 = 24,
	Misc6 = 25,
	Count = 26,
	Invalid = 255,
};

// Mapped gamepad axes. Sticks -32768..32767, triggers 0..32767.
enum class SIRGamepadAxis : uint8_t
{
	LeftX = 0,
	LeftY = 1,
	RightX = 2,
	RightY = 3,
	LeftTrigger = 4,
	RightTrigger = 5,
	Count = 6,
	Invalid = 255,
};

// Controller profile table indices. These are NOT SIRGamepadButton event values.
// Profile lookups use this ordering; use the conversion below when publishing
// button events. Triggers are axes in the canonical gamepad event representation.
enum class SIRInputControllerControl : uint16_t
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
};

// Controller profile axes use the same order as SIRGamepadAxis.
// Preserve the profile enum for existing profile and evdev consumers.
enum class SIRInputAxisCode : uint16_t
{
	SIR_INPUT_AXIS_LX = 0,
	SIR_INPUT_AXIS_LY = 1,
	SIR_INPUT_AXIS_RX = 2,
	SIR_INPUT_AXIS_RY = 3,
	SIR_INPUT_AXIS_LT = 4,
	SIR_INPUT_AXIS_RT = 5,
	SIR_INPUT_AXIS_COUNT = 6
};

// Never cast a profile control directly to SIRGamepadButton: the enums differ.
constexpr SIRGamepadButton SIRGamepadButtonFromProfileControl( SIRInputControllerControl eControl ) noexcept
{
	switch ( eControl ) {
	case SIRInputControllerControl::SIR_INPUT_PAD_SOUTH: return SIRGamepadButton::South;
	case SIRInputControllerControl::SIR_INPUT_PAD_EAST: return SIRGamepadButton::East;
	case SIRInputControllerControl::SIR_INPUT_PAD_WEST: return SIRGamepadButton::West;
	case SIRInputControllerControl::SIR_INPUT_PAD_NORTH: return SIRGamepadButton::North;
	case SIRInputControllerControl::SIR_INPUT_PAD_LB: return SIRGamepadButton::LeftShoulder;
	case SIRInputControllerControl::SIR_INPUT_PAD_RB: return SIRGamepadButton::RightShoulder;
	case SIRInputControllerControl::SIR_INPUT_PAD_BACK: return SIRGamepadButton::Back;
	case SIRInputControllerControl::SIR_INPUT_PAD_START: return SIRGamepadButton::Start;
	case SIRInputControllerControl::SIR_INPUT_PAD_GUIDE: return SIRGamepadButton::Guide;
	case SIRInputControllerControl::SIR_INPUT_PAD_L3: return SIRGamepadButton::LeftStick;
	case SIRInputControllerControl::SIR_INPUT_PAD_R3: return SIRGamepadButton::RightStick;
	case SIRInputControllerControl::SIR_INPUT_PAD_DPAD_UP: return SIRGamepadButton::DPadUp;
	case SIRInputControllerControl::SIR_INPUT_PAD_DPAD_DOWN: return SIRGamepadButton::DPadDown;
	case SIRInputControllerControl::SIR_INPUT_PAD_DPAD_LEFT: return SIRGamepadButton::DPadLeft;
	case SIRInputControllerControl::SIR_INPUT_PAD_DPAD_RIGHT: return SIRGamepadButton::DPadRight;
	default: return SIRGamepadButton::Invalid;
	}
}

// Digital trigger profile controls map to trigger AXES, not gamepad buttons.
constexpr SIRGamepadAxis SIRGamepadTriggerAxisFromProfileControl( SIRInputControllerControl eControl ) noexcept
{
	switch ( eControl ) {
	case SIRInputControllerControl::SIR_INPUT_PAD_LT_BUTTON: return SIRGamepadAxis::LeftTrigger;
	case SIRInputControllerControl::SIR_INPUT_PAD_RT_BUTTON: return SIRGamepadAxis::RightTrigger;
	default: return SIRGamepadAxis::Invalid;
	}
}

constexpr SIRGamepadAxis SIRGamepadAxisFromProfileCode( SIRInputAxisCode eAxis ) noexcept
{
	const uint16_t nCode = static_cast<uint16_t>( eAxis );
	return nCode < static_cast<uint16_t>( SIRGamepadAxis::Count )
	           ? static_cast<SIRGamepadAxis>( nCode )
	           : SIRGamepadAxis::Invalid;
}

static_assert( SIRGamepadButtonFromProfileControl( SIRInputControllerControl::SIR_INPUT_PAD_LB ) == SIRGamepadButton::LeftShoulder,
	"Profile shoulder mapping must not use profile ordinal as event code" );
static_assert( SIRGamepadButtonFromProfileControl( SIRInputControllerControl::SIR_INPUT_PAD_LT_BUTTON ) == SIRGamepadButton::Invalid,
	"Digital triggers are reported as axes" );
static_assert(
	SIRGamepadTriggerAxisFromProfileControl( SIRInputControllerControl::SIR_INPUT_PAD_RT_BUTTON ) == SIRGamepadAxis::RightTrigger,
	"Right trigger profile mapping changed" );
static_assert( static_cast<uint16_t>( SIRInputAxisCode::SIR_INPUT_AXIS_COUNT ) == static_cast<uint16_t>( SIRGamepadAxis::Count ),
	"Profile and canonical gamepad axis orders diverged" );

// Mapped gamepad capacitive sensors (SDL3 3.6+).
enum class SIRGamepadCapSense : uint8_t
{
	LeftStick = 0,
	RightStick = 1,
	LeftGrip = 2,
	RightGrip = 3,
	Count = 4,
	Invalid = 255,
};

// Display labels for face buttons; these are not positional button IDs.
enum class SIRGamepadButtonLabel : uint8_t
{
	Unknown = 0,
	A = 1,
	B = 2,
	X = 3,
	Y = 4,
	Cross = 5,
	Circle = 6,
	Square = 7,
	Triangle = 8,
};

// Generic joystick hat direction bitfield, SDL3-compatible.
enum class SIRJoystickHat : uint8_t
{
	Centered = 0,
	Up = 0x01,
	Right = 0x02,
	Down = 0x04,
	Left = 0x08,
	RightUp = 0x03,
	RightDown = 0x06,
	LeftUp = 0x09,
	LeftDown = 0x0C,
};

// Touch device classification, SDL3-compatible.
enum class SIRTouchDeviceType : int32_t
{
	Invalid = -1,
	Direct = 0,
	IndirectAbsolute = 1,
	IndirectRelative = 2,
};

// Touch contact actions are represented by SIREventType::FingerDown,
// FingerMotion, FingerUp, and FingerCanceled. No touch button index exists.

static_assert( sizeof( SIRScancode ) == 4 && sizeof( SIRKeycode ) == 4, "Keyboard codes must be 32-bit" );
static_assert( sizeof( SIRKeyModifier ) == 2, "Modifier mask must be 16-bit" );
static_assert( sizeof( SIRMouseButton ) == 1 && sizeof( SIRGamepadButton ) == 1, "Button codes must be 8-bit" );
static_assert( sizeof( SIRGamepadAxis ) == 1 && sizeof( SIRJoystickHat ) == 1, "Axis/hat codes must be 8-bit" );
static_assert( sizeof( SIRMouseWheelDirection ) == 4, "Wheel direction must be 32-bit" );
static_assert( std::is_trivially_copyable<SIRScancode>::value, "Scancodes must be trivially copyable" );
static_assert( static_cast<uint32_t>( SIRScancode::A ) == 4u, "Physical key values changed" );
static_assert( static_cast<uint32_t>( SIRKeycode::A ) == 0x61u, "Logical key values changed" );
static_assert( static_cast<uint32_t>( SIRKeycode::F1 ) == 0x4000003Au, "Special key values changed" );
static_assert( static_cast<uint8_t>( SIRGamepadButton::Touchpad ) == 20u, "Gamepad mapping changed" );
static_assert( SIRMouseButtonMask( SIRMouseButton::Left ) == 1u, "Mouse mask changed" );
static_assert( SIRMouseButtonMask( SIRMouseButton::X1 ) == 8u, "Mouse mask changed" );
static_assert( SIRMouseButtonMask( SIRMouseButton::Unknown ) == 0u, "Invalid mouse button mask must be zero" );

#endif // __SIR_INPUT_CODES_H__
