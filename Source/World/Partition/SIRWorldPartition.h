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

#ifndef __SIRENGINE_SIRWORLDPARTITION_H__
#define __SIRENGINE_SIRWORLDPARTITION_H__

#include <SIRPCH.hpp>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#include <Streaming/SIRStreamingTypes.h>

typedef enum sirWorldPartitionResult_e : uint8_t
{
	SIR_WORLD_PARTITION_OK = 0,
	SIR_WORLD_PARTITION_INVALID_ARGUMENT = 1,
	SIR_WORLD_PARTITION_NOT_READY = 2,
	SIR_WORLD_PARTITION_INVALID_DIRECTORY = 3,
	SIR_WORLD_PARTITION_OUT_OF_CAPACITY = 4
} sirWorldPartitionResult_t;

typedef enum sirWorldCellFlags_e : uint32_t
{
	SIR_WORLD_CELL_FLAG_NONE = 0,
	SIR_WORLD_CELL_FLAG_ALWAYS_LOADED = 1U << 0
} sirWorldCellFlags_t;

// This is an in-memory, not serialized, form of a cooked cell directory.
// Cells must be lexicographically sorted by X, Y, Z at mount time.
typedef struct sirWorldCellDescriptor_s
{
	uint64_t nCellId;
	uint64_t nRequiredLayers;
	int32_t iGridX;
	int32_t iGridY;
	int32_t iGridZ;
	uint32_t nFlags;
} sirWorldCellDescriptor_t;

typedef struct sirWorldPartitionConfig_s
{
	uint64_t nWorldId;
	uint64_t nConsumerId;
	uint32_t nManifestGeneration;
	uint32_t nCellCount;
	bool bHeightStratified;
} sirWorldPartitionConfig_t;

// Source position and radius are in cooked grid-cell units. World coordinates
// are converted at the authoring/runtime boundary, not in this hot path.
typedef struct sirWorldStreamingSource_s
{
	uint64_t nSourceId;
	uint64_t nDeadlineTick;
	int32_t iGridX;
	int32_t iGridY;
	int32_t iGridZ;
	uint32_t nGeneration;
	uint32_t nRadiusCells;
	uint32_t nHeightRadiusCells;
	sirStreamingTarget_t eTarget;
	uint8_t nPriority;
	uint16_t nReserved;
} sirWorldStreamingSource_t;

struct sirWorldCellEntry_s;

// Single-owner spatial demand planner. Owns only copied directory metadata and
// bounded per-evaluation scratch, never GRDG payloads or resource handles.
class SIRWorldPartition
{
private:
	sirWorldCellEntry_s *m_pCells;
	uint64_t *m_pszCellIds;
	sirWorldPartitionConfig_t m_Config;
	uint32_t m_nEvaluationStamp;
	bool m_bInitialized;

	uint32_t lowerBoundX( int64_t iGridX ) const noexcept;
	void accumulate( uint32_t iCell, sirStreamingTarget_t eTarget, uint8_t nPriority, uint64_t nDeadlineTick ) noexcept;

public:
	SIRWorldPartition( void ) noexcept;
	SIRWorldPartition( const SIRWorldPartition& ) = delete;
	SIRWorldPartition& operator=( const SIRWorldPartition& ) = delete;

	static uint64_t requiredBytes( uint32_t nCellCount ) noexcept;
	sirWorldPartitionResult_t initialize( void *pStorage, uint64_t nStorageBytes, const sirWorldPartitionConfig_t& config,
	                                      const sirWorldCellDescriptor_t *pCells ) noexcept;
	void shutdown( void ) noexcept;
	bool hasCell( uint64_t nCellId ) const noexcept;
	uint32_t getCellCount( void ) const noexcept;

	// Atomic output: when capacity is insufficient, returns required count and
	// leaves pDemands unchanged. Layer masks are AND-required (zero = global).
	// All sources are merged per cell into one partition-owned demand per epoch.
	sirWorldPartitionResult_t gather( const sirWorldStreamingSource_t *pSources, uint32_t nSourceCount, uint64_t nEnabledLayers,
	                                  uint32_t nRequestEpoch, sirStreamingDemand_t *pDemands, uint32_t nDemandCapacity,
	                                  uint32_t *pRequiredCount ) noexcept;
};

#endif // __SIRENGINE_SIRWORLDPARTITION_H__
