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

#include <Terrain/Integration/SIRTerrainRegionPersistence.h>

#include <stdio.h>
#include <string.h>
#include <vector>

#define CHECK( expression )                                                                                                                \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #expression );                                                     \
			return 1;                                                                                                                      \
		}                                                                                                                                  \
	} while ( false )

int main( void )
{
	static const char *pszTarget = "sir-terrain-persistence-roundtrip.bundle";
	static const char *pszStage = "sir-terrain-persistence-roundtrip.stage";
	( void )SIRPlatformFile::remove( pszTarget );
	( void )SIRPlatformFile::remove( pszStage );

	const float szHeights[4] = { -10.0f, 0.0f, 2.0f, 9.0f };
	sirTerrainRegionDescriptor_t descriptor = { -3, 7, 2, -10.0f, 9.0f, 1 };
	std::vector<uint8_t> encoded( SIRTerrainHeightfieldCodec::getEncodedByteCount( descriptor.nResolution ) );
	CHECK( SIRTerrainHeightfieldCodec::encode( descriptor, { szHeights, 4 }, { encoded.data(), encoded.size() } ) ==
	       SIR_TERRAIN_CODEC_SUCCESS );
	const sirTerrainConstByteSpan_t region = { encoded.data(), encoded.size() };
	size_t nBytes = 0;
	CHECK( SIRTerrainRegionBundle::measure( &region, 1, nBytes ) == SIR_TERRAIN_BUNDLE_SUCCESS );
	std::vector<uint8_t> bundle( nBytes );
	size_t nWritten = 0;
	CHECK( SIRTerrainRegionBundle::write( &region, 1, { bundle.data(), bundle.size() }, nWritten ) == SIR_TERRAIN_BUNDLE_SUCCESS );
	CHECK( nWritten == nBytes );
	const sirTerrainConstByteSpan_t packed = { bundle.data(), bundle.size() };
	std::vector<uint8_t> scratch( bundle.size() );
	sirTerrainRegionBundleView_t view = {};
	size_t nLoaded = 999;

	CHECK( SIRTerrainRegionPersistence::load( pszTarget, { scratch.data(), scratch.size() }, view, nLoaded ) ==
	       SIR_TERRAIN_PERSISTENCE_NOT_FOUND );
	CHECK( nLoaded == 999 && !view.pBytes );
	CHECK( SIRTerrainRegionPersistence::saveAtomic( nullptr, pszStage, packed ) == SIR_TERRAIN_PERSISTENCE_INVALID_ARGUMENT );
	CHECK( SIRTerrainRegionPersistence::saveAtomic( pszTarget, pszTarget, packed ) == SIR_TERRAIN_PERSISTENCE_INVALID_ARGUMENT );
	CHECK( SIRTerrainRegionPersistence::saveAtomic( pszTarget, pszStage, { bundle.data(), bundle.size() - 1 } ) ==
	       SIR_TERRAIN_PERSISTENCE_INVALID_BUNDLE );
	CHECK( SIRTerrainRegionPersistence::saveAtomic( pszTarget, pszStage, packed ) == SIR_TERRAIN_PERSISTENCE_SUCCESS );

	std::vector<uint8_t> shortScratch( bundle.size() - 1, 0xA5 );
	CHECK( SIRTerrainRegionPersistence::load( pszTarget, { shortScratch.data(), shortScratch.size() }, view, nLoaded ) ==
	       SIR_TERRAIN_PERSISTENCE_BUFFER_TOO_SMALL );
	CHECK( nLoaded == 999 && !view.pBytes && shortScratch[0] == 0xA5 );
	CHECK( SIRTerrainRegionPersistence::load( pszTarget, { scratch.data(), scratch.size() }, view, nLoaded ) ==
	       SIR_TERRAIN_PERSISTENCE_SUCCESS );
	CHECK( nLoaded == nBytes && view.pBytes == scratch.data() && view.nRegionCount == 1 );
	CHECK( memcmp( scratch.data(), bundle.data(), bundle.size() ) == 0 );
	sirTerrainConstByteSpan_t found = {};
	CHECK( SIRTerrainRegionBundle::find( view, -3, 7, found ) == SIR_TERRAIN_BUNDLE_SUCCESS );
	CHECK( found.nCapacity == encoded.size() && memcmp( found.pData, encoded.data(), encoded.size() ) == 0 );

	SIRPlatformFile existingStage;
	CHECK( existingStage.open( pszStage, SIR_FILE_OPEN_WRITE | SIR_FILE_OPEN_CREATE | SIR_FILE_OPEN_EXCLUSIVE ) == SIR_FILE_OK );
	const uint8_t nMarker = 0xBD;
	uint64_t nTransferred = 0;
	CHECK( existingStage.writeAt( 0, &nMarker, 1, &nTransferred ) == SIR_FILE_OK && nTransferred == 1 );
	CHECK( existingStage.close() == SIR_FILE_OK );
	CHECK( SIRTerrainRegionPersistence::saveAtomic( pszTarget, pszStage, packed ) == SIR_TERRAIN_PERSISTENCE_STAGING_EXISTS );
	CHECK( SIRTerrainRegionPersistence::load( pszTarget, { scratch.data(), scratch.size() }, view, nLoaded ) ==
	       SIR_TERRAIN_PERSISTENCE_SUCCESS );
	SIRPlatformFile stageReader;
	CHECK( stageReader.open( pszStage, SIR_FILE_OPEN_READ ) == SIR_FILE_OK );
	uint8_t nStageByte = 0;
	CHECK( stageReader.readAt( 0, &nStageByte, 1, &nTransferred ) == SIR_FILE_OK && nStageByte == nMarker );
	CHECK( stageReader.close() == SIR_FILE_OK );
	CHECK( SIRPlatformFile::remove( pszStage ) == SIR_FILE_OK );

	std::vector<uint8_t> invalid = bundle;
	invalid[invalid.size() - 1] ^= 1;
	CHECK( SIRTerrainRegionPersistence::saveAtomic( pszTarget, pszStage, { invalid.data(), invalid.size() } ) ==
	       SIR_TERRAIN_PERSISTENCE_INVALID_BUNDLE );
	CHECK( SIRTerrainRegionPersistence::load( pszTarget, { scratch.data(), scratch.size() }, view, nLoaded ) ==
	       SIR_TERRAIN_PERSISTENCE_SUCCESS );
	CHECK( memcmp( scratch.data(), bundle.data(), bundle.size() ) == 0 );

	SIRPlatformFile corrupt;
	CHECK( corrupt.open( pszTarget, SIR_FILE_OPEN_WRITE ) == SIR_FILE_OK );
	const uint8_t nCorruption = ( uint8_t )( bundle.back() ^ 1 );
	CHECK( corrupt.writeAt( bundle.size() - 1, &nCorruption, 1, &nTransferred ) == SIR_FILE_OK && nTransferred == 1 );
	CHECK( corrupt.close() == SIR_FILE_OK );
	const sirTerrainRegionBundleView_t lastView = view;
	nLoaded = 999;
	CHECK( SIRTerrainRegionPersistence::load( pszTarget, { scratch.data(), scratch.size() }, view, nLoaded ) ==
	       SIR_TERRAIN_PERSISTENCE_INVALID_BUNDLE );
	CHECK( view.pBytes == lastView.pBytes && view.nRegionCount == 1 && nLoaded == 999 );

	const float szReplacementHeights[4] = { 9.0f, 4.0f, 0.0f, -10.0f };
	descriptor.nRevision = 2;
	CHECK( SIRTerrainHeightfieldCodec::encode( descriptor, { szReplacementHeights, 4 }, { encoded.data(), encoded.size() } ) ==
	       SIR_TERRAIN_CODEC_SUCCESS );
	CHECK( SIRTerrainRegionBundle::write( &region, 1, { bundle.data(), bundle.size() }, nWritten ) == SIR_TERRAIN_BUNDLE_SUCCESS );
	CHECK( SIRTerrainRegionPersistence::saveAtomic( pszTarget, pszStage, packed ) == SIR_TERRAIN_PERSISTENCE_SUCCESS );
	CHECK( SIRTerrainRegionPersistence::load( pszTarget, { scratch.data(), scratch.size() }, view, nLoaded ) ==
	       SIR_TERRAIN_PERSISTENCE_SUCCESS );
	CHECK( SIRTerrainRegionBundle::find( view, -3, 7, found ) == SIR_TERRAIN_BUNDLE_SUCCESS );
	sirTerrainRegionDescriptor_t loadedDescriptor = {};
	CHECK( SIRTerrainHeightfieldCodec::inspect( found, loadedDescriptor ) == SIR_TERRAIN_CODEC_SUCCESS );
	CHECK( loadedDescriptor.nRevision == 2 );
	CHECK( SIRPlatformFile::getInfo( pszStage, nullptr ) == SIR_FILE_INVALID_ARGUMENT );
	CHECK( SIRPlatformFile::remove( pszTarget ) == SIR_FILE_OK );
	CHECK( SIRPlatformFile::remove( pszStage ) == SIR_FILE_NOT_FOUND );
	return 0;
}
