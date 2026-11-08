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
#include <SceneCooker/Animation/SIRAnimationCooker.h>
#include <cassert>
#include <cmath>

int main( void )
{
	assert( SIRAnimationSampler::normalizeTick( 125, 0, 100, SIR_ANIMATION_PLAY_LOOP ) == 25 );
	assert( SIRAnimationSampler::normalizeTick( 125, 0, 100, SIR_ANIMATION_PLAY_PINGPONG ) == 75 );
	sirAnimationCookKey_t keys[2] = {};
	keys[0].fComponents[6] = keys[1].fComponents[6] = 1;
	for ( uint32_t i = 7; i < 10; ++i ) {
		keys[0].fComponents[i] = keys[1].fComponents[i] = 1;
	}
	keys[1].nTime = 100;
	keys[1].fComponents[0] = 10;
	sirAnimationCookTrack_t track = {};
	track.nBoneIndex = 0;
	track.pKeys = keys;
	track.nKeyCount = 2;
	sirAnimationCookClipInput_t clip = {};
	clip.pTracks = &track;
	clip.nTrackCount = 1;
	clip.nDuration = 100;
	clip.nRevision = 1;
	clip.fMaximumQuantError = 0.001f;
	clip.fBoneRadius = 1;
	uint8_t archive[2048] = {};
	sirAnimationCookOutput_t output = { archive, sizeof( archive ), 0 };
	assert( SIRAnimationCooker::cookClip( clip, &output ) == SIR_ANIM_COOK_OK );
	sirAnimationCookKey_t scratchKeys[2] = {};
	sirAnimationSampleScratch_t scratch = { scratchKeys, 2, nullptr, 0, 0, false };
	sirAnimationTransform_t poseData[1] = {};
	sirAnimationMutablePose_t pose = { poseData, 1, 0 };
	sirAnimationSampleOptions_t options = { 50, 0, 1, SIR_ANIMATION_PLAY_CLAMP, SIR_ANIMATION_INTERPOLATION_LINEAR, false, true };
	assert( SIRAnimationSampler::sample( { archive, output.nWritten }, options, &scratch, &pose ) == SIR_ANIM_COOK_OK );
	assert( std::fabs( poseData[0].fTranslation[0] - 5.0f ) < 0.002f );
	return 0;
}
