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

#ifndef __SIRENGINE_SIRREGISTRATIONTYPES_H__
#define __SIRENGINE_SIRREGISTRATIONTYPES_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

constexpr uint32_t SIR_REGISTRATION_CATALOG_MAGIC = 0x43524953u; // "SIRC", little endian
constexpr uint16_t SIR_REGISTRATION_CATALOG_VERSION = 1;
constexpr uint32_t SIR_REGISTRATION_CATALOG_BYTE_ORDER = 0x01020304u;
constexpr uint32_t SIR_REGISTRATION_CATALOG_HEADER_BYTES = 120;
constexpr uint32_t SIR_REGISTRATION_MAX_NAME_BYTES = 96;
constexpr uint32_t SIR_REGISTRATION_MAX_RECORDS = 65536;
constexpr uint32_t SIR_REGISTRATION_NAME_STRIDE = 8;
constexpr uint32_t SIR_REGISTRATION_COMMAND_STRIDE = 32;
constexpr uint32_t SIR_REGISTRATION_CVAR_STRIDE = 32;
constexpr uint32_t SIR_REGISTRATION_SCALABILITY_STRIDE = 16;
constexpr uint64_t SIR_REGISTRATION_HASH_BASIS = 14695981039346656037ULL;
constexpr uint64_t SIR_REGISTRATION_HASH_PRIME = 1099511628211ULL;

typedef enum sirRegistrationResult_e : uint8_t
{
	SIR_REGISTRATION_OK = 0,
	SIR_REGISTRATION_INVALID_ARGUMENT,
	SIR_REGISTRATION_INVALID_NAME,
	SIR_REGISTRATION_BAD_MAGIC,
	SIR_REGISTRATION_BAD_VERSION,
	SIR_REGISTRATION_BAD_BYTE_ORDER,
	SIR_REGISTRATION_BAD_BOUNDS,
	SIR_REGISTRATION_BAD_ALIGNMENT,
	SIR_REGISTRATION_OVERLAPPING_SECTIONS,
	SIR_REGISTRATION_BAD_HASH,
	SIR_REGISTRATION_BAD_SCHEMA,
	SIR_REGISTRATION_DUPLICATE_NAME,
	SIR_REGISTRATION_UNBOUND_CALLBACK,
	SIR_REGISTRATION_NOT_INITIALIZED,
	SIR_REGISTRATION_STALE_HANDLE,
	SIR_REGISTRATION_TYPE_MISMATCH,
	SIR_REGISTRATION_WRONG_THREAD,
	SIR_REGISTRATION_DENIED,
	SIR_REGISTRATION_NO_CHANGE,
	SIR_REGISTRATION_QUEUE_FULL,
	SIR_REGISTRATION_CAPACITY_EXCEEDED,
	SIR_REGISTRATION_OUT_OF_MEMORY,
	SIR_REGISTRATION_CANCELLED,
	SIR_REGISTRATION_SHUTTING_DOWN,
	SIR_REGISTRATION_NOT_FOUND
} sirRegistrationResult_t;

typedef enum sirCVarType_e : uint8_t
{
	SIR_CVAR_BOOL = 0,
	SIR_CVAR_I32,
	SIR_CVAR_U32,
	SIR_CVAR_F32,
	SIR_CVAR_I64,
	SIR_CVAR_U64,
	SIR_CVAR_F64,
	SIR_CVAR_ENUM_U32,
	SIR_CVAR_UTF8_STRING,
	SIR_CVAR_TYPE_COUNT
} sirCVarType_t;

typedef enum sirCVarSource_e : uint8_t
{
	SIR_CVAR_SOURCE_ENGINE_DEFAULT = 0,
	SIR_CVAR_SOURCE_DEVICE_PROFILE,
	SIR_CVAR_SOURCE_PROJECT_PROFILE,
	SIR_CVAR_SOURCE_USER_ARCHIVE,
	SIR_CVAR_SOURCE_COMMAND_LINE,
	SIR_CVAR_SOURCE_SESSION_CONSOLE,
	SIR_CVAR_SOURCE_EDITOR_PREVIEW,
	SIR_CVAR_SOURCE_SERVER_AUTHORITY,
	SIR_CVAR_SOURCE_COUNT
} sirCVarSource_t;

typedef enum sirCVarApplyPhase_e : uint8_t
{
	SIR_CVAR_APPLY_NEXT_SAFE_POINT = 0,
	SIR_CVAR_APPLY_RESTART
} sirCVarApplyPhase_t;

typedef enum sirCVarFlag_e : uint32_t
{
	SIR_CVAR_FLAG_ARCHIVE = 1u << 0,
	SIR_CVAR_FLAG_READ_ONLY = 1u << 1,
	SIR_CVAR_FLAG_INIT_ONLY = 1u << 2,
	SIR_CVAR_FLAG_LATCHED = 1u << 3,
	SIR_CVAR_FLAG_DEVELOPMENT = 1u << 4,
	SIR_CVAR_FLAG_CHEAT = 1u << 5,
	SIR_CVAR_FLAG_REPLICATED = 1u << 6,
	SIR_CVAR_FLAG_HIDDEN = 1u << 7,
	SIR_CVAR_FLAG_SENSITIVE = 1u << 8,
	SIR_CVAR_FLAG_MODULE_OWNED = 1u << 9,
	SIR_CVAR_FLAG_TRANSIENT = 1u << 10
} sirCVarFlag_t;

typedef enum sirConsoleCapability_e : uint32_t
{
	SIR_CONSOLE_CAP_READ = 1u << 0,
	SIR_CONSOLE_CAP_EXECUTE = 1u << 1,
	SIR_CONSOLE_CAP_WRITE = 1u << 2,
	SIR_CONSOLE_CAP_DEVELOPMENT = 1u << 3,
	SIR_CONSOLE_CAP_CHEAT = 1u << 4,
	SIR_CONSOLE_CAP_INIT = 1u << 5,
	SIR_CONSOLE_CAP_SERVER = 1u << 6,
	SIR_CONSOLE_CAP_SENSITIVE = 1u << 7,
	SIR_CONSOLE_CAP_ADMIN = 1u << 8
} sirConsoleCapability_t;

typedef struct sirCVarHandle_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
} sirCVarHandle_t;

typedef struct sirCommandHandle_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
} sirCommandHandle_t;

typedef struct sirRegistrationSection_s
{
	uint64_t nOffset;
	uint32_t nCount;
	uint32_t nStride;
} sirRegistrationSection_t;

static_assert( sizeof( sirCVarHandle_t ) == 8, "CVar handle ABI must be eight bytes" );
static_assert( sizeof( sirCommandHandle_t ) == 8, "Command handle ABI must be eight bytes" );
static_assert( sizeof( sirRegistrationSection_t ) == 16, "Catalog section header must be sixteen bytes" );

uint8_t sirCVarSourcePriority( sirCVarSource_t eSource ) noexcept;
bool sirValidateCVarFlags( uint32_t nFlags ) noexcept;
bool sirValidateConsoleName( const char *pszName, size_t nLength ) noexcept;
bool sirNormalizeConsoleName( const char *pszName, size_t nLength, char *pszOutput, size_t nCapacity ) noexcept;
sirRegistrationResult_t sirValidateRegistrationCatalog( const uint8_t *pBytes, size_t nBytes, uint64_t nExpectedStringHash ) noexcept;

#endif // __SIRENGINE_SIRREGISTRATIONTYPES_H__
