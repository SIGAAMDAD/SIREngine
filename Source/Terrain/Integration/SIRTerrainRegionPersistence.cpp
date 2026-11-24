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

#include <Core/Platform/Shared/File/SIRPlatformFileSystem.h>
#include <Terrain/Integration/SIRTerrainRegionPersistence.h>

#include <limits.h>
#include <string.h>

SIRTerrainPersistenceResult SIRTerrainRegionPersistence::saveAtomic( const char *pszDestination, const char *pszStaging,
                                                                       sirTerrainConstByteSpan_t packed ) noexcept
{
	if ( !pszDestination || !pszStaging || !pszDestination[0] || !pszStaging[0] || strcmp( pszDestination, pszStaging ) == 0 ||
	     !packed.pData || packed.nCapacity == 0 || packed.nCapacity > static_cast<uint64_t>(INT64_MAX) ) {
		return SIRTerrainPersistenceResult::SIR_TERRAIN_PERSISTENCE_INVALID_ARGUMENT;
	}

	sirTerrainRegionBundleView_t checked {};
	if ( SIRTerrainRegionBundle::open( packed, checked ) != SIRTerrainBundleResult::SIR_TERRAIN_BUNDLE_SUCCESS ) {
		return SIRTerrainPersistenceResult::SIR_TERRAIN_PERSISTENCE_INVALID_BUNDLE;
	}

	SIRPlatformFile file;
	const sirFileResult eOpen = file.open( pszStaging, sirFileOpenFlag::SIR_FILE_OPEN_WRITE | sirFileOpenFlag::SIR_FILE_OPEN_CREATE | sirFileOpenFlag::SIR_FILE_OPEN_EXCLUSIVE );
	if ( eOpen == sirFileResult::SIR_FILE_ALREADY_EXISTS ) {
		return SIRTerrainPersistenceResult::SIR_TERRAIN_PERSISTENCE_STAGING_EXISTS;
	}
	if ( eOpen != sirFileResult::SIR_FILE_OK ) {
		return SIRTerrainPersistenceResult::SIR_TERRAIN_PERSISTENCE_IO_ERROR;
	}

	uint64_t nTransferred = 0;
	const sirFileResult eWrite = file.writeAt( 0, packed.pData, packed.nCapacity, &nTransferred );
	const sirFileResult eFlush = eWrite == sirFileResult::SIR_FILE_OK && nTransferred == packed.nCapacity ? file.flush() : sirFileResult::SIR_FILE_IO_ERROR;
	const sirFileResult eClose = file.close();
	if ( eWrite != sirFileResult::SIR_FILE_OK || nTransferred != packed.nCapacity || eFlush != sirFileResult::SIR_FILE_OK || eClose != sirFileResult::SIR_FILE_OK ) {
		static_cast<void>(SIRPlatformFileSystem::removeFile( pszStaging ));
		return SIRTerrainPersistenceResult::SIR_TERRAIN_PERSISTENCE_IO_ERROR;
	}
	if ( SIRPlatformFileSystem::renameFileReplace( pszStaging, pszDestination ) != sirFileResult::SIR_FILE_OK ) {
		static_cast<void>(SIRPlatformFileSystem::removeFile( pszStaging ));
		return SIRTerrainPersistenceResult::SIR_TERRAIN_PERSISTENCE_IO_ERROR;
	}
	return SIRTerrainPersistenceResult::SIR_TERRAIN_PERSISTENCE_SUCCESS;
}

SIRTerrainPersistenceResult SIRTerrainRegionPersistence::load( const char *pszPath, sirTerrainMutableByteSpan_t storage,
                                                                 sirTerrainRegionBundleView_t& view, size_t& nLoaded ) noexcept
{
	if ( !pszPath || !pszPath[0] || !storage.pData || !storage.nCapacity ) {
		return SIRTerrainPersistenceResult::SIR_TERRAIN_PERSISTENCE_INVALID_ARGUMENT;
	}
	SIRPlatformFile file;
	const sirFileResult eOpen = file.open( pszPath, sirFileOpenFlag::SIR_FILE_OPEN_READ );
	if ( eOpen == sirFileResult::SIR_FILE_NOT_FOUND ) {
		return SIRTerrainPersistenceResult::SIR_TERRAIN_PERSISTENCE_NOT_FOUND;
	}
	if ( eOpen != sirFileResult::SIR_FILE_OK ) {
		return SIRTerrainPersistenceResult::SIR_TERRAIN_PERSISTENCE_IO_ERROR;
	}

	uint64_t nFileBytes = 0;
	if ( file.getSize( &nFileBytes ) != sirFileResult::SIR_FILE_OK ) {
		return SIRTerrainPersistenceResult::SIR_TERRAIN_PERSISTENCE_IO_ERROR;
	}
	if ( nFileBytes > storage.nCapacity ) {
		return SIRTerrainPersistenceResult::SIR_TERRAIN_PERSISTENCE_BUFFER_TOO_SMALL;
	}
	if ( nFileBytes == 0 || nFileBytes > static_cast<uint64_t>(INT64_MAX) ) {
		return SIRTerrainPersistenceResult::SIR_TERRAIN_PERSISTENCE_INVALID_BUNDLE;
	}

	uint64_t nTransferred = 0;
	const sirFileResult eRead = file.readAt( 0, storage.pData, nFileBytes, &nTransferred );
	if ( eRead != sirFileResult::SIR_FILE_OK || nTransferred != nFileBytes ) {
		return SIRTerrainPersistenceResult::SIR_TERRAIN_PERSISTENCE_IO_ERROR;
	}
	if ( file.close() != sirFileResult::SIR_FILE_OK ) {
		return SIRTerrainPersistenceResult::SIR_TERRAIN_PERSISTENCE_IO_ERROR;
	}

	sirTerrainRegionBundleView_t candidate {};
	if ( SIRTerrainRegionBundle::open( { storage.pData, static_cast<size_t>(nFileBytes) }, candidate ) != SIRTerrainBundleResult::SIR_TERRAIN_BUNDLE_SUCCESS ) {
		return SIRTerrainPersistenceResult::SIR_TERRAIN_PERSISTENCE_INVALID_BUNDLE;
	}
	view = candidate;
	nLoaded = static_cast<size_t>(nFileBytes);
	return SIRTerrainPersistenceResult::SIR_TERRAIN_PERSISTENCE_SUCCESS;
}
