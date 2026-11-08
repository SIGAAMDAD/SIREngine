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
#include "SIRAnimationCooker.h"

#include <cmath>
#include <cstring>
#include <limits>

static constexpr uint64_t SIR_COOK_HEADER = 144;
static constexpr uint64_t SIR_COOK_BONE_RECORD = 92;
static constexpr uint64_t SIR_COOK_TRACK_RECORD = 128;

static uint32_t sirCookWriterCrc( const uint8_t *pData, uint64_t nLength, bool bSkip ) noexcept
{
	uint32_t nCrc = 0xffffffffU;
	for ( uint64_t i = 0; i < nLength; ++i ) {
		const uint8_t nByte = ( bSkip && i >= 140 && i < 144 ) ? 0 : pData[i];
		nCrc ^= nByte;
		for ( uint32_t n = 0; n < 8; ++n ) {
			nCrc = ( nCrc >> 1 ) ^ ( ( nCrc & 1U ) ? 0xedb88320U : 0U );
		}
	}
	return ~nCrc;
}

static void sirCookWrite16( uint8_t *p, uint16_t nValue ) noexcept
{
	p[0] = ( uint8_t )nValue;
	p[1] = ( uint8_t )( nValue >> 8 );
}

static void sirCookWrite32( uint8_t *p, uint32_t nValue ) noexcept
{
	for ( uint32_t i = 0; i < 4; ++i ) {
		p[i] = ( uint8_t )( nValue >> ( i * 8 ) );
	}
}

static void sirCookWrite64( uint8_t *p, uint64_t nValue ) noexcept
{
	sirCookWrite32( p, ( uint32_t )nValue );
	sirCookWrite32( p + 4, ( uint32_t )( nValue >> 32 ) );
}

static void sirCookWriteFloat( uint8_t *p, float fValue ) noexcept
{
	uint32_t nBits = 0;
	memcpy( &nBits, &fValue, sizeof( nBits ) );
	sirCookWrite32( p, nBits );
}

static bool sirCookAdd( uint64_t *pLength, uint64_t nAdditional ) noexcept
{
	if ( *pLength > UINT32_MAX - nAdditional ) {
		return false;
	}
	*pLength += nAdditional;
	return true;
}

static bool sirCookInputTransformFinite( const sirAnimationTransform_t& transform ) noexcept
{
	for ( uint32_t i = 0; i < 3; ++i ) {
		if ( !std::isfinite( transform.fTranslation[i] ) || !std::isfinite( transform.fScale[i] ) || transform.fScale[i] == 0.0f ) {
			return false;
		}
	}
	for ( uint32_t i = 0; i < 4; ++i ) {
		if ( !std::isfinite( transform.fRotation[i] ) ) {
			return false;
		}
	}
	return true;
}

static void sirCookWriteTransform( uint8_t *pData, const sirAnimationTransform_t& transform ) noexcept
{
	for ( uint32_t i = 0; i < 3; ++i ) {
		sirCookWriteFloat( pData + i * 4, transform.fTranslation[i] );
		sirCookWriteFloat( pData + ( i + 7 ) * 4, transform.fScale[i] );
	}
	for ( uint32_t i = 0; i < 4; ++i ) {
		sirCookWriteFloat( pData + ( i + 3 ) * 4, transform.fRotation[i] );
	}
}

static bool sirCookDependenciesValid( const SIRResourceKey *pDeps, uint32_t nCount ) noexcept
{
	if ( nCount > SIR_ANIM_COOK_MAX_METADATA || ( nCount && !pDeps ) ) {
		return false;
	}
	for ( uint32_t i = 0; i < nCount; ++i ) {
		if ( !pDeps[i].m_nAssetId || !pDeps[i].m_nTypeId ) {
			return false;
		}
		for ( uint32_t j = 0; j < i; ++j ) {
			if ( pDeps[i].m_nAssetId == pDeps[j].m_nAssetId && pDeps[i].m_nTypeId == pDeps[j].m_nTypeId &&
			     pDeps[i].m_nVariantId == pDeps[j].m_nVariantId ) {
				return false;
			}
		}
	}
	return true;
}

static void sirCookWriteDependencies( uint8_t *pData, const SIRResourceKey *pDeps, uint32_t nCount ) noexcept
{
	for ( uint32_t i = 0; i < nCount; ++i ) {
		sirCookWrite64( pData + i * 16, pDeps[i].m_nAssetId );
		sirCookWrite32( pData + i * 16 + 8, pDeps[i].m_nTypeId );
		sirCookWrite32( pData + i * 16 + 12, pDeps[i].m_nVariantId );
	}
}

typedef struct sirAnimationCookHeaderWrite_s
{
	uint8_t nKind;
	uint64_t nRevision;
	uint64_t nLength;
	const uint32_t *pCounts;
	const uint64_t *pOffsets;
	const float *pBounds;
} sirAnimationCookHeaderWrite_t;

static void sirCookWriteHeader( uint8_t *pData, const sirAnimationCookHeaderWrite_t& header ) noexcept
{
	memset( pData, 0, SIR_COOK_HEADER );
	sirCookWrite32( pData, SIR_ANIM_COOK_MAGIC );
	sirCookWrite16( pData + 4, SIR_ANIM_COOK_VERSION );
	pData[6] = header.nKind;
	sirCookWrite64( pData + 8, header.nLength );
	sirCookWrite64( pData + 16, header.nRevision );
	for ( uint32_t i = 0; i < 9; ++i ) {
		sirCookWrite32( pData + 24 + i * 4, header.pCounts[i] );
	}
	for ( uint32_t i = 0; i < 7; ++i ) {
		sirCookWrite64( pData + 60 + i * 8, header.pOffsets[i] );
	}
	for ( uint32_t i = 0; i < 6; ++i ) {
		sirCookWriteFloat( pData + 116 + i * 4, header.pBounds[i] );
	}
}

static bool sirCookSection( uint64_t *pPosition, uint64_t *pOffset, uint64_t nCount, uint64_t nStride ) noexcept
{
	*pOffset = *pPosition;
	return nCount <= UINT32_MAX / nStride && sirCookAdd( pPosition, nCount * nStride );
}

sirAnimationCookResult_t SIRAnimationCooker::cookSkeleton( const sirAnimationCookSkeletonInput_t& source,
                                                           sirAnimationCookOutput_t *pOutput ) noexcept
{
	if ( !pOutput || !source.pBones || !source.nRevision || !source.nBoneCount || source.nBoneCount > SIR_ANIM_COOK_MAX_BONES ||
	     source.nLodMappingCount > SIR_ANIM_COOK_MAX_METADATA || source.nInfluenceCount > SIR_ANIM_COOK_MAX_METADATA ||
	     ( source.nLodMappingCount && !source.pLodMappings ) || ( source.nInfluenceCount && !source.pInfluenceBoneIndices ) ||
	     !sirCookDependenciesValid( source.pDependencies, source.nDependencyCount ) ) {
		return SIR_ANIM_COOK_INVALID;
	}
	float fBounds[6] = {};
	for ( uint32_t i = 0; i < source.nBoneCount; ++i ) {
		const sirAnimationCookBone_t& bone = source.pBones[i];
		if ( bone.iParent < -1 || ( bone.iParent >= 0 && ( uint32_t )bone.iParent >= i ) ||
		     !sirCookInputTransformFinite( bone.localBind ) || !sirCookInputTransformFinite( bone.inverseBind ) ||
		     !std::isfinite( bone.fConservativeRadius ) || bone.fConservativeRadius < 0.0f || !bone.nStableId ) {
			return SIR_ANIM_COOK_INVALID;
		}
		for ( uint32_t j = 0; j < i; ++j ) {
			if ( source.pBones[j].nStableId == bone.nStableId ) {
				return SIR_ANIM_COOK_INVALID;
			}
		}
		for ( uint32_t axis = 0; axis < 3; ++axis ) {
			const float fMin = bone.localBind.fTranslation[axis] - bone.fConservativeRadius;
			const float fMax = bone.localBind.fTranslation[axis] + bone.fConservativeRadius;
			if ( !std::isfinite( fMin ) || !std::isfinite( fMax ) ) {
				return SIR_ANIM_COOK_INVALID;
			}
			if ( !i || fMin < fBounds[axis] ) {
				fBounds[axis] = fMin;
			}
			if ( !i || fMax > fBounds[axis + 3] ) {
				fBounds[axis + 3] = fMax;
			}
		}
	}
	for ( uint32_t i = 0; i < source.nLodMappingCount; ++i ) {
		const sirAnimationCookLodMapping_t& mapping = source.pLodMappings[i];
		if ( mapping.nSourceBoneIndex >= source.nBoneCount || mapping.nLodLevel > 255 ||
		     ( i && mapping.nLodLevel < source.pLodMappings[i - 1].nLodLevel ) ) {
			return SIR_ANIM_COOK_INVALID;
		}
		for ( uint32_t j = 0; j < i; ++j ) {
			if ( source.pLodMappings[j].nLodLevel == mapping.nLodLevel &&
			     source.pLodMappings[j].nSourceBoneIndex == mapping.nSourceBoneIndex ) {
				return SIR_ANIM_COOK_INVALID;
			}
		}
	}
	for ( uint32_t i = 0; i < source.nInfluenceCount; ++i ) {
		if ( source.pInfluenceBoneIndices[i] >= source.nBoneCount ) {
			return SIR_ANIM_COOK_INVALID;
		}
	}
	const uint32_t nCounts[9] = {
	    source.nBoneCount, source.nLodMappingCount, source.nInfluenceCount, 0, 0, 0, source.nDependencyCount, 0, 0 };
	uint64_t nOffsets[7] = {};
	uint64_t nTotal = SIR_COOK_HEADER;
	if ( !sirCookSection( &nTotal, &nOffsets[0], source.nBoneCount, SIR_COOK_BONE_RECORD ) ||
	     !sirCookSection( &nTotal, &nOffsets[1], source.nLodMappingCount, 8 ) ||
	     !sirCookSection( &nTotal, &nOffsets[2], source.nInfluenceCount, 4 ) || !sirCookSection( &nTotal, &nOffsets[3], 0, 12 ) ||
	     !sirCookSection( &nTotal, &nOffsets[4], 0, 8 ) || !sirCookSection( &nTotal, &nOffsets[5], 0, 8 ) ||
	     !sirCookSection( &nTotal, &nOffsets[6], source.nDependencyCount, 16 ) ) {
		return SIR_ANIM_COOK_LIMIT;
	}
	pOutput->nWritten = 0;
	if ( !pOutput->pBytes || pOutput->nCapacity < nTotal ) {
		pOutput->nWritten = nTotal;
		return SIR_ANIM_COOK_CAPACITY;
	}
	uint8_t *p = pOutput->pBytes;
	sirCookWriteHeader( p, { SIR_ANIM_COOK_SKELETON, source.nRevision, nTotal, nCounts, nOffsets, fBounds } );
	for ( uint32_t i = 0; i < source.nBoneCount; ++i ) {
		const sirAnimationCookBone_t& bone = source.pBones[i];
		uint8_t *pBone = p + nOffsets[0] + i * SIR_COOK_BONE_RECORD;
		sirCookWrite32( pBone, ( uint32_t )bone.iParent );
		sirCookWriteTransform( pBone + 4, bone.localBind );
		sirCookWriteTransform( pBone + 44, bone.inverseBind );
		sirCookWriteFloat( pBone + 84, bone.fConservativeRadius );
		sirCookWrite32( pBone + 88, bone.nStableId );
	}
	for ( uint32_t i = 0; i < source.nLodMappingCount; ++i ) {
		sirCookWrite32( p + nOffsets[1] + i * 8, source.pLodMappings[i].nLodLevel );
		sirCookWrite32( p + nOffsets[1] + i * 8 + 4, source.pLodMappings[i].nSourceBoneIndex );
	}
	for ( uint32_t i = 0; i < source.nInfluenceCount; ++i ) {
		sirCookWrite32( p + nOffsets[2] + i * 4, source.pInfluenceBoneIndices[i] );
	}
	sirCookWriteDependencies( p + nOffsets[6], source.pDependencies, source.nDependencyCount );
	sirCookWrite32( p + 140, sirCookWriterCrc( p, nTotal, true ) );
	pOutput->nWritten = nTotal;
	return SIR_ANIM_COOK_OK;
}

typedef struct sirAnimationCookTrackParams_s
{
	float fLow[10];
	float fHigh[10];
	uint32_t nEncoding;
	uint64_t nBytes;
} sirAnimationCookTrackParams_t;

static sirAnimationCookResult_t sirAnimationCookTrackParams_tMake( const sirAnimationCookTrack_t& track, float fMaximumError,
                                                                   sirAnimationCookTrackParams_t *pResult ) noexcept
{
	if ( !track.pKeys || !track.nKeyCount || track.nKeyCount > SIR_ANIM_COOK_MAX_SAMPLES || track.bCoarse > 1 || track.nExecutionMask > 3 ||
	     track.nReserved[0] || track.nReserved[1] ) {
		return SIR_ANIM_COOK_INVALID;
	}
	bool bConstant = true;
	for ( uint32_t c = 0; c < 10; ++c ) {
		pResult->fLow[c] = track.pKeys[0].fComponents[c];
		pResult->fHigh[c] = pResult->fLow[c];
	}
	for ( uint32_t k = 0; k < track.nKeyCount; ++k ) {
		const sirAnimationCookKey_t& key = track.pKeys[k];
		if ( k && key.nTime <= track.pKeys[k - 1].nTime ) {
			return SIR_ANIM_COOK_INVALID;
		}
		for ( uint32_t c = 0; c < 10; ++c ) {
			const float fValue = key.fComponents[c];
			if ( !std::isfinite( fValue ) ) {
				return SIR_ANIM_COOK_INVALID;
			}
			if ( fValue != track.pKeys[0].fComponents[c] ) {
				bConstant = false;
			}
			if ( fValue < pResult->fLow[c] ) {
				pResult->fLow[c] = fValue;
			}
			if ( fValue > pResult->fHigh[c] ) {
				pResult->fHigh[c] = fValue;
			}
		}
	}
	pResult->nEncoding = bConstant ? SIR_ANIM_TRACK_CONSTANT : SIR_ANIM_TRACK_QUANT16;
	if ( !bConstant ) {
		for ( uint32_t c = 0; c < 10; ++c ) {
			const double dRange = ( double )pResult->fHigh[c] - pResult->fLow[c];
			if ( dRange > ( double )fMaximumError * 131070.0 || !std::isfinite( dRange ) ) {
				pResult->nEncoding = SIR_ANIM_TRACK_RAW32;
				break;
			}
		}
	}
	pResult->nBytes = ( uint64_t )track.nKeyCount * 4 +
	                  ( pResult->nEncoding == SIR_ANIM_TRACK_CONSTANT
	                        ? 40
	                        : ( uint64_t )track.nKeyCount * ( pResult->nEncoding == SIR_ANIM_TRACK_QUANT16 ? 20 : 40 ) );
	return SIR_ANIM_COOK_OK;
}

sirAnimationCookResult_t SIRAnimationCooker::cookClip( const sirAnimationCookClipInput_t& source,
                                                       sirAnimationCookOutput_t *pOutput ) noexcept
{
	if ( !pOutput || !source.pTracks || !source.nRevision || !source.nTrackCount || source.nTrackCount > SIR_ANIM_COOK_MAX_TRACKS ||
	     !std::isfinite( source.fMaximumQuantError ) || source.fMaximumQuantError < 0.0f || !std::isfinite( source.fBoneRadius ) ||
	     source.fBoneRadius < 0.0f || ( source.nEventCount && !source.pEvents ) || ( source.nCurveCount && !source.pCurves ) ||
	     ( source.nMorphCount && !source.pMorphs ) || source.nEventCount > SIR_ANIM_COOK_MAX_METADATA ||
	     source.nCurveCount > SIR_ANIM_COOK_MAX_METADATA || source.nMorphCount > SIR_ANIM_COOK_MAX_METADATA ||
	     !sirCookDependenciesValid( source.pDependencies, source.nDependencyCount ) ) {
		return SIR_ANIM_COOK_INVALID;
	}
	float fBounds[6] = {};
	uint64_t nPayloadBytes = 0;
	for ( uint32_t i = 0; i < source.nTrackCount; ++i ) {
		const sirAnimationCookTrack_t& track = source.pTracks[i];
		sirAnimationCookTrackParams_t params = {};
		const sirAnimationCookResult_t eResult = sirAnimationCookTrackParams_tMake( track, source.fMaximumQuantError, &params );
		if ( eResult != SIR_ANIM_COOK_OK || track.pKeys[track.nKeyCount - 1].nTime > source.nDuration ||
		     track.nBoneIndex >= SIR_ANIM_COOK_MAX_BONES ) {
			return SIR_ANIM_COOK_INVALID;
		}
		for ( uint32_t j = 0; j < i; ++j ) {
			if ( source.pTracks[j].nBoneIndex == track.nBoneIndex && source.pTracks[j].bCoarse == track.bCoarse ) {
				return SIR_ANIM_COOK_INVALID;
			}
		}
		for ( uint32_t k = 0; k < track.nKeyCount; ++k ) {
			for ( uint32_t axis = 0; axis < 3; ++axis ) {
				const float fMin = track.pKeys[k].fComponents[axis] - source.fBoneRadius - source.fMaximumQuantError;
				const float fMax = track.pKeys[k].fComponents[axis] + source.fBoneRadius + source.fMaximumQuantError;
				if ( !std::isfinite( fMin ) || !std::isfinite( fMax ) ) {
					return SIR_ANIM_COOK_INVALID;
				}
				if ( !i && !k || fMin < fBounds[axis] ) {
					fBounds[axis] = fMin;
				}
				if ( !i && !k || fMax > fBounds[axis + 3] ) {
					fBounds[axis + 3] = fMax;
				}
			}
		}
		if ( !sirCookAdd( &nPayloadBytes, params.nBytes ) ) {
			return SIR_ANIM_COOK_LIMIT;
		}
	}
	for ( uint32_t i = 0; i < source.nEventCount; ++i ) {
		if ( source.pEvents[i].nTime > source.nDuration || ( i && source.pEvents[i].nTime < source.pEvents[i - 1].nTime ) ) {
			return SIR_ANIM_COOK_INVALID;
		}
	}
	for ( uint32_t type = 0; type < 2; ++type ) {
		const uint32_t nCount = type ? source.nMorphCount : source.nCurveCount;
		for ( uint32_t i = 0; i < nCount; ++i ) {
			const uint32_t nIndex = type ? source.pMorphs[i].nTrackIndex : source.pCurves[i].nTrackIndex;
			if ( nIndex >= source.nTrackCount ) {
				return SIR_ANIM_COOK_INVALID;
			}
		}
	}
	const uint32_t nCounts[9] = {
	    source.nTrackCount,   0, 0, source.nEventCount, source.nCurveCount, source.nMorphCount, source.nDependencyCount, source.nDuration,
	    source.nRootBoneIndex };
	uint64_t nOffsets[7] = {};
	uint64_t nTotal = SIR_COOK_HEADER;
	if ( !sirCookSection( &nTotal, &nOffsets[0], source.nTrackCount, SIR_COOK_TRACK_RECORD ) ||
	     !sirCookSection( &nTotal, &nOffsets[1], 0, 4 ) || !sirCookSection( &nTotal, &nOffsets[2], 0, 4 ) ||
	     !sirCookSection( &nTotal, &nOffsets[3], source.nEventCount, 12 ) ||
	     !sirCookSection( &nTotal, &nOffsets[4], source.nCurveCount, 8 ) ||
	     !sirCookSection( &nTotal, &nOffsets[5], source.nMorphCount, 8 ) ||
	     !sirCookSection( &nTotal, &nOffsets[6], source.nDependencyCount, 16 ) || !sirCookAdd( &nTotal, nPayloadBytes ) ) {
		return SIR_ANIM_COOK_LIMIT;
	}
	pOutput->nWritten = 0;
	if ( !pOutput->pBytes || pOutput->nCapacity < nTotal ) {
		pOutput->nWritten = nTotal;
		return SIR_ANIM_COOK_CAPACITY;
	}
	uint8_t *p = pOutput->pBytes;
	sirCookWriteHeader( p, { SIR_ANIM_COOK_CLIP, source.nRevision, nTotal, nCounts, nOffsets, fBounds } );
	for ( uint32_t i = 0; i < source.nEventCount; ++i ) {
		sirCookWrite32( p + nOffsets[3] + i * 12, source.pEvents[i].nTime );
		sirCookWrite32( p + nOffsets[3] + i * 12 + 4, source.pEvents[i].nEventId );
		sirCookWrite32( p + nOffsets[3] + i * 12 + 8, source.pEvents[i].nPayloadId );
	}
	for ( uint32_t i = 0; i < source.nCurveCount; ++i ) {
		sirCookWrite32( p + nOffsets[4] + i * 8, source.pCurves[i].nCurveId );
		sirCookWrite32( p + nOffsets[4] + i * 8 + 4, source.pCurves[i].nTrackIndex );
	}
	for ( uint32_t i = 0; i < source.nMorphCount; ++i ) {
		sirCookWrite32( p + nOffsets[5] + i * 8, source.pMorphs[i].nMorphId );
		sirCookWrite32( p + nOffsets[5] + i * 8 + 4, source.pMorphs[i].nTrackIndex );
	}
	sirCookWriteDependencies( p + nOffsets[6], source.pDependencies, source.nDependencyCount );
	uint64_t nPayload = nOffsets[6] + source.nDependencyCount * 16ULL;
	for ( uint32_t i = 0; i < source.nTrackCount; ++i ) {
		const sirAnimationCookTrack_t& track = source.pTracks[i];
		sirAnimationCookTrackParams_t params = {};
		if ( sirAnimationCookTrackParams_tMake( track, source.fMaximumQuantError, &params ) != SIR_ANIM_COOK_OK ) {
			return SIR_ANIM_COOK_INVALID;
		}
		uint8_t *pDesc = p + nOffsets[0] + i * SIR_COOK_TRACK_RECORD;
		memset( pDesc, 0, SIR_COOK_TRACK_RECORD );
		sirCookWrite32( pDesc, track.nBoneIndex );
		sirCookWrite32( pDesc + 4, track.nKeyCount );
		sirCookWrite32( pDesc + 8, track.bCoarse );
		sirCookWrite32( pDesc + 12, params.nEncoding );
		sirCookWrite64( pDesc + 16, nPayload );
		sirCookWrite64( pDesc + 24, params.nBytes );
		for ( uint32_t c = 0; c < 10; ++c ) {
			sirCookWriteFloat( pDesc + 32 + c * 4, params.fLow[c] );
			sirCookWriteFloat( pDesc + 72 + c * 4, params.fHigh[c] );
		}
		uint8_t *pTimes = p + nPayload;
		uint8_t *pValues = pTimes + track.nKeyCount * 4;
		for ( uint32_t k = 0; k < track.nKeyCount; ++k ) {
			sirCookWrite32( pTimes + k * 4, track.pKeys[k].nTime );
			if ( params.nEncoding == SIR_ANIM_TRACK_CONSTANT && k ) {
				continue;
			}
			for ( uint32_t c = 0; c < 10; ++c ) {
				const float fValue = track.pKeys[k].fComponents[c];
				if ( params.nEncoding == SIR_ANIM_TRACK_QUANT16 ) {
					const double dRange = ( double )params.fHigh[c] - params.fLow[c];
					const double dUnit = dRange == 0.0 ? 0.0 : ( ( double )fValue - params.fLow[c] ) / dRange;
					const uint16_t nQuant = ( uint16_t )( dUnit * 65535.0 + 0.5 );
					sirCookWrite16( pValues + k * 20 + c * 2, nQuant );
				} else {
					sirCookWriteFloat( pValues + ( params.nEncoding == SIR_ANIM_TRACK_CONSTANT ? 0 : k * 40 ) + c * 4, fValue );
				}
			}
		}
		sirCookWrite32( pDesc + 112, sirCookWriterCrc( pTimes, params.nBytes, false ) );
		sirCookWrite32( pDesc + 116, track.nExecutionMask ? track.nExecutionMask : 3 );
		nPayload += params.nBytes;
	}
	sirCookWrite32( p + 140, sirCookWriterCrc( p, nTotal, true ) );
	pOutput->nWritten = nTotal;
	return SIR_ANIM_COOK_OK;
}
