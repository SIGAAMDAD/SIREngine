/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
Copyright (c) 2023-2026 Cory Petkovsek, Roope Palmroos, and Terrain3D contributors

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Renderer/VirtualTexturing/SIRVirtualTextureDemandBundleCodec.h>

#include <string.h>

uint64_t sirVTDemandBundleHash( const uint8_t *pBytes, uint64_t nBytes ) noexcept
{
	if ( pBytes == nullptr && nBytes != 0 ) {
		return 0;
	}
	uint64_t nHash = UINT64_C( 14695981039346656037 );
	for ( uint64_t i = 0; i < nBytes; ++i ) {
		nHash = ( nHash ^ pBytes[i] ) * UINT64_C( 1099511628211 );
	}
	return nHash;
}

static bool sirVTDemandBundleOverlaps( const void *pFirst, uint64_t nFirst, const void *pSecond, uint64_t nSecond ) noexcept
{
	const uintptr_t nA = ( uintptr_t )pFirst;
	const uintptr_t nB = ( uintptr_t )pSecond;
	if ( nA > UINTPTR_MAX - nFirst || nB > UINTPTR_MAX - nSecond ) {
		return true;
	}
	return nA < nB + nSecond && nB < nA + nFirst;
}

static uint32_t sirVTSpatialReadU32( const uint8_t *pBytes ) noexcept
{
	return ( uint32_t )pBytes[0] | ( ( uint32_t )pBytes[1] << 8U ) | ( ( uint32_t )pBytes[2] << 16U ) | ( ( uint32_t )pBytes[3] << 24U );
}

static uint64_t sirVTSpatialReadU64( const uint8_t *pBytes ) noexcept
{
	return ( uint64_t )sirVTSpatialReadU32( pBytes ) | ( ( uint64_t )sirVTSpatialReadU32( pBytes + 4 ) << 32U );
}

static bool sirVTSpatialDecodeRecord( const uint8_t *pBytes, sirVirtualTextureDemandVisibilityRecord_t *pRecord ) noexcept
{
	if ( pBytes[50] != 0 || pBytes[51] != 0 ) {
		return false;
	}
	sirVirtualTextureDemandVisibilityRecord_t record = {};
	record.nStableMaterialKey = sirVTSpatialReadU64( pBytes );
	record.nViewBin = pBytes[8];
	record.nFlags = pBytes[9];
	record.nCoverageConfidence = ( uint16_t )( pBytes[10] | ( ( uint16_t )pBytes[11] << 8U ) );
	record.nSampleCount = sirVTSpatialReadU32( pBytes + 12 );
	uint64_t nSum = 0;
	for ( uint32_t i = 0; i < SIR_VT_DEMAND_VISIBILITY_MIP_BINS; ++i ) {
		record.szMipWeight[i] = ( uint16_t )( pBytes[16 + i * 2] | ( ( uint16_t )pBytes[17 + i * 2] << 8U ) );
		nSum += record.szMipWeight[i];
	}
	record.nNominalMipBase = ( uint16_t )( pBytes[48] | ( ( uint16_t )pBytes[49] << 8U ) );
	record.nSourceRevision = sirVTSpatialReadU32( pBytes + 52 );
	record.nProbeCount = sirVTSpatialReadU32( pBytes + 56 );
	record.nCoverageSampleCount = sirVTSpatialReadU32( pBytes + 60 );
	if ( record.nStableMaterialKey == 0 || record.nViewBin >= 8 ||
	     ( record.nFlags & ~( SIR_VT_DEMAND_VISIBILITY_PARTIAL | SIR_VT_DEMAND_VISIBILITY_NO_VISIBLE ) ) != 0 ||
	     record.nNominalMipBase >= SIR_VT_DEMAND_VISIBILITY_MIP_BINS || record.nSourceRevision == 0 || record.nProbeCount == 0 ||
	     record.nCoverageSampleCount > record.nSampleCount ) {
		return false;
	}
	if ( ( record.nFlags & SIR_VT_DEMAND_VISIBILITY_NO_VISIBLE ) != 0 ) {
		if ( nSum != 0 || record.nSampleCount != 0 || record.nCoverageSampleCount != 0 ) {
			return false;
		}
	} else if ( nSum == 0 || record.nSampleCount == 0 || record.nCoverageConfidence == 0 ) {
		return false;
	}
	*pRecord = record;
	return true;
}

static bool sirVTSpatialPackBitsDecode( const uint8_t *pEncoded, uint32_t nEncodedBytes, uint8_t *pOutput, uint32_t nDecodedBytes ) noexcept
{
	uint32_t nInput = 0;
	uint32_t nOutput = 0;
	while ( nInput < nEncodedBytes ) {
		const uint8_t nCode = pEncoded[nInput++];
		if ( nCode < 128U ) {
			const uint32_t nCount = ( uint32_t )nCode + 1U;
			if ( nCount > nEncodedBytes - nInput || nCount > nDecodedBytes - nOutput ) {
				return false;
			}
			memcpy( pOutput + nOutput, pEncoded + nInput, nCount );
			nInput += nCount;
			nOutput += nCount;
		} else if ( nCode > 128U ) {
			const uint32_t nCount = 257U - nCode;
			if ( nInput == nEncodedBytes || nCount > nDecodedBytes - nOutput ) {
				return false;
			}
			memset( pOutput + nOutput, pEncoded[nInput++], nCount );
			nOutput += nCount;
		} else {
			return false; // Canonical donor files reject the PackBits NOP.
		}
	}
	return nOutput == nDecodedBytes;
}

bool sirVTDemandBundleDecodeBundle( const sirVirtualTextureDemandBundle_t& bundle,
                                    const sirVirtualTextureDemandBundleBuffers_t& buffers ) noexcept
{
	if ( buffers.pEncoded == nullptr || buffers.pScratch == nullptr || buffers.pRecords == nullptr || bundle.nCount == 0 ||
	     bundle.nDecodedBytes > SIR_VT_DEMAND_VISIBILITY_MAX_BUNDLE_BYTES ||
	     bundle.nCount > SIR_VT_DEMAND_VISIBILITY_MAX_BUNDLE_BYTES / SIR_VT_DEMAND_VISIBILITY_RECORD_BYTES ||
	     bundle.nDecodedBytes != bundle.nCount * SIR_VT_DEMAND_VISIBILITY_RECORD_BYTES || bundle.nEncodedBytes != buffers.nEncodedBytes ||
	     bundle.nEncodedBytes == 0 || bundle.nEncodedBytes > SIR_VT_DEMAND_VISIBILITY_MAX_BUNDLE_BYTES ||
	     bundle.nDecodedBytes > buffers.nScratchCapacity || bundle.nCount > buffers.nRecordCapacity || bundle.nCodec > 1 ||
	     sirVTDemandBundleHash( buffers.pEncoded, buffers.nEncodedBytes ) != bundle.nHash ) {
		return false;
	}
	if ( sirVTDemandBundleOverlaps( buffers.pEncoded, buffers.nEncodedBytes, buffers.pScratch, bundle.nDecodedBytes ) ||
	     sirVTDemandBundleOverlaps( buffers.pEncoded, buffers.nEncodedBytes, buffers.pRecords,
	                                ( uint64_t )bundle.nCount * sizeof( sirVirtualTextureDemandVisibilityRecord_t ) ) ||
	     sirVTDemandBundleOverlaps( buffers.pScratch, bundle.nDecodedBytes, buffers.pRecords,
	                                ( uint64_t )bundle.nCount * sizeof( sirVirtualTextureDemandVisibilityRecord_t ) ) ) {
		return false;
	}
	if ( bundle.nCodec == 0 ) {
		if ( bundle.nEncodedBytes != bundle.nDecodedBytes ) {
			return false;
		}
		memcpy( buffers.pScratch, buffers.pEncoded, bundle.nDecodedBytes );
	} else if ( !sirVTSpatialPackBitsDecode( buffers.pEncoded, bundle.nEncodedBytes, buffers.pScratch, bundle.nDecodedBytes ) ) {
		return false;
	}
	// Validate all records before publishing any record to the caller.
	for ( uint32_t i = 0; i < bundle.nCount; ++i ) {
		sirVirtualTextureDemandVisibilityRecord_t ignored = {};
		if ( !sirVTSpatialDecodeRecord( buffers.pScratch + ( uint64_t )i * SIR_VT_DEMAND_VISIBILITY_RECORD_BYTES, &ignored ) ) {
			return false;
		}
	}
	for ( uint32_t i = 0; i < bundle.nCount; ++i ) {
		sirVTSpatialDecodeRecord( buffers.pScratch + ( uint64_t )i * SIR_VT_DEMAND_VISIBILITY_RECORD_BYTES, &buffers.pRecords[i] );
	}
	return true;
}
