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

#include <World/Partition/SIRWorldPartition.h>

#include <limits.h>
#include <string.h>

static sirWorldStreamingSource_t makeSource( uint64_t nId, int32_t iX, int32_t iY, uint32_t nRadius, sirStreamingTarget_t eTarget,
                                             uint8_t nPriority, uint64_t nDeadline ) noexcept
{
	sirWorldStreamingSource_t source = {};
	source.nSourceId = nId;
	source.nGeneration = 1;
	source.iGridX = iX;
	source.iGridY = iY;
	source.nRadiusCells = nRadius;
	source.eTarget = eTarget;
	source.nPriority = nPriority;
	source.nDeadlineTick = nDeadline;
	return source;
}

static bool testDirectoryValidation( void ) noexcept
{
	alignas( 64 ) uint8_t szStorage[2048] = {};
	SIRWorldPartition partition;
	const sirWorldPartitionConfig_t config = { 1, 2, 3, 2, false };
	sirWorldCellDescriptor_t cells[2] = { { 80, 0, 0, 0, 0, 0 }, { 70, 0, 1, 0, 0, 0 } };
	if ( partition.requiredBytes( 2 ) > sizeof( szStorage ) ) {
		return false;
	}
	if ( partition.initialize( szStorage + 1, sizeof( szStorage ) - 1, config, cells ) != SIR_WORLD_PARTITION_INVALID_ARGUMENT ) {
		return false;
	}
	if ( partition.initialize( szStorage, sizeof( szStorage ), config, ( const sirWorldCellDescriptor_t * )szStorage ) !=
	     SIR_WORLD_PARTITION_INVALID_ARGUMENT ) {
		return false;
	}
	if ( partition.initialize( szStorage, sizeof( szStorage ), config, cells ) != SIR_WORLD_PARTITION_OK ) {
		return false;
	}
	if ( !partition.hasCell( 70 ) || !partition.hasCell( 80 ) || partition.hasCell( 90 ) || partition.getCellCount() != 2 ) {
		return false;
	}
	if ( partition.initialize( szStorage, sizeof( szStorage ), config, cells ) != SIR_WORLD_PARTITION_INVALID_ARGUMENT ) {
		return false;
	}
	partition.shutdown();
	if ( partition.getCellCount() != 0 || partition.hasCell( 70 ) ) {
		return false;
	}

	cells[1].nCellId = 80;
	if ( partition.initialize( szStorage, sizeof( szStorage ), config, cells ) != SIR_WORLD_PARTITION_INVALID_DIRECTORY ) {
		return false;
	}
	cells[1].nCellId = 80;
	cells[1].iGridX = 0;
	if ( partition.initialize( szStorage, sizeof( szStorage ), config, cells ) != SIR_WORLD_PARTITION_INVALID_DIRECTORY ) {
		return false;
	}
	cells[1].iGridX = -1;
	if ( partition.initialize( szStorage, sizeof( szStorage ), config, cells ) != SIR_WORLD_PARTITION_INVALID_DIRECTORY ) {
		return false;
	}
	cells[1].iGridX = 1;
	cells[1].iGridZ = 1;
	if ( partition.initialize( szStorage, sizeof( szStorage ), config, cells ) != SIR_WORLD_PARTITION_INVALID_DIRECTORY ) {
		return false;
	}
	return true;
}

static bool testOverlappingSourcesAndLayers( void ) noexcept
{
	alignas( 64 ) uint8_t szStorage[8192] = {};
	const sirWorldPartitionConfig_t config = { 91, 191, 9, 6, false };
	const sirWorldCellDescriptor_t cells[6] = { { 11, 0, -1, 0, 0, 0 },
	                                            { 12, 1, 0, 0, 0, 0 },
	                                            { 13, 0, 1, 0, 0, 0 },
	                                            { 14, 2, 1, 1, 0, 0 },
	                                            { 15, 0, 3, 0, 0, SIR_WORLD_CELL_FLAG_ALWAYS_LOADED },
	                                            { 16, 3, 4, 0, 0, 0 } };
	SIRWorldPartition partition;
	if ( partition.initialize( szStorage, sizeof( szStorage ), config, cells ) != SIR_WORLD_PARTITION_OK ) {
		return false;
	}

	sirWorldStreamingSource_t sources[2] = { makeSource( 1, 0, 0, 1, SIR_STREAMING_TARGET_ACTIVE, 40, 80 ),
	                                         makeSource( 2, 1, 0, 1, SIR_STREAMING_TARGET_PREPARED, 100, 45 ) };
	sirStreamingDemand_t demands[6] = {};
	uint32_t nRequired = 0;
	if ( partition.gather( sources, 2, 1, 7, demands, 6, &nRequired ) != SIR_WORLD_PARTITION_OK || nRequired != 4 ) {
		return false;
	}
	if ( demands[0].keyUnit.nUnitId != 11 || demands[0].nPriority != 40 || demands[0].eTarget != SIR_STREAMING_TARGET_ACTIVE ) {
		return false;
	}
	if ( demands[1].keyUnit.nUnitId != 12 || demands[1].nPriority != 100 || demands[1].nDeadlineTick != 45 ||
	     demands[1].eTarget != SIR_STREAMING_TARGET_ACTIVE ) {
		return false;
	}
	if ( demands[2].keyUnit.nUnitId != 13 || demands[2].nPriority != 100 || demands[2].nDeadlineTick != 45 ||
	     demands[2].eTarget != SIR_STREAMING_TARGET_ACTIVE ) {
		return false;
	}
	if ( demands[3].keyUnit.nUnitId != 15 || demands[3].nPriority != 255 || demands[3].nDeadlineTick != 0 ) {
		return false;
	}
	for ( uint32_t iDemand = 0; iDemand < nRequired; ++iDemand ) {
		if ( demands[iDemand].keyUnit.nContainerId != 91 || demands[iDemand].keyUnit.nManifestGeneration != 9 ||
		     demands[iDemand].keyUnit.eKind != SIR_STREAMING_UNIT_WORLD_CELL || demands[iDemand].nConsumerId != 191 ||
		     demands[iDemand].nRequestEpoch != 7 || demands[iDemand].nConsumerGeneration != 9 ) {
			return false;
		}
	}

	if ( partition.gather( sources, 2, 0, 8, demands, 6, &nRequired ) != SIR_WORLD_PARTITION_OK || nRequired != 3 ) {
		return false;
	}
	if ( demands[0].keyUnit.nUnitId != 11 || demands[1].keyUnit.nUnitId != 13 || demands[2].keyUnit.nUnitId != 15 ) {
		return false;
	}
	if ( partition.gather( nullptr, 0, 0, 9, demands, 6, &nRequired ) != SIR_WORLD_PARTITION_OK || nRequired != 1 ||
	     demands[0].keyUnit.nUnitId != 15 ) {
		return false;
	}
	sirWorldStreamingSource_t layerSource = makeSource( 3, 4, 0, 0, SIR_STREAMING_TARGET_PREPARED, 2, 88 );
	if ( partition.gather( &layerSource, 1, 1, 10, demands, 6, &nRequired ) != SIR_WORLD_PARTITION_OK || nRequired != 1 ||
	     demands[0].keyUnit.nUnitId != 15 ) {
		return false;
	}
	if ( partition.gather( &layerSource, 1, 3, 11, demands, 6, &nRequired ) != SIR_WORLD_PARTITION_OK || nRequired != 2 ||
	     demands[0].keyUnit.nUnitId != 15 || demands[1].keyUnit.nUnitId != 16 ) {
		return false;
	}
	return true;
}

static bool testAtomicCapacityAndInvalidSources( void ) noexcept
{
	alignas( 64 ) uint8_t szStorage[8192] = {};
	const sirWorldPartitionConfig_t config = { 1, 2, 3, 3, false };
	const sirWorldCellDescriptor_t cells[3] = { { 1, 0, -1, 0, 0, 0 }, { 2, 0, 0, 0, 0, 0 }, { 3, 0, 1, 0, 0, 0 } };
	SIRWorldPartition partition;
	if ( partition.initialize( szStorage, sizeof( szStorage ), config, cells ) != SIR_WORLD_PARTITION_OK ) {
		return false;
	}

	sirWorldStreamingSource_t source = makeSource( 1, 0, 0, 1, SIR_STREAMING_TARGET_ACTIVE, 1, 20 );
	sirStreamingDemand_t demands[3] = {};
	demands[0].keyUnit.nUnitId = 888;
	uint32_t nRequired = 0;
	if ( partition.gather( &source, 1, 0, 5, demands, 1, &nRequired ) != SIR_WORLD_PARTITION_OUT_OF_CAPACITY || nRequired != 3 ||
	     demands[0].keyUnit.nUnitId != 888 ) {
		return false;
	}
	if ( partition.gather( &source, 1, 0, 5, demands, 3, &nRequired ) != SIR_WORLD_PARTITION_OK || nRequired != 3 ) {
		return false;
	}
	if ( partition.gather( &source, 1, 0, 0, demands, 3, &nRequired ) != SIR_WORLD_PARTITION_INVALID_ARGUMENT ) {
		return false;
	}
	source.nGeneration = 0;
	if ( partition.gather( &source, 1, 0, 6, demands, 3, &nRequired ) != SIR_WORLD_PARTITION_INVALID_ARGUMENT ) {
		return false;
	}
	source.nGeneration = 1;
	source.eTarget = ( sirStreamingTarget_t )3;
	return partition.gather( &source, 1, 0, 6, demands, 3, &nRequired ) == SIR_WORLD_PARTITION_INVALID_ARGUMENT;
}

static bool testVerticalAndIntegerBoundaries( void ) noexcept
{
	alignas( 64 ) uint8_t szStorage[4096] = {};
	const sirWorldPartitionConfig_t config = { 2, 8, 1, 3, true };
	const sirWorldCellDescriptor_t cells[3] = {
	    { 1, 0, INT32_MIN, 0, INT32_MIN, 0 }, { 2, 0, INT32_MIN, 0, INT32_MIN + 2, 0 }, { 3, 0, INT32_MAX, 0, INT32_MIN, 0 } };
	SIRWorldPartition partition;
	if ( partition.initialize( szStorage, sizeof( szStorage ), config, cells ) != SIR_WORLD_PARTITION_OK ) {
		return false;
	}

	sirWorldStreamingSource_t source = makeSource( 4, INT32_MIN, 0, UINT32_MAX, SIR_STREAMING_TARGET_ACTIVE, 12, 77 );
	source.iGridZ = INT32_MIN;
	source.nHeightRadiusCells = 1;
	sirStreamingDemand_t demands[3] = {};
	uint32_t nRequired = 0;
	if ( partition.gather( &source, 1, 0, 1, demands, 3, &nRequired ) != SIR_WORLD_PARTITION_OK || nRequired != 2 ) {
		return false;
	}
	if ( demands[0].keyUnit.nUnitId != 1 || demands[1].keyUnit.nUnitId != 3 ) {
		return false;
	}
	source.nRadiusCells = 0;
	source.nHeightRadiusCells = 2;
	return partition.gather( &source, 1, 0, 2, demands, 3, &nRequired ) == SIR_WORLD_PARTITION_OK && nRequired == 2 &&
	       demands[0].keyUnit.nUnitId == 1 && demands[1].keyUnit.nUnitId == 2;
}

int main( void )
{
	if ( !testDirectoryValidation() ) {
		return 1;
	}
	if ( !testOverlappingSourcesAndLayers() ) {
		return 2;
	}
	if ( !testAtomicCapacityAndInvalidSources() ) {
		return 3;
	}
	if ( !testVerticalAndIntegerBoundaries() ) {
		return 4;
	}
	return 0;
}
