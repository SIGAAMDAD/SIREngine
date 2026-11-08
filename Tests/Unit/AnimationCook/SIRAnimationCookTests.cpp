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

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

static sirAnimationTransform_t sirTestTransform( float fX ) noexcept
{
	sirAnimationTransform_t t = {};
	t.fTranslation[0] = fX;
	t.fRotation[3] = 1.0f;
	t.fScale[0] = t.fScale[1] = t.fScale[2] = 1.0f;
	return t;
}

static sirAnimationCookKey_t sirTestKey( uint32_t nTime, float fX ) noexcept
{
	sirAnimationCookKey_t key = {};
	key.nTime = nTime;
	key.fComponents[0] = fX;
	key.fComponents[6] = 1.0f;
	key.fComponents[7] = key.fComponents[8] = key.fComponents[9] = 1.0f;
	return key;
}

static void sirTestSkeleton( void )
{
	sirAnimationCookBone_t bones[3] = {};
	const sirAnimationCookLodMapping_t lod[2] = { { 0, 0 }, { 1, 2 } };
	const uint32_t weights[4] = { 2, 1, 0, 2 };
	const SIRResourceKey dependency = { 99, 3, 2 };
	for ( uint32_t i = 0; i < 3; ++i ) {
		bones[i].iParent = ( int32_t )i - 1;
		bones[i].localBind = sirTestTransform( ( float )i );
		bones[i].inverseBind = sirTestTransform( -( float )i );
		bones[i].fConservativeRadius = 0.75f;
		bones[i].nStableId = 123 + i;
	}
	sirAnimationCookSkeletonInput_t input = { bones, 3, lod, 2, weights, 4, &dependency, 1, 11 };
	uint8_t buffer[2048] = {};
	uint8_t second[2048] = {};
	sirAnimationCookOutput_t output = { buffer, sizeof( buffer ), 0 };
	assert( SIRAnimationCooker::cookSkeleton( input, &output ) == SIR_ANIM_COOK_OK );
	sirAnimationCookOutput_t repeated = { second, sizeof( second ), 0 };
	assert( SIRAnimationCooker::cookSkeleton( input, &repeated ) == SIR_ANIM_COOK_OK );
	assert( output.nWritten == repeated.nWritten && memcmp( buffer, second, output.nWritten ) == 0 );
	sirAnimationCookSpan_t source = { buffer, output.nWritten };
	sirAnimationCookAssetInfo_t info = {};
	assert( SIRAnimationCooked::inspect( source, &info ) == SIR_ANIM_COOK_OK );
	assert( info.nItemCount == 3 && info.nLodCount == 2 && info.nDependencyCount == 1 && info.nRevision == 11 );
	sirAnimationCookBone_t bone = {};
	assert( SIRAnimationCooked::readBone( source, 2, &bone ) == SIR_ANIM_COOK_OK );
	assert( bone.iParent == 1 && bone.nStableId == 125 && bone.localBind.fTranslation[0] == 2.0f );
	sirAnimationCookLodMapping_t mapping = {};
	assert( SIRAnimationCooked::readLodMapping( source, 1, &mapping ) == SIR_ANIM_COOK_OK && mapping.nSourceBoneIndex == 2 &&
	        mapping.nLodLevel == 1 );
	uint32_t index = 99;
	assert( SIRAnimationCooked::readInfluence( source, 1, &index ) == SIR_ANIM_COOK_OK && index == 1 );
	SIRResourceKey actual = {};
	assert( SIRAnimationCooked::readDependency( source, 0, &actual ) == SIR_ANIM_COOK_OK && actual.m_nAssetId == 99 );
	assert( info.bounds.fMinimum[0] <= -0.75f && info.bounds.fMaximum[0] >= 2.75f );
	assert( SIRAnimationCooked::readBone( source, 3, &bone ) == SIR_ANIM_COOK_INVALID );
	assert( SIRAnimationCooked::readDependency( source, 1, &actual ) == SIR_ANIM_COOK_INVALID );
	buffer[output.nWritten - 1] ^= 0x7f;
	assert( SIRAnimationCooked::inspect( source, &info ) == SIR_ANIM_COOK_CORRUPT );
	memcpy( buffer, second, output.nWritten );
	buffer[4] = 2;
	assert( SIRAnimationCooked::inspect( source, &info ) == SIR_ANIM_COOK_VERSION_ERROR );
	bones[2].iParent = 2;
	assert( SIRAnimationCooker::cookSkeleton( input, &output ) == SIR_ANIM_COOK_INVALID );
	bones[2].iParent = 1;
	bones[0].iParent = 1;
	assert( SIRAnimationCooker::cookSkeleton( input, &output ) == SIR_ANIM_COOK_INVALID );
	bones[0].iParent = -1;
	bones[2].nStableId = bones[1].nStableId;
	assert( SIRAnimationCooker::cookSkeleton( input, &output ) == SIR_ANIM_COOK_INVALID );
	bones[2].nStableId = 125;
	const sirAnimationCookLodMapping_t badLod[1] = { { 0, 999 } };
	input.pLodMappings = badLod;
	input.nLodMappingCount = 1;
	assert( SIRAnimationCooker::cookSkeleton( input, &output ) == SIR_ANIM_COOK_INVALID );
	input.pLodMappings = lod;
	input.nLodMappingCount = 2;
	output.nCapacity = 1;
	assert( SIRAnimationCooker::cookSkeleton( input, &output ) == SIR_ANIM_COOK_CAPACITY );
}

static void sirTestClip( void )
{
	sirAnimationCookKey_t fine[3] = { sirTestKey( 0, 0 ), sirTestKey( 50, 1 ), sirTestKey( 100, 2 ) };
	sirAnimationCookKey_t coarse[2] = { sirTestKey( 0, 0 ), sirTestKey( 100, 2 ) };
	sirAnimationCookKey_t constant[2] = { sirTestKey( 0, 7 ), sirTestKey( 100, 7 ) };
	sirAnimationCookTrack_t tracks[3] = { { 1, fine, 3, 0, {} }, { 1, coarse, 2, 1, {} }, { 3, constant, 2, 0, {} } };
	sirAnimationCookEvent_t events[2] = { { 10, 10, 200 }, { 50, 11, 201 } };
	sirAnimationCookCurve_t curves[1] = { { 20, 0 } };
	sirAnimationCookMorph_t morphs[1] = { { 21, 2 } };
	SIRResourceKey dep[2] = { { 100, 10, 0 }, { 200, 11, 1 } };
	sirAnimationCookClipInput_t input = { tracks, 3, events, 2, curves, 1, morphs, 1, dep, 2, 100, 0.001f, 0.5f, 1, 17 };
	uint8_t first[8192] = {};
	uint8_t second[8192] = {};
	sirAnimationCookOutput_t output = { first, sizeof( first ), 0 };
	assert( SIRAnimationCooker::cookClip( input, &output ) == SIR_ANIM_COOK_OK );
	sirAnimationCookOutput_t twice = { second, sizeof( second ), 0 };
	assert( SIRAnimationCooker::cookClip( input, &twice ) == SIR_ANIM_COOK_OK );
	assert( output.nWritten == twice.nWritten && memcmp( first, second, output.nWritten ) == 0 );
	sirAnimationCookSpan_t span = { first, output.nWritten };
	sirAnimationCookAssetInfo_t info = {};
	assert( SIRAnimationCooked::inspect( span, &info ) == SIR_ANIM_COOK_OK );
	assert( info.nDuration == 100 && info.nItemCount == 3 && info.nEventCount == 2 && info.nMorphCount == 1 );
	assert( info.bounds.fMinimum[0] <= -0.5f && info.bounds.fMaximum[0] >= 7.5f );
	sirAnimationCookKey_t decoded[3] = {};
	sirAnimationCookTrackOutput_t dest = { decoded, 3, 0, false };
	assert( SIRAnimationCooked::readTrack( span, 1, true, &dest ) == SIR_ANIM_COOK_OK );
	assert( !dest.bUsedCoarse && dest.nExecutionMask == 3 && dest.nWritten == 3 && std::fabs( decoded[1].fComponents[0] - 1 ) < 0.001f );
	assert( SIRAnimationCooked::readTrack( span, 1, false, &dest ) == SIR_ANIM_COOK_OK );
	assert( dest.bUsedCoarse && dest.nWritten == 2 );
	assert( SIRAnimationCooked::readTrack( span, 3, true, &dest ) == SIR_ANIM_COOK_OK );
	assert( dest.nWritten == 2 && decoded[0].fComponents[0] == 7 && decoded[1].fComponents[0] == 7 );
	assert( SIRAnimationCooked::readTrack( span, 3, false, &dest ) == SIR_ANIM_COOK_OK );
	assert( !dest.bUsedCoarse );
	uint32_t count = 0;
	assert( SIRAnimationCooked::getTrackCount( span, 1, true, &count ) == SIR_ANIM_COOK_OK && count == 3 );
	assert( SIRAnimationCooked::getTrackCount( span, 555, true, &count ) == SIR_ANIM_COOK_MISSING );
	dest.nCapacity = 1;
	assert( SIRAnimationCooked::readTrack( span, 1, true, &dest ) == SIR_ANIM_COOK_CAPACITY );
	sirAnimationCookEvent_t evt = {};
	assert( SIRAnimationCooked::readEvent( span, 1, &evt ) == SIR_ANIM_COOK_OK && evt.nPayloadId == 201 );
	sirAnimationCookCurve_t curve = {};
	sirAnimationCookMorph_t morph = {};
	assert( SIRAnimationCooked::readCurve( span, 0, &curve ) == SIR_ANIM_COOK_OK && curve.nTrackIndex == 0 );
	assert( SIRAnimationCooked::readMorph( span, 0, &morph ) == SIR_ANIM_COOK_OK && morph.nTrackIndex == 2 );
	SIRResourceKey key = {};
	assert( SIRAnimationCooked::readDependency( span, 1, &key ) == SIR_ANIM_COOK_OK && key.m_nAssetId == 200 );
	first[output.nWritten - 1] ^= 0x42;
	assert( SIRAnimationCooked::inspect( span, &info ) == SIR_ANIM_COOK_CORRUPT );
	memcpy( first, second, output.nWritten );
	first[16] ^= 0x44;
	assert( SIRAnimationCooked::inspect( span, &info ) == SIR_ANIM_COOK_CORRUPT );
	memcpy( first, second, output.nWritten );
	fine[1].nTime = 0;
	assert( SIRAnimationCooker::cookClip( input, &output ) == SIR_ANIM_COOK_INVALID );
	fine[1].nTime = 50;
	fine[1].fComponents[0] = NAN;
	assert( SIRAnimationCooker::cookClip( input, &output ) == SIR_ANIM_COOK_INVALID );
	fine[1].fComponents[0] = 1;
	tracks[1].bCoarse = 0;
	assert( SIRAnimationCooker::cookClip( input, &output ) == SIR_ANIM_COOK_INVALID );
	tracks[1].bCoarse = 1;
	input.nDuration = 40;
	assert( SIRAnimationCooker::cookClip( input, &output ) == SIR_ANIM_COOK_INVALID );
	input.nDuration = 100;
	output.nCapacity = 8;
	assert( SIRAnimationCooker::cookClip( input, &output ) == SIR_ANIM_COOK_CAPACITY );
	input.fMaximumQuantError = 0;
	output.nCapacity = sizeof( first );
	assert( SIRAnimationCooker::cookClip( input, &output ) == SIR_ANIM_COOK_OK );
	span.nBytes = output.nWritten;
	assert( SIRAnimationCooked::inspect( span, &info ) == SIR_ANIM_COOK_OK );
	assert( SIRAnimationCooked::readTrack( span, 1, true, &dest ) == SIR_ANIM_COOK_CAPACITY );
}

static void sirTestReferenceDecompression( void )
{
	uint32_t nRandom = 0x8a21f715U;
	for ( uint32_t nCase = 0; nCase < 128; ++nCase ) {
		sirAnimationCookKey_t keys[32] = {};
		for ( uint32_t k = 0; k < 32; ++k ) {
			keys[k].nTime = k * 17;
			for ( uint32_t c = 0; c < 10; ++c ) {
				nRandom ^= nRandom << 13;
				nRandom ^= nRandom >> 17;
				nRandom ^= nRandom << 5;
				keys[k].fComponents[c] = ( float )( ( int32_t )( nRandom % 20001 ) - 10000 ) / 512.0f;
			}
		}
		sirAnimationCookTrack_t track = { nCase % 8, keys, 32, 0, {} };
		sirAnimationCookClipInput_t input = { &track, 1, nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0, 527, 0.001f, 0.1f, 0, 1 };
		uint8_t cooked[4096] = {};
		sirAnimationCookOutput_t output = { cooked, sizeof( cooked ), 0 };
		assert( SIRAnimationCooker::cookClip( input, &output ) == SIR_ANIM_COOK_OK );
		sirAnimationCookSpan_t span = { cooked, output.nWritten };
		sirAnimationCookKey_t decoded[32] = {};
		sirAnimationCookTrackOutput_t result = { decoded, 32, 0, false };
		assert( SIRAnimationCooked::readTrack( span, track.nBoneIndex, true, &result ) == SIR_ANIM_COOK_OK );
		for ( uint32_t k = 0; k < 32; ++k ) {
			assert( decoded[k].nTime == keys[k].nTime );
			for ( uint32_t c = 0; c < 10; ++c ) {
				assert( std::fabs( decoded[k].fComponents[c] - keys[k].fComponents[c] ) < 0.00101f );
			}
		}
		// Every byte of the archive participates in the global CRC; bit
		// flips must not yield a valid decoded animation state.
		sirAnimationCookAssetInfo_t info = {};
		for ( uint32_t byte = 0; byte < output.nWritten; byte += 17 ) {
			cooked[byte] ^= 0x80;
			assert( SIRAnimationCooked::inspect( span, &info ) != SIR_ANIM_COOK_OK );
			cooked[byte] ^= 0x80;
		}
	}
}

static void sirTestMissingFineFallback( void )
{
	sirAnimationCookKey_t coarse[2] = { sirTestKey( 0, 0 ), sirTestKey( 20, 4 ) };
	sirAnimationCookTrack_t track = { 9, coarse, 2, 1, {} };
	sirAnimationCookClipInput_t input = { &track, 1, nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0, 20, 0.001f, 0.0f, 9, 1 };
	uint8_t buffer[1024] = {};
	sirAnimationCookOutput_t output = { buffer, sizeof( buffer ), 0 };
	assert( SIRAnimationCooker::cookClip( input, &output ) == SIR_ANIM_COOK_OK );
	sirAnimationCookKey_t decoded[2] = {};
	sirAnimationCookTrackOutput_t result = { decoded, 2, 0, false };
	assert( SIRAnimationCooked::readTrack( { buffer, output.nWritten }, 9, true, &result ) == SIR_ANIM_COOK_OK );
	assert( result.bUsedCoarse && result.nWritten == 2 );
}

static uint32_t sirTestRead32( const uint8_t *p ) noexcept
{
	return ( uint32_t )p[0] | ( ( uint32_t )p[1] << 8 ) | ( ( uint32_t )p[2] << 16 ) | ( ( uint32_t )p[3] << 24 );
}

static void sirTestWrite32( uint8_t *p, uint32_t n ) noexcept
{
	for ( uint32_t i = 0; i < 4; ++i ) {
		p[i] = ( uint8_t )( n >> ( i * 8 ) );
	}
}

static void sirTestWrite64( uint8_t *p, uint64_t n ) noexcept
{
	sirTestWrite32( p, ( uint32_t )n );
	sirTestWrite32( p + 4, ( uint32_t )( n >> 32 ) );
}

static void sirTestRepairChecksum( uint8_t *p, uint64_t nLength ) noexcept
{
	sirTestWrite32( p + 140, 0 );
	uint32_t nCrc = 0xffffffffU;
	for ( uint64_t i = 0; i < nLength; ++i ) {
		nCrc ^= p[i];
		for ( uint32_t j = 0; j < 8; ++j ) {
			nCrc = ( nCrc >> 1 ) ^ ( ( nCrc & 1U ) ? 0xedb88320U : 0 );
		}
	}
	sirTestWrite32( p + 140, ~nCrc );
}

static void sirTestMalformedArchiveFields( void )
{
	sirAnimationCookBone_t bone = {};
	bone.iParent = -1;
	bone.nStableId = 1;
	bone.localBind = sirTestTransform( 0 );
	bone.inverseBind = sirTestTransform( 0 );
	sirAnimationCookSkeletonInput_t rig = { &bone, 1, nullptr, 0, nullptr, 0, nullptr, 0, 1 };
	uint8_t good[2048] = {};
	uint8_t modified[2048] = {};
	sirAnimationCookOutput_t output = { good, sizeof( good ), 0 };
	assert( SIRAnimationCooker::cookSkeleton( rig, &output ) == SIR_ANIM_COOK_OK );
	sirAnimationCookAssetInfo_t info = {};
	struct Edit
	{
		uint32_t nOffset;
		uint32_t nValue;
	};
	const Edit edits[] = {
	    { 7, 1 },             // unsupported flags
	    { 16, 0 },            // zero revision
	    { 24, 0 },            // empty skeleton
	    { 24, 4097 },         // excessive bone count
	    { 28, 1 },            // bogus LOD count
	    { 36, 1 },            // skeleton events forbidden
	    { 60, 0 },            // section must begin at header
	    { 64, 0xffffffffU },  // enormous section offset
	    { 116, 0x7fc00000U }, // NaN bounds
	    { 144, 7 },           // cyclic/forward parent index
	    { 148, 0x7fc00000U }  // NaN transform
	};
	for ( const Edit& edit : edits ) {
		memcpy( modified, good, output.nWritten );
		sirTestWrite32( modified + edit.nOffset, edit.nValue );
		sirTestRepairChecksum( modified, output.nWritten );
		assert( SIRAnimationCooked::inspect( { modified, output.nWritten }, &info ) != SIR_ANIM_COOK_OK );
	}
	memcpy( modified, good, output.nWritten );
	modified[6] = 254;
	sirTestRepairChecksum( modified, output.nWritten );
	assert( SIRAnimationCooked::inspect( { modified, output.nWritten }, &info ) == SIR_ANIM_COOK_UNSUPPORTED );
	memcpy( modified, good, output.nWritten );
	sirTestWrite64( modified + 8, output.nWritten - 1 );
	sirTestRepairChecksum( modified, output.nWritten );
	assert( SIRAnimationCooked::inspect( { modified, output.nWritten }, &info ) == SIR_ANIM_COOK_CORRUPT );
	assert( SIRAnimationCooked::inspect( { nullptr, 0 }, &info ) == SIR_ANIM_COOK_INVALID );
	assert( SIRAnimationCooked::inspect( { good, 4 }, &info ) == SIR_ANIM_COOK_INVALID );
	assert( SIRAnimationCooked::readBone( { good, output.nWritten }, 0, nullptr ) == SIR_ANIM_COOK_INVALID );
	assert( SIRAnimationCooked::readLodMapping( { good, output.nWritten }, 0, nullptr ) == SIR_ANIM_COOK_INVALID );
	assert( SIRAnimationCooked::readInfluence( { good, output.nWritten }, 0, nullptr ) == SIR_ANIM_COOK_INVALID );
	assert( SIRAnimationCooked::readDependency( { good, output.nWritten }, 0, nullptr ) == SIR_ANIM_COOK_INVALID );
}

static void sirTestMalformedTrackFields( void )
{
	sirAnimationCookKey_t keys[2] = { sirTestKey( 0, 0 ), sirTestKey( 100, 1 ) };
	sirAnimationCookTrack_t track = { 7, keys, 2, 0, {} };
	sirAnimationCookClipInput_t clip = { &track, 1, nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0, 100, 0.001f, 0.01f, 7, 1 };
	uint8_t good[4096] = {};
	uint8_t modified[4096] = {};
	sirAnimationCookOutput_t output = { good, sizeof( good ), 0 };
	assert( SIRAnimationCooker::cookClip( clip, &output ) == SIR_ANIM_COOK_OK );
	sirAnimationCookAssetInfo_t info = {};
	const uint32_t nTrack = 144;
	const uint32_t nPayload = sirTestRead32( good + nTrack + 16 );
	const uint32_t offsets[][2] = {
	    { nTrack + 4, 0 },            // zero key count
	    { nTrack + 4, 65537 },        // sample count bomb
	    { nTrack + 8, 3 },            // coarse flag invalid
	    { nTrack + 12, 4 },           // encoding not recognized
	    { nTrack + 16, 0 },           // overlap with descriptors
	    { nTrack + 24, 1 },           // truncated payload
	    { nTrack + 32, 0x7fc00000U }, // NaN range
	    { nTrack + 72, 0xff800000U }, // invalid range
	    { nTrack + 112, 0 },          // wrong per-track CRC
	    { nPayload + 4, 0 },          // duplicate timestamps
	};
	for ( const auto& edit : offsets ) {
		memcpy( modified, good, output.nWritten );
		sirTestWrite32( modified + edit[0], edit[1] );
		sirTestRepairChecksum( modified, output.nWritten );
		assert( SIRAnimationCooked::inspect( { modified, output.nWritten }, &info ) != SIR_ANIM_COOK_OK );
	}
	sirAnimationCookKey_t out[2] = {};
	sirAnimationCookTrackOutput_t dst = { out, 2, 0, false };
	assert( SIRAnimationCooked::readTrack( { good, output.nWritten }, 7, true, nullptr ) == SIR_ANIM_COOK_INVALID );
	assert( SIRAnimationCooked::readTrack( { good, output.nWritten }, 7, true, &dst ) == SIR_ANIM_COOK_OK );
	assert( SIRAnimationCooked::getTrackCount( { good, output.nWritten }, 7, true, nullptr ) == SIR_ANIM_COOK_INVALID );
	assert( SIRAnimationCooked::readEvent( { good, output.nWritten }, 0, nullptr ) == SIR_ANIM_COOK_INVALID );
	assert( SIRAnimationCooked::readCurve( { good, output.nWritten }, 0, nullptr ) == SIR_ANIM_COOK_INVALID );
	assert( SIRAnimationCooked::readMorph( { good, output.nWritten }, 0, nullptr ) == SIR_ANIM_COOK_INVALID );
	assert( SIRAnimationCooked::readEvent( { good, output.nWritten }, 0, ( sirAnimationCookEvent_t * )out ) == SIR_ANIM_COOK_INVALID );
}

static void sirTestRawTrackAndInputFailures( void )
{
	sirAnimationCookKey_t keys[2] = { sirTestKey( 0, -1000 ), sirTestKey( 3, 1000 ) };
	sirAnimationCookTrack_t track = { 2, keys, 2, 0, {} };
	SIRResourceKey dependencies[2] = { { 1, 8, 9 }, { 1, 8, 9 } };
	sirAnimationCookClipInput_t clip = { &track, 1, nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0, 3, 0.0f, 0.0f, 2, 1 };
	uint8_t bytes[2048] = {};
	sirAnimationCookOutput_t output = { bytes, sizeof( bytes ), 0 };
	assert( SIRAnimationCooker::cookClip( clip, &output ) == SIR_ANIM_COOK_OK );
	assert( sirTestRead32( bytes + 144 + 12 ) == SIR_ANIM_TRACK_RAW32 );
	sirAnimationCookKey_t decoded[2] = {};
	sirAnimationCookTrackOutput_t result = { decoded, 2, 0, false };
	assert( SIRAnimationCooked::readTrack( { bytes, output.nWritten }, 2, true, &result ) == SIR_ANIM_COOK_OK );
	assert( decoded[1].fComponents[0] == 1000.0f && decoded[0].fComponents[0] == -1000.0f );
	clip.pDependencies = dependencies;
	clip.nDependencyCount = 2;
	assert( SIRAnimationCooker::cookClip( clip, &output ) == SIR_ANIM_COOK_INVALID );
	dependencies[1].m_nVariantId = 10;
	assert( SIRAnimationCooker::cookClip( clip, &output ) == SIR_ANIM_COOK_OK );
	clip.nDependencyCount = SIR_ANIM_COOK_MAX_METADATA + 1;
	assert( SIRAnimationCooker::cookClip( clip, &output ) == SIR_ANIM_COOK_INVALID );
	clip.nDependencyCount = 0;
	clip.pDependencies = nullptr;
	clip.fMaximumQuantError = -1.0f;
	assert( SIRAnimationCooker::cookClip( clip, &output ) == SIR_ANIM_COOK_INVALID );
	clip.fMaximumQuantError = 0;
	clip.fBoneRadius = NAN;
	assert( SIRAnimationCooker::cookClip( clip, &output ) == SIR_ANIM_COOK_INVALID );
	clip.fBoneRadius = 0;
	clip.nRevision = 0;
	assert( SIRAnimationCooker::cookClip( clip, &output ) == SIR_ANIM_COOK_INVALID );
	clip.nRevision = 1;
	track.nReserved[1] = 1;
	assert( SIRAnimationCooker::cookClip( clip, &output ) == SIR_ANIM_COOK_INVALID );
	track.nReserved[1] = 0;
	track.nExecutionMask = 1;
	assert( SIRAnimationCooker::cookClip( clip, &output ) == SIR_ANIM_COOK_OK );
	track.nExecutionMask = 2;
	assert( SIRAnimationCooker::cookClip( clip, &output ) == SIR_ANIM_COOK_OK );
	track.nExecutionMask = 4;
	assert( SIRAnimationCooker::cookClip( clip, &output ) == SIR_ANIM_COOK_INVALID );
	track.nExecutionMask = 0;
	keys[1].fComponents[2] = INFINITY;
	assert( SIRAnimationCooker::cookClip( clip, &output ) == SIR_ANIM_COOK_INVALID );
	keys[1].fComponents[2] = 0;
	sirAnimationCookCurve_t invalidCurve = { 5, 99 };
	clip.pCurves = &invalidCurve;
	clip.nCurveCount = 1;
	assert( SIRAnimationCooker::cookClip( clip, &output ) == SIR_ANIM_COOK_INVALID );
	clip.nCurveCount = 0;
	clip.pCurves = nullptr;
	sirAnimationCookEvent_t badEvent[2] = { { 2, 1, 2 }, { 1, 2, 3 } };
	clip.pEvents = badEvent;
	clip.nEventCount = 2;
	assert( SIRAnimationCooker::cookClip( clip, &output ) == SIR_ANIM_COOK_INVALID );
	clip.nEventCount = 0;
	clip.pEvents = nullptr;
	assert( SIRAnimationCooker::cookClip( clip, nullptr ) == SIR_ANIM_COOK_INVALID );
	assert( SIRAnimationCooker::cookClip( clip, &output ) == SIR_ANIM_COOK_OK );
}

static uint32_t sirTestChecksumValue( const uint8_t *p, uint64_t nLength ) noexcept
{
	uint32_t nCrc = 0xffffffffU;
	for ( uint64_t i = 0; i < nLength; ++i ) {
		nCrc ^= p[i];
		for ( uint32_t bit = 0; bit < 8; ++bit ) {
			nCrc = ( nCrc >> 1 ) ^ ( ( nCrc & 1U ) ? 0xedb88320U : 0 );
		}
	}
	return ~nCrc;
}

static void sirTestMutatedArchives( void )
{
	sirAnimationCookBone_t rigBones[3] = {};
	for ( uint32_t i = 0; i < 3; ++i ) {
		rigBones[i].iParent = ( int32_t )i - 1;
		rigBones[i].nStableId = i + 3;
		rigBones[i].localBind = sirTestTransform( ( float )i );
		rigBones[i].inverseBind = sirTestTransform( -( float )i );
		rigBones[i].fConservativeRadius = 0.5f;
	}
	const sirAnimationCookLodMapping_t mappings[2] = { { 0, 0 }, { 1, 2 } };
	const uint32_t influences[2] = { 0, 2 };
	const SIRResourceKey dependencies[2] = { { 11, 7, 1 }, { 12, 7, 1 } };
	sirAnimationCookSkeletonInput_t skeleton = { rigBones, 3, mappings, 2, influences, 2, dependencies, 2, 9 };
	sirAnimationCookKey_t keys[2] = { sirTestKey( 0, 0 ), sirTestKey( 100, 4 ) };
	sirAnimationCookTrack_t tracks[2] = { { 1, keys, 2, 0, 1, {} }, { 2, keys, 2, 1, 2, {} } };
	const sirAnimationCookEvent_t events[2] = { { 0, 5, 7 }, { 100, 6, 8 } };
	const sirAnimationCookCurve_t curves[1] = { { 77, 0 } };
	const sirAnimationCookMorph_t morphs[1] = { { 79, 1 } };
	sirAnimationCookClipInput_t clip = { tracks, 2, events, 2, curves, 1, morphs, 1, dependencies, 2, 100, 0.001f, 0.5f, 1, 9 };
	uint8_t good[2][4096] = {};
	uint8_t mutated[4096] = {};
	sirAnimationCookOutput_t rigOutput = { good[0], sizeof( good[0] ), 0 };
	sirAnimationCookOutput_t clipOutput = { good[1], sizeof( good[1] ), 0 };
	assert( SIRAnimationCooker::cookSkeleton( skeleton, &rigOutput ) == SIR_ANIM_COOK_OK );
	assert( SIRAnimationCooker::cookClip( clip, &clipOutput ) == SIR_ANIM_COOK_OK );
	const uint64_t length[2] = { rigOutput.nWritten, clipOutput.nWritten };
	const uint8_t mutations[5] = { 0, 1, 127, 128, 255 };
	for ( uint32_t kind = 0; kind < 2; ++kind ) {
		for ( uint64_t pos = 0; pos < length[kind]; ++pos ) {
			for ( uint32_t mode = 0; mode < 5; ++mode ) {
				memcpy( mutated, good[kind], length[kind] );
				mutated[pos] = mutations[mode];
				if ( kind == 1 ) {
					for ( uint32_t i = 0; i < 2; ++i ) {
						const uint8_t *pDescriptor = mutated + 144 + i * 128;
						const uint64_t nOffset = sirTestRead32( pDescriptor + 16 );
						const uint64_t nBytes = sirTestRead32( pDescriptor + 24 );
						if ( nOffset <= length[kind] && nBytes <= length[kind] - nOffset ) {
							sirTestWrite32( mutated + 144 + i * 128 + 112, sirTestChecksumValue( mutated + nOffset, nBytes ) );
						}
					}
				}
				sirTestRepairChecksum( mutated, length[kind] );
				sirAnimationCookAssetInfo_t info = {};
				( void )SIRAnimationCooked::inspect( { mutated, length[kind] }, &info );
			}
		}
	}
}

static void sirTestInputValidationMatrix( void )
{
	sirAnimationCookBone_t bones[2] = {};
	for ( uint32_t i = 0; i < 2; ++i ) {
		bones[i].iParent = ( int32_t )i - 1;
		bones[i].nStableId = 100 + i;
		bones[i].localBind = sirTestTransform( ( float )i );
		bones[i].inverseBind = sirTestTransform( -( float )i );
		bones[i].fConservativeRadius = 0.2f;
	}
	sirAnimationCookLodMapping_t lods[2] = { { 0, 0 }, { 1, 1 } };
	uint32_t influences[2] = { 0, 1 };
	SIRResourceKey deps[2] = { { 100, 10, 0 }, { 200, 10, 0 } };
	sirAnimationCookSkeletonInput_t rig = { bones, 2, lods, 2, influences, 2, deps, 2, 1 };
	uint8_t bytes[4096] = {};
	sirAnimationCookOutput_t output = { bytes, sizeof( bytes ), 0 };
	assert( SIRAnimationCooker::cookSkeleton( rig, &output ) == SIR_ANIM_COOK_OK );
	bones[1].localBind.fScale[0] = 0;
	assert( SIRAnimationCooker::cookSkeleton( rig, &output ) == SIR_ANIM_COOK_INVALID );
	bones[1].localBind.fScale[0] = 1;
	bones[1].inverseBind.fRotation[0] = NAN;
	assert( SIRAnimationCooker::cookSkeleton( rig, &output ) == SIR_ANIM_COOK_INVALID );
	bones[1].inverseBind.fRotation[0] = 0;
	bones[1].fConservativeRadius = -1;
	assert( SIRAnimationCooker::cookSkeleton( rig, &output ) == SIR_ANIM_COOK_INVALID );
	bones[1].fConservativeRadius = 0.2f;
	bones[1].localBind.fTranslation[0] = 3.4e38f;
	bones[1].fConservativeRadius = 3.4e38f;
	assert( SIRAnimationCooker::cookSkeleton( rig, &output ) == SIR_ANIM_COOK_INVALID );
	bones[1].localBind.fTranslation[0] = 1;
	bones[1].fConservativeRadius = 0.2f;
	lods[1].nLodLevel = 0;
	lods[1].nSourceBoneIndex = 0;
	assert( SIRAnimationCooker::cookSkeleton( rig, &output ) == SIR_ANIM_COOK_INVALID );
	lods[1].nLodLevel = 1;
	lods[1].nSourceBoneIndex = 1;
	lods[0].nLodLevel = 2;
	assert( SIRAnimationCooker::cookSkeleton( rig, &output ) == SIR_ANIM_COOK_INVALID );
	lods[0].nLodLevel = 0;
	lods[1].nLodLevel = 256;
	assert( SIRAnimationCooker::cookSkeleton( rig, &output ) == SIR_ANIM_COOK_INVALID );
	lods[1].nLodLevel = 1;
	influences[1] = 2;
	assert( SIRAnimationCooker::cookSkeleton( rig, &output ) == SIR_ANIM_COOK_INVALID );
	influences[1] = 1;
	deps[1].m_nAssetId = 100;
	assert( SIRAnimationCooker::cookSkeleton( rig, &output ) == SIR_ANIM_COOK_INVALID );
	deps[1].m_nAssetId = 200;
	deps[0].m_nAssetId = 0;
	assert( SIRAnimationCooker::cookSkeleton( rig, &output ) == SIR_ANIM_COOK_INVALID );
	deps[0].m_nAssetId = 100;
	deps[0].m_nTypeId = 0;
	assert( SIRAnimationCooker::cookSkeleton( rig, &output ) == SIR_ANIM_COOK_INVALID );
	deps[0].m_nTypeId = 10;
	rig.nBoneCount = SIR_ANIM_COOK_MAX_BONES + 1;
	assert( SIRAnimationCooker::cookSkeleton( rig, &output ) == SIR_ANIM_COOK_INVALID );
	rig.nBoneCount = 2;
	rig.pLodMappings = nullptr;
	assert( SIRAnimationCooker::cookSkeleton( rig, &output ) == SIR_ANIM_COOK_INVALID );
	rig.pLodMappings = lods;
	rig.pInfluenceBoneIndices = nullptr;
	assert( SIRAnimationCooker::cookSkeleton( rig, &output ) == SIR_ANIM_COOK_INVALID );
	rig.pInfluenceBoneIndices = influences;
	assert( SIRAnimationCooker::cookSkeleton( rig, &output ) == SIR_ANIM_COOK_OK );

	sirAnimationCookKey_t keys[2] = { sirTestKey( 0, 0 ), sirTestKey( 10, 1 ) };
	sirAnimationCookTrack_t tracks[2] = { { 1, keys, 2, 0, 0, {} }, { 1, keys, 2, 1, 0, {} } };
	sirAnimationCookEvent_t events[2] = { { 0, 1, 0 }, { 5, 2, 0 } };
	sirAnimationCookCurve_t curve = { 1, 0 };
	sirAnimationCookMorph_t morph = { 2, 1 };
	sirAnimationCookClipInput_t clip = { tracks, 2, events, 2, &curve, 1, &morph, 1, deps, 2, 10, 0.001f, 0.1f, 1, 1 };
	assert( SIRAnimationCooker::cookClip( clip, &output ) == SIR_ANIM_COOK_OK );
	tracks[1].bCoarse = 0;
	assert( SIRAnimationCooker::cookClip( clip, &output ) == SIR_ANIM_COOK_INVALID );
	tracks[1].bCoarse = 1;
	tracks[1].nKeyCount = 0;
	assert( SIRAnimationCooker::cookClip( clip, &output ) == SIR_ANIM_COOK_INVALID );
	tracks[1].nKeyCount = 2;
	tracks[1].nBoneIndex = SIR_ANIM_COOK_MAX_BONES;
	assert( SIRAnimationCooker::cookClip( clip, &output ) == SIR_ANIM_COOK_INVALID );
	tracks[1].nBoneIndex = 1;
	tracks[1].nExecutionMask = 5;
	assert( SIRAnimationCooker::cookClip( clip, &output ) == SIR_ANIM_COOK_INVALID );
	tracks[1].nExecutionMask = 0;
	tracks[1].pKeys = nullptr;
	assert( SIRAnimationCooker::cookClip( clip, &output ) == SIR_ANIM_COOK_INVALID );
	tracks[1].pKeys = keys;
	keys[1].nTime = 0;
	assert( SIRAnimationCooker::cookClip( clip, &output ) == SIR_ANIM_COOK_INVALID );
	keys[1].nTime = 10;
	keys[1].fComponents[0] = 3.4e38f;
	clip.fBoneRadius = 3.4e38f;
	assert( SIRAnimationCooker::cookClip( clip, &output ) == SIR_ANIM_COOK_INVALID );
	clip.fBoneRadius = 0.1f;
	keys[1].fComponents[0] = 1;
	morph.nTrackIndex = 9;
	assert( SIRAnimationCooker::cookClip( clip, &output ) == SIR_ANIM_COOK_INVALID );
	morph.nTrackIndex = 1;
	events[1].nTime = 11;
	assert( SIRAnimationCooker::cookClip( clip, &output ) == SIR_ANIM_COOK_INVALID );
	events[1].nTime = 5;
	clip.nTrackCount = SIR_ANIM_COOK_MAX_TRACKS + 1;
	assert( SIRAnimationCooker::cookClip( clip, &output ) == SIR_ANIM_COOK_INVALID );
	clip.nTrackCount = 2;
	clip.nMorphCount = SIR_ANIM_COOK_MAX_METADATA + 1;
	assert( SIRAnimationCooker::cookClip( clip, &output ) == SIR_ANIM_COOK_INVALID );
	clip.nMorphCount = 1;
	clip.nDuration = 0;
	assert( SIRAnimationCooker::cookClip( clip, &output ) == SIR_ANIM_COOK_INVALID );
	clip.nDuration = 10;
	assert( SIRAnimationCooker::cookClip( clip, &output ) == SIR_ANIM_COOK_OK );
}

int main( void )
{
	sirTestSkeleton();
	sirTestClip();
	sirTestReferenceDecompression();
	sirTestMissingFineFallback();
	sirTestMalformedArchiveFields();
	sirTestMalformedTrackFields();
	sirTestRawTrackAndInputFailures();
	sirTestMutatedArchives();
	sirTestInputValidationMatrix();
	puts( "ANM-01 animation cooker: tests passed" );
	return 0;
}
