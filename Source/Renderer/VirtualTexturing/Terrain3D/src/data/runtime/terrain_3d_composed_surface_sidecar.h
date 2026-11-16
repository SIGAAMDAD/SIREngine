// CS01 sidecar byte reader. Byte-copy avoids unaligned native object loads.
#ifndef TERRAIN3D_COMPOSED_SURFACE_SIDECAR_H
#define TERRAIN3D_COMPOSED_SURFACE_SIDECAR_H

#include "rendering/rd/vt/core/terrain_3d_composed_surface_contract.h"

inline bool t3d_comp_surf_validate_sidecar_bytes( const uint8_t *pFile, const size_t nFileBytes, const bool bVerifyChannelCrc )
{
	if ( !pFile || nFileBytes < sizeof( t3dComposedSurfaceFileHeader_t ) || !t3d_comp_surf_host_little_endian() ) {
		return false;
	}
	t3dComposedSurfaceFileHeader_t header{};
	memcpy( &header, pFile, sizeof( header ) );
	// Reuse the exact schema and bounds checks with a zero-page synthetic
	// header, then validate each record without materializing an unbounded
	// directory or casting an unaligned mapped-file pointer to a C++ struct.
	if ( header.m_nPageCount > T3D_COMP_SURF_MAX_SIDECAR_PAGES || header.m_nRecordOffset != sizeof( header ) ||
	     !t3d_comp_surf_range_within( header.m_nRecordOffset, uint64_t( header.m_nPageCount ) * sizeof( t3dComposedSurfacePageRecord_t ),
	                                  uint64_t( nFileBytes ) ) ||
	     header.m_nDataOffset < header.m_nRecordOffset + uint64_t( header.m_nPageCount ) * sizeof( t3dComposedSurfacePageRecord_t ) ) {
		return false;
	}
	t3dComposedSurfaceFileHeader_t empty = header;
	empty.m_nPageCount = 0u;
	// The zero-page validation verifies fixed ABI identity and file bounds.
	if ( !t3d_comp_surf_validate_sidecar( empty, nullptr, uint64_t( nFileBytes ) ) )
		return false;
	uint64_t cursor = header.m_nDataOffset;
	for ( uint32_t page = 0u; page < header.m_nPageCount; ++page ) {
		t3dComposedSurfacePageRecord_t r{};
		memcpy( &r, pFile + header.m_nRecordOffset + uint64_t( page ) * sizeof( r ), sizeof( r ) );
		if ( r.m_nRegionKey == 0u || r.m_nSourceFingerprint != header.m_nSourceFingerprint || r.m_nCompositionRevision == 0u ||
		     r.m_nGeneration == 0u || r.m_nFlags != 0u || r.m_nChannelMask != T3D_COMP_SURF_CHANNEL_MASK_ALL ||
		     r.m_nContentFingerprint == 0u || r.m_nMip >= 32u || r.m_nReserved[0] != 0u || r.m_nReserved[1] != 0u )
			return false;
		for ( uint32_t channel = 0u; channel < T3D_COMP_SURF_CHANNEL_COUNT; ++channel ) {
			const t3dComposedSurfaceChannelRecord_t& blob = r.m_channels[channel];
			if ( blob.m_nByteCount != t3d_comp_surf_channel_bytes( channel ) || ( blob.m_nFileOffset & 7u ) != 0u ||
			     blob.m_nFileOffset < cursor ||
			     !t3d_comp_surf_range_within( blob.m_nFileOffset, blob.m_nByteCount, uint64_t( nFileBytes ) ) )
				return false;
			if ( bVerifyChannelCrc && !t3d_comp_surf_validate_channel_payload( blob, pFile + blob.m_nFileOffset, blob.m_nByteCount ) )
				return false;
			cursor = blob.m_nFileOffset + blob.m_nByteCount;
		}
	}
	return true;
}

#endif // TERRAIN3D_COMPOSED_SURFACE_SIDECAR_H
