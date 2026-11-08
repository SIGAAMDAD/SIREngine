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
#include <Animation/Cooked/SIRAnimationCooked.h>
#include <SceneCooker/Animation/SIRAnimationCooker.h>
#include <gtest/gtest.h>

TEST( SIRAnimationCook, InvalidInputsDoNotPublish )
{
	sirAnimationCookSkeletonInput_t rig = {};
	uint8_t bytes[256] = {};
	sirAnimationCookOutput_t output = { bytes, sizeof( bytes ), 0 };
	EXPECT_EQ( SIR_ANIM_COOK_INVALID, SIRAnimationCooker::cookSkeleton( rig, &output ) );
	sirAnimationCookClipInput_t clip = {};
	EXPECT_EQ( SIR_ANIM_COOK_INVALID, SIRAnimationCooker::cookClip( clip, &output ) );
}

TEST( SIRAnimationCook, InvalidArchiveRejected )
{
	uint8_t bytes[256] = {};
	sirAnimationCookAssetInfo_t info = {};
	EXPECT_EQ( SIR_ANIM_COOK_CORRUPT, SIRAnimationCooked::inspect( { bytes, sizeof( bytes ) }, &info ) );
	EXPECT_EQ( SIR_ANIM_COOK_INVALID, SIRAnimationCooked::inspect( { nullptr, 0 }, &info ) );
	EXPECT_EQ( SIR_ANIM_COOK_INVALID, SIRAnimationCooked::inspect( { bytes, sizeof( bytes ) }, nullptr ) );
}

TEST( SIRAnimationCook, CoarseOnlyTrackDecodes )
{
	sirAnimationCookKey_t key = {};
	key.fComponents[6] = 1.0f;
	key.fComponents[7] = key.fComponents[8] = key.fComponents[9] = 1.0f;
	sirAnimationCookTrack_t track = { 5, &key, 1, 1, {} };
	sirAnimationCookClipInput_t clip = { &track, 1, nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0, 10, 0.0f, 1.0f, 5, 1 };
	uint8_t bytes[512] = {};
	sirAnimationCookOutput_t output = { bytes, sizeof( bytes ), 0 };
	ASSERT_EQ( SIR_ANIM_COOK_OK, SIRAnimationCooker::cookClip( clip, &output ) );
	sirAnimationCookKey_t decoded = {};
	sirAnimationCookTrackOutput_t result = { &decoded, 1, 0, false };
	EXPECT_EQ( SIR_ANIM_COOK_OK, SIRAnimationCooked::readTrack( { bytes, output.nWritten }, 5, true, &result ) );
	EXPECT_TRUE( result.bUsedCoarse );
	EXPECT_EQ( 1U, result.nWritten );
}
