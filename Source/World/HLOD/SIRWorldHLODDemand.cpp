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

#include <World/HLOD/SIRWorldHLODDemand.h>

static bool sirWorldHLODValidDemandBatch( const sirWorldHLODDemand_t *pDemands, uint32_t nCount ) noexcept
{
	if ( nCount == 0 ) {
		return true;
	}
	if ( pDemands == nullptr ) {
		return false;
	}

	const uint32_t nWorldGeneration = pDemands[0].nWorldGeneration;
	for ( uint32_t iDemand = 0; iDemand < nCount; ++iDemand ) {
		const sirWorldHLODDemand_t& demand = pDemands[iDemand];
		if ( demand.keyGeometry.m_nAssetId == 0 || demand.keyGeometry.m_nTypeId == 0 || demand.nSourceRevision == 0 ||
		     demand.nWorldGeneration == 0 || demand.nWorldGeneration != nWorldGeneration ) {
			return false;
		}
		if ( iDemand != 0 && demand.nNodeIndex <= pDemands[iDemand - 1].nNodeIndex ) {
			return false;
		}
	}
	return true;
}

static bool sirWorldHLODSameDemand( const sirWorldHLODDemand_t& previous, const sirWorldHLODDemand_t& current ) noexcept
{
	return previous.nNodeIndex == current.nNodeIndex && previous.nWorldGeneration == current.nWorldGeneration &&
	       previous.nSourceRevision == current.nSourceRevision && previous.keyGeometry.m_nAssetId == current.keyGeometry.m_nAssetId &&
	       previous.keyGeometry.m_nTypeId == current.keyGeometry.m_nTypeId &&
	       previous.keyGeometry.m_nVariantId == current.keyGeometry.m_nVariantId;
}

sirWorldHLODResult_t sirWorldHLODReconcileDemands( const sirWorldHLODDemandDeltaInput_t& input,
                                                   const sirWorldHLODDemandDeltaOutput_t& output ) noexcept
{
	if ( output.pnOperationCount == nullptr || ( output.nCapacity != 0 && output.pOperations == nullptr ) ||
	     !sirWorldHLODValidDemandBatch( input.pPrevious, input.nPreviousCount ) ||
	     !sirWorldHLODValidDemandBatch( input.pCurrent, input.nCurrentCount ) ) {
		return SIR_WORLD_HLOD_INVALID_ARGUMENT;
	}

	// Pass zero validates the exact required capacity. Pass one is guaranteed
	// not to fail, so no partially written operation sequence escapes.
	uint32_t nWritten = 0;
	for ( uint32_t iPass = 0; iPass < 2; ++iPass ) {
		uint32_t iPrevious = 0;
		uint32_t iCurrent = 0;
		uint64_t nRequired = 0;
		while ( iPrevious < input.nPreviousCount || iCurrent < input.nCurrentCount ) {
			const bool bHasPrevious = iPrevious < input.nPreviousCount;
			const bool bHasCurrent = iCurrent < input.nCurrentCount;
			const bool bCancel =
			    bHasPrevious && ( !bHasCurrent || input.pPrevious[iPrevious].nNodeIndex <= input.pCurrent[iCurrent].nNodeIndex );
			const bool bRequest =
			    bHasCurrent && ( !bHasPrevious || input.pCurrent[iCurrent].nNodeIndex <= input.pPrevious[iPrevious].nNodeIndex );
			const bool bIdentical = bCancel && bRequest && sirWorldHLODSameDemand( input.pPrevious[iPrevious], input.pCurrent[iCurrent] );

			if ( bCancel && !bIdentical ) {
				if ( iPass != 0 ) {
					output.pOperations[nWritten++] = { input.pPrevious[iPrevious], SIR_WORLD_HLOD_DEMAND_CANCEL };
				}
				++nRequired;
			}
			if ( bRequest && !bIdentical ) {
				if ( iPass != 0 ) {
					output.pOperations[nWritten++] = { input.pCurrent[iCurrent], SIR_WORLD_HLOD_DEMAND_REQUEST };
				}
				++nRequired;
			}
			if ( bCancel ) {
				++iPrevious;
			}
			if ( bRequest ) {
				++iCurrent;
			}
		}
		if ( iPass == 0 && nRequired > output.nCapacity ) {
			return SIR_WORLD_HLOD_CAPACITY;
		}
	}
	*output.pnOperationCount = nWritten;
	return SIR_WORLD_HLOD_OK;
}
