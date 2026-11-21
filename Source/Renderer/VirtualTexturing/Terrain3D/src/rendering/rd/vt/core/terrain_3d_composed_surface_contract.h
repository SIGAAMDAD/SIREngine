// CS01: independent composed-result sidecar; the Sparse VT V1 contract is unchanged.
#ifndef TERRAIN3D_COMPOSED_SURFACE_CONTRACT_H
#define TERRAIN3D_COMPOSED_SURFACE_CONTRACT_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <type_traits>

#define T3D_COMP_SURF_FILE_MAGIC 0x43533354u // "T3SC" in little-endian bytes
#define T3D_COMP_SURF_FILE_VERSION 1u
#define T3D_COMP_SURF_COMPOSITION_ABI 1u
// Changes to input math/filter semantics invalidate cold source identities,
// without changing the existing T3SC or V1 shipping file layouts.
#define T3D_COMP_SURF_SOURCE_RECIPE_REVISION 3u
#define T3D_COMP_SURF_MAX_REGION_MIP_PAGES 21u // 4x4 + 2x2 + 1
// Hole/alpha coverage remains sourced from authoritative Terrain3DData
// control maps; the three-channel T3SC V1 file has no coverage channel.
#define T3D_COMP_SURF_COVERAGE_FROM_CONTROLS 1u
#define T3D_COMP_SURF_PAGE_INTERIOR 128u
#define T3D_COMP_SURF_PAGE_GUTTER 4u
#define T3D_COMP_SURF_PHYSICAL_TILE 136u
#define T3D_COMP_SURF_CHANNEL_COUNT 3u
#define T3D_COMP_SURF_CHANNEL_MASK_ALL 7u
#define T3D_COMP_SURF_CHANNEL_ALBEDO 0u  // BC1 UNORM; explicitly encoded sRGB samples
#define T3D_COMP_SURF_CHANNEL_NORMAL 1u  // BC5 UNORM; signed tangent XY mapped to [0,1]
#define T3D_COMP_SURF_CHANNEL_SURFACE 2u // BC7 UNORM; R=AO, G=roughness, B=metallic, A=AO influence
#define T3D_COMP_SURF_PAGE_RESIDENT 1u
#define T3D_COMP_SURF_INVALID_SLOT 0xffffffffu
#define T3D_COMP_SURF_BC1_BYTES_PER_BLOCK 8u
#define T3D_COMP_SURF_BC5_BYTES_PER_BLOCK 16u
#define T3D_COMP_SURF_BC7_BYTES_PER_BLOCK 16u
#define T3D_COMP_SURF_PAGE_PAYLOAD_BYTES 46240u
#define T3D_COMP_SURF_MAX_SIDECAR_PAGES 1048576u

// All file structures are little-endian, naturally aligned and serialized
// explicitly as fixed-width values. Do not extend them without incrementing
// FILE_VERSION; no pointers, bool, enums or process-size types in the ABI.
struct t3dComposedSurfaceFileHeader_t
{
	uint32_t m_nMagic;
	uint32_t m_nVersion;
	uint32_t m_nHeaderBytes;
	uint32_t m_nRecordBytes;
	uint32_t m_nCompositionAbi;
	uint32_t m_nChannelMask;
	uint32_t m_nPageInterior;
	uint32_t m_nPageGutter;
	uint32_t m_nPhysicalTile;
	uint32_t m_nPageCount;
	uint32_t m_nFlags;
	uint32_t m_nReserved;
	uint64_t m_nRecordOffset;
	uint64_t m_nDataOffset;
	uint64_t m_nFileBytes;
	uint64_t m_nSourceFingerprint;
	uint64_t m_nReserved64[2];
};

struct t3dComposedSurfaceChannelRecord_t
{
	uint64_t m_nFileOffset;
	uint32_t m_nByteCount;
	uint32_t m_nCrc32;
};

struct t3dComposedSurfacePageRecord_t
{
	uint64_t m_nRegionKey;
	uint64_t m_nSourceFingerprint;
	uint64_t m_nCompositionRevision;
	uint64_t m_nContentFingerprint;
	uint32_t m_nMip;
	uint32_t m_nPageX;
	uint32_t m_nPageY;
	uint32_t m_nGeneration;
	uint32_t m_nFlags;
	uint32_t m_nChannelMask;
	uint32_t m_nReserved[2];
	t3dComposedSurfaceChannelRecord_t m_channels[T3D_COMP_SURF_CHANNEL_COUNT];
};

// A single page-set lookup references *all* surface channels; publish one
// mapping only after the complete page-set transaction has succeeded in CS05.
struct t3dComposedSurfacePageMapping_t
{
	uint64_t m_nRegionKey;
	uint64_t m_nCompositionRevision;
	uint32_t m_nPageX;
	uint32_t m_nPageY;
	uint32_t m_nMip;
	uint32_t m_nGeneration;
	uint32_t m_nDeviceGeneration;
	uint32_t m_nPhysicalSlot;
	uint32_t m_nResidentChannels;
	uint32_t m_nState;
};

static_assert( std::is_standard_layout<t3dComposedSurfaceFileHeader_t>::value, "No ABI object layout extensions" );
static_assert( std::is_trivially_copyable<t3dComposedSurfacePageRecord_t>::value, "Safe byte reader" );
static_assert( sizeof( t3dComposedSurfaceFileHeader_t ) == 96u, "Composed sidecar header ABI" );
static_assert( sizeof( t3dComposedSurfaceChannelRecord_t ) == 16u, "Composed sidecar channel ABI" );
static_assert( sizeof( t3dComposedSurfacePageRecord_t ) == 112u, "Composed sidecar record ABI" );
static_assert( sizeof( t3dComposedSurfacePageMapping_t ) == 48u, "Composed mapping ABI" );
static_assert( T3D_COMP_SURF_PHYSICAL_TILE == T3D_COMP_SURF_PAGE_INTERIOR + 2u * T3D_COMP_SURF_PAGE_GUTTER, "Page borders" );
static_assert( ( T3D_COMP_SURF_PHYSICAL_TILE & 3u ) == 0u, "BC block alignment" );
static_assert( ( T3D_COMP_SURF_PHYSICAL_TILE / 4u ) * ( T3D_COMP_SURF_PHYSICAL_TILE / 4u ) *
                       ( T3D_COMP_SURF_BC1_BYTES_PER_BLOCK + T3D_COMP_SURF_BC5_BYTES_PER_BLOCK + T3D_COMP_SURF_BC7_BYTES_PER_BLOCK ) ==
                   T3D_COMP_SURF_PAGE_PAYLOAD_BYTES,
               "BC page-set payload" );

inline bool t3d_comp_surf_host_little_endian()
{
	const uint32_t sentinel = 1u;
	return ( ( const uint8_t * )&sentinel )[0] == 1u;
}
inline bool t3d_comp_surf_range_within( const uint64_t offset, const uint64_t size, const uint64_t end )
{
	return offset <= end && size <= end - offset;
}
inline uint32_t t3d_comp_surf_channel_bytes( const uint32_t channel )
{
	const uint32_t block_bytes = channel == T3D_COMP_SURF_CHANNEL_ALBEDO   ? T3D_COMP_SURF_BC1_BYTES_PER_BLOCK
	                             : channel == T3D_COMP_SURF_CHANNEL_NORMAL ? T3D_COMP_SURF_BC5_BYTES_PER_BLOCK
	                                                                       : T3D_COMP_SURF_BC7_BYTES_PER_BLOCK;
	return ( T3D_COMP_SURF_PHYSICAL_TILE / 4u ) * ( T3D_COMP_SURF_PHYSICAL_TILE / 4u ) * block_bytes;
}
inline uint32_t t3d_comp_surf_crc32( const uint8_t *pBytes, const size_t nCount )
{
	uint32_t crc = 0xffffffffu;
	for ( size_t n = 0; n < nCount; ++n ) {
		crc ^= pBytes[n];
		for ( uint32_t bit = 0; bit < 8u; ++bit ) {
			crc = ( crc >> 1u ) ^ ( ( crc & 1u ) != 0u ? 0xedb88320u : 0u );
		}
	}
	return ~crc;
}
inline bool t3d_comp_surf_validate_channel_payload( const t3dComposedSurfaceChannelRecord_t& channel, const uint8_t *pData,
                                                    const size_t nActualBytes )
{
	return pData && nActualBytes == channel.m_nByteCount && t3d_comp_surf_crc32( pData, nActualBytes ) == channel.m_nCrc32;
}
inline bool t3d_comp_surf_validate_mapping( const t3dComposedSurfacePageMapping_t& mapping, const uint32_t nDeviceGeneration,
                                            const uint64_t nCompositionRevision )
{
	return mapping.m_nState == T3D_COMP_SURF_PAGE_RESIDENT && mapping.m_nPhysicalSlot != T3D_COMP_SURF_INVALID_SLOT &&
	       mapping.m_nResidentChannels == T3D_COMP_SURF_CHANNEL_MASK_ALL && mapping.m_nGeneration != 0u &&
	       mapping.m_nDeviceGeneration == nDeviceGeneration && mapping.m_nCompositionRevision == nCompositionRevision;
}

// UCMP-03: validate an entire region's cooked LOD hierarchy before importing
// any page into the existing V1 physical allocation. Sorted (mip,y,x), exact
// square footprints, and complete parent levels are required. Cold-only.
inline bool t3d_comp_surf_import_region_mips_complete( const t3dComposedSurfacePageRecord_t *pRecords, uint32_t nCount,
                                                       uint32_t& rMip0Tiles, uint64_t& rContentSignature )
{
	rMip0Tiles = 0u;
	rContentSignature = 0u;
	if ( !pRecords || !nCount || nCount > T3D_COMP_SURF_MAX_REGION_MIP_PAGES )
		return false;
	const uint64_t region = pRecords[0].m_nRegionKey;
	if ( !region || pRecords[0].m_nMip != 0u || pRecords[0].m_nPageX != 0u || pRecords[0].m_nPageY != 0u )
		return false;
	// Region footprints supported by the existing TerrainMegaVT are 1x1,
	// 2x2, or 4x4 pages at mip zero. Reject partial pyramids outright.
	const uint32_t tiles = nCount == 1u ? 1u : nCount == 5u ? 2u : nCount == 21u ? 4u : 0u;
	if ( !tiles )
		return false;
	uint32_t expected = 0u;
	for ( uint32_t n = tiles; n != 0u; n >>= 1u )
		expected += n * n;
	if ( nCount != expected )
		return false;
	uint64_t signature = 14695981039346656037ull;
	uint32_t index = 0u;
	for ( uint32_t mip = 0u, n = tiles; n != 0u; ++mip, n >>= 1u ) {
		for ( uint32_t y = 0u; y < n; ++y ) {
			for ( uint32_t x = 0u; x < n; ++x ) {
				const t3dComposedSurfacePageRecord_t& r = pRecords[index++];
				if ( r.m_nRegionKey != region || r.m_nMip != mip || r.m_nPageX != x || r.m_nPageY != y || !r.m_nContentFingerprint ||
				     !r.m_nCompositionRevision || !r.m_nGeneration || r.m_nChannelMask != T3D_COMP_SURF_CHANNEL_MASK_ALL )
					return false;
				const uint64_t elements[3] = { r.m_nContentFingerprint, r.m_nCompositionRevision, r.m_nGeneration };
				for ( uint32_t i = 0u; i < 3u; ++i ) {
					signature ^= elements[i] + 0x9e3779b97f4a7c15ull + ( signature << 6u ) + ( signature >> 2u );
				}
			}
		}
	}
	rMip0Tiles = tiles;
	rContentSignature = signature ? signature : 1u;
	return true;
}

// Header and on-disk record validation, including channel payload bounds and
// monotone non-overlapping offsets. Checksum verification happens after reading
// each compressed channel: compressed bytes are not required to validate layout.
inline bool t3d_comp_surf_validate_sidecar( const t3dComposedSurfaceFileHeader_t& header, const t3dComposedSurfacePageRecord_t *pRecords,
                                            const uint64_t nActualFileBytes )
{
	if ( !t3d_comp_surf_host_little_endian() || header.m_nMagic != T3D_COMP_SURF_FILE_MAGIC ||
	     header.m_nVersion != T3D_COMP_SURF_FILE_VERSION || header.m_nCompositionAbi != T3D_COMP_SURF_COMPOSITION_ABI ||
	     header.m_nHeaderBytes != sizeof( t3dComposedSurfaceFileHeader_t ) ||
	     header.m_nRecordBytes != sizeof( t3dComposedSurfacePageRecord_t ) || header.m_nPageInterior != T3D_COMP_SURF_PAGE_INTERIOR ||
	     header.m_nPageGutter != T3D_COMP_SURF_PAGE_GUTTER || header.m_nPhysicalTile != T3D_COMP_SURF_PHYSICAL_TILE ||
	     header.m_nChannelMask != T3D_COMP_SURF_CHANNEL_MASK_ALL || header.m_nFlags != 0u || header.m_nReserved != 0u ||
	     header.m_nReserved64[0] != 0u || header.m_nReserved64[1] != 0u || header.m_nPageCount > T3D_COMP_SURF_MAX_SIDECAR_PAGES ||
	     header.m_nFileBytes != nActualFileBytes || header.m_nRecordOffset != sizeof( t3dComposedSurfaceFileHeader_t ) ||
	     !t3d_comp_surf_range_within( header.m_nRecordOffset, uint64_t( header.m_nPageCount ) * sizeof( t3dComposedSurfacePageRecord_t ),
	                                  header.m_nFileBytes ) ||
	     header.m_nDataOffset < header.m_nRecordOffset + uint64_t( header.m_nPageCount ) * sizeof( t3dComposedSurfacePageRecord_t ) ||
	     header.m_nDataOffset > header.m_nFileBytes || ( header.m_nDataOffset & 7u ) != 0u || ( header.m_nPageCount != 0u && !pRecords ) ) {
		return false;
	}
	uint64_t cursor = header.m_nDataOffset;
	for ( uint32_t page = 0u; page < header.m_nPageCount; ++page ) {
		const t3dComposedSurfacePageRecord_t& r = pRecords[page];
		if ( r.m_nRegionKey == 0u || r.m_nCompositionRevision == 0u || r.m_nGeneration == 0u ||
		     r.m_nSourceFingerprint != header.m_nSourceFingerprint || r.m_nFlags != 0u ||
		     r.m_nChannelMask != T3D_COMP_SURF_CHANNEL_MASK_ALL || r.m_nContentFingerprint == 0u || r.m_nMip >= 32u ||
		     r.m_nReserved[0] != 0u || r.m_nReserved[1] != 0u ) {
			return false;
		}
		for ( uint32_t channel = 0u; channel < T3D_COMP_SURF_CHANNEL_COUNT; ++channel ) {
			const t3dComposedSurfaceChannelRecord_t& blob = r.m_channels[channel];
			if ( blob.m_nByteCount != t3d_comp_surf_channel_bytes( channel ) || ( blob.m_nFileOffset & 0x7u ) != 0u ||
			     blob.m_nFileOffset < cursor ||
			     !t3d_comp_surf_range_within( blob.m_nFileOffset, blob.m_nByteCount, header.m_nFileBytes ) ) {
				return false;
			}
			cursor = blob.m_nFileOffset + blob.m_nByteCount;
		}
	}
	return true;
}

#endif // TERRAIN3D_COMPOSED_SURFACE_CONTRACT_H
