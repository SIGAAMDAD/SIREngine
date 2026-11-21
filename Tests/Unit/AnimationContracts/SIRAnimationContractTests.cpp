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

#include <Animation/Contracts/SIRAnimationSystem.h>
#include <Animation/Contracts/SIRAnimationContractLeasePool.h>
#include <Animation/Contracts/SIRAnimationContractValidation.h>
#include <Core/Jobs/SIRTaskScheduler.h>
#include <Resources/SIRResourceGraph.h>
#include <Renderer/SIRRenderingDevice.h>

#include <cstdio>
#include <limits>
#include <type_traits>

static uint32_t s_nChecks = 0;
static uint32_t s_nFailures = 0;

#define CHECK( expression )                                                                                                                \
	do {                                                                                                                                   \
		++s_nChecks;                                                                                                                       \
		if ( !( expression ) ) {                                                                                                           \
			++s_nFailures;                                                                                                                 \
			fprintf( stderr, "ANM-00 FAIL:%u: %s\n", ( unsigned )__LINE__, #expression );                                                  \
		}                                                                                                                                  \
	} while ( 0 )

static sirAnimationAssetRef_t createAsset( uint64_t nRevision, sirAnimationAssetKind_t eKind )
{
	sirAnimationAssetRef_t asset = {};
	asset.keyResource.m_nAssetId = 10;
	asset.keyResource.m_nTypeId = 4;
	asset.nRevision = nRevision;
	asset.nCookedFormatId = 1;
	asset.nSchemaVersion = 1;
	asset.eKind = eKind;
	return asset;
}

static sirAnimationInstance_t createInstance( uint32_t nGeneration )
{
	sirAnimationInstance_t instance = {};
	instance.nEntityId = 7;
	instance.nEntityGeneration = nGeneration;
	instance.nWorldGeneration = 2;
	instance.nSlot = 0;
	instance.nSlotGeneration = 1;
	return instance;
}

static sirAnimationTick_t createTick( uint64_t nTick )
{
	sirAnimationTick_t tick = {};
	tick.nFixedTick = nTick;
	tick.nWorldEpoch = 3;
	tick.nCancellationGeneration = 1;
	return tick;
}

static void checkAbi( void )
{
	static_assert( std::is_standard_layout<sirAnimationAssetRef_t>::value, "POD API" );
	static_assert( std::is_trivially_copyable<sirAnimationAssetRef_t>::value, "POD API" );
	static_assert( std::is_standard_layout<sirAnimationPoseLease_t>::value, "POD API" );
	static_assert( std::is_trivially_copyable<sirAnimationPoseLease_t>::value, "POD API" );
	CHECK( sizeof( sirAnimationAssetRef_t ) == 32 );
	CHECK( sizeof( sirAnimationContractVersion_t ) == 12 );
	const sirAnimationContractVersion_t current = { sizeof( sirAnimationContractVersion_t ), SIR_ANIMATION_CONTRACT_MAJOR,
	                                                SIR_ANIMATION_CONTRACT_MINOR, 0 };
	CHECK( SIRAnimationContractValidation::validateVersion( current ) == SIR_ANIMATION_SUCCESS );
	sirAnimationContractVersion_t copy = {};
	memcpy( &copy, &current, sizeof( copy ) );
	CHECK( memcmp( &copy, &current, sizeof( copy ) ) == 0 );
	CHECK( SIRAnimationContractValidation::validateVersion( copy ) == SIR_ANIMATION_SUCCESS );
	copy.nMajor++;
	CHECK( SIRAnimationContractValidation::validateVersion( copy ) == SIR_ANIMATION_VERSION_MISMATCH );
	copy = current;
	copy.nMinor++;
	CHECK( SIRAnimationContractValidation::validateVersion( copy ) == SIR_ANIMATION_VERSION_MISMATCH );
	copy = current;
	copy.nStructSize--;
	CHECK( SIRAnimationContractValidation::validateVersion( copy ) == SIR_ANIMATION_VERSION_MISMATCH );
	copy = current;
	copy.nReserved = 1;
	CHECK( SIRAnimationContractValidation::validateVersion( copy ) == SIR_ANIMATION_VERSION_MISMATCH );
}

static void checkCapacities( void )
{
	sirAnimationCapacityContract_t capacity = {};
	capacity.nMaximumInstances = 2;
	capacity.nMaximumBonesPerInstance = 64;
	capacity.nMaximumPoseSlots = 3;
	capacity.nMaximumConcurrentReaders = 10;
	capacity.nCpuScratchBudgetBytes = 64 * sizeof( sirAnimationTransform_t );
	capacity.nPoseStorageBudgetBytes = 3 * capacity.nCpuScratchBudgetBytes;
	CHECK( SIRAnimationContractValidation::validateCapacity( capacity ) == SIR_ANIMATION_SUCCESS );
	capacity.nMaximumInstances = 0;
	CHECK( SIRAnimationContractValidation::validateCapacity( capacity ) == SIR_ANIMATION_INVALID_ARGUMENT );
	capacity.nMaximumInstances = 2;
	capacity.nMaximumBonesPerInstance = 0;
	CHECK( SIRAnimationContractValidation::validateCapacity( capacity ) == SIR_ANIMATION_INVALID_ARGUMENT );
	capacity.nMaximumBonesPerInstance = 64;
	capacity.nMaximumPoseSlots = 0;
	CHECK( SIRAnimationContractValidation::validateCapacity( capacity ) == SIR_ANIMATION_INVALID_ARGUMENT );
	capacity.nMaximumPoseSlots = 3;
	capacity.nMaximumConcurrentReaders = 0;
	CHECK( SIRAnimationContractValidation::validateCapacity( capacity ) == SIR_ANIMATION_INVALID_ARGUMENT );
	capacity.nMaximumConcurrentReaders = 10;
	capacity.nCpuScratchBudgetBytes = 0;
	CHECK( SIRAnimationContractValidation::validateCapacity( capacity ) == SIR_ANIMATION_INVALID_ARGUMENT );
	capacity.nCpuScratchBudgetBytes = 64 * sizeof( sirAnimationTransform_t );
	capacity.nPoseStorageBudgetBytes = 0;
	CHECK( SIRAnimationContractValidation::validateCapacity( capacity ) == SIR_ANIMATION_INVALID_ARGUMENT );
	capacity.nPoseStorageBudgetBytes = 3 * capacity.nCpuScratchBudgetBytes;
	capacity.nCpuScratchBudgetBytes--;
	CHECK( SIRAnimationContractValidation::validateCapacity( capacity ) == SIR_ANIMATION_OUT_OF_CAPACITY );
	capacity.nCpuScratchBudgetBytes++;
	capacity.nPoseStorageBudgetBytes--;
	CHECK( SIRAnimationContractValidation::validateCapacity( capacity ) == SIR_ANIMATION_OUT_OF_CAPACITY );
	capacity.nPoseStorageBudgetBytes++;
	CHECK( SIRAnimationContractValidation::validateCapacity( capacity ) == SIR_ANIMATION_SUCCESS );
}

static void checkResources( void )
{
	sirAnimationAssetRef_t asset = createAsset( 12, SIR_ANIMATION_ASSET_CLIP );
	CHECK( SIRAnimationContractValidation::validateAsset( asset, SIR_ANIMATION_ASSET_CLIP, 12 ) == SIR_ANIMATION_SUCCESS );
	sirAnimationAssetRef_t copy = {};
	memcpy( &copy, &asset, sizeof( copy ) );
	CHECK( memcmp( &copy, &asset, sizeof( copy ) ) == 0 );
	CHECK( SIRAnimationContractValidation::validateAsset( asset, SIR_ANIMATION_ASSET_CLIP, 13 ) == SIR_ANIMATION_STALE_ASSET );
	CHECK( SIRAnimationContractValidation::validateAsset( asset, SIR_ANIMATION_ASSET_SKELETON, 12 ) == SIR_ANIMATION_INVALID_ARGUMENT );
	asset.eKind = ( sirAnimationAssetKind_t )255;
	CHECK( SIRAnimationContractValidation::validateAsset( asset, asset.eKind, 12 ) == SIR_ANIMATION_INVALID_ARGUMENT );
	asset = createAsset( 12, SIR_ANIMATION_ASSET_CLIP );
	asset.keyResource.m_nAssetId = 0;
	CHECK( SIRAnimationContractValidation::validateAsset( asset, SIR_ANIMATION_ASSET_CLIP, 12 ) == SIR_ANIMATION_INVALID_ARGUMENT );
	asset = createAsset( 12, SIR_ANIMATION_ASSET_CLIP );
	asset.keyResource.m_nTypeId = 0;
	CHECK( SIRAnimationContractValidation::validateAsset( asset, SIR_ANIMATION_ASSET_CLIP, 12 ) == SIR_ANIMATION_INVALID_ARGUMENT );
	asset = createAsset( 12, SIR_ANIMATION_ASSET_CLIP );
	asset.nCookedFormatId = 0;
	CHECK( SIRAnimationContractValidation::validateAsset( asset, SIR_ANIMATION_ASSET_CLIP, 12 ) == SIR_ANIMATION_INVALID_ARGUMENT );
	asset = createAsset( 12, SIR_ANIMATION_ASSET_CLIP );
	asset.nSchemaVersion = 0;
	CHECK( SIRAnimationContractValidation::validateAsset( asset, SIR_ANIMATION_ASSET_CLIP, 12 ) == SIR_ANIMATION_INVALID_ARGUMENT );
	asset = createAsset( 12, SIR_ANIMATION_ASSET_CLIP );
	asset.nReserved = 1;
	CHECK( SIRAnimationContractValidation::validateAsset( asset, SIR_ANIMATION_ASSET_CLIP, 12 ) == SIR_ANIMATION_INVALID_ARGUMENT );
	asset = createAsset( 0, SIR_ANIMATION_ASSET_CLIP );
	CHECK( SIRAnimationContractValidation::validateAsset( asset, SIR_ANIMATION_ASSET_CLIP, 0 ) == SIR_ANIMATION_STALE_ASSET );
}

static void checkState( void )
{
	sirAnimationInstance_t instance = createInstance( 1 );
	CHECK( SIRAnimationContractValidation::validateInstance( instance ) == SIR_ANIMATION_SUCCESS );
	instance.nEntityId = 0;
	CHECK( SIRAnimationContractValidation::validateInstance( instance ) == SIR_ANIMATION_INVALID_HANDLE );
	instance = createInstance( 1 );
	instance.nEntityGeneration = 0;
	CHECK( SIRAnimationContractValidation::validateInstance( instance ) == SIR_ANIMATION_INVALID_HANDLE );
	instance = createInstance( 1 );
	instance.nWorldGeneration = 0;
	CHECK( SIRAnimationContractValidation::validateInstance( instance ) == SIR_ANIMATION_INVALID_HANDLE );
	instance = createInstance( 1 );
	instance.nSlotGeneration = 0;
	CHECK( SIRAnimationContractValidation::validateInstance( instance ) == SIR_ANIMATION_INVALID_HANDLE );

	sirAnimationStateSnapshot_t current = {};
	current.instance = createInstance( 1 );
	current.tick = createTick( 15 );
	current.graph = createAsset( 9, SIR_ANIMATION_ASSET_GRAPH );
	current.nStateRevision = 3;
	current.nEventCursor = 5;
	current.nRootMotionCursor = 7;
	sirAnimationStateSnapshot_t next = current;
	next.nStateRevision++;
	next.tick.nFixedTick++;
	next.nEventCursor++;
	CHECK( SIRAnimationContractValidation::validateStateAdvance( current, next ) == SIR_ANIMATION_SUCCESS );
	next.instance.nEntityGeneration++;
	CHECK( SIRAnimationContractValidation::validateStateAdvance( current, next ) == SIR_ANIMATION_STALE_ENTITY );
	next = current;
	next.tick.nWorldEpoch++;
	CHECK( SIRAnimationContractValidation::validateStateAdvance( current, next ) == SIR_ANIMATION_STALE_ENTITY );
	next = current;
	next.tick.nCancellationGeneration++;
	CHECK( SIRAnimationContractValidation::validateStateAdvance( current, next ) == SIR_ANIMATION_STALE_ENTITY );
	next = current;
	next.instance.nEntityId = 0;
	CHECK( SIRAnimationContractValidation::validateStateAdvance( current, next ) == SIR_ANIMATION_INVALID_HANDLE );
	next = current;
	next.graph.nRevision++;
	CHECK( SIRAnimationContractValidation::validateStateAdvance( current, next ) == SIR_ANIMATION_STALE_ASSET );
	next = current;
	CHECK( SIRAnimationContractValidation::validateStateAdvance( current, next ) == SIR_ANIMATION_STALE_POSE );
	next.nStateRevision++;
	next.tick.nFixedTick--;
	CHECK( SIRAnimationContractValidation::validateStateAdvance( current, next ) == SIR_ANIMATION_STALE_POSE );
	next = current;
	next.nStateRevision++;
	next.tick.nSubstep++;
	CHECK( SIRAnimationContractValidation::validateStateAdvance( current, next ) == SIR_ANIMATION_SUCCESS );
	next.tick.nSubstep = 0;
	current.tick.nSubstep = 2;
	CHECK( SIRAnimationContractValidation::validateStateAdvance( current, next ) == SIR_ANIMATION_STALE_POSE );
	current.tick.nSubstep = 0;
	next = current;
	next.nStateRevision++;
	next.nEventCursor--;
	CHECK( SIRAnimationContractValidation::validateStateAdvance( current, next ) == SIR_ANIMATION_STALE_POSE );
	next = current;
	next.nStateRevision++;
	next.nRootMotionCursor--;
	CHECK( SIRAnimationContractValidation::validateStateAdvance( current, next ) == SIR_ANIMATION_STALE_POSE );
	next = current;
	current.nStateRevision = UINT64_MAX;
	next.nStateRevision = 0;
	CHECK( SIRAnimationContractValidation::validateStateAdvance( current, next ) == SIR_ANIMATION_STALE_POSE );
	current.instance.nEntityGeneration = 0;
	CHECK( SIRAnimationContractValidation::validateStateAdvance( current, next ) == SIR_ANIMATION_INVALID_HANDLE );
}

static void checkBounds( void )
{
	CHECK( SIRAnimationContractValidation::validateBufferRange( 4, 8, 2, 20 ) == SIR_ANIMATION_SUCCESS );
	CHECK( SIRAnimationContractValidation::validateBufferRange( 4, 8, 3, 20 ) == SIR_ANIMATION_OUT_OF_CAPACITY );
	CHECK( SIRAnimationContractValidation::validateBufferRange( UINT64_MAX, 2, 2, UINT64_MAX ) == SIR_ANIMATION_OUT_OF_CAPACITY );
	CHECK( SIRAnimationContractValidation::validateBufferRange( 0, 0, 1, 100 ) == SIR_ANIMATION_INVALID_ARGUMENT );
	CHECK( SIRAnimationContractValidation::validateBufferRange( 101, 0, 0, 100 ) == SIR_ANIMATION_OUT_OF_CAPACITY );
	CHECK( SIRAnimationContractValidation::validateBufferRange( 0, 0, 0, 0 ) == SIR_ANIMATION_SUCCESS );
	uint32_t indices[] = { 0, 3 };
	sirAnimationBoneSelection_t selection = { indices, 2, 0 };
	CHECK( SIRAnimationContractValidation::validateBoneSelection( selection, 4 ) == SIR_ANIMATION_SUCCESS );
	CHECK( SIRAnimationContractValidation::validateBoneSelection( selection, 3 ) == SIR_ANIMATION_OUT_OF_CAPACITY );
	CHECK( SIRAnimationContractValidation::validateBoneSelection( selection, 0 ) == SIR_ANIMATION_INVALID_ARGUMENT );
	selection.nBoneCount = 5;
	CHECK( SIRAnimationContractValidation::validateBoneSelection( selection, 4 ) == SIR_ANIMATION_INVALID_ARGUMENT );
	selection.nBoneCount = 2;
	selection.pBoneIndices = nullptr;
	CHECK( SIRAnimationContractValidation::validateBoneSelection( selection, 4 ) == SIR_ANIMATION_INVALID_ARGUMENT );
	selection.pBoneIndices = indices;
	indices[1] = 0;
	CHECK( SIRAnimationContractValidation::validateBoneSelection( selection, 4 ) == SIR_ANIMATION_INVALID_ARGUMENT );
	indices[1] = 3;
	indices[0] = 4;
	CHECK( SIRAnimationContractValidation::validateBoneSelection( selection, 4 ) == SIR_ANIMATION_OUT_OF_CAPACITY );
	indices[0] = 0;
	indices[1] = 1;
	CHECK( SIRAnimationContractValidation::validateBoneSelection( selection, 4 ) == SIR_ANIMATION_SUCCESS );
	indices[1] = 3;
	selection.nBoneCount = 0;
	selection.pBoneIndices = nullptr;
	CHECK( SIRAnimationContractValidation::validateBoneSelection( selection, 4 ) == SIR_ANIMATION_SUCCESS );
	CHECK( SIRAnimationContractValidation::validateBoneSelection( selection, 0 ) == SIR_ANIMATION_INVALID_ARGUMENT );

	sirAnimationGraphRequest_t graph = {};
	graph.nMaximumEvents = 2;
	graph.nMaximumLayers = 2;
	sirAnimationGraphLayer_t layers[2] = {};
	sirAnimationEvent_t events[2] = {};
	sirAnimationGraphScratch_t scratch = { layers, events, 2, 2 };
	CHECK( SIRAnimationContractValidation::validateGraphScratch( graph, scratch ) == SIR_ANIMATION_SUCCESS );
	graph.nMaximumEvents = 3;
	CHECK( SIRAnimationContractValidation::validateGraphScratch( graph, scratch ) == SIR_ANIMATION_OUT_OF_CAPACITY );
	graph.nMaximumEvents = 2;
	graph.nMaximumLayers = 3;
	CHECK( SIRAnimationContractValidation::validateGraphScratch( graph, scratch ) == SIR_ANIMATION_OUT_OF_CAPACITY );
	graph.nMaximumLayers = 2;
	scratch.pLayers = nullptr;
	CHECK( SIRAnimationContractValidation::validateGraphScratch( graph, scratch ) == SIR_ANIMATION_INVALID_ARGUMENT );
	scratch.pLayers = layers;
	scratch.pEvents = nullptr;
	CHECK( SIRAnimationContractValidation::validateGraphScratch( graph, scratch ) == SIR_ANIMATION_INVALID_ARGUMENT );
	graph.nMaximumLayers = 0;
	graph.nMaximumEvents = 0;
	CHECK( SIRAnimationContractValidation::validateGraphScratch( graph, scratch ) == SIR_ANIMATION_SUCCESS );
}

static void checkPublication( void )
{
	sirAnimationTransform_t transforms[2] = {};
	transforms[0].fRotation[3] = 1;
	transforms[1].fRotation[3] = 1;
	sirAnimationPosePublication_t pose = {};
	pose.instance = createInstance( 1 );
	pose.rig = createAsset( 4, SIR_ANIMATION_ASSET_SKELETON );
	pose.tick = createTick( 12 );
	pose.pModelTransforms = transforms;
	pose.nTransformCount = 2;
	for ( int i = 0; i < 3; ++i ) {
		pose.conservativeBounds.fMaximum[i] = 1;
	}
	CHECK( SIRAnimationContractValidation::validatePublication( pose, 2 ) == SIR_ANIMATION_SUCCESS );
	CHECK( SIRAnimationContractValidation::validatePublication( pose, 1 ) == SIR_ANIMATION_OUT_OF_CAPACITY );
	CHECK( SIRAnimationContractValidation::validatePublication( pose, 0 ) == SIR_ANIMATION_OUT_OF_CAPACITY );
	pose.nTransformCount = 0;
	CHECK( SIRAnimationContractValidation::validatePublication( pose, 2 ) == SIR_ANIMATION_OUT_OF_CAPACITY );
	pose.nTransformCount = 2;
	pose.pModelTransforms = nullptr;
	CHECK( SIRAnimationContractValidation::validatePublication( pose, 2 ) == SIR_ANIMATION_OUT_OF_CAPACITY );
	pose.pModelTransforms = transforms;
	pose.rig.nRevision = 0;
	CHECK( SIRAnimationContractValidation::validatePublication( pose, 2 ) == SIR_ANIMATION_STALE_ASSET );
	pose.rig.nRevision = 4;
	pose.instance.nEntityGeneration = 0;
	CHECK( SIRAnimationContractValidation::validatePublication( pose, 2 ) == SIR_ANIMATION_STALE_ENTITY );
	pose.instance = createInstance( 1 );
	transforms[0].fTranslation[0] = std::numeric_limits<float>::quiet_NaN();
	CHECK( SIRAnimationContractValidation::validatePublication( pose, 2 ) == SIR_ANIMATION_INVALID_ARGUMENT );
	transforms[0].fTranslation[0] = 0;
	transforms[0].fRotation[0] = std::numeric_limits<float>::infinity();
	CHECK( SIRAnimationContractValidation::validatePublication( pose, 2 ) == SIR_ANIMATION_INVALID_ARGUMENT );
	transforms[0].fRotation[0] = 0;
	transforms[0].fScale[0] = std::numeric_limits<float>::quiet_NaN();
	CHECK( SIRAnimationContractValidation::validatePublication( pose, 2 ) == SIR_ANIMATION_INVALID_ARGUMENT );
	transforms[0].fScale[0] = 0;
	pose.conservativeBounds.fMinimum[0] = std::numeric_limits<float>::quiet_NaN();
	CHECK( SIRAnimationContractValidation::validatePublication( pose, 2 ) == SIR_ANIMATION_INVALID_ARGUMENT );
	pose.conservativeBounds.fMinimum[0] = 0;
	pose.conservativeBounds.fMinimum[1] = 2;
	CHECK( SIRAnimationContractValidation::validatePublication( pose, 2 ) == SIR_ANIMATION_INVALID_ARGUMENT );
	pose.conservativeBounds.fMinimum[1] = 0;
	pose.conservativeBounds.fMaximum[0] = std::numeric_limits<float>::infinity();
	CHECK( SIRAnimationContractValidation::validatePublication( pose, 2 ) == SIR_ANIMATION_INVALID_ARGUMENT );
	pose.conservativeBounds.fMaximum[0] = 1;
}

static void checkAuthority( void )
{
	sirAnimationAuthorityRequest_t request = {};
	request.instance = createInstance( 1 );
	request.effectiveTick = createTick( 20 );
	request.eNewOwner = SIR_ANIMATION_AUTHORITY_PHYSICS;
	request.nExpectedAuthorityGeneration = 7;
	sirAnimationAuthorityAck_t ack = {};
	ack.effectiveTick = request.effectiveTick;
	ack.eOwner = request.eNewOwner;
	ack.nAuthorityGeneration = 8;
	CHECK( SIRAnimationContractValidation::validateAuthorityAck( request, ack ) == SIR_ANIMATION_SUCCESS );
	ack.eOwner = SIR_ANIMATION_AUTHORITY_ANIMATION;
	CHECK( SIRAnimationContractValidation::validateAuthorityAck( request, ack ) == SIR_ANIMATION_AUTHORITY_CONFLICT );
	ack.eOwner = request.eNewOwner;
	ack.effectiveTick.nWorldEpoch++;
	CHECK( SIRAnimationContractValidation::validateAuthorityAck( request, ack ) == SIR_ANIMATION_STALE_POSE );
	ack.effectiveTick = request.effectiveTick;
	ack.effectiveTick.nFixedTick++;
	CHECK( SIRAnimationContractValidation::validateAuthorityAck( request, ack ) == SIR_ANIMATION_STALE_POSE );
	ack.effectiveTick = request.effectiveTick;
	ack.effectiveTick.nSubstep++;
	CHECK( SIRAnimationContractValidation::validateAuthorityAck( request, ack ) == SIR_ANIMATION_STALE_POSE );
	ack.effectiveTick = request.effectiveTick;
	ack.effectiveTick.nCancellationGeneration++;
	CHECK( SIRAnimationContractValidation::validateAuthorityAck( request, ack ) == SIR_ANIMATION_STALE_POSE );
	ack.effectiveTick = request.effectiveTick;
	ack.nAuthorityGeneration++;
	CHECK( SIRAnimationContractValidation::validateAuthorityAck( request, ack ) == SIR_ANIMATION_STALE_POSE );
	ack.nAuthorityGeneration = 8;
	request.nExpectedAuthorityGeneration = UINT64_MAX;
	CHECK( SIRAnimationContractValidation::validateAuthorityAck( request, ack ) == SIR_ANIMATION_STALE_POSE );
	request.nExpectedAuthorityGeneration = 7;
	request.eNewOwner = ( sirAnimationAuthority_t )255;
	CHECK( SIRAnimationContractValidation::validateAuthorityAck( request, ack ) == SIR_ANIMATION_AUTHORITY_CONFLICT );
	request.eNewOwner = SIR_ANIMATION_AUTHORITY_PHYSICS;
	request.instance.nEntityId = 0;
	CHECK( SIRAnimationContractValidation::validateAuthorityAck( request, ack ) == SIR_ANIMATION_STALE_ENTITY );
}

static void checkGraphics( void )
{
	sirGraphicsCapabilities_t graphics = {};
	sirAnimationDeformationMode_t mode = SIR_ANIMATION_DEFORM_COMPUTE;
	CHECK( SIRAnimationContractValidation::selectDeformation( graphics, SIR_ANIMATION_DEFORM_COMPUTE, &mode ) == SIR_ANIMATION_SUCCESS );
	CHECK( mode == SIR_ANIMATION_DEFORM_CPU );
	graphics.nFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS;
	graphics.nPipelineModelMask = 1U << ( SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE - 1 );
	CHECK( SIRAnimationContractValidation::selectDeformation( graphics, SIR_ANIMATION_DEFORM_COMPUTE, &mode ) == SIR_ANIMATION_SUCCESS );
	CHECK( mode == SIR_ANIMATION_DEFORM_VERTEX );
	graphics.nFeatureMask |= SIR_GRAPHICS_FEATURE_COMPUTE | SIR_GRAPHICS_FEATURE_SHADER_STORAGE;
	CHECK( SIRAnimationContractValidation::selectDeformation( graphics, SIR_ANIMATION_DEFORM_COMPUTE, &mode ) == SIR_ANIMATION_SUCCESS );
	CHECK( mode == SIR_ANIMATION_DEFORM_COMPUTE );
	CHECK( SIRAnimationContractValidation::selectDeformation( graphics, SIR_ANIMATION_DEFORM_CPU, &mode ) == SIR_ANIMATION_SUCCESS );
	CHECK( mode == SIR_ANIMATION_DEFORM_CPU );
	CHECK( SIRAnimationContractValidation::selectDeformation( graphics, SIR_ANIMATION_DEFORM_VERTEX, nullptr ) ==
	       SIR_ANIMATION_INVALID_ARGUMENT );
	CHECK( SIRAnimationContractValidation::selectDeformation( graphics, ( sirAnimationDeformationMode_t )255, &mode ) ==
	       SIR_ANIMATION_INVALID_ARGUMENT );
	graphics.nFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_FIXED_FUNCTION;
	graphics.nPipelineModelMask = 1U << ( SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION - 1 );
	CHECK( SIRAnimationContractValidation::selectDeformation( graphics, SIR_ANIMATION_DEFORM_VERTEX, &mode ) == SIR_ANIMATION_SUCCESS );
	CHECK( mode == SIR_ANIMATION_DEFORM_CPU );
	graphics.nFeatureMask = SIR_GRAPHICS_FEATURE_COMPUTE;
	CHECK( SIRAnimationContractValidation::selectDeformation( graphics, SIR_ANIMATION_DEFORM_COMPUTE, &mode ) == SIR_ANIMATION_SUCCESS );
	CHECK( mode == SIR_ANIMATION_DEFORM_CPU );

	sirGraphicsCompletionSet_t completion = {};
	completion.nDeviceGeneration = 9;
	completion.nQueueMask = ( 1U << SIR_GRAPHICS_QUEUE_DIRECT ) | ( 1U << SIR_GRAPHICS_QUEUE_COPY );
	completion.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT] = 4;
	completion.szQueueValues[SIR_GRAPHICS_QUEUE_COPY] = 12;
	CHECK( SIRAnimationContractValidation::validateGraphicsCompletion( completion, 9 ) == SIR_ANIMATION_SUCCESS );
	CHECK( SIRAnimationContractValidation::validateGraphicsCompletion( completion, 10 ) == SIR_ANIMATION_STALE_POSE );
	CHECK( SIRAnimationContractValidation::validateGraphicsCompletion( completion, 0 ) == SIR_ANIMATION_STALE_POSE );
	completion.nQueueMask |= 1U << 7;
	CHECK( SIRAnimationContractValidation::validateGraphicsCompletion( completion, 9 ) == SIR_ANIMATION_STALE_POSE );
	completion.nQueueMask &= ~( 1U << 7 );
	completion.szQueueValues[SIR_GRAPHICS_QUEUE_COPY] = 0;
	CHECK( SIRAnimationContractValidation::validateGraphicsCompletion( completion, 9 ) == SIR_ANIMATION_INVALID_ARGUMENT );
	completion.nQueueMask = 0;
	CHECK( SIRAnimationContractValidation::validateGraphicsCompletion( completion, 9 ) == SIR_ANIMATION_SUCCESS );
}

static void checkLeases( void )
{
	sirAnimationContractPoseSlot_t poses[2] = {};
	sirAnimationContractReaderSlot_t readers[2] = {};
	SIRAnimationContractLeasePool pool;
	CHECK( pool.initialize( nullptr, 2, readers, 2 ) == SIR_ANIMATION_INVALID_ARGUMENT );
	CHECK( pool.initialize( poses, 2, nullptr, 2 ) == SIR_ANIMATION_INVALID_ARGUMENT );
	CHECK( pool.initialize( poses, 0, readers, 2 ) == SIR_ANIMATION_INVALID_ARGUMENT );
	CHECK( pool.initialize( poses, 2, readers, 0 ) == SIR_ANIMATION_INVALID_ARGUMENT );
	CHECK( pool.acquire( { 0, 1 }, nullptr ) == SIR_ANIMATION_INVALID_ARGUMENT );
	CHECK( pool.acquire( { 0, 1 }, ( sirAnimationPoseLease_t * )nullptr ) == SIR_ANIMATION_INVALID_ARGUMENT );
	CHECK( pool.registerPose( { 0, 1 }, createInstance( 1 ), createTick( 1 ) ) == SIR_ANIMATION_INVALID_HANDLE );
	CHECK( pool.cancelEpoch( 3, 1 ) == SIR_ANIMATION_INVALID_HANDLE );
	CHECK( pool.retirePose( { 0, 1 } ) == SIR_ANIMATION_INVALID_HANDLE );
	CHECK( pool.initialize( poses, 2, readers, 2 ) == SIR_ANIMATION_SUCCESS );
	CHECK( pool.initialize( poses, 2, readers, 2 ) == SIR_ANIMATION_BUSY );
	CHECK( pool.getOutstandingReaders() == 0 );
	CHECK( pool.registerPose( { 2, 1 }, createInstance( 1 ), createTick( 1 ) ) == SIR_ANIMATION_INVALID_HANDLE );
	CHECK( pool.registerPose( { 0, 0 }, createInstance( 1 ), createTick( 1 ) ) == SIR_ANIMATION_INVALID_HANDLE );
	CHECK( pool.registerPose( { 0, 1 }, createInstance( 0 ), createTick( 1 ) ) == SIR_ANIMATION_STALE_ENTITY );
	CHECK( pool.registerPose( { 0, 1 }, createInstance( 1 ), createTick( 1 ) ) == SIR_ANIMATION_SUCCESS );
	CHECK( pool.registerPose( { 0, 1 }, createInstance( 1 ), createTick( 1 ) ) == SIR_ANIMATION_STALE_POSE );
	sirAnimationPoseLease_t leaseA = {};
	sirAnimationPoseLease_t leaseB = {};
	CHECK( pool.acquire( { 0, 1 }, &leaseA ) == SIR_ANIMATION_SUCCESS );
	CHECK( pool.acquire( { 0, 1 }, &leaseB ) == SIR_ANIMATION_SUCCESS );
	CHECK( leaseA.nReaderEpoch != leaseB.nReaderEpoch );
	CHECK( pool.getOutstandingReaders() == 2 );
	sirAnimationPoseLease_t rejected = {};
	CHECK( pool.acquire( { 0, 1 }, &rejected ) == SIR_ANIMATION_OUT_OF_CAPACITY );
	CHECK( pool.registerPose( { 0, 2 }, createInstance( 1 ), createTick( 2 ) ) == SIR_ANIMATION_BUSY );
	CHECK( pool.retirePose( { 0, 1 } ) == SIR_ANIMATION_BUSY );
	CHECK( pool.cancelEpoch( 3, 0 ) == SIR_ANIMATION_SUCCESS );
	CHECK( pool.acquire( { 0, 1 }, &rejected ) == SIR_ANIMATION_OUT_OF_CAPACITY );
	CHECK( pool.cancelEpoch( 3, 1 ) == SIR_ANIMATION_SUCCESS );
	CHECK( pool.acquire( { 0, 1 }, &rejected ) == SIR_ANIMATION_STALE_POSE );
	CHECK( pool.getOutstandingReaders() == 2 );
	CHECK( pool.release( leaseA ) == SIR_ANIMATION_SUCCESS );
	CHECK( pool.release( leaseA ) == SIR_ANIMATION_INVALID_HANDLE );
	CHECK( pool.getOutstandingReaders() == 1 );
	CHECK( pool.registerPose( { 0, 2 }, createInstance( 1 ), createTick( 2 ) ) == SIR_ANIMATION_BUSY );
	CHECK( pool.release( leaseB ) == SIR_ANIMATION_SUCCESS );
	CHECK( pool.getOutstandingReaders() == 0 );
	CHECK( pool.retirePose( { 0, 1 } ) == SIR_ANIMATION_SUCCESS );
	CHECK( pool.retirePose( { 0, 1 } ) == SIR_ANIMATION_STALE_POSE );
	CHECK( pool.registerPose( { 0, 1 }, createInstance( 1 ), createTick( 2 ) ) == SIR_ANIMATION_STALE_POSE );
	CHECK( pool.registerPose( { 0, 2 }, createInstance( 1 ), createTick( 2 ) ) == SIR_ANIMATION_CANCELLED );
	sirAnimationTick_t resumed = createTick( 2 );
	resumed.nCancellationGeneration = 2;
	CHECK( pool.registerPose( { 0, 2 }, createInstance( 1 ), resumed ) == SIR_ANIMATION_SUCCESS );
	CHECK( pool.acquire( { 0, 1 }, &rejected ) == SIR_ANIMATION_STALE_POSE );
	CHECK( pool.acquire( { 0, 2 }, &rejected ) == SIR_ANIMATION_SUCCESS );
	CHECK( rejected.nLeaseGeneration != leaseA.nLeaseGeneration );
	CHECK( pool.release( leaseA ) == SIR_ANIMATION_INVALID_HANDLE );
	CHECK( pool.release( rejected ) == SIR_ANIMATION_SUCCESS );
	CHECK( pool.retirePose( { 0, 2 } ) == SIR_ANIMATION_SUCCESS );
	CHECK( pool.registerPose( { 0, 3 }, createInstance( 2 ), resumed ) == SIR_ANIMATION_SUCCESS );
	CHECK( pool.registerPose( { 0, 2 }, createInstance( 1 ), createTick( 3 ) ) == SIR_ANIMATION_STALE_POSE );
	CHECK( pool.acquire( { 3, 1 }, &rejected ) == SIR_ANIMATION_INVALID_HANDLE );
	CHECK( pool.acquire( { 0, 0 }, &rejected ) == SIR_ANIMATION_INVALID_HANDLE );
	CHECK( pool.retirePose( { 3, 1 } ) == SIR_ANIMATION_INVALID_HANDLE );
	CHECK( pool.release( { { 0, 3 }, 999, 1, 0 } ) == SIR_ANIMATION_INVALID_HANDLE );
	CHECK( pool.release( { { 0, 3 }, 0, 1, 0 } ) == SIR_ANIMATION_INVALID_HANDLE );
	CHECK( pool.release( { { 0, 3 }, 1, 1, 1 } ) == SIR_ANIMATION_INVALID_HANDLE );
	CHECK( pool.acquire( { 0, 3 }, &rejected ) == SIR_ANIMATION_SUCCESS );
	const sirAnimationPoseLease_t validLease = rejected;
	rejected.pose.nGeneration--;
	CHECK( pool.release( rejected ) == SIR_ANIMATION_INVALID_HANDLE );
	rejected = validLease;
	rejected.nLeaseGeneration++;
	CHECK( pool.release( rejected ) == SIR_ANIMATION_INVALID_HANDLE );
	CHECK( pool.getOutstandingReaders() == 1 );
	CHECK( pool.shutdown() == SIR_ANIMATION_BUSY );
	CHECK( pool.getOutstandingReaders() == 1 );
	CHECK( pool.release( validLease ) == SIR_ANIMATION_SUCCESS );
	CHECK( pool.retirePose( { 0, 3 } ) == SIR_ANIMATION_SUCCESS );
	CHECK( pool.shutdown() == SIR_ANIMATION_SUCCESS );
	CHECK( pool.getOutstandingReaders() == 0 );
	CHECK( pool.registerPose( { 0, 4 }, createInstance( 1 ), createTick( 4 ) ) == SIR_ANIMATION_INVALID_HANDLE );
}

static void checkAdversarialLeaseStorage( void )
{
	sirAnimationContractPoseSlot_t poses[2] = {};
	sirAnimationContractReaderSlot_t readers[2] = {};
	SIRAnimationContractLeasePool pool;
	struct OrderedStorage
	{
		sirAnimationContractPoseSlot_t poses[2];
		sirAnimationContractReaderSlot_t readers[2];
	};
	OrderedStorage ordered = {};
	SIRAnimationContractLeasePool orderedPool;
	CHECK( orderedPool.initialize( ordered.poses, 2, ordered.readers, 2 ) == SIR_ANIMATION_SUCCESS );
	CHECK( orderedPool.shutdown() == SIR_ANIMATION_SUCCESS );
	// A malformed caller buffer must never be dereferenced.
	alignas( sirAnimationContractPoseSlot_t ) uint8_t unalignedPoses[sizeof( poses ) + 8] = {};
	alignas( sirAnimationContractReaderSlot_t ) uint8_t unalignedReaders[sizeof( readers ) + 8] = {};
	CHECK( pool.initialize( ( sirAnimationContractPoseSlot_t * )( unalignedPoses + 1 ), 2, readers, 2 ) == SIR_ANIMATION_INVALID_ARGUMENT );
	CHECK( pool.initialize( poses, 2, ( sirAnimationContractReaderSlot_t * )( unalignedReaders + 1 ), 2 ) ==
	       SIR_ANIMATION_INVALID_ARGUMENT );
	CHECK( pool.initialize( poses, 2, ( sirAnimationContractReaderSlot_t * )poses, 2 ) == SIR_ANIMATION_INVALID_ARGUMENT );
	const uintptr_t nAlignedHigh = UINTPTR_MAX - ( UINTPTR_MAX % alignof( sirAnimationContractPoseSlot_t ) );
	CHECK( pool.initialize( ( sirAnimationContractPoseSlot_t * )nAlignedHigh, 2, readers, 2 ) == SIR_ANIMATION_OUT_OF_CAPACITY );
	const uintptr_t nReaderHigh = UINTPTR_MAX - ( UINTPTR_MAX % alignof( sirAnimationContractReaderSlot_t ) );
	CHECK( pool.initialize( poses, 2, ( sirAnimationContractReaderSlot_t * )nReaderHigh, 2 ) == SIR_ANIMATION_OUT_OF_CAPACITY );
	sirAnimationPoseLease_t lease = {};
	CHECK( pool.acquire( { 0, 1 }, &lease ) == SIR_ANIMATION_INVALID_HANDLE );
	CHECK( pool.release( lease ) == SIR_ANIMATION_INVALID_HANDLE );
	CHECK( pool.initialize( poses, 2, readers, 2 ) == SIR_ANIMATION_SUCCESS );
	CHECK( pool.registerPose( { 0, 1 }, createInstance( 1 ), createTick( 1 ) ) == SIR_ANIMATION_SUCCESS );
	CHECK( pool.retirePose( { 0, 0 } ) == SIR_ANIMATION_INVALID_HANDLE );
	CHECK( pool.acquire( { 0, 1 }, &lease ) == SIR_ANIMATION_SUCCESS );
	// External bounded storage is caller-owned. Fault injection deliberately
	// corrupts bookkeeping to test every stale-ticket rejection path.
	sirAnimationPoseLease_t forged = lease;
	forged.pose.nSlot = 1;
	CHECK( pool.release( forged ) == SIR_ANIMATION_INVALID_HANDLE );
	forged = lease;
	forged.pose.nGeneration++;
	CHECK( pool.release( forged ) == SIR_ANIMATION_INVALID_HANDLE );
	readers[0].pose.nSlot = 2;
	forged = lease;
	forged.pose.nSlot = 2;
	CHECK( pool.release( forged ) == SIR_ANIMATION_INVALID_HANDLE );
	readers[0].pose = lease.pose;
	poses[0].nGeneration++;
	CHECK( pool.release( lease ) == SIR_ANIMATION_STALE_POSE );
	poses[0].nGeneration--;
	poses[0].nActiveReaders = 0;
	CHECK( pool.release( lease ) == SIR_ANIMATION_STALE_POSE );
	poses[0].nActiveReaders = 1;
	CHECK( pool.release( lease ) == SIR_ANIMATION_SUCCESS );
	CHECK( pool.retirePose( { 0, 1 } ) == SIR_ANIMATION_SUCCESS );
	CHECK( pool.acquire( { 0, 1 }, &lease ) == SIR_ANIMATION_STALE_POSE );
	CHECK( pool.registerPose( { 0, 2 }, createInstance( 1 ), createTick( 2 ) ) == SIR_ANIMATION_SUCCESS );
	CHECK( pool.retirePose( { 0, 1 } ) == SIR_ANIMATION_STALE_POSE );
	// Force ticket-generation exhaustion; the producer must report capacity.
	readers[0].nGeneration = UINT32_MAX;
	readers[1].nGeneration = UINT32_MAX;
	CHECK( pool.acquire( { 0, 2 }, &lease ) == SIR_ANIMATION_OUT_OF_CAPACITY );
	readers[0].nGeneration = 0;
	readers[1].nGeneration = 0;
	CHECK( pool.cancelEpoch( 3, 1 ) == SIR_ANIMATION_SUCCESS );
	CHECK( pool.cancelEpoch( 3, 1 ) == SIR_ANIMATION_SUCCESS );
	CHECK( pool.cancelEpoch( 3, 0 ) == SIR_ANIMATION_SUCCESS );
	CHECK( pool.cancelEpoch( 4, 0 ) == SIR_ANIMATION_SUCCESS );
	CHECK( pool.cancelEpoch( 3, 0 ) == SIR_ANIMATION_SUCCESS );
	CHECK( pool.registerPose( { 0, 3 }, createInstance( 1 ), createTick( 3 ) ) == SIR_ANIMATION_CANCELLED );
	sirAnimationTick_t future = createTick( 3 );
	future.nWorldEpoch = 5;
	CHECK( pool.registerPose( { 0, 3 }, createInstance( 1 ), future ) == SIR_ANIMATION_SUCCESS );
	CHECK( pool.acquire( { 0, 3 }, &lease ) == SIR_ANIMATION_SUCCESS );
	CHECK( pool.cancelEpoch( 4, 0 ) == SIR_ANIMATION_SUCCESS );
	CHECK( pool.release( lease ) == SIR_ANIMATION_SUCCESS );
	CHECK( pool.retirePose( { 0, 3 } ) == SIR_ANIMATION_SUCCESS );
	CHECK( pool.shutdown() == SIR_ANIMATION_SUCCESS );
}

int main( void )
{
	checkAbi();
	checkCapacities();
	checkResources();
	checkState();
	checkBounds();
	checkPublication();
	checkAuthority();
	checkGraphics();
	checkLeases();
	checkAdversarialLeaseStorage();
	printf( "ANM-00: %u assertions, %u failures\n", s_nChecks, s_nFailures );
	return s_nFailures ? 1 : 0;
}
