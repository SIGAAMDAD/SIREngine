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

#include <limits.h>
#include <string.h>

sirTerrainPersistenceResult_t SIRTerrainRegionPersistence::saveAtomic( const char *pszDestination, const char *pszStaging,
                                                                       sirTerrainConstByteSpan_t packed ) noexcept
{
	if ( !pszDestination || !pszStaging || !pszDestination[0] || !pszStaging[0] || strcmp( pszDestination, pszStaging ) == 0 ||
	     !packed.pData || packed.nCapacity == 0 || packed.nCapacity > ( uint64_t )INT64_MAX ) {
		return SIR_TERRAIN_PERSISTENCE_INVALID_ARGUMENT;
	}

	sirTerrainRegionBundleView_t checked = {};
	if ( SIRTerrainRegionBundle::open( packed, checked ) != SIR_TERRAIN_BUNDLE_SUCCESS ) {
		return SIR_TERRAIN_PERSISTENCE_INVALID_BUNDLE;
	}

	SIRPlatformFile file;
	const sirFileResult_t eOpen = file.open( pszStaging, SIR_FILE_OPEN_WRITE | SIR_FILE_OPEN_CREATE | SIR_FILE_OPEN_EXCLUSIVE );
	if ( eOpen == SIR_FILE_ALREADY_EXISTS ) {
		return SIR_TERRAIN_PERSISTENCE_STAGING_EXISTS;
	}
	if ( eOpen != SIR_FILE_OK ) {
		return SIR_TERRAIN_PERSISTENCE_IO_ERROR;
	}

	uint64_t nTransferred = 0;
	const sirFileResult_t eWrite = file.writeAt( 0, packed.pData, packed.nCapacity, &nTransferred );
	const sirFileResult_t eFlush = eWrite == SIR_FILE_OK && nTransferred == packed.nCapacity ? file.flush() : SIR_FILE_IO_ERROR;
	const sirFileResult_t eClose = file.close();
	if ( eWrite != SIR_FILE_OK || nTransferred != packed.nCapacity || eFlush != SIR_FILE_OK || eClose != SIR_FILE_OK ) {
		( void )SIRPlatformFile::remove( pszStaging );
		return SIR_TERRAIN_PERSISTENCE_IO_ERROR;
	}
	if ( SIRPlatformFile::renameReplace( pszStaging, pszDestination ) != SIR_FILE_OK ) {
		( void )SIRPlatformFile::remove( pszStaging );
		return SIR_TERRAIN_PERSISTENCE_IO_ERROR;
	}
	return SIR_TERRAIN_PERSISTENCE_SUCCESS;
}

sirTerrainPersistenceResult_t SIRTerrainRegionPersistence::load( const char *pszPath, sirTerrainMutableByteSpan_t storage,
                                                                 sirTerrainRegionBundleView_t& view, size_t& nLoaded ) noexcept
{
	if ( !pszPath || !pszPath[0] || !storage.pData || !storage.nCapacity ) {
		return SIR_TERRAIN_PERSISTENCE_INVALID_ARGUMENT;
	}
	SIRPlatformFile file;
	const sirFileResult_t eOpen = file.open( pszPath, SIR_FILE_OPEN_READ );
	if ( eOpen == SIR_FILE_NOT_FOUND ) {
		return SIR_TERRAIN_PERSISTENCE_NOT_FOUND;
	}
	if ( eOpen != SIR_FILE_OK ) {
		return SIR_TERRAIN_PERSISTENCE_IO_ERROR;
	}

	uint64_t nFileBytes = 0;
	if ( file.getSize( &nFileBytes ) != SIR_FILE_OK ) {
		return SIR_TERRAIN_PERSISTENCE_IO_ERROR;
	}
	if ( nFileBytes > storage.nCapacity ) {
		return SIR_TERRAIN_PERSISTENCE_BUFFER_TOO_SMALL;
	}
	if ( nFileBytes == 0 || nFileBytes > ( uint64_t )INT64_MAX ) {
		return SIR_TERRAIN_PERSISTENCE_INVALID_BUNDLE;
	}

	uint64_t nTransferred = 0;
	const sirFileResult_t eRead = file.readAt( 0, storage.pData, nFileBytes, &nTransferred );
	if ( eRead != SIR_FILE_OK || nTransferred != nFileBytes ) {
		return SIR_TERRAIN_PERSISTENCE_IO_ERROR;
	}
	if ( file.close() != SIR_FILE_OK ) {
		return SIR_TERRAIN_PERSISTENCE_IO_ERROR;
	}

	sirTerrainRegionBundleView_t candidate = {};
	if ( SIRTerrainRegionBundle::open( { storage.pData, ( size_t )nFileBytes }, candidate ) != SIR_TERRAIN_BUNDLE_SUCCESS ) {
		return SIR_TERRAIN_PERSISTENCE_INVALID_BUNDLE;
	}
	view = candidate;
	nLoaded = ( size_t )nFileBytes;
	return SIR_TERRAIN_PERSISTENCE_SUCCESS;
}
