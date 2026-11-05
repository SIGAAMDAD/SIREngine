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

#include <World/HLOD/SIRWorldHLOD.h>

#include <math.h>
#include <string.h>

static bool sirWorldHLODIsReady( const sirWorldHLODInput_t& input, uint32_t iNode ) noexcept
{
	const sirWorldHLODResidency_t& resident = input.pResidency[iNode];
	return resident.bReady && resident.nWorldGeneration == input.nWorldGeneration &&
	       resident.nSourceRevision == input.pNodes[iNode].nSourceRevision;
}

sirWorldHLODResult_t sirWorldHLODPlan( const sirWorldHLODInput_t& input, sirWorldHLODOutput_t& output ) noexcept
{
	if ( input.pNodes == nullptr || input.pResidency == nullptr || input.pProjectedError == nullptr || output.pScratch == nullptr ||
	     output.pnSelectedCount == nullptr || output.pnDemandCount == nullptr || input.nNodeCount == 0 ||
	     output.nScratchCapacity < input.nNodeCount || input.nWorldGeneration == 0 || !isfinite( input.fMaxProjectedError ) ||
	     input.fMaxProjectedError < 0.0f || ( output.pSelectedNodes == nullptr && output.nSelectedCapacity != 0 ) ||
	     ( output.pDemands == nullptr && output.nDemandCapacity != 0 ) ||
	     ( size_t )input.nNodeCount > SIZE_MAX / sizeof( sirWorldHLODScratch_t ) ) {
		return SIR_WORLD_HLOD_INVALID_ARGUMENT;
	}

	memset( output.pScratch, 0, sizeof( sirWorldHLODScratch_t ) * input.nNodeCount );
	for ( uint32_t iNode = 0; iNode < input.nNodeCount; ++iNode ) {
		const sirWorldHLODNode_t& node = input.pNodes[iNode];
		if ( !isfinite( input.pProjectedError[iNode] ) || input.pProjectedError[iNode] < 0.0f || node.nSourceRevision == 0 ||
		     node.keyGeometry.m_nAssetId == 0 || node.keyGeometry.m_nTypeId == 0 ) {
			return SIR_WORLD_HLOD_INVALID_HIERARCHY;
		}
		if ( node.nChildCount == 0 ) {
			continue;
		}
		if ( node.nFirstChild <= iNode || node.nFirstChild >= input.nNodeCount || node.nChildCount > input.nNodeCount - node.nFirstChild ) {
			return SIR_WORLD_HLOD_INVALID_HIERARCHY;
		}
		for ( uint32_t iChild = node.nFirstChild; iChild < node.nFirstChild + node.nChildCount; ++iChild ) {
			if ( ++output.pScratch[iChild].nParentReferences != 1 ) {
				return SIR_WORLD_HLOD_INVALID_HIERARCHY;
			}
		}
	}
	if ( output.pScratch[0].nParentReferences != 0 ) {
		return SIR_WORLD_HLOD_INVALID_HIERARCHY;
	}
	for ( uint32_t iNode = 1; iNode < input.nNodeCount; ++iNode ) {
		if ( output.pScratch[iNode].nParentReferences != 1 ) {
			return SIR_WORLD_HLOD_INVALID_HIERARCHY;
		}
	}

	// Parent-first indexes permit an O(nodes + edges) reverse feasibility pass.
	for ( uint32_t nReverse = input.nNodeCount; nReverse > 0; --nReverse ) {
		const uint32_t iNode = nReverse - 1;
		const sirWorldHLODNode_t& node = input.pNodes[iNode];
		const bool bReady = sirWorldHLODIsReady( input, iNode );
		bool bChildrenCover = node.nChildCount != 0;
		for ( uint32_t iChild = node.nFirstChild; bChildrenCover && iChild < node.nFirstChild + node.nChildCount; ++iChild ) {
			if ( !output.pScratch[iChild].bCanCover ) {
				bChildrenCover = false;
			}
		}
		output.pScratch[iNode].bCanCover = bReady || bChildrenCover;
		output.pScratch[iNode].bUseChildren = bChildrenCover && ( input.pProjectedError[iNode] > input.fMaxProjectedError || !bReady );
	}
	if ( !output.pScratch[0].bCanCover ) {
		return SIR_WORLD_HLOD_NO_RESIDENT_CUT;
	}

	// Only active coarse nodes can emit requests: missing child pages are prefetched,
	// but deeper grandchildren wait until their parents have an actual coherent cut.
	output.pScratch[0].bActive = 1;
	uint32_t nSelected = 0;
	uint32_t nDemands = 0;
	for ( uint32_t iNode = 0; iNode < input.nNodeCount; ++iNode ) {
		if ( !output.pScratch[iNode].bActive ) {
			continue;
		}
		const sirWorldHLODNode_t& node = input.pNodes[iNode];
		if ( output.pScratch[iNode].bUseChildren ) {
			for ( uint32_t iChild = node.nFirstChild; iChild < node.nFirstChild + node.nChildCount; ++iChild ) {
				output.pScratch[iChild].bActive = 1;
			}
			continue;
		}
		++nSelected;
		if ( input.pProjectedError[iNode] <= input.fMaxProjectedError ) {
			continue;
		}
		for ( uint32_t iChild = node.nFirstChild; iChild < node.nFirstChild + node.nChildCount; ++iChild ) {
			if ( !output.pScratch[iChild].bCanCover ) {
				output.pScratch[iChild].bRequest = 1;
				++nDemands;
			}
		}
	}
	if ( nSelected > output.nSelectedCapacity || nDemands > output.nDemandCapacity ) {
		return SIR_WORLD_HLOD_CAPACITY;
	}
	if ( ( nSelected != 0 && output.pSelectedNodes == nullptr ) || ( nDemands != 0 && output.pDemands == nullptr ) ) {
		return SIR_WORLD_HLOD_INVALID_ARGUMENT;
	}

	uint32_t nWritten = 0;
	uint32_t nRequested = 0;
	for ( uint32_t iNode = 0; iNode < input.nNodeCount; ++iNode ) {
		if ( output.pScratch[iNode].bActive && !output.pScratch[iNode].bUseChildren ) {
			output.pSelectedNodes[nWritten++] = iNode;
		}
		if ( output.pScratch[iNode].bRequest ) {
			const sirWorldHLODNode_t& node = input.pNodes[iNode];
			output.pDemands[nRequested++] = { node.keyGeometry, node.nSourceRevision, input.nWorldGeneration, iNode };
		}
	}
	*output.pnSelectedCount = nWritten;
	*output.pnDemandCount = nRequested;
	return SIR_WORLD_HLOD_OK;
}
