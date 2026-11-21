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
#include "SIRAnimationCooked.h"
#include "SIRAnimationCookedClipReader.h"

#include <cmath>
#include <cstring>
#include <limits>

// All offsets below are part of ANM1 v1. The header and records are encoded
// field-by-field, never by dumping C++ structures and padding to disk.
static constexpr uint32_t SIR_COOK_HEADER_SIZE = 144;
static constexpr uint32_t SIR_COOK_BONE_BYTES = 92;
static constexpr uint32_t SIR_COOK_TRACK_BYTES = 128;
static constexpr uint32_t SIR_COOK_CHECKSUM_OFFSET = 140;
static constexpr uint32_t SIR_COOK_SECTION_COUNT = 7;
static constexpr uint32_t SIR_COOK_TRACK_FLOATS = 10;

static constexpr uint32_t SIR_COOK_PRIMARY = 0;
static constexpr uint32_t SIR_COOK_LOD = 1;
static constexpr uint32_t SIR_COOK_INFLUENCE = 2;
static constexpr uint32_t SIR_COOK_EVENT = 3;
static constexpr uint32_t SIR_COOK_CURVE = 4;
static constexpr uint32_t SIR_COOK_MORPH = 5;
static constexpr uint32_t SIR_COOK_DEPENDENCY = 6;

typedef struct sirAnimationCookHeaderData_s
{
	uint64_t nTotal;
	uint64_t nRevision;
	uint64_t nOffsets[SIR_COOK_SECTION_COUNT];
	uint32_t nCount[9];
	float fBounds[6];
	uint8_t nKind;
} sirAnimationCookHeaderData_t;

typedef struct sirAnimationCookEncodedTrack_s
{
	uint32_t nBone;
	uint32_t nCount;
	uint32_t nCoarse;
	uint32_t nEncoding;
	uint64_t nOffset;
	uint64_t nBytes;
	float fLow[SIR_COOK_TRACK_FLOATS];
	float fHigh[SIR_COOK_TRACK_FLOATS];
	uint32_t nCrc;
	uint32_t nExecutionMask;
} sirAnimationCookEncodedTrack_t;

static bool sirCookRange( uint64_t nOffset, uint64_t nBytes, uint64_t nLength ) noexcept
{
	return nOffset <= nLength && nBytes <= nLength - nOffset;
}

static uint16_t sirCookRead16( const uint8_t *pData ) noexcept
{
	return ( uint16_t )( pData[0] | ( ( uint16_t )pData[1] << 8 ) );
}

static uint32_t sirCookRead32( const uint8_t *pData ) noexcept
{
	return ( uint32_t )pData[0] | ( ( uint32_t )pData[1] << 8 ) | ( ( uint32_t )pData[2] << 16 ) | ( ( uint32_t )pData[3] << 24 );
}

static uint64_t sirCookRead64( const uint8_t *pData ) noexcept
{
	return ( uint64_t )sirCookRead32( pData ) | ( ( uint64_t )sirCookRead32( pData + 4 ) << 32 );
}

static float sirCookReadFloat( const uint8_t *pData ) noexcept
{
	const uint32_t nBits = sirCookRead32( pData );
	float fValue;
	memcpy( &fValue, &nBits, sizeof( fValue ) );
	return fValue;
}

static uint32_t sirCookCrc( const uint8_t *pData, uint64_t nLength, bool bSkipHeaderCrc ) noexcept
{
	uint32_t nCrc = 0xffffffffU;
	for ( uint64_t i = 0; i < nLength; ++i ) {
		const uint8_t nByte = ( bSkipHeaderCrc && i >= SIR_COOK_CHECKSUM_OFFSET && i < SIR_COOK_HEADER_SIZE ) ? 0 : pData[i];
		nCrc ^= nByte;
		for ( uint32_t n = 0; n < 8; ++n ) {
			nCrc = ( nCrc >> 1 ) ^ ( ( nCrc & 1U ) ? 0xedb88320U : 0U );
		}
	}
	return ~nCrc;
}

static bool sirCookMul( uint64_t nLeft, uint64_t nRight, uint64_t *pProduct ) noexcept
{
	if ( nRight && nLeft > UINT64_MAX / nRight ) {
		return false;
	}
	*pProduct = nLeft * nRight;
	return true;
}

static sirAnimationCookResult_t sirAnimationCookHeaderData_tRead( sirAnimationCookSpan_t source,
                                                                  sirAnimationCookHeaderData_t *pHeader ) noexcept
{
	if ( !source.pBytes || !pHeader || source.nBytes < SIR_COOK_HEADER_SIZE ) {
		return SIR_ANIM_COOK_INVALID;
	}
	const uint8_t *p = source.pBytes;
	if ( sirCookRead32( p ) != SIR_ANIM_COOK_MAGIC ) {
		return SIR_ANIM_COOK_CORRUPT;
	}
	if ( sirCookRead16( p + 4 ) != SIR_ANIM_COOK_VERSION ) {
		return SIR_ANIM_COOK_VERSION_ERROR;
	}
	pHeader->nKind = p[6];
	if ( p[7] || ( p[6] != SIR_ANIM_COOK_SKELETON && p[6] != SIR_ANIM_COOK_CLIP ) ) {
		return SIR_ANIM_COOK_UNSUPPORTED;
	}
	pHeader->nTotal = sirCookRead64( p + 8 );
	pHeader->nRevision = sirCookRead64( p + 16 );
	if ( pHeader->nTotal != source.nBytes || !pHeader->nRevision || source.nBytes > UINT32_MAX ) {
		return SIR_ANIM_COOK_CORRUPT;
	}
	for ( uint32_t i = 0; i < 9; ++i ) {
		pHeader->nCount[i] = sirCookRead32( p + 24 + i * 4 );
	}
	for ( uint32_t i = 0; i < SIR_COOK_SECTION_COUNT; ++i ) {
		pHeader->nOffsets[i] = sirCookRead64( p + 60 + i * 8 );
	}
	for ( uint32_t i = 0; i < 6; ++i ) {
		pHeader->fBounds[i] = sirCookReadFloat( p + 116 + i * 4 );
		if ( !std::isfinite( pHeader->fBounds[i] ) ) {
			return SIR_ANIM_COOK_CORRUPT;
		}
	}
	for ( uint32_t i = 0; i < 3; ++i ) {
		if ( pHeader->fBounds[i] > pHeader->fBounds[i + 3] ) {
			return SIR_ANIM_COOK_CORRUPT;
		}
	}
	if ( sirCookRead32( p + SIR_COOK_CHECKSUM_OFFSET ) != sirCookCrc( p, source.nBytes, true ) ) {
		return SIR_ANIM_COOK_CORRUPT;
	}
	return SIR_ANIM_COOK_OK;
}

static bool sirCookSectionLengths( const sirAnimationCookHeaderData_t& header, uint64_t *pLengths ) noexcept
{
	const uint64_t nCount = header.nCount[0];
	return sirCookMul( nCount, header.nKind == SIR_ANIM_COOK_SKELETON ? SIR_COOK_BONE_BYTES : SIR_COOK_TRACK_BYTES, &pLengths[0] ) &&
	       sirCookMul( header.nCount[1], 8, &pLengths[1] ) && sirCookMul( header.nCount[2], 4, &pLengths[2] ) &&
	       sirCookMul( header.nCount[3], 12, &pLengths[3] ) && sirCookMul( header.nCount[4], 8, &pLengths[4] ) &&
	       sirCookMul( header.nCount[5], 8, &pLengths[5] ) && sirCookMul( header.nCount[6], 16, &pLengths[6] );
}

static sirAnimationCookEncodedTrack_t sirAnimationCookEncodedTrack_tRead( const uint8_t *pData ) noexcept
{
	sirAnimationCookEncodedTrack_t result = {};
	result.nBone = sirCookRead32( pData );
	result.nCount = sirCookRead32( pData + 4 );
	result.nCoarse = sirCookRead32( pData + 8 );
	result.nEncoding = sirCookRead32( pData + 12 );
	result.nOffset = sirCookRead64( pData + 16 );
	result.nBytes = sirCookRead64( pData + 24 );
	for ( uint32_t i = 0; i < SIR_COOK_TRACK_FLOATS; ++i ) {
		result.fLow[i] = sirCookReadFloat( pData + 32 + i * 4 );
		result.fHigh[i] = sirCookReadFloat( pData + 72 + i * 4 );
	}
	result.nCrc = sirCookRead32( pData + 112 );
	result.nExecutionMask = sirCookRead32( pData + 116 );
	return result;
}

static bool sirAnimationCookEncodedTrack_tValid( sirAnimationCookSpan_t source, const sirAnimationCookEncodedTrack_t& track,
                                                 uint64_t nPayloadBegin ) noexcept
{
	if ( !track.nCount || track.nCount > SIR_ANIM_COOK_MAX_SAMPLES || track.nCoarse > 1 || track.nEncoding > SIR_ANIM_TRACK_RAW32 ||
	     !track.nExecutionMask || track.nExecutionMask > 3 || track.nOffset < nPayloadBegin ||
	     !sirCookRange( track.nOffset, track.nBytes, source.nBytes ) ) {
		return false;
	}
	const uint64_t nBytesPerKey = track.nEncoding == SIR_ANIM_TRACK_CONSTANT ? 0 : ( track.nEncoding == SIR_ANIM_TRACK_QUANT16 ? 20 : 40 );
	const uint64_t nExpected =
	    ( uint64_t )track.nCount * 4 + ( track.nEncoding == SIR_ANIM_TRACK_CONSTANT ? 40 : ( uint64_t )track.nCount * nBytesPerKey );
	if ( nExpected != track.nBytes || sirCookCrc( source.pBytes + track.nOffset, track.nBytes, false ) != track.nCrc ) {
		return false;
	}
	for ( uint32_t j = 0; j < SIR_COOK_TRACK_FLOATS; ++j ) {
		if ( !std::isfinite( track.fLow[j] ) || !std::isfinite( track.fHigh[j] ) || track.fHigh[j] < track.fLow[j] ) {
			return false;
		}
	}
	const uint8_t *pTime = source.pBytes + track.nOffset;
	for ( uint32_t k = 1; k < track.nCount; ++k ) {
		if ( sirCookRead32( pTime + k * 4 ) <= sirCookRead32( pTime + ( k - 1 ) * 4 ) ) {
			return false;
		}
	}
	return true;
}

static sirAnimationCookResult_t sirCookValidate( sirAnimationCookSpan_t source, sirAnimationCookHeaderData_t *pHeader ) noexcept
{
	sirAnimationCookResult_t eResult = sirAnimationCookHeaderData_tRead( source, pHeader );
	if ( eResult != SIR_ANIM_COOK_OK ) {
		return eResult;
	}
	const sirAnimationCookHeaderData_t& h = *pHeader;
	if ( h.nCount[0] == 0 || h.nCount[0] > ( h.nKind == SIR_ANIM_COOK_SKELETON ? SIR_ANIM_COOK_MAX_BONES : SIR_ANIM_COOK_MAX_TRACKS ) ||
	     h.nCount[1] > SIR_ANIM_COOK_MAX_METADATA || h.nCount[2] > SIR_ANIM_COOK_MAX_METADATA || h.nCount[3] > SIR_ANIM_COOK_MAX_METADATA ||
	     h.nCount[4] > SIR_ANIM_COOK_MAX_METADATA || h.nCount[5] > SIR_ANIM_COOK_MAX_METADATA ||
	     h.nCount[6] > SIR_ANIM_COOK_MAX_METADATA ) {
		return SIR_ANIM_COOK_LIMIT;
	}
	if ( h.nKind == SIR_ANIM_COOK_SKELETON && ( h.nCount[3] || h.nCount[4] || h.nCount[5] || h.nCount[7] || h.nCount[8] ) ) {
		return SIR_ANIM_COOK_CORRUPT;
	}
	if ( h.nKind == SIR_ANIM_COOK_CLIP && ( h.nCount[1] || h.nCount[2] ) ) {
		return SIR_ANIM_COOK_CORRUPT;
	}
	uint64_t nLengths[SIR_COOK_SECTION_COUNT] = {};
	if ( !sirCookSectionLengths( h, nLengths ) ) {
		return SIR_ANIM_COOK_CORRUPT;
	}
	uint64_t nEnd = SIR_COOK_HEADER_SIZE;
	for ( uint32_t i = 0; i < SIR_COOK_SECTION_COUNT; ++i ) {
		if ( h.nOffsets[i] != nEnd || !sirCookRange( nEnd, nLengths[i], h.nTotal ) ) {
			return SIR_ANIM_COOK_CORRUPT;
		}
		nEnd += nLengths[i];
	}
	const uint64_t nPayloadStart = nEnd;
	if ( h.nKind == SIR_ANIM_COOK_SKELETON ) {
		if ( nEnd != h.nTotal ) {
			return SIR_ANIM_COOK_CORRUPT;
		}
		for ( uint32_t i = 0; i < h.nCount[0]; ++i ) {
			const uint8_t *pBone = source.pBytes + h.nOffsets[0] + i * SIR_COOK_BONE_BYTES;
			const int32_t iParent = ( int32_t )sirCookRead32( pBone );
			if ( iParent < -1 || ( iParent >= 0 && ( uint32_t )iParent >= i ) ) {
				return SIR_ANIM_COOK_CORRUPT;
			}
			for ( uint32_t f = 0; f < 21; ++f ) {
				const float fValue = sirCookReadFloat( pBone + 4 + 4 * f );
				if ( !std::isfinite( fValue ) || ( ( f >= 7 && f <= 9 ) || ( f >= 17 && f <= 19 ) ) && fValue == 0.0f ||
				     ( f == 20 && fValue < 0.0f ) ) {
					return SIR_ANIM_COOK_CORRUPT;
				}
			}
			const uint32_t nStableId = sirCookRead32( pBone + 88 );
			if ( !nStableId ) {
				return SIR_ANIM_COOK_CORRUPT;
			}
			for ( uint32_t j = 0; j < i; ++j ) {
				if ( sirCookRead32( source.pBytes + h.nOffsets[0] + j * SIR_COOK_BONE_BYTES + 88 ) == nStableId ) {
					return SIR_ANIM_COOK_CORRUPT;
				}
			}
		}
		for ( uint32_t i = 0; i < h.nCount[1]; ++i ) {
			const uint8_t *pMapping = source.pBytes + h.nOffsets[1] + i * 8;
			if ( sirCookRead32( pMapping ) > 255 || sirCookRead32( pMapping + 4 ) >= h.nCount[0] ||
			     ( i && sirCookRead32( pMapping ) < sirCookRead32( pMapping - 8 ) ) ) {
				return SIR_ANIM_COOK_CORRUPT;
			}
		}
		for ( uint32_t section = 2; section <= 2; ++section ) {
			for ( uint32_t i = 0; i < h.nCount[section]; ++i ) {
				if ( sirCookRead32( source.pBytes + h.nOffsets[section] + i * 4 ) >= h.nCount[0] ) {
					return SIR_ANIM_COOK_CORRUPT;
				}
			}
		}
	} else {
		uint64_t nExpectedPayload = nPayloadStart;
		for ( uint32_t i = 0; i < h.nCount[0]; ++i ) {
			const sirAnimationCookEncodedTrack_t track =
			    sirAnimationCookEncodedTrack_tRead( source.pBytes + h.nOffsets[0] + i * SIR_COOK_TRACK_BYTES );
			if ( track.nBone >= SIR_ANIM_COOK_MAX_BONES || track.nOffset != nExpectedPayload ||
			     !sirAnimationCookEncodedTrack_tValid( source, track, nPayloadStart ) ) {
				return SIR_ANIM_COOK_CORRUPT;
			}
			nExpectedPayload += track.nBytes;
			if ( sirCookRead32( source.pBytes + track.nOffset + ( track.nCount - 1 ) * 4 ) > h.nCount[7] ) {
				return SIR_ANIM_COOK_CORRUPT;
			}
			for ( uint32_t j = 0; j < i; ++j ) {
				const sirAnimationCookEncodedTrack_t other =
				    sirAnimationCookEncodedTrack_tRead( source.pBytes + h.nOffsets[0] + j * SIR_COOK_TRACK_BYTES );
				if ( other.nBone == track.nBone && other.nCoarse == track.nCoarse ) {
					return SIR_ANIM_COOK_CORRUPT;
				}
			}
		}
		if ( nExpectedPayload != h.nTotal ) {
			return SIR_ANIM_COOK_CORRUPT;
		}
		for ( uint32_t i = 0; i < h.nCount[4]; ++i ) {
			if ( sirCookRead32( source.pBytes + h.nOffsets[4] + i * 8 + 4 ) >= h.nCount[0] ) {
				return SIR_ANIM_COOK_CORRUPT;
			}
		}
		for ( uint32_t i = 0; i < h.nCount[5]; ++i ) {
			if ( sirCookRead32( source.pBytes + h.nOffsets[5] + i * 8 + 4 ) >= h.nCount[0] ) {
				return SIR_ANIM_COOK_CORRUPT;
			}
		}
		for ( uint32_t i = 0; i < h.nCount[3]; ++i ) {
			if ( sirCookRead32( source.pBytes + h.nOffsets[3] + i * 12 ) > h.nCount[7] ||
			     ( i && sirCookRead32( source.pBytes + h.nOffsets[3] + i * 12 ) <
			                sirCookRead32( source.pBytes + h.nOffsets[3] + ( i - 1 ) * 12 ) ) ) {
				return SIR_ANIM_COOK_CORRUPT;
			}
		}
	}
	for ( uint32_t i = 0; i < h.nCount[6]; ++i ) {
		const uint8_t *pDep = source.pBytes + h.nOffsets[6] + i * 16;
		if ( !sirCookRead64( pDep ) || !sirCookRead32( pDep + 8 ) ) {
			return SIR_ANIM_COOK_CORRUPT;
		}
	}
	return SIR_ANIM_COOK_OK;
}

sirAnimationCookResult_t SIRAnimationCooked::inspect( sirAnimationCookSpan_t source, sirAnimationCookAssetInfo_t *pInfo ) noexcept
{
	if ( !pInfo ) {
		return SIR_ANIM_COOK_INVALID;
	}
	sirAnimationCookHeaderData_t header = {};
	const sirAnimationCookResult_t eResult = sirCookValidate( source, &header );
	if ( eResult != SIR_ANIM_COOK_OK ) {
		return eResult;
	}
	*pInfo = {};
	pInfo->eKind = ( sirAnimationCookKind_t )header.nKind;
	pInfo->nTotalBytes = header.nTotal;
	pInfo->nRevision = header.nRevision;
	pInfo->nItemCount = header.nCount[0];
	pInfo->nLodCount = header.nCount[1];
	pInfo->nInfluenceCount = header.nCount[2];
	pInfo->nEventCount = header.nCount[3];
	pInfo->nCurveCount = header.nCount[4];
	pInfo->nMorphCount = header.nCount[5];
	pInfo->nDependencyCount = header.nCount[6];
	pInfo->nDuration = header.nCount[7];
	pInfo->nRootBoneIndex = header.nCount[8];
	for ( uint32_t i = 0; i < 3; ++i ) {
		pInfo->bounds.fMinimum[i] = header.fBounds[i];
		pInfo->bounds.fMaximum[i] = header.fBounds[i + 3];
	}
	return SIR_ANIM_COOK_OK;
}

sirAnimationCookResult_t SIRAnimationCooked::readBone( sirAnimationCookSpan_t source, uint32_t nIndex,
                                                       sirAnimationCookBone_t *pBone ) noexcept
{
	sirAnimationCookHeaderData_t h = {};
	const sirAnimationCookResult_t eResult = sirCookValidate( source, &h );
	if ( eResult != SIR_ANIM_COOK_OK || !pBone || h.nKind != SIR_ANIM_COOK_SKELETON || nIndex >= h.nCount[0] ) {
		return eResult != SIR_ANIM_COOK_OK ? eResult : SIR_ANIM_COOK_INVALID;
	}
	const uint8_t *p = source.pBytes + h.nOffsets[0] + nIndex * SIR_COOK_BONE_BYTES;
	*pBone = {};
	pBone->iParent = ( int32_t )sirCookRead32( p );
	float *pLoc = pBone->localBind.fTranslation;
	float *pInv = pBone->inverseBind.fTranslation;
	for ( uint32_t i = 0; i < 10; ++i ) {
		const float fLoc = sirCookReadFloat( p + 4 + i * 4 );
		const float fInv = sirCookReadFloat( p + 44 + i * 4 );
		if ( i < 3 ) {
			pLoc[i] = fLoc;
			pInv[i] = fInv;
		} else if ( i < 7 ) {
			pBone->localBind.fRotation[i - 3] = fLoc;
			pBone->inverseBind.fRotation[i - 3] = fInv;
		} else {
			pBone->localBind.fScale[i - 7] = fLoc;
			pBone->inverseBind.fScale[i - 7] = fInv;
		}
	}
	pBone->fConservativeRadius = sirCookReadFloat( p + 84 );
	pBone->nStableId = sirCookRead32( p + 88 );
	return SIR_ANIM_COOK_OK;
}

static sirAnimationCookResult_t sirCookReadIndex( sirAnimationCookSpan_t source, uint32_t nIndex, uint32_t nSection,
                                                  uint32_t *pOut ) noexcept
{
	sirAnimationCookHeaderData_t h = {};
	const sirAnimationCookResult_t eResult = sirCookValidate( source, &h );
	if ( eResult != SIR_ANIM_COOK_OK || !pOut || h.nKind != SIR_ANIM_COOK_SKELETON || nIndex >= h.nCount[nSection] ) {
		return eResult != SIR_ANIM_COOK_OK ? eResult : SIR_ANIM_COOK_INVALID;
	}
	*pOut = sirCookRead32( source.pBytes + h.nOffsets[nSection] + nIndex * 4 );
	return SIR_ANIM_COOK_OK;
}

sirAnimationCookResult_t SIRAnimationCooked::readLodMapping( sirAnimationCookSpan_t source, uint32_t nIndex,
                                                             sirAnimationCookLodMapping_t *pMapping ) noexcept
{
	sirAnimationCookHeaderData_t h = {};
	const sirAnimationCookResult_t eResult = sirCookValidate( source, &h );
	if ( eResult != SIR_ANIM_COOK_OK || !pMapping || h.nKind != SIR_ANIM_COOK_SKELETON || nIndex >= h.nCount[1] ) {
		return eResult != SIR_ANIM_COOK_OK ? eResult : SIR_ANIM_COOK_INVALID;
	}
	const uint8_t *pData = source.pBytes + h.nOffsets[1] + nIndex * 8;
	pMapping->nLodLevel = sirCookRead32( pData );
	pMapping->nSourceBoneIndex = sirCookRead32( pData + 4 );
	return SIR_ANIM_COOK_OK;
}

sirAnimationCookResult_t SIRAnimationCooked::readInfluence( sirAnimationCookSpan_t source, uint32_t nIndex, uint32_t *pBoneIndex ) noexcept
{
	return sirCookReadIndex( source, nIndex, SIR_COOK_INFLUENCE, pBoneIndex );
}

sirAnimationCookResult_t SIRAnimationCooked::readDependency( sirAnimationCookSpan_t source, uint32_t nIndex, SIRResourceKey *pKey ) noexcept
{
	sirAnimationCookHeaderData_t h = {};
	const sirAnimationCookResult_t eResult = sirCookValidate( source, &h );
	if ( eResult != SIR_ANIM_COOK_OK || !pKey || nIndex >= h.nCount[6] ) {
		return eResult != SIR_ANIM_COOK_OK ? eResult : SIR_ANIM_COOK_INVALID;
	}
	const uint8_t *p = source.pBytes + h.nOffsets[6] + nIndex * 16;
	pKey->m_nAssetId = sirCookRead64( p );
	pKey->m_nTypeId = sirCookRead32( p + 8 );
	pKey->m_nVariantId = sirCookRead32( p + 12 );
	return SIR_ANIM_COOK_OK;
}

typedef struct sirAnimationCookFoundTrack_s
{
	sirAnimationCookEncodedTrack_t track;
	bool bCoarse;
} sirAnimationCookFoundTrack_t;

typedef struct sirAnimationCookTrackLookup_s
{
	uint32_t nTrackCount;
	uint64_t nTrackOffset;
	uint32_t nBoneIndex;
	bool bPreferFine;
} sirAnimationCookTrackLookup_t;

static sirAnimationCookResult_t sirCookFindTrackPrepared( sirAnimationCookSpan_t source, const sirAnimationCookTrackLookup_t& request,
                                                          sirAnimationCookFoundTrack_t *pFound ) noexcept
{
	bool bFoundCoarse = false;
	sirAnimationCookEncodedTrack_t coarse = {};
	for ( uint32_t i = 0; i < request.nTrackCount; ++i ) {
		const sirAnimationCookEncodedTrack_t candidate =
		    sirAnimationCookEncodedTrack_tRead( source.pBytes + request.nTrackOffset + i * SIR_COOK_TRACK_BYTES );
		if ( candidate.nBone != request.nBoneIndex ) {
			continue;
		}
		if ( !candidate.nCoarse ) {
			if ( request.bPreferFine ) {
				pFound->track = candidate;
				pFound->bCoarse = false;
				return SIR_ANIM_COOK_OK;
			}
			if ( !bFoundCoarse ) {
				coarse = candidate;
				bFoundCoarse = true;
			}
		} else {
			coarse = candidate;
			bFoundCoarse = true;
			if ( !request.bPreferFine ) {
				pFound->track = candidate;
				pFound->bCoarse = true;
				return SIR_ANIM_COOK_OK;
			}
		}
	}
	if ( bFoundCoarse ) {
		pFound->track = coarse;
		pFound->bCoarse = coarse.nCoarse != 0;
		return SIR_ANIM_COOK_OK;
	}
	return SIR_ANIM_COOK_MISSING;
}

static sirAnimationCookResult_t sirCookFindTrack( sirAnimationCookSpan_t source, uint32_t nBoneIndex, bool bFine,
                                                  sirAnimationCookFoundTrack_t *pFound ) noexcept
{
	sirAnimationCookHeaderData_t h = {};
	const sirAnimationCookResult_t eResult = sirCookValidate( source, &h );
	if ( eResult != SIR_ANIM_COOK_OK || h.nKind != SIR_ANIM_COOK_CLIP ) {
		return eResult != SIR_ANIM_COOK_OK ? eResult : SIR_ANIM_COOK_INVALID;
	}
	return sirCookFindTrackPrepared( source, { h.nCount[0], h.nOffsets[0], nBoneIndex, bFine }, pFound );
}

sirAnimationCookResult_t SIRAnimationCooked::getTrackCount( sirAnimationCookSpan_t source, uint32_t nBoneIndex, bool bPreferFine,
                                                            uint32_t *pCount ) noexcept
{
	if ( !pCount ) {
		return SIR_ANIM_COOK_INVALID;
	}
	sirAnimationCookFoundTrack_t found = {};
	const sirAnimationCookResult_t eResult = sirCookFindTrack( source, nBoneIndex, bPreferFine, &found );
	if ( eResult == SIR_ANIM_COOK_OK ) {
		*pCount = found.track.nCount;
	}
	return eResult;
}

static sirAnimationCookResult_t sirCookDecodeTrack( sirAnimationCookSpan_t source, const sirAnimationCookFoundTrack_t& found,
                                                    sirAnimationCookTrackOutput_t *pOutput ) noexcept
{
	if ( !pOutput->pKeys || pOutput->nCapacity < found.track.nCount ) {
		return SIR_ANIM_COOK_CAPACITY;
	}
	const uint8_t *pData = source.pBytes + found.track.nOffset;
	const uint8_t *pValues = pData + found.track.nCount * 4;
	for ( uint32_t k = 0; k < found.track.nCount; ++k ) {
		sirAnimationCookKey_t& key = pOutput->pKeys[k];
		key.nTime = sirCookRead32( pData + k * 4 );
		for ( uint32_t c = 0; c < 10; ++c ) {
			if ( found.track.nEncoding == SIR_ANIM_TRACK_CONSTANT ) {
				key.fComponents[c] = sirCookReadFloat( pValues + c * 4 );
			} else if ( found.track.nEncoding == SIR_ANIM_TRACK_RAW32 ) {
				key.fComponents[c] = sirCookReadFloat( pValues + k * 40 + c * 4 );
			} else {
				const uint16_t nQ = sirCookRead16( pValues + k * 20 + c * 2 );
				key.fComponents[c] = found.track.fLow[c] + ( found.track.fHigh[c] - found.track.fLow[c] ) * ( float )nQ / 65535.0f;
			}
			if ( !std::isfinite( key.fComponents[c] ) ) {
				pOutput->nWritten = 0;
				return SIR_ANIM_COOK_CORRUPT;
			}
		}
	}
	pOutput->nWritten = found.track.nCount;
	pOutput->bUsedCoarse = found.bCoarse;
	pOutput->nExecutionMask = ( uint8_t )found.track.nExecutionMask;
	return SIR_ANIM_COOK_OK;
}

sirAnimationCookResult_t SIRAnimationCooked::readTrack( sirAnimationCookSpan_t source, uint32_t nBoneIndex, bool bPreferFine,
                                                        sirAnimationCookTrackOutput_t *pOutput ) noexcept
{
	if ( !pOutput ) {
		return SIR_ANIM_COOK_INVALID;
	}
	pOutput->nWritten = 0;
	sirAnimationCookFoundTrack_t found = {};
	const sirAnimationCookResult_t eResult = sirCookFindTrack( source, nBoneIndex, bPreferFine, &found );
	return eResult == SIR_ANIM_COOK_OK ? sirCookDecodeTrack( source, found, pOutput ) : eResult;
}

sirAnimationCookResult_t SIRAnimationCookedClipReader::initialize( sirAnimationCookSpan_t source,
                                                                   sirAnimationCookAssetInfo_t *pInfo ) noexcept
{
	m_Source = {};
	m_nTrackCount = 0;
	m_nTrackOffset = 0;
	if ( !pInfo ) {
		return SIR_ANIM_COOK_INVALID;
	}
	const sirAnimationCookResult_t eResult = SIRAnimationCooked::inspect( source, pInfo );
	if ( eResult != SIR_ANIM_COOK_OK ) {
		return eResult;
	}
	if ( pInfo->eKind != SIR_ANIM_COOK_CLIP ) {
		return SIR_ANIM_COOK_INVALID;
	}
	m_Source = source;
	m_nTrackCount = pInfo->nItemCount;
	m_nTrackOffset = sirCookRead64( source.pBytes + 60 );
	return SIR_ANIM_COOK_OK;
}

sirAnimationCookResult_t SIRAnimationCookedClipReader::getTrackCount( uint32_t nBoneIndex, bool bPreferFine,
                                                                      uint32_t *pCount ) const noexcept
{
	if ( !m_Source.pBytes || !pCount ) {
		return SIR_ANIM_COOK_INVALID;
	}
	sirAnimationCookFoundTrack_t found = {};
	const sirAnimationCookResult_t eResult =
	    sirCookFindTrackPrepared( m_Source, { m_nTrackCount, m_nTrackOffset, nBoneIndex, bPreferFine }, &found );
	if ( eResult == SIR_ANIM_COOK_OK ) {
		*pCount = found.track.nCount;
	}
	return eResult;
}

sirAnimationCookResult_t SIRAnimationCookedClipReader::readTrack( uint32_t nBoneIndex, bool bPreferFine,
                                                                  sirAnimationCookTrackOutput_t *pOutput ) const noexcept
{
	if ( !m_Source.pBytes || !pOutput ) {
		return SIR_ANIM_COOK_INVALID;
	}
	pOutput->nWritten = 0;
	sirAnimationCookFoundTrack_t found = {};
	const sirAnimationCookResult_t eResult =
	    sirCookFindTrackPrepared( m_Source, { m_nTrackCount, m_nTrackOffset, nBoneIndex, bPreferFine }, &found );
	return eResult == SIR_ANIM_COOK_OK ? sirCookDecodeTrack( m_Source, found, pOutput ) : eResult;
}

static sirAnimationCookResult_t sirCookReadMetadata( sirAnimationCookSpan_t source, uint32_t nIndex, uint32_t nSection,
                                                     uint32_t *pValues ) noexcept
{
	sirAnimationCookHeaderData_t h = {};
	const sirAnimationCookResult_t eResult = sirCookValidate( source, &h );
	if ( eResult != SIR_ANIM_COOK_OK || !pValues || h.nKind != SIR_ANIM_COOK_CLIP || nIndex >= h.nCount[nSection] ) {
		return eResult != SIR_ANIM_COOK_OK ? eResult : SIR_ANIM_COOK_INVALID;
	}
	const uint8_t *pData = source.pBytes + h.nOffsets[nSection] + nIndex * ( nSection == SIR_COOK_EVENT ? 12 : 8 );
	pValues[0] = sirCookRead32( pData );
	pValues[1] = sirCookRead32( pData + 4 );
	if ( nSection == SIR_COOK_EVENT ) {
		pValues[2] = sirCookRead32( pData + 8 );
	}
	return SIR_ANIM_COOK_OK;
}

sirAnimationCookResult_t SIRAnimationCooked::readEvent( sirAnimationCookSpan_t source, uint32_t nIndex,
                                                        sirAnimationCookEvent_t *pEvent ) noexcept
{
	if ( !pEvent ) {
		return SIR_ANIM_COOK_INVALID;
	}
	uint32_t nFields[3] = {};
	const sirAnimationCookResult_t eResult = sirCookReadMetadata( source, nIndex, SIR_COOK_EVENT, nFields );
	if ( eResult == SIR_ANIM_COOK_OK ) {
		*pEvent = { nFields[0], nFields[1], nFields[2] };
	}
	return eResult;
}

sirAnimationCookResult_t SIRAnimationCooked::readCurve( sirAnimationCookSpan_t source, uint32_t nIndex,
                                                        sirAnimationCookCurve_t *pCurve ) noexcept
{
	if ( !pCurve ) {
		return SIR_ANIM_COOK_INVALID;
	}
	uint32_t nFields[3] = {};
	const sirAnimationCookResult_t eResult = sirCookReadMetadata( source, nIndex, SIR_COOK_CURVE, nFields );
	if ( eResult == SIR_ANIM_COOK_OK ) {
		*pCurve = { nFields[0], nFields[1] };
	}
	return eResult;
}

sirAnimationCookResult_t SIRAnimationCooked::readMorph( sirAnimationCookSpan_t source, uint32_t nIndex,
                                                        sirAnimationCookMorph_t *pMorph ) noexcept
{
	if ( !pMorph ) {
		return SIR_ANIM_COOK_INVALID;
	}
	uint32_t nFields[3] = {};
	const sirAnimationCookResult_t eResult = sirCookReadMetadata( source, nIndex, SIR_COOK_MORPH, nFields );
	if ( eResult == SIR_ANIM_COOK_OK ) {
		*pMorph = { nFields[0], nFields[1] };
	}
	return eResult;
}
