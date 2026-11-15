// Copyright (c) 2023-2026 Cory Petkovsek, Roope Palmroos, and Contributors.
// Native SIREngine Platform file boundary for the original Terrain3D composed
// sidecar publication algorithm. Sidecar bytes and validation are unchanged.

#include "authoring/vt/io/terrain_3d_composed_surface_sidecar_writer.h"
#include "data/runtime/terrain_3d_composed_surface_sidecar.h"

#include <Core/Platform/SIRPlatformFile.h>

#include <string>
#include <string.h>

static bool t3d_comp_surf_identical_published_sidecar( const char *pszDestination, const uint8_t *pBytes, uint64_t nBytes )
{
	SIRPlatformFile existing;
	if ( existing.open( pszDestination, SIR_FILE_OPEN_READ ) != SIR_FILE_OK ) {
		return false;
	}
	uint64_t fileBytes = 0u;
	if ( existing.getSize( &fileBytes ) != SIR_FILE_OK || fileBytes != nBytes ) {
		return false;
	}
	uint8_t scratch[16384];
	uint64_t offset = 0u;
	while ( offset < nBytes ) {
		const uint64_t remaining = nBytes - offset;
		const uint64_t blockBytes = remaining < sizeof( scratch ) ? remaining : sizeof( scratch );
		uint64_t transferred = 0u;
		if ( existing.readAt( offset, scratch, blockBytes, &transferred ) != SIR_FILE_OK || transferred != blockBytes ||
		     memcmp( scratch, pBytes + offset, ( size_t )blockBytes ) != 0 ) {
			return false;
		}
		offset += blockBytes;
	}
	return true;
}

bool t3d_comp_surf_write_sidecar_cold( const char *pszDestination, const uint8_t *pBytes, uint64_t nBytes )
{
	if ( pszDestination == nullptr || pszDestination[0] == '\0' || pBytes == nullptr || nBytes > SIZE_MAX ||
	     !t3d_comp_surf_validate_sidecar_bytes( pBytes, ( size_t )nBytes, true ) ) {
		return false;
	}
	// Byte-identical rewrites are a no-op; this preserves native publication
	// timestamps and avoids flushing an unchanged sidecar to disk.
	if ( t3d_comp_surf_identical_published_sidecar( pszDestination, pBytes, nBytes ) ) {
		return true;
	}
	const std::string temporary = std::string( pszDestination ) + ".t3sc.tmp";
	SIRPlatformFile staging;
	if ( staging.open( temporary.c_str(), SIR_FILE_OPEN_WRITE | SIR_FILE_OPEN_CREATE | SIR_FILE_OPEN_EXCLUSIVE ) != SIR_FILE_OK ) {
		return false;
	}
	uint64_t transferred = 0u;
	const bool written =
	    staging.writeAt( 0u, pBytes, nBytes, &transferred ) == SIR_FILE_OK && transferred == nBytes && staging.flush() == SIR_FILE_OK;
	const bool closed = staging.close() == SIR_FILE_OK;
	if ( !written || !closed || SIRPlatformFile::renameReplace( temporary.c_str(), pszDestination ) != SIR_FILE_OK ) {
		SIRPlatformFile::remove( temporary.c_str() );
		return false;
	}
	return true;
}
