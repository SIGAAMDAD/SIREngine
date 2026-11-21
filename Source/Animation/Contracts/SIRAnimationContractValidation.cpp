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

#include "SIRAnimationContractValidation.h"

#include <cmath>
#include <limits>

sirAnimationResult_t SIRAnimationContractValidation::validateVersion( const sirAnimationContractVersion_t& requested ) noexcept
{
	if ( requested.nMajor != SIR_ANIMATION_CONTRACT_MAJOR || requested.nStructSize < sizeof( sirAnimationContractVersion_t ) ) {
		return SIR_ANIMATION_VERSION_MISMATCH;
	}
	if ( requested.nMinor > SIR_ANIMATION_CONTRACT_MINOR || requested.nReserved != 0 ) {
		return SIR_ANIMATION_VERSION_MISMATCH;
	}
	return SIR_ANIMATION_SUCCESS;
}

sirAnimationResult_t SIRAnimationContractValidation::validateCapacity( const sirAnimationCapacityContract_t& capacity ) noexcept
{
	if ( !capacity.nMaximumInstances || !capacity.nMaximumBonesPerInstance || !capacity.nMaximumPoseSlots ||
	     !capacity.nMaximumConcurrentReaders || !capacity.nCpuScratchBudgetBytes || !capacity.nPoseStorageBudgetBytes ) {
		return SIR_ANIMATION_INVALID_ARGUMENT;
	}
	if ( validateBufferRange( 0, sizeof( sirAnimationTransform_t ), capacity.nMaximumBonesPerInstance, capacity.nCpuScratchBudgetBytes ) !=
	     SIR_ANIMATION_SUCCESS ) {
		return SIR_ANIMATION_OUT_OF_CAPACITY;
	}
	const uint64_t nBytesPerPose = ( uint64_t )sizeof( sirAnimationTransform_t ) * capacity.nMaximumBonesPerInstance;
	if ( validateBufferRange( 0, nBytesPerPose, capacity.nMaximumPoseSlots, capacity.nPoseStorageBudgetBytes ) != SIR_ANIMATION_SUCCESS ) {
		return SIR_ANIMATION_OUT_OF_CAPACITY;
	}
	return SIR_ANIMATION_SUCCESS;
}

sirAnimationResult_t SIRAnimationContractValidation::validateAsset( const sirAnimationAssetRef_t& asset,
                                                                    sirAnimationAssetKind_t eExpectedKind,
                                                                    uint64_t nExpectedRevision ) noexcept
{
	if ( asset.eKind != eExpectedKind || asset.eKind > SIR_ANIMATION_ASSET_RETARGET || asset.keyResource.m_nAssetId == 0 ||
	     asset.keyResource.m_nTypeId == 0 || asset.nCookedFormatId == 0 || asset.nSchemaVersion == 0 || asset.nReserved != 0 ) {
		return SIR_ANIMATION_INVALID_ARGUMENT;
	}
	if ( asset.nRevision == 0 || asset.nRevision != nExpectedRevision ) {
		return SIR_ANIMATION_STALE_ASSET;
	}
	return SIR_ANIMATION_SUCCESS;
}

sirAnimationResult_t SIRAnimationContractValidation::validateInstance( const sirAnimationInstance_t& instance ) noexcept
{
	if ( instance.nEntityId == 0 || instance.nEntityGeneration == 0 || instance.nWorldGeneration == 0 || instance.nSlotGeneration == 0 ) {
		return SIR_ANIMATION_INVALID_HANDLE;
	}
	return SIR_ANIMATION_SUCCESS;
}

sirAnimationResult_t SIRAnimationContractValidation::validateBoneSelection( const sirAnimationBoneSelection_t& bones,
                                                                            uint32_t nMaximumBones ) noexcept
{
	if ( nMaximumBones == 0 || bones.nBoneCount > nMaximumBones || ( bones.nBoneCount != 0 && bones.pBoneIndices == nullptr ) ) {
		return SIR_ANIMATION_INVALID_ARGUMENT;
	}
	for ( uint32_t i = 0; i < bones.nBoneCount; ++i ) {
		if ( bones.pBoneIndices[i] >= nMaximumBones ) {
			return SIR_ANIMATION_OUT_OF_CAPACITY;
		}
		if ( i != 0 && bones.pBoneIndices[i] <= bones.pBoneIndices[i - 1] ) {
			return SIR_ANIMATION_INVALID_ARGUMENT;
		}
	}
	return SIR_ANIMATION_SUCCESS;
}

sirAnimationResult_t SIRAnimationContractValidation::validateStateAdvance( const sirAnimationStateSnapshot_t& current,
                                                                           const sirAnimationStateSnapshot_t& next ) noexcept
{
	if ( validateInstance( current.instance ) != SIR_ANIMATION_SUCCESS || validateInstance( next.instance ) != SIR_ANIMATION_SUCCESS ) {
		return SIR_ANIMATION_INVALID_HANDLE;
	}
	if ( memcmp( &current.instance, &next.instance, sizeof( current.instance ) ) != 0 ||
	     current.tick.nWorldEpoch != next.tick.nWorldEpoch || current.tick.nCancellationGeneration != next.tick.nCancellationGeneration ) {
		return SIR_ANIMATION_STALE_ENTITY;
	}
	if ( memcmp( &current.graph, &next.graph, sizeof( current.graph ) ) != 0 ) {
		return SIR_ANIMATION_STALE_ASSET;
	}
	if ( current.nStateRevision == UINT64_MAX || next.nStateRevision != current.nStateRevision + 1 ||
	     next.tick.nFixedTick < current.tick.nFixedTick ||
	     ( next.tick.nFixedTick == current.tick.nFixedTick && next.tick.nSubstep < current.tick.nSubstep ) ||
	     next.nEventCursor < current.nEventCursor || next.nRootMotionCursor < current.nRootMotionCursor ) {
		return SIR_ANIMATION_STALE_POSE;
	}
	return SIR_ANIMATION_SUCCESS;
}

sirAnimationResult_t SIRAnimationContractValidation::validatePublication( const sirAnimationPosePublication_t& pose,
                                                                          uint32_t nMaximumBones ) noexcept
{
	if ( validateInstance( pose.instance ) != SIR_ANIMATION_SUCCESS ) {
		return SIR_ANIMATION_STALE_ENTITY;
	}
	if ( validateAsset( pose.rig, SIR_ANIMATION_ASSET_SKELETON, pose.rig.nRevision ) != SIR_ANIMATION_SUCCESS ) {
		return SIR_ANIMATION_STALE_ASSET;
	}
	if ( pose.nTransformCount == 0 || pose.nTransformCount > nMaximumBones || pose.pModelTransforms == nullptr ) {
		return SIR_ANIMATION_OUT_OF_CAPACITY;
	}
	for ( uint32_t i = 0; i < pose.nTransformCount; ++i ) {
		const sirAnimationTransform_t& t = pose.pModelTransforms[i];
		for ( float f : t.fTranslation ) {
			if ( !std::isfinite( f ) ) {
				return SIR_ANIMATION_INVALID_ARGUMENT;
			}
		}
		for ( float f : t.fRotation ) {
			if ( !std::isfinite( f ) ) {
				return SIR_ANIMATION_INVALID_ARGUMENT;
			}
		}
		for ( float f : t.fScale ) {
			if ( !std::isfinite( f ) ) {
				return SIR_ANIMATION_INVALID_ARGUMENT;
			}
		}
	}
	for ( uint32_t i = 0; i < 3; ++i ) {
		if ( !std::isfinite( pose.conservativeBounds.fMinimum[i] ) || !std::isfinite( pose.conservativeBounds.fMaximum[i] ) ||
		     pose.conservativeBounds.fMinimum[i] > pose.conservativeBounds.fMaximum[i] ) {
			return SIR_ANIMATION_INVALID_ARGUMENT;
		}
	}
	return SIR_ANIMATION_SUCCESS;
}

sirAnimationResult_t SIRAnimationContractValidation::validateGraphScratch( const sirAnimationGraphRequest_t& request,
                                                                           const sirAnimationGraphScratch_t& scratch ) noexcept
{
	if ( request.nMaximumLayers > scratch.nLayerCapacity || request.nMaximumEvents > scratch.nEventCapacity ) {
		return SIR_ANIMATION_OUT_OF_CAPACITY;
	}
	if ( ( request.nMaximumLayers && !scratch.pLayers ) || ( request.nMaximumEvents && !scratch.pEvents ) ) {
		return SIR_ANIMATION_INVALID_ARGUMENT;
	}
	return SIR_ANIMATION_SUCCESS;
}

sirAnimationResult_t SIRAnimationContractValidation::validateBufferRange( uint64_t nOffset, uint64_t nStride, uint64_t nCount,
                                                                          uint64_t nCapacity ) noexcept
{
	if ( nCount != 0 && nStride == 0 ) {
		return SIR_ANIMATION_INVALID_ARGUMENT;
	}
	if ( nCount != 0 && nStride > ( UINT64_MAX - nOffset ) / nCount ) {
		return SIR_ANIMATION_OUT_OF_CAPACITY;
	}
	if ( nOffset > nCapacity || nCount * nStride > nCapacity - nOffset ) {
		return SIR_ANIMATION_OUT_OF_CAPACITY;
	}
	return SIR_ANIMATION_SUCCESS;
}

sirAnimationResult_t SIRAnimationContractValidation::validateAuthorityAck( const sirAnimationAuthorityRequest_t& request,
                                                                           const sirAnimationAuthorityAck_t& acknowledgement ) noexcept
{
	if ( validateInstance( request.instance ) != SIR_ANIMATION_SUCCESS ) {
		return SIR_ANIMATION_STALE_ENTITY;
	}
	if ( request.eNewOwner > SIR_ANIMATION_AUTHORITY_PHYSICS || acknowledgement.eOwner != request.eNewOwner ) {
		return SIR_ANIMATION_AUTHORITY_CONFLICT;
	}
	if ( acknowledgement.effectiveTick.nFixedTick != request.effectiveTick.nFixedTick ||
	     acknowledgement.effectiveTick.nWorldEpoch != request.effectiveTick.nWorldEpoch ||
	     acknowledgement.effectiveTick.nSubstep != request.effectiveTick.nSubstep ||
	     acknowledgement.effectiveTick.nCancellationGeneration != request.effectiveTick.nCancellationGeneration ||
	     request.nExpectedAuthorityGeneration == UINT64_MAX ||
	     acknowledgement.nAuthorityGeneration != request.nExpectedAuthorityGeneration + 1 ) {
		return SIR_ANIMATION_STALE_POSE;
	}
	return SIR_ANIMATION_SUCCESS;
}

sirAnimationResult_t SIRAnimationContractValidation::validateGraphicsCompletion( const sirGraphicsCompletionSet_t& completion,
                                                                                 uint32_t nExpectedDeviceGeneration ) noexcept
{
	const uint32_t nAllLanes = ( 1U << SIR_GRAPHICS_QUEUE_COUNT ) - 1U;
	if ( nExpectedDeviceGeneration == 0 || completion.nDeviceGeneration != nExpectedDeviceGeneration ||
	     ( completion.nQueueMask & ~nAllLanes ) != 0 ) {
		return SIR_ANIMATION_STALE_POSE;
	}
	for ( uint32_t i = 0; i < SIR_GRAPHICS_QUEUE_COUNT; ++i ) {
		if ( ( completion.nQueueMask & ( 1U << i ) ) != 0 && completion.szQueueValues[i] == 0 ) {
			return SIR_ANIMATION_INVALID_ARGUMENT;
		}
	}
	return SIR_ANIMATION_SUCCESS;
}

sirAnimationResult_t SIRAnimationContractValidation::selectDeformation( const sirGraphicsCapabilities_t& graphics,
                                                                        sirAnimationDeformationMode_t ePreferred,
                                                                        sirAnimationDeformationMode_t *pSelected ) noexcept
{
	if ( !pSelected || ePreferred > SIR_ANIMATION_DEFORM_COMPUTE ) {
		return SIR_ANIMATION_INVALID_ARGUMENT;
	}
	const bool bCompute = ( graphics.nFeatureMask & SIR_GRAPHICS_FEATURE_COMPUTE ) != 0 &&
	                      ( graphics.nFeatureMask & SIR_GRAPHICS_FEATURE_SHADER_STORAGE ) != 0;
	const uint32_t nProgrammableModels =
	    ( 1U << ( SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE - 1 ) ) | ( 1U << ( SIR_GRAPHICS_PIPELINE_MODEL_MODERN - 1 ) );
	const bool bVertex =
	    ( graphics.nFeatureMask & SIR_GRAPHICS_FEATURE_GRAPHICS ) != 0 && ( graphics.nPipelineModelMask & nProgrammableModels ) != 0;
	*pSelected = ( ePreferred == SIR_ANIMATION_DEFORM_COMPUTE && bCompute ) ? SIR_ANIMATION_DEFORM_COMPUTE
	             : ( ePreferred != SIR_ANIMATION_DEFORM_CPU && bVertex )    ? SIR_ANIMATION_DEFORM_VERTEX
	                                                                        : SIR_ANIMATION_DEFORM_CPU;
	return SIR_ANIMATION_SUCCESS;
}
