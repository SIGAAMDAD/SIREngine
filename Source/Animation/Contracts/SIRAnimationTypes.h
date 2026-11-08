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

#ifndef __SIRENGINE_SIRANIMATIONTYPES_H__
#define __SIRENGINE_SIRANIMATIONTYPES_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Resources/SIRResourceTypes.h>

// This header specifies the animation-owned contract, not a cooked SIRPack layout.
// Resource keys/revisions belong to GRDG; entity generations belong to World.
// No native driver or physics-provider type is permitted in this interface.

// Version negotiation is independent of cooked skeleton/clip schema versions.
// A reader accepts same-major contracts up to its implemented minor version.
static constexpr uint16_t SIR_ANIMATION_CONTRACT_MAJOR = 1;
static constexpr uint16_t SIR_ANIMATION_CONTRACT_MINOR = 0;

typedef struct sirAnimationContractVersion_s
{
	uint32_t nStructSize;
	uint16_t nMajor;
	uint16_t nMinor;
	uint32_t nReserved;
} sirAnimationContractVersion_t;

// Memory is preallocated/accounted by SIRMemory, without allocation in an
// animation update. Only the animation publisher owner mutates ticket tables;
// concurrent clients retain immutable views until owner-affine release.
typedef struct sirAnimationCapacityContract_s
{
	uint64_t nCpuScratchBudgetBytes;
	uint64_t nPoseStorageBudgetBytes;
	uint64_t nGpuDerivedBudgetBytes;
	uint32_t nMaximumInstances;
	uint32_t nMaximumBonesPerInstance;
	uint32_t nMaximumPoseSlots;
	uint32_t nMaximumConcurrentReaders;
} sirAnimationCapacityContract_t;

typedef enum sirAnimationResult_e : uint8_t
{
	SIR_ANIMATION_SUCCESS = 0,
	SIR_ANIMATION_ACCEPTED,
	SIR_ANIMATION_PENDING,
	SIR_ANIMATION_INVALID_ARGUMENT,
	SIR_ANIMATION_INVALID_HANDLE,
	SIR_ANIMATION_STALE_ENTITY,
	SIR_ANIMATION_STALE_ASSET,
	SIR_ANIMATION_STALE_POSE,
	SIR_ANIMATION_VERSION_MISMATCH,
	SIR_ANIMATION_NOT_RESIDENT,
	SIR_ANIMATION_OUT_OF_CAPACITY,
	SIR_ANIMATION_AUTHORITY_CONFLICT,
	SIR_ANIMATION_UNSUPPORTED,
	SIR_ANIMATION_BUSY,
	SIR_ANIMATION_CANCELLED,
	SIR_ANIMATION_FAILED
} sirAnimationResult_t;

typedef enum sirAnimationAssetKind_e : uint8_t
{
	SIR_ANIMATION_ASSET_SKELETON = 0,
	SIR_ANIMATION_ASSET_CLIP,
	SIR_ANIMATION_ASSET_GRAPH,
	SIR_ANIMATION_ASSET_MORPH,
	SIR_ANIMATION_ASSET_RETARGET
} sirAnimationAssetKind_t;

typedef enum sirAnimationAuthority_e : uint8_t
{
	SIR_ANIMATION_AUTHORITY_ANIMATION = 0,
	SIR_ANIMATION_AUTHORITY_PHYSICS
} sirAnimationAuthority_t;

typedef enum sirAnimationDeformationMode_e : uint8_t
{
	SIR_ANIMATION_DEFORM_CPU = 0,
	SIR_ANIMATION_DEFORM_VERTEX,
	SIR_ANIMATION_DEFORM_COMPUTE
} sirAnimationDeformationMode_t;

typedef enum sirAnimationResidency_e : uint8_t
{
	SIR_ANIMATION_RESIDENCY_FULL = 0,
	SIR_ANIMATION_RESIDENCY_REDUCED,
	SIR_ANIMATION_RESIDENCY_COARSE,
	SIR_ANIMATION_RESIDENCY_DORMANT
} sirAnimationResidency_t;

typedef struct sirAnimationAssetRef_s
{
	SIRResourceKey keyResource;
	uint64_t nRevision;
	uint32_t nCookedFormatId;
	uint16_t nSchemaVersion;
	sirAnimationAssetKind_t eKind;
	uint8_t nReserved;
} sirAnimationAssetRef_t;

// An instance is process-local and never serialized as a cooked source ID.
typedef struct sirAnimationInstance_s
{
	uint64_t nEntityId;
	uint32_t nEntityGeneration;
	uint32_t nWorldGeneration;
	uint32_t nSlot;
	uint32_t nSlotGeneration;
} sirAnimationInstance_t;

typedef struct sirAnimationTick_s
{
	uint64_t nFixedTick;
	uint64_t nWorldEpoch;
	uint32_t nSubstep;
	uint32_t nCancellationGeneration;
} sirAnimationTick_t;

typedef struct sirAnimationTransform_s
{
	float fTranslation[3];
	float fRotation[4];
	float fScale[3];
} sirAnimationTransform_t;

typedef struct sirAnimationBounds_s
{
	float fMinimum[3];
	float fMaximum[3];
} sirAnimationBounds_t;

// Caller-owned, borrowed selection; an empty selection means the full rig.
typedef struct sirAnimationBoneSelection_s
{
	const uint32_t *pBoneIndices;
	uint32_t nBoneCount;
	uint32_t nSelectionRevision;
} sirAnimationBoneSelection_t;

typedef struct sirAnimationInstanceDesc_s
{
	sirAnimationInstance_t identity;
	sirAnimationAssetRef_t rig;
	sirAnimationAssetRef_t graph;
	uint32_t nInitialVisualLod;
	uint32_t nRequiredGameplayBoneCount;
} sirAnimationInstanceDesc_t;

// Snapshot of SoA state, never a second mutable owner or a serialized memory image.
typedef struct sirAnimationStateSnapshot_s
{
	sirAnimationInstance_t instance;
	sirAnimationAssetRef_t graph;
	sirAnimationTick_t tick;
	uint64_t nStateRevision;
	uint64_t nGraphNode;
	uint64_t nClipPhaseTick;
	uint64_t nEventCursor;
	uint64_t nRootMotionCursor;
} sirAnimationStateSnapshot_t;

typedef struct sirAnimationGraphLayer_s
{
	sirAnimationAssetRef_t clip;
	uint64_t nSampleTick;
	uint32_t nMaskId;
	float fWeight;
	uint8_t bAdditive;
	uint8_t nReserved[3];
} sirAnimationGraphLayer_t;

typedef struct sirAnimationEvent_s
{
	uint64_t nEventId;
	uint64_t nEventTick;
	uint32_t nEventType;
	uint32_t nPayloadId;
} sirAnimationEvent_t;

// Layer/event spans are borrowed from caller-supplied bounded work memory.
typedef struct sirAnimationGraphOutput_s
{
	const sirAnimationGraphLayer_t *pLayers;
	const sirAnimationEvent_t *pEvents;
	uint32_t nLayerCount;
	uint32_t nEventCount;
	uint64_t nNextEventCursor;
	sirAnimationTransform_t rootMotionDelta;
} sirAnimationGraphOutput_t;

// Bounded graph output storage is supplied by the evaluating caller.
typedef struct sirAnimationGraphScratch_s
{
	sirAnimationGraphLayer_t *pLayers;
	sirAnimationEvent_t *pEvents;
	uint32_t nLayerCapacity;
	uint32_t nEventCapacity;
} sirAnimationGraphScratch_t;

typedef struct sirAnimationGraphRequest_s
{
	sirAnimationStateSnapshot_t state;
	sirAnimationTick_t targetTick;
	uint32_t nMaximumLayers;
	uint32_t nMaximumEvents;
} sirAnimationGraphRequest_t;

typedef struct sirAnimationClipSampleRequest_s
{
	sirAnimationAssetRef_t clip;
	uint64_t nSampleTick;
	sirAnimationBoneSelection_t requiredBones;
	uint32_t nInterpolationMode;
} sirAnimationClipSampleRequest_t;

typedef struct sirAnimationMutablePose_s
{
	sirAnimationTransform_t *pLocalTransforms;
	uint32_t nTransformCapacity;
	uint32_t nTransformCount;
} sirAnimationMutablePose_t;

typedef struct sirAnimationRetargetRequest_s
{
	sirAnimationAssetRef_t sourceRig;
	sirAnimationAssetRef_t targetRig;
	sirAnimationTick_t tick;
	sirAnimationBoneSelection_t requiredBones;
	uint32_t nProfileId;
} sirAnimationRetargetRequest_t;

typedef struct sirAnimationPoseEvaluation_s
{
	sirAnimationInstance_t instance;
	sirAnimationAssetRef_t rig;
	sirAnimationTick_t tick;
	sirAnimationBoneSelection_t requiredBones;
	uint32_t nVisualLod;
	uint32_t nMaximumScratchBytes;
} sirAnimationPoseEvaluation_t;

typedef struct sirAnimationPoseHandle_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
} sirAnimationPoseHandle_t;

typedef struct sirAnimationPoseLease_s
{
	sirAnimationPoseHandle_t pose;
	uint64_t nReaderEpoch;
	uint32_t nLeaseGeneration;
	uint32_t nReserved;
} sirAnimationPoseLease_t;

// Only a completed, single-writer pose may be published. The publisher copies
// into its owned bounded pose slots before returning success.
typedef struct sirAnimationPosePublication_s
{
	sirAnimationInstance_t instance;
	sirAnimationAssetRef_t rig;
	sirAnimationTick_t tick;
	const sirAnimationTransform_t *pModelTransforms;
	uint32_t nTransformCount;
	uint32_t nMaskRevision;
	sirAnimationBounds_t conservativeBounds;
} sirAnimationPosePublication_t;

// Immutable and valid only until releasePose of this exact lease; all render
// passes and physics/attachment readers must retain the same coherent version.
typedef struct sirAnimationPoseView_s
{
	sirAnimationPoseLease_t lease;
	sirAnimationInstance_t instance;
	sirAnimationAssetRef_t rig;
	sirAnimationTick_t tick;
	const sirAnimationTransform_t *pModelTransforms;
	uint32_t nTransformCount;
	uint32_t nMaskRevision;
	sirAnimationBounds_t conservativeBounds;
} sirAnimationPoseView_t;

typedef struct sirAnimationEventBatch_s
{
	sirAnimationInstance_t instance;
	sirAnimationTick_t tick;
	const sirAnimationEvent_t *pEvents;
	uint32_t nEventCount;
	uint64_t nExpectedCursor;
	uint64_t nNextCursor;
	sirAnimationTransform_t rootMotionDelta;
} sirAnimationEventBatch_t;

typedef struct sirAnimationCostHint_s
{
	sirAnimationInstance_t instance;
	sirAnimationTick_t tick;
	uint64_t nDeadlineTick;
	uint32_t nRequiredBoneCount;
	uint32_t nVisibleBoneCount;
	uint32_t nEstimatedCpuMicroseconds;
	uint32_t nEstimatedGpuMicroseconds;
	uint64_t nEstimatedScratchBytes;
	float fProjectedVisualError;
} sirAnimationCostHint_t;

typedef struct sirAnimationBudgetGrant_s
{
	uint32_t nAdmittedCpuMicroseconds;
	uint32_t nAdmittedGpuMicroseconds;
	uint64_t nAdmittedScratchBytes;
	uint32_t nMaximumVisualLod;
	uint32_t nMinimumGameplayUpdateHz;
} sirAnimationBudgetGrant_t;

typedef struct sirAnimationQualityDecision_s
{
	sirAnimationDeformationMode_t eDeformation;
	uint32_t nVisualLod;
	uint32_t nVisualUpdateIntervalTicks;
	uint32_t nPoseSharingClass;
	uint8_t bInterpolateVisualPose;
	uint8_t nReserved[3];
} sirAnimationQualityDecision_t;

typedef struct sirAnimationSkinRequest_s
{
	sirAnimationPoseHandle_t pose;
	sirAnimationAssetRef_t rig;
	SIRResourceKey keyBindMesh;
	uint64_t nBindMeshRevision;
	sirAnimationTick_t tick;
	sirAnimationDeformationMode_t eMode;
	uint32_t nMeshLod;
	uint32_t nDeviceGeneration;
} sirAnimationSkinRequest_t;

typedef struct sirAnimationSkinTicket_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
	uint32_t nDeviceGeneration;
	uint32_t nReserved;
} sirAnimationSkinTicket_t;

typedef struct sirAnimationAuthorityRequest_s
{
	sirAnimationInstance_t instance;
	sirAnimationTick_t effectiveTick;
	sirAnimationBoneSelection_t bones;
	sirAnimationAuthority_t eNewOwner;
	uint64_t nExpectedAuthorityGeneration;
} sirAnimationAuthorityRequest_t;

// Physics-owned asynchronous safe-point transaction. A nonterminal ticket
// cannot be interpreted as exclusive bone ownership.
typedef struct sirAnimationAuthorityTicket_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
	uint32_t nWorldGeneration;
	uint32_t nReserved;
} sirAnimationAuthorityTicket_t;

typedef struct sirAnimationAuthorityAck_s
{
	sirAnimationTick_t effectiveTick;
	sirAnimationAuthority_t eOwner;
	uint64_t nAuthorityGeneration;
} sirAnimationAuthorityAck_t;

// Physics provider owns the source and retains its own snapshot lease until
// animation finishes ingesting the completed fixed-tick transforms.
typedef struct sirAnimationPhysicsPoseView_s
{
	const uint32_t *pBoneIndices;
	const sirAnimationTransform_t *pModelTransforms;
	uint32_t nBoneCount;
	uint64_t nPhysicsRevision;
	uint64_t nProviderLease;
	uint32_t nProviderLeaseGeneration;
	uint32_t nReserved;
	sirAnimationAuthorityAck_t authority;
} sirAnimationPhysicsPoseView_t;

typedef struct sirAnimationResidencyChange_s
{
	sirAnimationInstance_t instance;
	sirAnimationResidency_t eWorldResidency;
	sirAnimationTick_t tick;
	uint32_t nWorldGeneration;
} sirAnimationResidencyChange_t;

typedef struct sirAnimationStats_s
{
	uint64_t nEvaluatedPoses;
	uint64_t nSharedPoses;
	uint64_t nRejectedStaleTasks;
	uint64_t nPublishedEvents;
	uint64_t nCpuScratchHighWaterBytes;
	uint64_t nSkinningDerivedBytes;
} sirAnimationStats_t;

static_assert( sizeof( sirAnimationContractVersion_t ) == 12, "Animation contract version ABI layout" );
static_assert( sizeof( SIRResourceKey ) == 16, "Animation asset keys use canonical GRDG layout" );
static_assert( sizeof( sirAnimationAssetRef_t ) == 32, "Animation assets carry a canonical 16-byte GRDG key and revision" );
static_assert( sizeof( sirAnimationPoseHandle_t ) == 8, "Pose handles are generation tagged" );

#endif // __SIRENGINE_SIRANIMATIONTYPES_H__
