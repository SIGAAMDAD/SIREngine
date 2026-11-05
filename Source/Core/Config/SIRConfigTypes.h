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

#ifndef __SIRENGINE_SIRCONFIGTYPES_H__
#define __SIRENGINE_SIRCONFIGTYPES_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRContainerResult.h>
#include <Core/Containers/SIRString.h>

typedef enum sirConfigResult_e : uint8_t
{
	SIR_CONFIG_OK = 0,
	SIR_CONFIG_END,
	SIR_CONFIG_NOT_FOUND,
	SIR_CONFIG_INVALID_ARGUMENT,
	SIR_CONFIG_INVALID_ENCODING,
	SIR_CONFIG_SYNTAX_ERROR,
	SIR_CONFIG_TYPE_MISMATCH,
	SIR_CONFIG_DUPLICATE_KEY,
	SIR_CONFIG_INVALID_HANDLE,
	SIR_CONFIG_LIMIT_EXCEEDED,
	SIR_CONFIG_OUT_OF_MEMORY,
	SIR_CONFIG_IO_ERROR,
	SIR_CONFIG_RESOLUTION_ERROR,
	SIR_CONFIG_CANCELLED,
	SIR_CONFIG_UNSUPPORTED_FEATURE,
	SIR_CONFIG_UNSUPPORTED_VERSION,
	SIR_CONFIG_INTERNAL_ERROR
} sirConfigResult_t;

typedef struct sirConfigInput_s
{
	const uint8_t *pBytes;
	uint64_t nBytes;
	sirStringView_t sourceName;
} sirConfigInput_t;

typedef struct sirConfigSource_s
{
	void *pContext;
	sirConfigResult_t ( *pfnRead )( void *, uint8_t *, uint64_t, uint64_t * );
} sirConfigSource_t;

typedef struct sirConfigSink_s
{
	void *pContext;
	sirConfigResult_t ( *pfnWrite )( void *, const uint8_t *, uint64_t );
} sirConfigSink_t;

typedef struct sirConfigLimits_s
{
	uint64_t nMaxInputBytes;
	uint64_t nMaxDocumentLiveBytes;
	uint64_t nMaxScratchLiveBytes;
	uint64_t nMaxOutputBytes;
	uint64_t nMaxIncludeBytes;
	uint64_t nMaxExpandedBytes;
	uint32_t nMaxNodes;
	uint32_t nMaxDepth;
	uint32_t nMaxScalarBytes;
	uint32_t nMaxIncludeDepth;
	uint32_t nMaxAliases;
	uint32_t nMaxRowBytes;
	uint32_t nMaxColumns;
	uint32_t nMaxRows;
} sirConfigLimits_t;

typedef struct sirConfigParseContext_s
{
	const sirContainerAllocator_t *pDocumentAllocator;
	const sirContainerAllocator_t *pScratchAllocator;
	sirConfigLimits_t limits;
} sirConfigParseContext_t;

typedef struct sirConfigDiagnostic_s
{
	sirConfigResult_t eResult;
	uint64_t nByteOffset;
	uint32_t nLine;
	uint32_t nColumn;
	uint32_t nSourceId;
	uint32_t nFormatErrorCode;
} sirConfigDiagnostic_t;

#endif // __SIRENGINE_SIRCONFIGTYPES_H__
