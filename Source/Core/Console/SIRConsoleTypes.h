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

#ifndef __SIR_CONSOLE_TYPES_H__
#define __SIR_CONSOLE_TYPES_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRString.h>

enum class SIRConsoleResult : uint8_t
{
	SIR_CONSOLE_OK = 0,
	SIR_CONSOLE_NOT_READY,
	SIR_CONSOLE_INVALID,
	SIR_CONSOLE_NOT_FOUND,
	SIR_CONSOLE_STALE_HANDLE,
	SIR_CONSOLE_WRONG_THREAD,
	SIR_CONSOLE_ACCESS_DENIED,
	SIR_CONSOLE_TYPE_MISMATCH,
	SIR_CONSOLE_OUT_OF_RANGE,
	SIR_CONSOLE_CAPACITY,
	SIR_CONSOLE_QUEUE_FULL,
	SIR_CONSOLE_BUSY,
	SIR_CONSOLE_IO_ERROR,
	SIR_CONSOLE_UNSUPPORTED
};

enum class SIRCVarSource : uint8_t
{
	SIR_CVAR_ENGINE_DEFAULT = 0,
	SIR_CVAR_DEVICE_PROFILE,
	SIR_CVAR_PROJECT_PROFILE,
	SIR_CVAR_USER_ARCHIVE,
	SIR_CVAR_COMMAND_LINE,
	SIR_CVAR_SESSION_CONSOLE,
	SIR_CVAR_EDITOR_PREVIEW,
	SIR_CVAR_SERVER_AUTHORITY
};

constexpr uint32_t SIR_CVAR_FLAG_ARCHIVE = 1U << 0;
constexpr uint32_t SIR_CVAR_FLAG_READ_ONLY = 1U << 1;
constexpr uint32_t SIR_CVAR_FLAG_INIT_ONLY = 1U << 2;
constexpr uint32_t SIR_CVAR_FLAG_LATCHED = 1U << 3;
constexpr uint32_t SIR_CVAR_FLAG_DEVELOPMENT = 1U << 4;
constexpr uint32_t SIR_CVAR_FLAG_CHEAT = 1U << 5;
constexpr uint32_t SIR_CVAR_FLAG_REPLICATED = 1U << 6;
constexpr uint32_t SIR_CVAR_FLAG_HIDDEN = 1U << 7;
constexpr uint32_t SIR_CVAR_FLAG_SENSITIVE = 1U << 8;
constexpr uint32_t SIR_CVAR_FLAG_MODULE_OWNED = 1U << 9;
constexpr uint32_t SIR_CVAR_FLAG_TRANSIENT = 1U << 10;

typedef struct sirCVarHandle_s
{
	uint32_t nSlot{};
	uint32_t nGeneration{};
} sirCVarHandle_t;

typedef struct sirCommandHandle_s
{
	uint32_t nSlot{};
	uint32_t nGeneration{};
} sirCommandHandle_t;

#endif // __SIR_CONSOLE_TYPES_H__
