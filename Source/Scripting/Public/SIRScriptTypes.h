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

#ifndef __SIRENGINE_SIRSCRIPTTYPES_H__
#define __SIRENGINE_SIRSCRIPTTYPES_H__

#include <SIRPCH.hpp>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#include <stdint.h>

constexpr uint32_t SIR_SCRIPT_BACKEND_ABI_REVISION = 3;
constexpr uint32_t SIR_SCRIPT_PROGRAM_SOURCE_ABI_REVISION = 1;

typedef enum sirScriptResult_e : uint8_t
{
	SIR_SCRIPT_OK = 0,
	SIR_SCRIPT_INVALID_ARGUMENT = 1,
	SIR_SCRIPT_NOT_FOUND = 2,
	SIR_SCRIPT_DUPLICATE = 3,
	SIR_SCRIPT_CAPACITY_EXCEEDED = 4,
	SIR_SCRIPT_WRONG_STATE = 5,
	SIR_SCRIPT_INVALID_HANDLE = 6,
	SIR_SCRIPT_TYPE_MISMATCH = 7,
	SIR_SCRIPT_BUSY = 8,
	SIR_SCRIPT_BACKEND_FAILURE = 9
} sirScriptResult_t;

typedef struct sirScriptProgramHandle_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
} sirScriptProgramHandle_t;

typedef struct sirScriptInstanceHandle_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
} sirScriptInstanceHandle_t;

typedef struct sirScriptSubscriptionHandle_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
} sirScriptSubscriptionHandle_t;

// All buffers in the host ABI have explicit byte length and alignment.
// A pointer must satisfy the declared alignment when the byte count is nonzero.
typedef struct sirScriptReadFrame_s
{
	const void *pData;
	uint32_t nBytes;
	uint32_t nAlignment;
} sirScriptReadFrame_t;

typedef struct sirScriptWriteFrame_s
{
	void *pData;
	uint32_t nBytes;
	uint32_t nAlignment;
} sirScriptWriteFrame_t;

// Borrowed immutable cooked program. A successful source acquisition pins the
// view until the last backend unload finishes. The format ID identifies the
// cooked representation; the backend decides whether it supports that format.
typedef struct sirScriptProgramView_s
{
	uint64_t nProgramId;
	uint64_t nRevision;
	uint64_t nFormatId;
	const void *pData;
	uint64_t nBytes;
	uint32_t nAlignment;
} sirScriptProgramView_t;

// Engine-owned resource/GRDG adapter. Source callbacks are invoked on the
// script owner thread and may not reenter runtime lifecycle operations.
// A successful acquire must return a nonzero lease token. Its bytes and
// metadata remain valid until the matching release callback returns.
typedef struct sirScriptProgramSourceAPI_s
{
	uint32_t nABIRevision;
	void *pUser;
	sirScriptResult_t ( *pAcquire )( void *, uint64_t, uint64_t, uint64_t, sirScriptProgramView_t *, uint64_t * ) noexcept;
	void ( *pRelease )( void *, uint64_t ) noexcept;
} sirScriptProgramSourceAPI_t;

// Backend-private tokens are opaque to the shared host. No VM pointer or
// language-specific object crosses this interface.
typedef struct sirScriptBackendAPI_s
{
	uint32_t nABIRevision;
	void *pUser;
	sirScriptResult_t ( *pStart )( void * ) noexcept;
	void ( *pStop )( void * ) noexcept;
	sirScriptResult_t ( *pLoadProgram )( void *, sirScriptProgramView_t, uint64_t * ) noexcept;
	void ( *pUnloadProgram )( void *, uint64_t ) noexcept;
	sirScriptResult_t ( *pCreateInstance )( void *, uint64_t, uint64_t, uint64_t * ) noexcept;
	sirScriptResult_t ( *pInitializeInstance )( void *, uint64_t ) noexcept;
	void ( *pShutdownInstance )( void *, uint64_t ) noexcept;
	void ( *pDestroyInstance )( void *, uint64_t ) noexcept;
	sirScriptResult_t ( *pInvoke )( void *, uint64_t, uint64_t, sirScriptReadFrame_t, sirScriptWriteFrame_t ) noexcept;
} sirScriptBackendAPI_t;

#endif // __SIRENGINE_SIRSCRIPTTYPES_H__
