#include "authoring/vt/build/terrain_3d_composed_surface_cooker.h"
#include "data/runtime/terrain_3d_composed_surface_sidecar.h"

static bool t3d_comp_surf_cook_less( const t3dComposedSurfaceCookPage_t& a, const t3dComposedSurfaceCookPage_t& b )
{
	if ( a.m_nRegionKey != b.m_nRegionKey )
		return a.m_nRegionKey < b.m_nRegionKey;
	if ( a.m_nMip != b.m_nMip )
		return a.m_nMip < b.m_nMip;
	if ( a.m_nPageY != b.m_nPageY )
		return a.m_nPageY < b.m_nPageY;
	return a.m_nPageX < b.m_nPageX;
}

bool t3d_comp_surf_cook_total_bytes( const uint32_t nPages, uint64_t& rBytes )
{
	rBytes = 0u;
	if ( nPages == 0u || nPages > T3D_COMP_SURF_MAX_SIDECAR_PAGES )
		return false;
	const uint64_t records_end = sizeof( t3dComposedSurfaceFileHeader_t ) + uint64_t( nPages ) * sizeof( t3dComposedSurfacePageRecord_t );
	const uint64_t data_start = ( records_end + 7u ) & ~uint64_t( 7u );
	rBytes = data_start + uint64_t( nPages ) * T3D_COMP_SURF_PAGE_PAYLOAD_BYTES;
	return true;
}

// Per-page incremental reuse only applies to a previously complete, validated
// sidecar for the identical material-source fingerprint and composition ABI.
static bool t3d_comp_surf_cook_internal( const t3dComposedSurfaceCookPage_t *pPages, const uint32_t nPages,
                                         const t3dComposedSurfaceCookSource_t& source, const uint8_t *pPreviousFile,
                                         const uint64_t nPreviousBytes, const uint8_t *pDirtyFlags, uint8_t *pScratch,
                                         const uint32_t nScratchBytes, uint8_t *pOutput, const uint64_t nOutputCapacity,
                                         uint64_t& rWrittenBytes )
{
	rWrittenBytes = 0u;
	uint64_t file_bytes = 0u;
	if ( !t3d_comp_surf_host_little_endian() || !t3d_comp_surf_cook_total_bytes( nPages, file_bytes ) || !pPages || !pScratch ||
	     nScratchBytes < T3D_COMP_SURF_COOK_SCRATCH_BYTES || !pOutput || file_bytes > SIZE_MAX || file_bytes > nOutputCapacity ||
	     !( source.m_bCanonicalMaterialParityAccepted || source.m_bReferenceOnly ) || source.m_nSourceFingerprint == 0u ||
	     !source.m_pEvaluate || !source.m_pEncode ||
	     ( pPreviousFile && ( pPreviousFile == pOutput || nPreviousBytes > SIZE_MAX ||
	                          !t3d_comp_surf_validate_sidecar_bytes( pPreviousFile, size_t( nPreviousBytes ), true ) ) ) )
		return false;
	t3dComposedSurfaceFileHeader_t previous_header{};
	bool reuse_possible = pPreviousFile != nullptr;
	if ( reuse_possible ) {
		memcpy( &previous_header, pPreviousFile, sizeof( previous_header ) );
		reuse_possible = previous_header.m_nSourceFingerprint == source.m_nSourceFingerprint;
	}
	for ( uint32_t i = 0u; i < nPages; ++i ) {
		const t3dComposedSurfaceCookPage_t& p = pPages[i];
		if ( p.m_nRegionKey == 0u || p.m_nGeneration == 0u || p.m_nCompositionRevision == 0u || p.m_nMip >= 32u ||
		     ( i && !t3d_comp_surf_cook_less( pPages[i - 1u], p ) ) )
			return false;
	}
	// The output is committed only after all callback results, channel lengths
	// and CRC validation have succeeded. On failure the output is untrusted.
	memset( pOutput, 0, size_t( file_bytes ) );
	t3dComposedSurfaceFileHeader_t header{};
	header.m_nMagic = T3D_COMP_SURF_FILE_MAGIC;
	header.m_nVersion = T3D_COMP_SURF_FILE_VERSION;
	header.m_nHeaderBytes = sizeof( header );
	header.m_nRecordBytes = sizeof( t3dComposedSurfacePageRecord_t );
	header.m_nCompositionAbi = T3D_COMP_SURF_COMPOSITION_ABI;
	header.m_nChannelMask = T3D_COMP_SURF_CHANNEL_MASK_ALL;
	header.m_nPageInterior = T3D_COMP_SURF_PAGE_INTERIOR;
	header.m_nPageGutter = T3D_COMP_SURF_PAGE_GUTTER;
	header.m_nPhysicalTile = T3D_COMP_SURF_PHYSICAL_TILE;
	header.m_nPageCount = nPages;
	header.m_nRecordOffset = sizeof( header );
	header.m_nDataOffset =
	    ( header.m_nRecordOffset + uint64_t( nPages ) * sizeof( t3dComposedSurfacePageRecord_t ) + 7u ) & ~uint64_t( 7u );
	header.m_nFileBytes = file_bytes;
	header.m_nSourceFingerprint = source.m_nSourceFingerprint;
	uint64_t cursor = header.m_nDataOffset;
	uint32_t previous_cursor = 0u;
	for ( uint32_t i = 0u; i < nPages; ++i ) {
		const t3dComposedSurfaceCookPage_t& p = pPages[i];
		t3dComposedSurfacePageRecord_t r{};
		r.m_nRegionKey = p.m_nRegionKey;
		r.m_nCompositionRevision = p.m_nCompositionRevision;
		r.m_nSourceFingerprint = source.m_nSourceFingerprint;
		r.m_nMip = p.m_nMip;
		r.m_nPageX = p.m_nPageX;
		r.m_nPageY = p.m_nPageY;
		r.m_nGeneration = p.m_nGeneration;
		r.m_nChannelMask = T3D_COMP_SURF_CHANNEL_MASK_ALL;
		bool copy_previous = reuse_possible && ( !pDirtyFlags || !pDirtyFlags[i] );
		t3dComposedSurfacePageRecord_t previous{};
		if ( copy_previous ) {
			// Merge two ordered page streams without allocating a directory.
			while ( previous_cursor < previous_header.m_nPageCount ) {
				memcpy( &previous, pPreviousFile + previous_header.m_nRecordOffset + uint64_t( previous_cursor ) * sizeof( previous ),
				        sizeof( previous ) );
				const t3dComposedSurfaceCookPage_t old = { previous.m_nRegionKey, previous.m_nCompositionRevision,
				                                           previous.m_nMip,       previous.m_nPageX,
				                                           previous.m_nPageY,     previous.m_nGeneration };
				if ( !t3d_comp_surf_cook_less( old, p ) )
					break;
				++previous_cursor;
			}
			copy_previous = previous_cursor < previous_header.m_nPageCount && previous.m_nRegionKey == p.m_nRegionKey &&
			                previous.m_nMip == p.m_nMip && previous.m_nPageX == p.m_nPageX && previous.m_nPageY == p.m_nPageY &&
			                previous.m_nGeneration == p.m_nGeneration && previous.m_nCompositionRevision == p.m_nCompositionRevision;
		}
		// A page-local single-pass evaluator avoids recomputing the same
		// material three times to populate ALBEDO, NORMAL and SURFACE.
		// Only use it with caller-provided storage for all three channels;
		// previously published files and legacy evaluators remain compatible.
		const bool fused = !copy_previous && source.m_pEvaluateAll && nScratchBytes >= T3D_COMP_SURF_COOK_FUSED_SCRATCH_BYTES;
		if ( fused && !source.m_pEvaluateAll( source.m_pUser, p, pScratch, T3D_COMP_SURF_RGBA_SCRATCH_BYTES ) )
			return false;
		uint64_t fingerprint = 14695981039346656037ull;
		for ( uint32_t channel = 0u; channel < T3D_COMP_SURF_CHANNEL_COUNT; ++channel ) {
			const uint32_t bytes = t3d_comp_surf_channel_bytes( channel );
			uint32_t written = 0u;
			if ( copy_previous ) {
				const auto& old = previous.m_channels[channel];
				if ( old.m_nByteCount != bytes )
					return false;
				memcpy( pOutput + cursor, pPreviousFile + old.m_nFileOffset, bytes );
				written = bytes;
			} else {
				if ( !fused && !source.m_pEvaluate( source.m_pUser, p, channel, pScratch, T3D_COMP_SURF_RGBA_SCRATCH_BYTES ) )
					return false;
				const uint8_t *pChannel = fused ? pScratch + channel * T3D_COMP_SURF_RGBA_SCRATCH_BYTES : pScratch;
				if ( !source.m_pEncode( source.m_pUser, channel, pChannel, T3D_COMP_SURF_RGBA_SCRATCH_BYTES, pOutput + cursor, bytes,
				                        written ) ||
				     written != bytes )
					return false;
			}
			r.m_channels[channel].m_nFileOffset = cursor;
			r.m_channels[channel].m_nByteCount = written;
			r.m_channels[channel].m_nCrc32 = t3d_comp_surf_crc32( pOutput + cursor, written );
			for ( uint32_t j = 0u; j < written; ++j ) {
				fingerprint = ( fingerprint ^ pOutput[cursor + j] ) * 1099511628211ull;
			}
			cursor += written;
		}
		r.m_nContentFingerprint = fingerprint ? fingerprint : 1u;
		memcpy( pOutput + header.m_nRecordOffset + uint64_t( i ) * sizeof( r ), &r, sizeof( r ) );
	}
	memcpy( pOutput, &header, sizeof( header ) );
	if ( cursor != file_bytes || !t3d_comp_surf_validate_sidecar_bytes( pOutput, size_t( file_bytes ), true ) )
		return false;
	rWrittenBytes = file_bytes;
	return true;
}

bool t3d_comp_surf_cook_pages_cold( const t3dComposedSurfaceCookPage_t *pPages, const uint32_t nPages,
                                    const t3dComposedSurfaceCookSource_t& source, uint8_t *pScratch, const uint32_t nScratchBytes,
                                    uint8_t *pOutput, const uint64_t nOutputCapacity, uint64_t& rWrittenBytes )
{
	return t3d_comp_surf_cook_internal( pPages, nPages, source, nullptr, 0u, nullptr, pScratch, nScratchBytes, pOutput, nOutputCapacity,
	                                    rWrittenBytes );
}

bool t3d_comp_surf_recook_incremental_cold( const t3dComposedSurfaceCookPage_t *pPages, const uint32_t nPages,
                                            const t3dComposedSurfaceCookSource_t& source, const uint8_t *pPreviousFile,
                                            const uint64_t nPreviousBytes, const uint8_t *pDirtyFlags, uint8_t *pScratch,
                                            const uint32_t nScratchBytes, uint8_t *pOutput, const uint64_t nOutputCapacity,
                                            uint64_t& rWrittenBytes )
{
	return t3d_comp_surf_cook_internal( pPages, nPages, source, pPreviousFile, nPreviousBytes, pDirtyFlags, pScratch, nScratchBytes,
	                                    pOutput, nOutputCapacity, rWrittenBytes );
}
