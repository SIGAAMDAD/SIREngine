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
#include <Animation/Runtime/SIRAnimationSampler.h>
#include <Animation/Cooked/SIRAnimationCookedClipReader.h>
#include <Animation/Runtime/SIRAnimationSIMD.h>
#ifdef SIR_ANIM_WITH_JOBS
#include <Animation/Runtime/SIRAnimationSamplingJobs.h>
#include <Core/Jobs/SIRTaskScheduler.h>
#include <Core/Memory/SIRMemoryManager.h>
#endif
#include <SceneCooker/Animation/SIRAnimationCooker.h>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

static sirAnimationCookKey_t sirSamplingKey( uint32_t nTime, float fValue ) noexcept
{
	sirAnimationCookKey_t key = {};
	key.nTime = nTime;
	key.fComponents[0] = fValue;
	key.fComponents[6] = 1.0f;
	key.fComponents[7] = key.fComponents[8] = key.fComponents[9] = 1.0f;
	return key;
}

static bool sirSamplingClose( float a, float b ) noexcept
{
	return std::fabs( a - b ) < 0.002f;
}

static void sirTestPlayback( void )
{
	assert( SIRAnimationSampler::normalizeTick( 55, 0, 100, SIR_ANIMATION_PLAY_CLAMP ) == 55 );
	assert( SIRAnimationSampler::normalizeTick( 110, 0, 100, SIR_ANIMATION_PLAY_CLAMP ) == 100 );
	assert( SIRAnimationSampler::normalizeTick( -10, 0, 100, SIR_ANIMATION_PLAY_CLAMP ) == 0 );
	assert( SIRAnimationSampler::normalizeTick( 125, 0, 100, SIR_ANIMATION_PLAY_LOOP ) == 25 );
	assert( SIRAnimationSampler::normalizeTick( -1, 0, 100, SIR_ANIMATION_PLAY_LOOP ) == 99 );
	assert( SIRAnimationSampler::normalizeTick( 125, 0, 100, SIR_ANIMATION_PLAY_PINGPONG ) == 75 );
	assert( SIRAnimationSampler::normalizeTick( -1, 0, 100, SIR_ANIMATION_PLAY_PINGPONG ) == 1 );
	assert( SIRAnimationSampler::normalizeTick( 0, 0, 0, SIR_ANIMATION_PLAY_LOOP ) == 0 );
}

static void sirTestSampling( void )
{
	sirAnimationCookKey_t rootKeys[3] = { sirSamplingKey( 0, 0 ), sirSamplingKey( 50, 10 ), sirSamplingKey( 100, 20 ) };
	sirAnimationCookKey_t coarseKeys[2] = { sirSamplingKey( 0, 0 ), sirSamplingKey( 100, 20 ) };
	sirAnimationCookKey_t constant[2] = { sirSamplingKey( 0, 5 ), sirSamplingKey( 100, 5 ) };
	sirAnimationCookTrack_t tracks[3] = { { 0, rootKeys, 3, 0, 0, {} }, { 0, coarseKeys, 2, 1, 0, {} }, { 2, constant, 2, 0, 0, {} } };
	sirAnimationCookClipInput_t clip = {};
	clip.pTracks = tracks;
	clip.nTrackCount = 3;
	clip.nDuration = 100;
	clip.nRevision = 1;
	clip.nRootBoneIndex = 0;
	clip.fMaximumQuantError = 0.001f;
	clip.fBoneRadius = 0.5f;
	uint8_t bytes[8192] = {};
	sirAnimationCookOutput_t out = { bytes, sizeof( bytes ), 0 };
	assert( SIRAnimationCooker::cookClip( clip, &out ) == SIR_ANIM_COOK_OK );
	const sirAnimationCookSpan_t span = { bytes, out.nWritten };
	SIRAnimationCookedClipReader verified;
	sirAnimationCookAssetInfo_t verifiedInfo = {};
	assert( verified.getTrackCount( 0, true, nullptr ) == SIR_ANIM_COOK_INVALID );
	assert( verified.initialize( span, &verifiedInfo ) == SIR_ANIM_COOK_OK );
	assert( verifiedInfo.nItemCount == 3 && verifiedInfo.nDuration == 100 );
	uint32_t nVerifiedCount = 0;
	assert( verified.getTrackCount( 0, true, &nVerifiedCount ) == SIR_ANIM_COOK_OK && nVerifiedCount == 3 );
	assert( verified.getTrackCount( 0, false, &nVerifiedCount ) == SIR_ANIM_COOK_OK && nVerifiedCount == 2 );
	assert( verified.getTrackCount( 1, true, &nVerifiedCount ) == SIR_ANIM_COOK_MISSING );
	sirAnimationCookKey_t verifiedKeys[3] = {};
	sirAnimationCookTrackOutput_t verifiedOutput = { verifiedKeys, 3, 0, false, 0 };
	assert( verified.readTrack( 0, false, &verifiedOutput ) == SIR_ANIM_COOK_OK );
	assert( verifiedOutput.bUsedCoarse && verifiedOutput.nWritten == 2 );
	assert( verified.readTrack( 0, true, &verifiedOutput ) == SIR_ANIM_COOK_OK );
	assert( !verifiedOutput.bUsedCoarse && verifiedOutput.nWritten == 3 );
	assert( verified.readTrack( 1, true, &verifiedOutput ) == SIR_ANIM_COOK_MISSING );
	sirAnimationCookKey_t scratchKeys[3] = {};
	uint32_t cursors[3] = {};
	sirAnimationSampleScratch_t scratch = { scratchKeys, 3, cursors, 3, 0, false };
	sirAnimationTransform_t transforms[3] = {};
	sirAnimationMutablePose_t pose = { transforms, 3, 0 };
	sirAnimationSampleOptions_t opts = { 25, 0, 3, SIR_ANIMATION_PLAY_CLAMP, SIR_ANIMATION_INTERPOLATION_LINEAR, false, true };
	assert( SIRAnimationSampler::sample( span, opts, &scratch, &pose ) == SIR_ANIM_COOK_OK );
	assert( pose.nTransformCount == 3 && sirSamplingClose( transforms[0].fTranslation[0], 5 ) );
	assert( sirSamplingClose( transforms[1].fTranslation[0], 0 ) );
	assert( sirSamplingClose( transforms[1].fScale[0], 1 ) );
	assert( sirSamplingClose( transforms[2].fTranslation[0], 5 ) );
	assert( cursors[0] == 0 && scratch.nLastSampleTime == 25 );
	sirAnimationTransform_t defaults[3] = {};
	defaults[1].fTranslation[0] = 27.0f;
	defaults[1].fRotation[3] = 1.0f;
	defaults[1].fScale[0] = defaults[1].fScale[1] = defaults[1].fScale[2] = 1.0f;
	opts.pDefaultTransforms = defaults;
	assert( SIRAnimationSampler::sample( span, opts, &scratch, &pose ) == SIR_ANIM_COOK_OK );
	assert( sirSamplingClose( transforms[1].fTranslation[0], 27 ) );
	opts.iTick = 75;
	assert( SIRAnimationSampler::sample( span, opts, &scratch, &pose ) == SIR_ANIM_COOK_OK );
	assert( sirSamplingClose( transforms[0].fTranslation[0], 15 ) && cursors[0] == 1 );
	opts.iTick = 5;
	assert( SIRAnimationSampler::sample( span, opts, &scratch, &pose ) == SIR_ANIM_COOK_OK );
	assert( sirSamplingClose( transforms[0].fTranslation[0], 1 ) && cursors[0] == 0 );
	opts.iTick = 75;
	opts.eInterpolation = SIR_ANIMATION_INTERPOLATION_STEP;
	assert( SIRAnimationSampler::sample( span, opts, &scratch, &pose ) == SIR_ANIM_COOK_OK );
	assert( sirSamplingClose( transforms[0].fTranslation[0], 10 ) );
	opts.eInterpolation = SIR_ANIMATION_INTERPOLATION_LINEAR;
	opts.bPreferFine = false;
	assert( SIRAnimationSampler::sample( span, opts, &scratch, &pose ) == SIR_ANIM_COOK_OK );
	assert( sirSamplingClose( transforms[0].fTranslation[0], 15 ) );
	opts.bPreferFine = true;
	opts.bReverse = true;
	opts.iTick = 25;
	assert( SIRAnimationSampler::sample( span, opts, &scratch, &pose ) == SIR_ANIM_COOK_OK );
	assert( sirSamplingClose( transforms[0].fTranslation[0], 15 ) );
	opts.bReverse = false;
	opts.ePlayback = SIR_ANIMATION_PLAY_LOOP;
	opts.iTick = 125;
	assert( SIRAnimationSampler::sample( span, opts, &scratch, &pose ) == SIR_ANIM_COOK_OK );
	assert( sirSamplingClose( transforms[0].fTranslation[0], 5 ) );
	opts.ePlayback = SIR_ANIMATION_PLAY_PINGPONG;
	assert( SIRAnimationSampler::sample( span, opts, &scratch, &pose ) == SIR_ANIM_COOK_OK );
	assert( sirSamplingClose( transforms[0].fTranslation[0], 15 ) );
	sirAnimationSampleScratch_t insufficient = scratch;
	insufficient.nKeyCapacity = 2;
	const float old = transforms[0].fTranslation[0];
	assert( SIRAnimationSampler::sample( span, opts, &insufficient, &pose ) == SIR_ANIM_COOK_CAPACITY );
	assert( transforms[0].fTranslation[0] == old );
	opts.ePlayback = SIR_ANIMATION_PLAY_CLAMP;
	opts.eInterpolation = SIR_ANIMATION_INTERPOLATION_STEP;
	opts.iTick = 25;
	assert( SIRAnimationSampler::sample( span, opts, &scratch, &pose ) == SIR_ANIM_COOK_OK );
	assert( sirSamplingClose( transforms[0].fTranslation[0], 0 ) );
	bytes[out.nWritten - 1] ^= 0x91;
	assert( verified.initialize( span, &verifiedInfo ) == SIR_ANIM_COOK_CORRUPT );
	assert( verified.readTrack( 0, true, &verifiedOutput ) == SIR_ANIM_COOK_INVALID );
	assert( SIRAnimationSampler::sample( span, opts, &scratch, &pose ) == SIR_ANIM_COOK_CORRUPT );
}

static void sirTestBlendAndHierarchy( void )
{
	sirAnimationTransform_t original[3] = {};
	sirAnimationTransform_t source[3] = {};
	for ( uint32_t b = 0; b < 3; ++b ) {
		original[b].fRotation[3] = source[b].fRotation[3] = 1;
		original[b].fScale[0] = original[b].fScale[1] = original[b].fScale[2] = 1;
		source[b].fScale[0] = source[b].fScale[1] = source[b].fScale[2] = 1;
		source[b].fTranslation[0] = 4;
	}
	uint8_t mask[3] = { 1, 0, 1 };
	sirAnimationMutablePose_t dest = { original, 3, 3 };
	sirAnimationBlendOptions_t blend = { source, 0.5f, mask, 3, false };
	assert( SIRAnimationSampler::blend( &dest, blend ) == SIR_ANIM_COOK_OK );
	assert( sirSamplingClose( original[0].fTranslation[0], 2 ) );
	assert( sirSamplingClose( original[1].fTranslation[0], 0 ) );
	blend.bAdditive = true;
	assert( SIRAnimationSampler::blend( &dest, blend ) == SIR_ANIM_COOK_OK );
	assert( sirSamplingClose( original[0].fTranslation[0], 4 ) );
	blend.fWeight = 2;
	assert( SIRAnimationSampler::blend( &dest, blend ) == SIR_ANIM_COOK_INVALID );
	int32_t parents[3] = { -1, 0, 1 };
	original[1].fTranslation[1] = 3;
	original[2].fTranslation[2] = 4;
	sirAnimationTransform_t model[3] = {};
	sirAnimationHierarchyOptions_t hierarchy = { parents, original, model, 3 };
	assert( SIRAnimationSampler::evaluateHierarchy( hierarchy ) == SIR_ANIM_COOK_OK );
	assert( sirSamplingClose( model[2].fTranslation[0], 8 ) );
	assert( sirSamplingClose( model[2].fTranslation[1], 3 ) );
	assert( sirSamplingClose( model[2].fTranslation[2], 4 ) );
	parents[1] = 2;
	assert( SIRAnimationSampler::evaluateHierarchy( hierarchy ) == SIR_ANIM_COOK_INVALID );
	parents[1] = 0;
	original[0].fRotation[2] = 0.70710678118f;
	original[0].fRotation[3] = 0.70710678118f;
	original[0].fTranslation[0] = 0;
	original[1].fTranslation[0] = 1;
	original[1].fTranslation[1] = 0;
	original[2].fTranslation[0] = 0;
	original[2].fTranslation[1] = 0;
	original[2].fTranslation[2] = 0;
	assert( SIRAnimationSampler::evaluateHierarchy( { parents, original, model, 3 } ) == SIR_ANIM_COOK_OK );
	assert( sirSamplingClose( model[1].fTranslation[0], 0 ) );
	assert( sirSamplingClose( model[1].fTranslation[1], 1 ) );
	assert( SIRAnimationSampler::evaluateHierarchy( { parents, original, original, 3 } ) == SIR_ANIM_COOK_OK );
	assert( sirSamplingClose( original[1].fTranslation[0], 0 ) );
	assert( sirSamplingClose( original[1].fTranslation[1], 1 ) );
	const float fUntouched = model[0].fTranslation[0];
	original[1].fScale[0] = 0.0f;
	assert( SIRAnimationSampler::evaluateHierarchy( { parents, original, model, 3 } ) == SIR_ANIM_COOK_INVALID );
	assert( model[0].fTranslation[0] == fUntouched );
	original[1].fScale[0] = 1.0f;
	original[1].fRotation[1] = NAN;
	assert( SIRAnimationSampler::evaluateHierarchy( { parents, original, model, 3 } ) == SIR_ANIM_COOK_INVALID );
	assert( model[0].fTranslation[0] == fUntouched );
	original[1].fRotation[1] = 0.0f;
	original[1].fRotation[0] = original[1].fRotation[2] = original[1].fRotation[3] = 0.0f;
	assert( SIRAnimationSampler::evaluateHierarchy( { parents, original, model, 3 } ) == SIR_ANIM_COOK_INVALID );
}

static void sirTestSIMD( void )
{
	float source[67] = {};
	float scalar[67] = {};
	float simd[67] = {};
	for ( uint32_t i = 0; i < 67; ++i ) {
		source[i] = ( float )i * 0.125f + 1.0f;
		scalar[i] = simd[i] = ( float )i * 0.0625f - 2.0f;
	}
	for ( uint32_t i = 0; i < 67; ++i ) {
		scalar[i] += ( source[i] - scalar[i] ) * 0.375f;
	}
	const sirAnimationSoaBlend_t options = { simd, source, 67, 0.375f };
	assert( SIRAnimationSIMD::blendChannels( options ) == SIR_ANIM_COOK_OK );
	for ( uint32_t i = 0; i < 67; ++i ) {
		assert( std::fabs( scalar[i] - simd[i] ) < 0.00001f );
	}
	assert( SIRAnimationSIMD::blendChannels( { simd, simd, 67, 0.375f } ) == SIR_ANIM_COOK_OK );
	assert( SIRAnimationSIMD::blendChannels( { simd, source, 67, -1.0f } ) == SIR_ANIM_COOK_INVALID );
	assert( SIRAnimationSIMD::blendChannels( { simd + 1, simd, 32, 0.5f } ) == SIR_ANIM_COOK_INVALID );
}

#ifdef SIR_ANIM_WITH_JOBS
static void sirTestSchedulerBatch( void )
{
	SIRMemoryManager memory;
	const sirMemoryConfig_t memCfg = { 64ULL * 1024 * 1024, 32ULL * 1024 * 1024, 8ULL * 1024 * 1024, 32 };
	assert( memory.init( memCfg ) );
	SIRTaskScheduler scheduler;
	assert( scheduler.init( &memory, { 16, 0, 32 } ) );
	sirAnimationCookKey_t keys[2] = { sirSamplingKey( 0, 0 ), sirSamplingKey( 10, 10 ) };
	sirAnimationCookTrack_t track = { 0, keys, 2, 0, 0, {} };
	sirAnimationCookClipInput_t clip = {};
	clip.pTracks = &track;
	clip.nTrackCount = 1;
	clip.nDuration = 10;
	clip.nRevision = 1;
	clip.nRootBoneIndex = 0;
	clip.fBoneRadius = 0.5f;
	uint8_t bytes[4096] = {};
	sirAnimationCookOutput_t output = { bytes, sizeof( bytes ), 0 };
	assert( SIRAnimationCooker::cookClip( clip, &output ) == SIR_ANIM_COOK_OK );
	SIRAtomic<uint32_t> generation( 3 );
	sirAnimationCookKey_t scratch[2][2] = {};
	uint32_t cursors[2][1] = {};
	sirAnimationTransform_t poses[2] = {};
	sirAnimationSamplingJob_t jobs[2] = {};
	for ( uint32_t i = 0; i < 2; ++i ) {
		jobs[i].clip = { bytes, output.nWritten };
		jobs[i].options = { ( int64_t )( i * 5 ), 0, 1, SIR_ANIMATION_PLAY_CLAMP, SIR_ANIMATION_INTERPOLATION_LINEAR, false, true };
		jobs[i].scratch = { scratch[i], 2, cursors[i], 1, 0, false };
		jobs[i].pose = { &poses[i], 1, 0 };
		jobs[i].pCancellationGeneration = &generation;
		jobs[i].nExpectedGeneration = 3;
	}
	sirTaskBatchTask_t descriptors[2] = {};
	sirTaskHandle_t handles[2] = {};
	const sirAnimationSamplingBatch_t batch = { jobs, descriptors, handles, 2 };
	assert( SIRAnimationSamplingJobs::submitBatch( scheduler, batch ) );
	assert( scheduler.pumpCPU( 2 ) == 2 );
	assert( scheduler.getState( handles[0] ) == SIR_TASK_COMPLETED );
	assert( scheduler.getState( handles[1] ) == SIR_TASK_COMPLETED );
	assert( sirSamplingClose( poses[0].fTranslation[0], 0 ) );
	assert( sirSamplingClose( poses[1].fTranslation[0], 5 ) );
	assert( scheduler.releaseTask( handles[0] ) );
	assert( scheduler.releaseTask( handles[1] ) );
	jobs[0].nExpectedGeneration = 3;
	jobs[0].pose.nTransformCount = 0;
	const sirAnimationSamplingBatch_t cancelled = { jobs, descriptors, handles, 1 };
	assert( SIRAnimationSamplingJobs::submitBatch( scheduler, cancelled ) );
	generation.store( 4, SIR_MEMORY_ORDER_RELEASE );
	assert( scheduler.pumpCPU( 1 ) == 1 );
	assert( scheduler.getState( handles[0] ) == SIR_TASK_FAILED );
	assert( jobs[0].pose.nTransformCount == 0 );
	assert( scheduler.releaseTask( handles[0] ) );
	assert( scheduler.shutdown() );
	memory.shutdown();
}
#endif

int main( void )
{
	sirTestPlayback();
	sirTestSampling();
	sirTestBlendAndHierarchy();
	sirTestSIMD();
#ifdef SIR_ANIM_WITH_JOBS
	sirTestSchedulerBatch();
#endif
	puts( "ANM-02 CPU animation sampling: tests passed" );
	return 0;
}
