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
#include "SIRAnimationSampler.h"
#include <Animation/Cooked/SIRAnimationCookedClipReader.h>

#include <cmath>
#include <cstring>
#include <limits>

static bool sirSampleFinite( float f ) noexcept
{
	return std::isfinite( f );
}

static void sirSampleIdentity( sirAnimationTransform_t *pValue ) noexcept
{
	memset( pValue, 0, sizeof( *pValue ) );
	pValue->fRotation[3] = 1.0f;
	for ( uint32_t i = 0; i < 3; ++i ) {
		pValue->fScale[i] = 1.0f;
	}
}

static void sirSampleNormalizeRotation( float *pQ ) noexcept
{
	const float fLength2 = pQ[0] * pQ[0] + pQ[1] * pQ[1] + pQ[2] * pQ[2] + pQ[3] * pQ[3];
	if ( fLength2 <= 1e-20f || !sirSampleFinite( fLength2 ) ) {
		pQ[0] = pQ[1] = pQ[2] = 0.0f;
		pQ[3] = 1.0f;
		return;
	}
	const float fReciprocal = 1.0f / std::sqrt( fLength2 );
	for ( uint32_t i = 0; i < 4; ++i ) {
		pQ[i] *= fReciprocal;
	}
}

static void sirSampleNlerp( const float *pA, const float *pB, float fAlpha, float *pOut ) noexcept
{
	float fDot = 0.0f;
	for ( uint32_t c = 0; c < 4; ++c ) {
		fDot += pA[c] * pB[c];
	}
	const float fSign = fDot < 0.0f ? -1.0f : 1.0f;
	for ( uint32_t c = 0; c < 4; ++c ) {
		pOut[c] = pA[c] + fAlpha * ( fSign * pB[c] - pA[c] );
	}
	sirSampleNormalizeRotation( pOut );
}

static void sirSampleFromKeys( const sirAnimationCookKey_t& a, const sirAnimationCookKey_t& b, float fAlpha,
                               sirAnimationTransform_t *pOut ) noexcept
{
	for ( uint32_t c = 0; c < 3; ++c ) {
		pOut->fTranslation[c] = a.fComponents[c] + ( b.fComponents[c] - a.fComponents[c] ) * fAlpha;
		pOut->fScale[c] = a.fComponents[c + 7] + ( b.fComponents[c + 7] - a.fComponents[c + 7] ) * fAlpha;
	}
	sirSampleNlerp( &a.fComponents[3], &b.fComponents[3], fAlpha, pOut->fRotation );
}

uint32_t SIRAnimationSampler::normalizeTick( int64_t iTick, int64_t iStartTick, uint32_t nDuration,
                                             sirAnimationPlaybackMode_t ePlayback ) noexcept
{
	if ( !nDuration ) {
		return 0;
	}
	// Subtraction through the unsigned domain is defined for INT64_MIN/MAX.
	const uint64_t nDelta = ( uint64_t )iTick - ( uint64_t )iStartTick;
	const bool bNegative = iTick < iStartTick;
	const uint64_t nMagnitude = bNegative ? ( uint64_t )( 0 - nDelta ) : nDelta;
	if ( ePlayback == SIR_ANIMATION_PLAY_CLAMP ) {
		return bNegative ? 0 : ( uint32_t )( nMagnitude >= nDuration ? nDuration : nMagnitude );
	}
	if ( ePlayback == SIR_ANIMATION_PLAY_LOOP ) {
		const uint64_t nPeriod = nDuration;
		const uint64_t nPhase = nMagnitude % nPeriod;
		return ( uint32_t )( bNegative && nPhase ? nPeriod - nPhase : nPhase );
	}
	const uint64_t nPeriod = ( uint64_t )nDuration * 2;
	const uint64_t nPhase = nMagnitude % nPeriod;
	const uint64_t nMirrored = bNegative && nPhase ? nPeriod - nPhase : nPhase;
	return ( uint32_t )( nMirrored > nDuration ? nPeriod - nMirrored : nMirrored );
}

sirAnimationCookResult_t SIRAnimationSampler::sampleBone( sirAnimationCookSpan_t clip, uint32_t nBoneIndex, uint32_t nTime,
                                                          sirAnimationSampleScratch_t *pScratch,
                                                          sirAnimationTransform_t *pTransform ) noexcept
{
	if ( !pScratch || !pTransform || !pScratch->pKeys || !pScratch->nKeyCapacity ) {
		return SIR_ANIM_COOK_INVALID;
	}
	SIRAnimationCookedClipReader reader;
	sirAnimationCookAssetInfo_t info = {};
	const sirAnimationCookResult_t eInit = reader.initialize( clip, &info );
	if ( eInit != SIR_ANIM_COOK_OK ) {
		return eInit;
	}
	sirAnimationCookTrackOutput_t output = { pScratch->pKeys, pScratch->nKeyCapacity, 0, false, 0 };
	const sirAnimationCookResult_t eRead = reader.readTrack( nBoneIndex, true, &output );
	if ( eRead != SIR_ANIM_COOK_OK ) {
		return eRead;
	}
	uint32_t nCursor = 0;
	if ( pScratch->pCursorByBone && nBoneIndex < pScratch->nCursorCapacity && pScratch->bCursorValid &&
	     nTime >= pScratch->nLastSampleTime ) {
		nCursor = pScratch->pCursorByBone[nBoneIndex];
		if ( nCursor >= output.nWritten || pScratch->pKeys[nCursor].nTime > nTime ) {
			nCursor = 0;
		}
	}
	while ( nCursor + 1 < output.nWritten && pScratch->pKeys[nCursor + 1].nTime <= nTime ) {
		++nCursor;
	}
	const uint32_t nNext = nCursor + 1 < output.nWritten ? nCursor + 1 : nCursor;
	const uint32_t nLowTime = pScratch->pKeys[nCursor].nTime;
	const uint32_t nHighTime = pScratch->pKeys[nNext].nTime;
	const float fAlpha =
	    nLowTime == nHighTime || nTime <= nLowTime ? 0.0f : ( float )( nTime - nLowTime ) / ( float )( nHighTime - nLowTime );
	sirSampleFromKeys( pScratch->pKeys[nCursor], pScratch->pKeys[nNext], fAlpha, pTransform );
	if ( pScratch->pCursorByBone && nBoneIndex < pScratch->nCursorCapacity ) {
		pScratch->pCursorByBone[nBoneIndex] = nCursor;
	}
	pScratch->nLastSampleTime = nTime;
	pScratch->bCursorValid = true;
	return SIR_ANIM_COOK_OK;
}

sirAnimationCookResult_t SIRAnimationSampler::sample( sirAnimationCookSpan_t clip, const sirAnimationSampleOptions_t& options,
                                                      sirAnimationSampleScratch_t *pScratch, sirAnimationMutablePose_t *pPose ) noexcept
{
	if ( !pPose || !pPose->pLocalTransforms || !pScratch || !pScratch->pKeys || !options.nBoneCount ||
	     options.nBoneCount > SIR_ANIM_COOK_MAX_BONES || pPose->nTransformCapacity < options.nBoneCount ||
	     options.ePlayback > SIR_ANIMATION_PLAY_PINGPONG || options.eInterpolation > SIR_ANIMATION_INTERPOLATION_LINEAR ||
	     ( pScratch->pCursorByBone && pScratch->nCursorCapacity < options.nBoneCount ) ) {
		return SIR_ANIM_COOK_INVALID;
	}
	SIRAnimationCookedClipReader reader;
	sirAnimationCookAssetInfo_t info = {};
	const sirAnimationCookResult_t eInspect = reader.initialize( clip, &info );
	if ( eInspect != SIR_ANIM_COOK_OK ) {
		return eInspect;
	}
	if ( info.eKind != SIR_ANIM_COOK_CLIP || !info.nDuration || info.nRootBoneIndex >= options.nBoneCount ) {
		return SIR_ANIM_COOK_INVALID;
	}
	uint32_t nTime = normalizeTick( options.iTick, options.iStartTick, info.nDuration, options.ePlayback );
	if ( options.bReverse ) {
		nTime = info.nDuration - nTime;
	}
	// Work is all-or-nothing at the caller-visible boundary: preflight every
	// required track's count before writing a single destination transform.
	if ( options.pDefaultTransforms ) {
		for ( uint32_t b = 0; b < options.nBoneCount; ++b ) {
			for ( uint32_t c = 0; c < 3; ++c ) {
				if ( !sirSampleFinite( options.pDefaultTransforms[b].fTranslation[c] ) ||
				     !sirSampleFinite( options.pDefaultTransforms[b].fScale[c] ) ) {
					return SIR_ANIM_COOK_INVALID;
				}
			}
			for ( uint32_t c = 0; c < 4; ++c ) {
				if ( !sirSampleFinite( options.pDefaultTransforms[b].fRotation[c] ) ) {
					return SIR_ANIM_COOK_INVALID;
				}
			}
		}
	}
	for ( uint32_t b = 0; b < options.nBoneCount; ++b ) {
		uint32_t nCount = 0;
		const sirAnimationCookResult_t eRead = reader.getTrackCount( b, options.bPreferFine, &nCount );
		if ( eRead != SIR_ANIM_COOK_OK && eRead != SIR_ANIM_COOK_MISSING ) {
			return eRead;
		}
		if ( eRead == SIR_ANIM_COOK_OK && nCount > pScratch->nKeyCapacity ) {
			return SIR_ANIM_COOK_CAPACITY;
		}
	}
	for ( uint32_t b = 0; b < options.nBoneCount; ++b ) {
		sirAnimationTransform_t value = {};
		if ( options.pDefaultTransforms ) {
			value = options.pDefaultTransforms[b];
		} else {
			sirSampleIdentity( &value );
		}
		sirAnimationCookTrackOutput_t output = { pScratch->pKeys, pScratch->nKeyCapacity, 0, false, 0 };
		const sirAnimationCookResult_t eRead = reader.readTrack( b, options.bPreferFine, &output );
		if ( eRead != SIR_ANIM_COOK_OK && eRead != SIR_ANIM_COOK_MISSING ) {
			return eRead;
		}
		if ( eRead == SIR_ANIM_COOK_OK ) {
			uint32_t nCursor = 0;
			if ( pScratch->pCursorByBone && pScratch->bCursorValid && nTime >= pScratch->nLastSampleTime ) {
				nCursor = pScratch->pCursorByBone[b];
				if ( nCursor >= output.nWritten || pScratch->pKeys[nCursor].nTime > nTime ) {
					nCursor = 0;
				}
			}
			while ( nCursor + 1 < output.nWritten && pScratch->pKeys[nCursor + 1].nTime <= nTime ) {
				++nCursor;
			}
			const uint32_t nNext = nCursor + 1 < output.nWritten ? nCursor + 1 : nCursor;
			const uint32_t nLow = pScratch->pKeys[nCursor].nTime;
			const uint32_t nHigh = pScratch->pKeys[nNext].nTime;
			const float fAlpha = options.eInterpolation == SIR_ANIMATION_INTERPOLATION_STEP || nHigh == nLow || nTime <= nLow
			                         ? 0.0f
			                         : ( float )( nTime - nLow ) / ( float )( nHigh - nLow );
			sirSampleFromKeys( pScratch->pKeys[nCursor], pScratch->pKeys[nNext], fAlpha, &value );
			if ( pScratch->pCursorByBone ) {
				pScratch->pCursorByBone[b] = nCursor;
			}
		} else if ( pScratch->pCursorByBone ) {
			pScratch->pCursorByBone[b] = 0;
		}
		pPose->pLocalTransforms[b] = value;
	}
	pPose->nTransformCount = options.nBoneCount;
	pScratch->nLastSampleTime = nTime;
	pScratch->bCursorValid = true;
	return SIR_ANIM_COOK_OK;
}

sirAnimationCookResult_t SIRAnimationSampler::blend( sirAnimationMutablePose_t *pDestination,
                                                     const sirAnimationBlendOptions_t& options ) noexcept
{
	if ( !pDestination || !pDestination->pLocalTransforms || !options.pSource || !sirSampleFinite( options.fWeight ) ||
	     options.fWeight < 0.0f || options.fWeight > 1.0f || options.nBoneCount > pDestination->nTransformCount ) {
		return SIR_ANIM_COOK_INVALID;
	}
	for ( uint32_t b = 0; b < options.nBoneCount; ++b ) {
		if ( options.pMask && !options.pMask[b] ) {
			continue;
		}
		for ( uint32_t c = 0; c < 3; ++c ) {
			if ( !sirSampleFinite( options.pSource[b].fTranslation[c] ) || !sirSampleFinite( options.pSource[b].fScale[c] ) ||
			     !sirSampleFinite( pDestination->pLocalTransforms[b].fTranslation[c] ) ||
			     !sirSampleFinite( pDestination->pLocalTransforms[b].fScale[c] ) ) {
				return SIR_ANIM_COOK_INVALID;
			}
		}
		for ( uint32_t c = 0; c < 4; ++c ) {
			if ( !sirSampleFinite( options.pSource[b].fRotation[c] ) ||
			     !sirSampleFinite( pDestination->pLocalTransforms[b].fRotation[c] ) ) {
				return SIR_ANIM_COOK_INVALID;
			}
		}
	}
	for ( uint32_t b = 0; b < options.nBoneCount; ++b ) {
		if ( options.pMask && !options.pMask[b] ) {
			continue;
		}
		sirAnimationTransform_t& dest = pDestination->pLocalTransforms[b];
		const sirAnimationTransform_t& source = options.pSource[b];
		for ( uint32_t c = 0; c < 3; ++c ) {
			if ( !sirSampleFinite( source.fTranslation[c] ) || !sirSampleFinite( source.fScale[c] ) ) {
				return SIR_ANIM_COOK_INVALID;
			}
			dest.fTranslation[c] = options.bAdditive
			                           ? dest.fTranslation[c] + source.fTranslation[c] * options.fWeight
			                           : dest.fTranslation[c] + ( source.fTranslation[c] - dest.fTranslation[c] ) * options.fWeight;
			dest.fScale[c] = options.bAdditive ? dest.fScale[c] * ( 1.0f + ( source.fScale[c] - 1.0f ) * options.fWeight )
			                                   : dest.fScale[c] + ( source.fScale[c] - dest.fScale[c] ) * options.fWeight;
		}
		float quaternion[4] = {};
		for ( uint32_t c = 0; c < 4; ++c ) {
			if ( !sirSampleFinite( source.fRotation[c] ) ) {
				return SIR_ANIM_COOK_INVALID;
			}
		}
		if ( options.bAdditive ) {
			const float identity[4] = { 0, 0, 0, 1 };
			float delta[4] = {};
			sirSampleNlerp( identity, source.fRotation, options.fWeight, delta );
			quaternion[0] =
			    dest.fRotation[3] * delta[0] + dest.fRotation[0] * delta[3] + dest.fRotation[1] * delta[2] - dest.fRotation[2] * delta[1];
			quaternion[1] =
			    dest.fRotation[3] * delta[1] - dest.fRotation[0] * delta[2] + dest.fRotation[1] * delta[3] + dest.fRotation[2] * delta[0];
			quaternion[2] =
			    dest.fRotation[3] * delta[2] + dest.fRotation[0] * delta[1] - dest.fRotation[1] * delta[0] + dest.fRotation[2] * delta[3];
			quaternion[3] =
			    dest.fRotation[3] * delta[3] - dest.fRotation[0] * delta[0] - dest.fRotation[1] * delta[1] - dest.fRotation[2] * delta[2];
			sirSampleNormalizeRotation( quaternion );
		} else {
			sirSampleNlerp( dest.fRotation, source.fRotation, options.fWeight, quaternion );
		}
		memcpy( dest.fRotation, quaternion, sizeof( quaternion ) );
	}
	return SIR_ANIM_COOK_OK;
}

static void sirSampleRotate( const float *pQ, const float *pV, float *pOut ) noexcept
{
	const float x = pQ[0], y = pQ[1], z = pQ[2], w = pQ[3];
	const float uvx = y * pV[2] - z * pV[1];
	const float uvy = z * pV[0] - x * pV[2];
	const float uvz = x * pV[1] - y * pV[0];
	const float uuvx = y * uvz - z * uvy;
	const float uuvy = z * uvx - x * uvz;
	const float uuvz = x * uvy - y * uvx;
	pOut[0] = pV[0] + 2.0f * ( w * uvx + uuvx );
	pOut[1] = pV[1] + 2.0f * ( w * uvy + uuvy );
	pOut[2] = pV[2] + 2.0f * ( w * uvz + uuvz );
}

sirAnimationCookResult_t SIRAnimationSampler::evaluateHierarchy( const sirAnimationHierarchyOptions_t& options ) noexcept
{
	if ( !options.pParents || !options.pLocal || !options.pModel || !options.nBoneCount || options.nBoneCount > SIR_ANIM_COOK_MAX_BONES ) {
		return SIR_ANIM_COOK_INVALID;
	}
	for ( uint32_t b = 0; b < options.nBoneCount; ++b ) {
		const int32_t iParent = options.pParents[b];
		if ( iParent < -1 || ( iParent >= 0 && ( uint32_t )iParent >= b ) ) {
			return SIR_ANIM_COOK_INVALID;
		}
		for ( uint32_t c = 0; c < 3; ++c ) {
			if ( !sirSampleFinite( options.pLocal[b].fTranslation[c] ) || !sirSampleFinite( options.pLocal[b].fScale[c] ) ||
			     options.pLocal[b].fScale[c] == 0.0f ) {
				return SIR_ANIM_COOK_INVALID;
			}
		}
		float fRotationLength2 = 0.0f;
		for ( uint32_t c = 0; c < 4; ++c ) {
			const float fComponent = options.pLocal[b].fRotation[c];
			if ( !sirSampleFinite( fComponent ) ) {
				return SIR_ANIM_COOK_INVALID;
			}
			fRotationLength2 += fComponent * fComponent;
		}
		if ( !sirSampleFinite( fRotationLength2 ) || fRotationLength2 <= 1e-20f ) {
			return SIR_ANIM_COOK_INVALID;
		}
	}
	for ( uint32_t b = 0; b < options.nBoneCount; ++b ) {
		const int32_t iParent = options.pParents[b];
		if ( iParent == -1 ) {
			options.pModel[b] = options.pLocal[b];
			sirSampleNormalizeRotation( options.pModel[b].fRotation );
			continue;
		}
		const sirAnimationTransform_t& parent = options.pModel[iParent];
		const sirAnimationTransform_t local = options.pLocal[b];
		sirAnimationTransform_t& out = options.pModel[b];
		const float scaled[3] = { local.fTranslation[0] * parent.fScale[0], local.fTranslation[1] * parent.fScale[1],
		                          local.fTranslation[2] * parent.fScale[2] };
		float rotated[3] = {};
		sirSampleRotate( parent.fRotation, scaled, rotated );
		for ( uint32_t c = 0; c < 3; ++c ) {
			out.fTranslation[c] = parent.fTranslation[c] + rotated[c];
			out.fScale[c] = parent.fScale[c] * local.fScale[c];
		}
		out.fRotation[0] = parent.fRotation[3] * local.fRotation[0] + parent.fRotation[0] * local.fRotation[3] +
		                   parent.fRotation[1] * local.fRotation[2] - parent.fRotation[2] * local.fRotation[1];
		out.fRotation[1] = parent.fRotation[3] * local.fRotation[1] - parent.fRotation[0] * local.fRotation[2] +
		                   parent.fRotation[1] * local.fRotation[3] + parent.fRotation[2] * local.fRotation[0];
		out.fRotation[2] = parent.fRotation[3] * local.fRotation[2] + parent.fRotation[0] * local.fRotation[1] -
		                   parent.fRotation[1] * local.fRotation[0] + parent.fRotation[2] * local.fRotation[3];
		out.fRotation[3] = parent.fRotation[3] * local.fRotation[3] - parent.fRotation[0] * local.fRotation[0] -
		                   parent.fRotation[1] * local.fRotation[1] - parent.fRotation[2] * local.fRotation[2];
		sirSampleNormalizeRotation( out.fRotation );
	}
	return SIR_ANIM_COOK_OK;
}
