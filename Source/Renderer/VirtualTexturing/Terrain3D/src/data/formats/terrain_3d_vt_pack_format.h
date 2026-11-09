// Shared frozen VT V1 disk/reader contract (editor + exported runtime).
#ifndef TERRAIN3D_VT_PACK_FORMAT_H
#define TERRAIN3D_VT_PACK_FORMAT_H
#include "Terrain3DTypes.h"
#include "rendering/rd/vt/core/terrain_3d_vt_contract.h"
#include <stdint.h>

#define T3D_VT_PACK_HEADER_BYTES 256u
#define T3D_VT_PACK_ALIGNMENT 16u
#define T3D_VT_PACK_CODEC_NONE 0u
#define T3D_VT_PACK_CODEC_ZSTD 1u
#define T3D_VT_PACK_FLAG_CLUSTERED_PAYLOADS ( 1u << 0u )
#define T3D_VT_PACK_FLAG_TAILS_PRESENT ( 1u << 1u )
#define T3D_VT_PACK_FLAG_GPU_READY ( 1u << 2u )
#define T3D_VT_PACK_FLAG_OUTER_COMPRESSION ( 1u << 3u )
#define T3D_VT_PACK_FLAG_QUALITY_VIEWS ( 1u << 4u )
#define T3D_VT_PACK_FLAG_FORMAT_FAMILY_MAP ( 1u << 5u )
#define T3D_VT_PACK_PAYLOAD_FLAG_TAIL ( 1u << 0u )
#define T3D_VT_PACK_TAIL_LAYOUT_HORIZONTAL 1u

// Frozen Sparse VT V1 package ABI; shared by native readers and editor writers.
uint32_t t3d_vt_pack_fully_supported_profile_mask( const t3dVTSerializedPackSource_t& rSource );
uint64_t t3d_vt_pack_hash64( const void *pBytes, uint64_t nBytes );
uint32_t t3d_vt_pack_checksum32( const void *pBytes, uint64_t nBytes );
uint32_t t3d_vt_pack_expected_tile_bytes( uint32_t nFormat );
uint32_t t3d_vt_pack_expected_physical_tile_bytes( uint32_t nPhysicalFamily );
bool t3d_vt_pack_init_format_family_records( t3dVTPackFormatFamilyRecord_t *pRecords, uint32_t nRecordCount );
bool t3d_vt_pack_open_baseline_view( const void *pPackage, uint64_t nPackageBytes, t3dVTPackBaselineView_t& rView );
bool t3d_vt_pack_open_metadata_view( const void *pPrefix, uint64_t nPrefixBytes, uint64_t nFileBytes, t3dVTPackBaselineView_t& rView );

static_assert( sizeof( t3dVTPackHeader_t ) == T3D_VT_PACK_HEADER_BYTES, "VT pack v1 header ABI changed" );
static_assert( sizeof( t3dVTPackAllocationRecord_t ) == 16u, "VT pack v1 allocation ABI changed" );
static_assert( sizeof( t3dVTPackPageRecord_t ) == 36u, "VT pack v1 page ABI changed" );
static_assert( sizeof( t3dVTPackTailRecord_t ) == 40u, "VT pack v1 tail ABI changed" );
static_assert( sizeof( t3dVTPackQualityView_t ) == 20u, "VT pack v1 quality-view ABI changed" );
static_assert( sizeof( t3dVTPackFormatFamilyRecord_t ) == 8u, "VT pack v1 format-family ABI changed" );
static_assert( sizeof( t3dVTPackAllocationKeyRecord_t ) == 16u, "VT pack v1 allocation-key ABI changed" );
static_assert( sizeof( t3dVTPackPayloadRecord_t ) == 48u, "VT pack v1 payload ABI changed" );
static_assert( sizeof( t3dVTPackClusterRecord_t ) == 40u, "VT pack v1 cluster ABI changed" );

#endif // TERRAIN3D_VT_PACK_FORMAT_H
