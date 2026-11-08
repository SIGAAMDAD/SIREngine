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

#include <Animation/Contracts/SIRAnimationContractLeasePool.h>
#include <Animation/Contracts/SIRAnimationContractValidation.h>
#include <gtest/gtest.h>

TEST( SIRAnimationContract, VersionAndAssetIdentity )
{
	sirAnimationContractVersion_t version = { sizeof( sirAnimationContractVersion_t ), SIR_ANIMATION_CONTRACT_MAJOR,
	                                          SIR_ANIMATION_CONTRACT_MINOR, 0 };
	EXPECT_EQ( SIR_ANIMATION_SUCCESS, SIRAnimationContractValidation::validateVersion( version ) );
	version.nMajor++;
	EXPECT_EQ( SIR_ANIMATION_VERSION_MISMATCH, SIRAnimationContractValidation::validateVersion( version ) );
	sirAnimationAssetRef_t asset = {};
	asset.keyResource.m_nAssetId = 10;
	asset.keyResource.m_nTypeId = 4;
	asset.nRevision = 7;
	asset.nCookedFormatId = 1;
	asset.nSchemaVersion = 1;
	asset.eKind = SIR_ANIMATION_ASSET_CLIP;
	EXPECT_EQ( SIR_ANIMATION_SUCCESS, SIRAnimationContractValidation::validateAsset( asset, SIR_ANIMATION_ASSET_CLIP, 7 ) );
	EXPECT_EQ( SIR_ANIMATION_STALE_ASSET, SIRAnimationContractValidation::validateAsset( asset, SIR_ANIMATION_ASSET_CLIP, 8 ) );
}

TEST( SIRAnimationContract, RangeOverflowAndScratchBounds )
{
	EXPECT_EQ( SIR_ANIMATION_OUT_OF_CAPACITY, SIRAnimationContractValidation::validateBufferRange( UINT64_MAX, 8, 2, UINT64_MAX ) );
	EXPECT_EQ( SIR_ANIMATION_SUCCESS, SIRAnimationContractValidation::validateBufferRange( 0, 8, 4, 32 ) );
	sirAnimationGraphRequest_t request = {};
	request.nMaximumEvents = 2;
	sirAnimationGraphScratch_t scratch = {};
	EXPECT_EQ( SIR_ANIMATION_OUT_OF_CAPACITY, SIRAnimationContractValidation::validateGraphScratch( request, scratch ) );
}

TEST( SIRAnimationContract, CancellationRetainsActiveBorrow )
{
	sirAnimationContractPoseSlot_t poses[1] = {};
	sirAnimationContractReaderSlot_t readers[2] = {};
	SIRAnimationContractLeasePool pool;
	ASSERT_EQ( SIR_ANIMATION_SUCCESS, pool.initialize( poses, 1, readers, 2 ) );
	sirAnimationInstance_t instance = { 99, 1, 1, 0, 1 };
	sirAnimationTick_t tick = { 25, 10, 0, 1 };
	ASSERT_EQ( SIR_ANIMATION_SUCCESS, pool.registerPose( { 0, 1 }, instance, tick ) );
	sirAnimationPoseLease_t lease = {};
	ASSERT_EQ( SIR_ANIMATION_SUCCESS, pool.acquire( { 0, 1 }, &lease ) );
	EXPECT_EQ( SIR_ANIMATION_SUCCESS, pool.cancelEpoch( 10, 1 ) );
	EXPECT_EQ( SIR_ANIMATION_STALE_POSE, pool.acquire( { 0, 1 }, &lease ) );
	EXPECT_EQ( SIR_ANIMATION_BUSY, pool.retirePose( { 0, 1 } ) );
	EXPECT_EQ( 1U, pool.getOutstandingReaders() );
	EXPECT_EQ( SIR_ANIMATION_SUCCESS, pool.release( lease ) );
	EXPECT_EQ( SIR_ANIMATION_INVALID_HANDLE, pool.release( lease ) );
	EXPECT_EQ( SIR_ANIMATION_SUCCESS, pool.retirePose( { 0, 1 } ) );
	EXPECT_EQ( SIR_ANIMATION_SUCCESS, pool.shutdown() );
}

TEST( SIRAnimationContract, FixedFunctionFallsBackToCPU )
{
	sirGraphicsCapabilities_t graphics = {};
	graphics.nFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_FIXED_FUNCTION;
	graphics.nPipelineModelMask = 1U << ( SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION - 1 );
	sirAnimationDeformationMode_t selected = SIR_ANIMATION_DEFORM_COMPUTE;
	ASSERT_EQ( SIR_ANIMATION_SUCCESS,
	           SIRAnimationContractValidation::selectDeformation( graphics, SIR_ANIMATION_DEFORM_VERTEX, &selected ) );
	EXPECT_EQ( SIR_ANIMATION_DEFORM_CPU, selected );
}
