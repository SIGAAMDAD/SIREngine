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
#include <SceneCooker/Animation/SIRAnimationCooker.h>
#include <cassert>
#include <cstring>

int main( void )
{
	sirAnimationTransform_t transform = {};
	transform.fRotation[3] = 1.0f;
	transform.fScale[0] = transform.fScale[1] = transform.fScale[2] = 1.0f;
	sirAnimationCookBone_t bone = { -1, transform, transform, 1.0f, 42 };
	sirAnimationCookSkeletonInput_t skeleton = {};
	skeleton.pBones = &bone;
	skeleton.nBoneCount = 1;
	skeleton.nRevision = 7;
	uint8_t first[1024] = {};
	uint8_t second[1024] = {};
	sirAnimationCookOutput_t a = { first, sizeof( first ), 0 };
	sirAnimationCookOutput_t b = { second, sizeof( second ), 0 };
	assert( SIRAnimationCooker::cookSkeleton( skeleton, &a ) == SIR_ANIM_COOK_OK );
	assert( SIRAnimationCooker::cookSkeleton( skeleton, &b ) == SIR_ANIM_COOK_OK );
	assert( a.nWritten == b.nWritten && memcmp( first, second, a.nWritten ) == 0 );
	sirAnimationCookAssetInfo_t info = {};
	assert( SIRAnimationCooked::inspect( { first, a.nWritten }, &info ) == SIR_ANIM_COOK_OK );
	assert( info.nItemCount == 1 && info.nRevision == 7 );
	first[a.nWritten - 1] ^= 1;
	assert( SIRAnimationCooked::inspect( { first, a.nWritten }, &info ) == SIR_ANIM_COOK_CORRUPT );
	bone.iParent = 0;
	assert( SIRAnimationCooker::cookSkeleton( skeleton, &a ) == SIR_ANIM_COOK_INVALID );
	return 0;
}
