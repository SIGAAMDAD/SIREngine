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
#ifndef __SIRENGINE_SIRANIMATIONSAMPLER_H__
#define __SIRENGINE_SIRANIMATIONSAMPLER_H__

#include <SIRPCH.hpp>
// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Animation/Cooked/SIRAnimationCooked.h>

typedef enum sirAnimationPlaybackMode_e : uint8_t
{
	SIR_ANIMATION_PLAY_CLAMP = 0,
	SIR_ANIMATION_PLAY_LOOP = 1,
	SIR_ANIMATION_PLAY_PINGPONG = 2
} sirAnimationPlaybackMode_t;

typedef enum sirAnimationInterpolation_e : uint8_t
{
	SIR_ANIMATION_INTERPOLATION_STEP = 0,
	SIR_ANIMATION_INTERPOLATION_LINEAR = 1
} sirAnimationInterpolation_t;

typedef struct sirAnimationSampleOptions_s
{
	int64_t iTick;
	int64_t iStartTick;
	uint32_t nBoneCount;
	sirAnimationPlaybackMode_t ePlayback;
	sirAnimationInterpolation_t eInterpolation;
	bool bReverse;
	bool bPreferFine;
	const sirAnimationTransform_t *pDefaultTransforms;
} sirAnimationSampleOptions_t;

typedef struct sirAnimationSampleScratch_s
{
	sirAnimationCookKey_t *pKeys;
	uint32_t nKeyCapacity;
	uint32_t *pCursorByBone;
	uint32_t nCursorCapacity;
	uint64_t nLastSampleTime;
	bool bCursorValid;
} sirAnimationSampleScratch_t;

typedef struct sirAnimationBlendOptions_s
{
	const sirAnimationTransform_t *pSource;
	float fWeight;
	const uint8_t *pMask;
	uint32_t nBoneCount;
	bool bAdditive;
} sirAnimationBlendOptions_t;

typedef struct sirAnimationHierarchyOptions_s
{
	const int32_t *pParents;
	const sirAnimationTransform_t *pLocal;
	sirAnimationTransform_t *pModel;
	uint32_t nBoneCount;
} sirAnimationHierarchyOptions_t;

class SIRAnimationSampler
{
public:
	SIRAnimationSampler( void ) noexcept = delete;

	static sirAnimationCookResult_t sample( sirAnimationCookSpan_t clip, const sirAnimationSampleOptions_t& options,
	                                        sirAnimationSampleScratch_t *pScratch, sirAnimationMutablePose_t *pPose ) noexcept;
	static sirAnimationCookResult_t sampleBone( sirAnimationCookSpan_t clip, uint32_t nBoneIndex, uint32_t nTime,
	                                            sirAnimationSampleScratch_t *pScratch, sirAnimationTransform_t *pTransform ) noexcept;
	static sirAnimationCookResult_t blend( sirAnimationMutablePose_t *pDestination, const sirAnimationBlendOptions_t& options ) noexcept;
	static sirAnimationCookResult_t evaluateHierarchy( const sirAnimationHierarchyOptions_t& options ) noexcept;
	static uint32_t normalizeTick( int64_t iTick, int64_t iStartTick, uint32_t nDuration, sirAnimationPlaybackMode_t ePlayback ) noexcept;

protected:
private:
};

#endif // __SIRENGINE_SIRANIMATIONSAMPLER_H__
