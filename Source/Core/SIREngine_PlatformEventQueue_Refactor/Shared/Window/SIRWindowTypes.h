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

#ifndef __SIR_WINDOW_TYPES_H__
#define __SIR_WINDOW_TYPES_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Strings/SIRFixedString.h>

enum class SIRWindowPlatform : uint8_t
{
	Win32,
	X11,
	Wayland,
	Headless
};

enum class SIRWindowResult : uint8_t
{
	Ok,
	InvalidArgument,
	InvalidHandle,
	WrongThread,
	OutOfCapacity,
	EventsLost,
	NotReady,
	Unsupported,
	Busy,
	NativeFailure,
	Stopped
};

// No native window driver may inspect an unbounded caller-owned title.
// nTitleBytes = 0 accepts a bounded NUL-terminated UTF-8 title. An explicit
// byte count permits a nonterminated caller buffer; WindowSystem copies and
// terminates it before crossing the internal native driver boundary.
constexpr const uint32_t MAX_NATIVE_WINDOW_NAME_LENGTH = 64;
constexpr const uint32_t SIR_WINDOW_MAX_WINDOWS = 8;
constexpr const uint32_t SIR_WINDOW_MAX_LEASES = 32;
constexpr const uint32_t SIR_WINDOW_EVENT_CAPACITY = 64;

struct SIRWindowHandle
{
	uint32_t nSlot;
	uint32_t nGeneration;
	uint64_t nSystemEpoch;
};

struct SIRNativeWindowConfiguration
{
	const SIRFixedString<MAX_NATIVE_WINDOW_NAME_LENGTH> szName;
	uint32_t nPositionX;
	uint32_t nPositionY;
	uint32_t nWidth;
	uint32_t nHeight;
	float fDpi;
};

#endif // __SIR_WINDOW_TYPES_H__
