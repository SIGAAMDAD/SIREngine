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

#ifndef __SIRENGINE_SIRSTREAMINGTYPES_H__
#define __SIRENGINE_SIRSTREAMINGTYPES_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

// These identify demand units, not GRDG resource IDs or physical allocations.
// The future GRDG adapter resolves a unit through the cooked manifest.
typedef enum sirStreamingUnitKind_e : uint8_t
{
	SIR_STREAMING_UNIT_WORLD_CELL = 0,
	SIR_STREAMING_UNIT_TERRAIN_REGION = 1,
	SIR_STREAMING_UNIT_HLOD = 2,
	SIR_STREAMING_UNIT_VIRTUAL_TEXTURE = 3,
	SIR_STREAMING_UNIT_RESOURCE_BUNDLE = 4,
	SIR_STREAMING_UNIT_NAVIGATION = 5,
	SIR_STREAMING_UNIT_AUDIO = 6,
	SIR_STREAMING_UNIT_ENTITY = 7,
	SIR_STREAMING_UNIT_COUNT = 8
} sirStreamingUnitKind_t;

typedef enum sirStreamingTarget_e : uint8_t
{
	SIR_STREAMING_TARGET_PREPARED = 1,
	SIR_STREAMING_TARGET_ACTIVE = 2
} sirStreamingTarget_t;

typedef enum sirStreamingStage_e : uint8_t
{
	SIR_STREAMING_STAGE_EMPTY = 0,
	SIR_STREAMING_STAGE_QUEUED = 1,
	SIR_STREAMING_STAGE_PREPARING = 2,
	SIR_STREAMING_STAGE_PREPARED = 3,
	SIR_STREAMING_STAGE_ACTIVE = 4,
	SIR_STREAMING_STAGE_RETIRING = 5,
	SIR_STREAMING_STAGE_FAILED = 6
} sirStreamingStage_t;

typedef enum sirStreamingResult_e : uint8_t
{
	SIR_STREAMING_OK = 0,
	SIR_STREAMING_INVALID_ARGUMENT = 1,
	SIR_STREAMING_NOT_READY = 2,
	SIR_STREAMING_OUT_OF_CAPACITY = 3,
	SIR_STREAMING_DUPLICATE = 4,
	SIR_STREAMING_STALE_TICKET = 5,
	SIR_STREAMING_INVALID_TRANSITION = 6,
	SIR_STREAMING_BUSY = 7
} sirStreamingResult_t;

typedef struct sirStreamingUnitKey_s
{
	uint64_t nContainerId;
	uint64_t nUnitId;
	uint32_t nManifestGeneration;
	uint16_t nVariant;
	sirStreamingUnitKind_t eKind;
	uint8_t nReserved;
} sirStreamingUnitKey_t;

typedef struct sirStreamingDemand_s
{
	sirStreamingUnitKey_t keyUnit;
	uint64_t nConsumerId;
	uint64_t nDeadlineTick;
	uint32_t nConsumerGeneration;
	uint32_t nRequestEpoch;
	sirStreamingTarget_t eTarget;
	uint8_t nPriority;
	uint16_t nReserved;
} sirStreamingDemand_t;

typedef struct sirStreamingTicket_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
} sirStreamingTicket_t;

typedef struct sirStreamingSnapshot_s
{
	sirStreamingDemand_t demand;
	sirStreamingStage_t eStage;
	bool bCancellationRequested;
} sirStreamingSnapshot_t;

typedef struct sirStreamingStats_s
{
	uint32_t nCapacity;
	uint32_t nOccupied;
	uint32_t nQueued;
	uint32_t nPreparing;
	uint32_t nPrepared;
	uint32_t nActive;
	uint32_t nRetiring;
	uint32_t nFailed;
	uint32_t nCancellationPending;
} sirStreamingStats_t;

static_assert( sizeof( sirStreamingTicket_t ) == 8, "streaming tickets require slot and generation" );

#endif // __SIRENGINE_SIRSTREAMINGTYPES_H__
