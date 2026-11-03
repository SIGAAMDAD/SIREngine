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

#ifndef __SIRENGINE_SIRCONTAINERRESULT_H__
#define __SIRENGINE_SIRCONTAINERRESULT_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

typedef enum sirContainerResult_e : uint8_t
{
	SIR_CONTAINER_OK = 0,
	SIR_CONTAINER_OUT_OF_MEMORY = 1,
	SIR_CONTAINER_CAPACITY_EXCEEDED = 2,
	SIR_CONTAINER_SIZE_OVERFLOW = 3,
	SIR_CONTAINER_EMPTY = 4,
	SIR_CONTAINER_FULL = 5,
	SIR_CONTAINER_INVALID_HANDLE = 6,
	SIR_CONTAINER_CLOSED = 7,
	SIR_CONTAINER_BUSY = 8,
	SIR_CONTAINER_INVALID_ARGUMENT = 9
} sirContainerResult_t;

typedef struct sirContainerAllocator_s
{
	void *pContext;
	void *( *pfnAllocate )( void *pContext, size_t nBytes, size_t nAlignment );
	void ( *pfnDeallocate )( void *pContext, void *pMemory, size_t nBytes, size_t nAlignment );
	uint32_t nDomainId;
	uint32_t nOwnerTag;
} sirContainerAllocator_t;

typedef struct sirContainerFootprint_s
{
	uint64_t nInlineBytes;
	uint64_t nLiveDomainBytes;
	uint64_t nRetainedDomainBytes;
	uint64_t nExternalBorrowedBytes;
	uint64_t nLogicalPayloadBytes;
} sirContainerFootprint_t;

#endif // __SIRENGINE_SIRCONTAINERRESULT_H__
