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
#ifndef __SIRENGINE_SIRANIMATIONCOOKED_H__
#define __SIRENGINE_SIRANIMATIONCOOKED_H__

#include <SIRPCH.hpp>
// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Animation/Contracts/SIRAnimationTypes.h>

// Encoded assets are little-endian, offset-based and strictly versioned.
// All byte spans are caller-owned. No process pointers or native handles are serialized.
static constexpr uint32_t SIR_ANIM_COOK_MAGIC = 0x4d494e41U;
static constexpr uint16_t SIR_ANIM_COOK_VERSION = 1;
static constexpr uint32_t SIR_ANIM_COOK_MAX_BONES = 4096;
static constexpr uint32_t SIR_ANIM_COOK_MAX_TRACKS = 4096;
static constexpr uint32_t SIR_ANIM_COOK_MAX_SAMPLES = 65536;
static constexpr uint32_t SIR_ANIM_COOK_MAX_METADATA = 65536;

typedef enum sirAnimationCookResult_e : uint8_t
{
	SIR_ANIM_COOK_OK = 0,
	SIR_ANIM_COOK_INVALID,
	SIR_ANIM_COOK_VERSION_ERROR,
	SIR_ANIM_COOK_LIMIT,
	SIR_ANIM_COOK_CAPACITY,
	SIR_ANIM_COOK_CORRUPT,
	SIR_ANIM_COOK_MISSING,
	SIR_ANIM_COOK_UNSUPPORTED
} sirAnimationCookResult_t;

typedef enum sirAnimationCookKind_e : uint8_t
{
	SIR_ANIM_COOK_SKELETON = 1,
	SIR_ANIM_COOK_CLIP = 2
} sirAnimationCookKind_t;

typedef enum sirAnimationTrackEncoding_e : uint8_t
{
	SIR_ANIM_TRACK_CONSTANT = 0,
	SIR_ANIM_TRACK_QUANT16 = 1,
	SIR_ANIM_TRACK_RAW32 = 2
} sirAnimationTrackEncoding_t;

typedef struct sirAnimationCookSpan_s
{
	const uint8_t *pBytes;
	uint64_t nBytes;
} sirAnimationCookSpan_t;

typedef struct sirAnimationCookOutput_s
{
	uint8_t *pBytes;
	uint64_t nCapacity;
	uint64_t nWritten;
} sirAnimationCookOutput_t;

typedef struct sirAnimationCookBone_s
{
	int32_t iParent;
	sirAnimationTransform_t localBind;
	sirAnimationTransform_t inverseBind;
	float fConservativeRadius;
	uint32_t nStableId;
} sirAnimationCookBone_t;

typedef struct sirAnimationCookLodMapping_s
{
	uint32_t nLodLevel;
	uint32_t nSourceBoneIndex;
} sirAnimationCookLodMapping_t;

typedef struct sirAnimationCookSkeletonInput_s
{
	const sirAnimationCookBone_t *pBones;
	uint32_t nBoneCount;
	const sirAnimationCookLodMapping_t *pLodMappings;
	uint32_t nLodMappingCount;
	const uint32_t *pInfluenceBoneIndices;
	uint32_t nInfluenceCount;
	const SIRResourceKey *pDependencies;
	uint32_t nDependencyCount;
	uint64_t nRevision;
} sirAnimationCookSkeletonInput_t;

typedef struct sirAnimationCookKey_s
{
	uint32_t nTime;
	float fComponents[10]; // translation xyz, rotation xyzw, scale xyz
} sirAnimationCookKey_t;

typedef struct sirAnimationCookTrack_s
{
	uint32_t nBoneIndex;
	const sirAnimationCookKey_t *pKeys;
	uint32_t nKeyCount;
	uint8_t bCoarse;
	uint8_t nExecutionMask; // 0 selects CPU+GPU; 1 CPU, 2 GPU, 3 both
	uint8_t nReserved[2];
} sirAnimationCookTrack_t;

typedef struct sirAnimationCookEvent_s
{
	uint32_t nTime;
	uint32_t nEventId;
	uint32_t nPayloadId;
} sirAnimationCookEvent_t;

typedef struct sirAnimationCookCurve_s
{
	uint32_t nCurveId;
	uint32_t nTrackIndex;
} sirAnimationCookCurve_t;

typedef struct sirAnimationCookMorph_s
{
	uint32_t nMorphId;
	uint32_t nTrackIndex;
} sirAnimationCookMorph_t;

typedef struct sirAnimationCookClipInput_s
{
	const sirAnimationCookTrack_t *pTracks;
	uint32_t nTrackCount;
	const sirAnimationCookEvent_t *pEvents;
	uint32_t nEventCount;
	const sirAnimationCookCurve_t *pCurves;
	uint32_t nCurveCount;
	const sirAnimationCookMorph_t *pMorphs;
	uint32_t nMorphCount;
	const SIRResourceKey *pDependencies;
	uint32_t nDependencyCount;
	uint32_t nDuration;
	float fMaximumQuantError;
	float fBoneRadius;
	uint32_t nRootBoneIndex;
	uint64_t nRevision;
} sirAnimationCookClipInput_t;

typedef struct sirAnimationCookTrackOutput_s
{
	sirAnimationCookKey_t *pKeys;
	uint32_t nCapacity;
	uint32_t nWritten;
	bool bUsedCoarse;
	uint8_t nExecutionMask;
} sirAnimationCookTrackOutput_t;

typedef struct sirAnimationCookAssetInfo_s
{
	uint64_t nRevision;
	uint64_t nTotalBytes;
	uint32_t nItemCount;
	uint32_t nLodCount;
	uint32_t nInfluenceCount;
	uint32_t nEventCount;
	uint32_t nCurveCount;
	uint32_t nMorphCount;
	uint32_t nDependencyCount;
	uint32_t nDuration;
	uint32_t nRootBoneIndex;
	sirAnimationBounds_t bounds;
	sirAnimationCookKind_t eKind;
} sirAnimationCookAssetInfo_t;

class SIRAnimationCooked
{
public:
	SIRAnimationCooked( void ) noexcept = delete;

	static sirAnimationCookResult_t inspect( sirAnimationCookSpan_t source, sirAnimationCookAssetInfo_t *pInfo ) noexcept;
	static sirAnimationCookResult_t readBone( sirAnimationCookSpan_t source, uint32_t nIndex, sirAnimationCookBone_t *pBone ) noexcept;
	static sirAnimationCookResult_t readLodMapping( sirAnimationCookSpan_t source, uint32_t nIndex,
	                                                sirAnimationCookLodMapping_t *pMapping ) noexcept;
	static sirAnimationCookResult_t readInfluence( sirAnimationCookSpan_t source, uint32_t nIndex, uint32_t *pBoneIndex ) noexcept;
	static sirAnimationCookResult_t readDependency( sirAnimationCookSpan_t source, uint32_t nIndex, SIRResourceKey *pKey ) noexcept;
	static sirAnimationCookResult_t readTrack( sirAnimationCookSpan_t source, uint32_t nBoneIndex, bool bPreferFine,
	                                           sirAnimationCookTrackOutput_t *pOutput ) noexcept;
	static sirAnimationCookResult_t getTrackCount( sirAnimationCookSpan_t source, uint32_t nBoneIndex, bool bPreferFine,
	                                               uint32_t *pCount ) noexcept;
	static sirAnimationCookResult_t readEvent( sirAnimationCookSpan_t source, uint32_t nIndex, sirAnimationCookEvent_t *pEvent ) noexcept;
	static sirAnimationCookResult_t readCurve( sirAnimationCookSpan_t source, uint32_t nIndex, sirAnimationCookCurve_t *pCurve ) noexcept;
	static sirAnimationCookResult_t readMorph( sirAnimationCookSpan_t source, uint32_t nIndex, sirAnimationCookMorph_t *pMorph ) noexcept;

protected:
private:
};

#endif // __SIRENGINE_SIRANIMATIONCOOKED_H__
