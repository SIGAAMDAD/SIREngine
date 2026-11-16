// Copyright © 2023-2026 Cory Petkovsek, Roope Palmroos, and Contributors.

#ifndef TERRAIN3D_VT_CONTRACT_H
#define TERRAIN3D_VT_CONTRACT_H

#include "Terrain3DTypes.h"

#include <stddef.h>
#include <stdint.h>

// Terrain3D Sparse VT V1 is the frozen shipping package ABI.
// Filter/compressor ABI revisions are explicit cache-invalidating contracts.
#define T3D_VT_FILE_MAGIC 0x56543154u
#define T3D_VT_FILE_FORMAT_VERSION 1u
#define T3D_VT_FILE_VERSION T3D_VT_FILE_FORMAT_VERSION
#define T3D_VT_ABI_VERSION 2u
#define T3D_VT_FILTER_ABI_VERSION 1u
#define T3D_VT_COMPRESSOR_ABI_VERSION 1u
static_assert( T3D_VT_FILE_FORMAT_VERSION == 1u, "Sparse VT V1 file ABI version is frozen" );
static_assert( T3D_VT_ABI_VERSION == 2u, "Sparse VT V1 ABI version is frozen" );
static_assert( T3D_VT_FILTER_ABI_VERSION == 1u, "Sparse VT V1 filter ABI version is frozen" );
static_assert( T3D_VT_COMPRESSOR_ABI_VERSION == 1u, "Sparse VT V1 RGBA compressor recipe revision" );
// V1 regional coarse-surface source recipe. Changes invalidate cooked fingerprints.
#define T3D_VT_REGIONAL_SOURCE_RECIPE_VERSION 3u
#define T3D_VT_PAGE_INTERIOR 128u
#define T3D_VT_PAGE_GUTTER 4u
#define T3D_VT_PHYSICAL_TILE 136u
#define T3D_VT_MAX_ANISOTROPY 4u
#define T3D_VT_CHANNEL_MASK_ALL ( ( 1u << T3D_VT_CHANNEL_COUNT ) - 1u )
#define T3D_VT_LOGICAL_CHANNEL_MASK_ALL ( ( ( 1u << T3D_VT_LOGICAL_CHANNEL_COUNT ) - 1u ) & ~( 1u << 5u ) )
#define T3D_VT_INVALID_PAGE 0xffffffffu
#define T3D_VT_INVALID_SLOT 0xffffffffu
#define T3D_VT_INVALID_TAIL 0xffffffffu
#define T3D_VT_ANCESTOR_PROBE_CAP 16u
#define T3D_VT_TAIL_TILE 16u
#define T3D_VT_TAIL_ATLAS_WIDTH 2048u
#define T3D_VT_TAIL_TILES_PER_ROW ( T3D_VT_TAIL_ATLAS_WIDTH / T3D_VT_TAIL_TILE )
// Sparse V1 cooks the compact tail mip chain horizontally inside the 128 texel
// page interior. Runtime preserves that layout verbatim so BC-compressed tail
// payloads never need to be decoded/re-encoded merely to expose lower mips.
#define T3D_VT_PACK_TAIL_SLOT_WIDTH 128u
#define T3D_VT_PACK_TAIL_SLOT_HEIGHT 64u
// Runtime V1 tails use a wider atlas; serialized payload layout is unchanged.
#define T3D_VT_PACK_TAIL_ATLAS_WIDTH 8192u
#define T3D_VT_PACK_TAILS_PER_ROW ( T3D_VT_PACK_TAIL_ATLAS_WIDTH / T3D_VT_PACK_TAIL_SLOT_WIDTH )
#define T3D_TERRAIN_MEGAVT_ALLOCATION_CAPACITY 1024u

// A distinct stable-key namespace identifies logical terrain regions.
inline uint64_t t3d_vt_terrain_region_stable_key( const uint32_t nRegionMapIndex )
{
	return 0x5445525245470000ull | ( uint64_t )nRegionMapIndex;
}
inline uint32_t t3d_vt_allocation_capacity( const uint32_t nMaxMaterials )
{
	return nMaxMaterials <= UINT32_MAX / 4u - T3D_TERRAIN_MEGAVT_ALLOCATION_CAPACITY * 3u
	           ? ( nMaxMaterials + T3D_TERRAIN_MEGAVT_ALLOCATION_CAPACITY * 3u ) * 4u
			   : 0u;
}

// Stable-key namespace reserved for Terrain3D logical terrain material layers.
// Instance rendering-material keys are content hashes and must not occupy this
// namespace; the Godot cooker rejects such a collision when collecting assets.
#define T3D_VT_TERRAIN_MATERIAL_KEY_TAG 0x5433445400000000ULL
#define T3D_VT_TERRAIN_MATERIAL_KEY_MASK 0xffffffff00000000ULL

static inline uint64_t t3d_vt_terrain_material_stable_key( const uint32_t nLogicalMaterialId )
{
	return T3D_VT_TERRAIN_MATERIAL_KEY_TAG | ( uint64_t )( nLogicalMaterialId + 1u );
}

static inline bool t3d_vt_decode_terrain_material_stable_key( const uint64_t nStableKey, uint32_t& rLogicalMaterialId )
{
	if ( ( nStableKey & T3D_VT_TERRAIN_MATERIAL_KEY_MASK ) != T3D_VT_TERRAIN_MATERIAL_KEY_TAG ) {
		return false;
	}
	const uint32_t nEncoded = ( uint32_t )( nStableKey & 0xffffffffULL );
	if ( nEncoded == 0u ) {
		return false;
	}
	rLogicalMaterialId = nEncoded - 1u;
	return true;
}

// Disjoint V1 stable-key namespace for offline-composed pages. The key is
// stored in the existing allocation table, never a second composed page table.
inline uint64_t t3d_vt_composed_region_stable_key( const uint32_t nRegionMapIndex )
{
	return 0x5443535245470000ull | ( uint64_t )nRegionMapIndex;
}
inline bool t3d_vt_is_composed_region_stable_key( const uint64_t nStableKey )
{
	return ( nStableKey & 0xffffffffffff0000ull ) == 0x5443535245470000ull;
}

inline bool t3d_vt_is_terrain_region_stable_key( const uint64_t nStableKey )
{
	return ( nStableKey & 0xffffffffffff0000ull ) == 0x5445525245470000ull;
}

// SH-B frozen shared shader descriptor contract. These constants mirror the
// literal GLSL layout declarations; the literals stay in GLSL so SH-A's
// descriptor reflection remains authoritative and can detect drift.
#define T3D_VT_SAMPLE_DESCRIPTOR_SET 0u
#define T3D_VT_SAMPLE_ALLOCATION_BINDING 8u
#define T3D_VT_SAMPLE_PAGE_TABLE_BINDING 9u
#define T3D_VT_SAMPLE_ALBEDO_BANK_BINDING_BASE 10u
#define T3D_VT_SAMPLE_NORMAL_BANK_BINDING_BASE 26u
#define T3D_VT_SAMPLE_SURFACE_BANK_BINDING_BASE 42u
#define T3D_VT_SAMPLE_HEIGHT_BANK_BINDING_BASE 58u
#define T3D_VT_SAMPLE_TAIL_METADATA_BINDING 74u
#define T3D_VT_SAMPLE_ALBEDO_TAIL_BINDING 75u
#define T3D_VT_SAMPLE_NORMAL_TAIL_BINDING 76u
#define T3D_VT_SAMPLE_SURFACE_TAIL_BINDING 77u
#define T3D_VT_SAMPLE_HEIGHT_TAIL_BINDING 78u

#define T3D_VT_FRAGMENT_FEEDBACK_CONTRACT_VERSION 2u
#define T3D_VT_FRAGMENT_FEEDBACK_DESCRIPTOR_SET 3u
#define T3D_VT_FRAGMENT_FEEDBACK_ALLOCATION_BINDING 0u
#define T3D_VT_FRAGMENT_FEEDBACK_PAGE_TABLE_BINDING 1u
#define T3D_VT_FRAGMENT_FEEDBACK_HASH_BINDING 2u
#define T3D_VT_FRAGMENT_FEEDBACK_GRID_BINDING 2u
#define T3D_VT_FRAGMENT_FEEDBACK_COUNTER_BINDING 3u
#define T3D_VT_FRAGMENT_FEEDBACK_PARAMS_BINDING 4u
#define T3D_VT_FRAGMENT_FEEDBACK_DESCRIPTOR_COUNT 5u

#define T3D_VT_MATERIAL_METALLIC_SHIFT 0u
#define T3D_VT_MATERIAL_ROUGHNESS_SHIFT 8u
#define T3D_VT_MATERIAL_AO_SHIFT 16u
#define T3D_VT_MATERIAL_ALPHA_SCISSOR_SHIFT 24u
#define T3D_VT_MATERIAL_UNORM8_MASK 0xffu
#define T3D_VT_MATERIAL_NORMAL_SCALE_MASK 0xffffu
#define T3D_VT_MATERIAL_SURFACE_LAYOUT_SHIFT 16u
#define T3D_VT_MATERIAL_SURFACE_LAYOUT_MASK ( 0x7u << T3D_VT_MATERIAL_SURFACE_LAYOUT_SHIFT )
#define T3D_VT_MATERIAL_FILTER_SHIFT 19u
#define T3D_VT_MATERIAL_FILTER_MASK ( 0x7u << T3D_VT_MATERIAL_FILTER_SHIFT )
#define T3D_VT_MATERIAL_EMISSION_FORMAT_RGB9E5 1u

// Missing material channels are cooked to these semantic samples. Scalar
// material parameters remain authoritative for roughness/metallic/AO defaults.
#define T3D_VT_NEUTRAL_ALBEDO_R8 255u
#define T3D_VT_NEUTRAL_ALBEDO_G8 255u
#define T3D_VT_NEUTRAL_ALBEDO_B8 255u
#define T3D_VT_NEUTRAL_ALBEDO_A8 255u
#define T3D_VT_NEUTRAL_NORMAL_X8 128u
#define T3D_VT_NEUTRAL_NORMAL_Y8 128u
#define T3D_VT_NEUTRAL_SURFACE_AO8 255u
#define T3D_VT_NEUTRAL_SURFACE_ROUGHNESS8 255u
#define T3D_VT_NEUTRAL_SURFACE_METALLIC8 255u
#define T3D_VT_NEUTRAL_SURFACE_ALPHA8 255u
#define T3D_VT_NEUTRAL_HEIGHT8 128u

// V1 logical channels are semantic/format pools. Terrain clipmaps consume
// ALBEDO/PBR/(HEIGHT); instances consume ALBEDO/(NORMAL)/(ORM).

// V1 physical residency is format-family based. Logical ALBEDO and ORM both
// normally occupy the BC1 UNORM family; the shader applies semantic decode from the
// logical channel rather than from the physical bank.

// V1 internal page-table addresses reserve three high bits for five format families.
// The remaining 29 bits address physical layers; all resident pages use one allocator.
#define T3D_VT_PHYSICAL_FAMILY_SHIFT 29u
#define T3D_VT_PHYSICAL_FAMILY_MASK 0xe0000000u
#define T3D_VT_PHYSICAL_SLOT_MASK 0x1fffffffu

static inline uint32_t t3d_vt_pack_physical_address( const t3dVTPhysicalFormatFamily_t Family, const uint32_t nSlot )
{
	if ( ( uint32_t )Family >= T3D_VT_PHYSICAL_FORMAT_COUNT || nSlot > T3D_VT_PHYSICAL_SLOT_MASK ) {
		return T3D_VT_INVALID_SLOT;
	}
	return ( ( uint32_t )Family << T3D_VT_PHYSICAL_FAMILY_SHIFT ) | nSlot;
}

static inline t3dVTPhysicalFormatFamily_t t3d_vt_physical_address_family( const uint32_t nAddress )
{
	return ( t3dVTPhysicalFormatFamily_t )( ( nAddress & T3D_VT_PHYSICAL_FAMILY_MASK ) >> T3D_VT_PHYSICAL_FAMILY_SHIFT );
}

static inline uint32_t t3d_vt_physical_address_slot( const uint32_t nAddress )
{
	return nAddress & T3D_VT_PHYSICAL_SLOT_MASK;
}

// The current World owns four physical arrays already. P04 reinterprets
// them by format family without increasing descriptor count: BC1=albedo array,
// BC7=surface array, BC5=normal array, R8=height array.
static inline uint32_t t3d_vt_physical_family_legacy_resource_channel( const t3dVTPhysicalFormatFamily_t Family )
{
	switch ( Family ) {
	case T3D_VT_PHYSICAL_FORMAT_BC1_UNORM:
		return T3D_VT_CHANNEL_ALBEDO;
	case T3D_VT_PHYSICAL_FORMAT_BC7_UNORM:
		return T3D_VT_CHANNEL_SURFACE;
	case T3D_VT_PHYSICAL_FORMAT_BC5_UNORM:
		return T3D_VT_CHANNEL_NORMAL;
	case T3D_VT_PHYSICAL_FORMAT_R8_UNORM:
		return T3D_VT_CHANNEL_HEIGHT;
	case T3D_VT_PHYSICAL_FORMAT_R16_UNORM:
		return T3D_VT_CHANNEL_HEIGHT16;
	default:
		return T3D_VT_INVALID_SLOT;
	}
}

// ALBEDO RGBA is an allocation-wide choice, shared by regular pages and tails.
#define T3D_VT_FORMAT_MAP_ALBEDO_RGBA 1u
static inline t3dVTPhysicalFormatFamily_t t3d_vt_logical_channel_physical_family( const t3dVTLogicalChannel_t Channel,
                                                                                  const uint16_t nFlags = 0u )
{
	if ( Channel == T3D_VT_LOGICAL_CHANNEL_ALBEDO && ( nFlags & T3D_VT_ALLOCATION_ALBEDO_RGBA ) != 0u ) {
		return T3D_VT_PHYSICAL_FORMAT_BC7_UNORM;
	}
	switch ( Channel ) {
	case T3D_VT_LOGICAL_CHANNEL_ALBEDO:
	case T3D_VT_LOGICAL_CHANNEL_ORM:
		return T3D_VT_PHYSICAL_FORMAT_BC1_UNORM;
	case T3D_VT_LOGICAL_CHANNEL_PBR:
		return T3D_VT_PHYSICAL_FORMAT_BC7_UNORM;
	case T3D_VT_LOGICAL_CHANNEL_NORMAL:
		return T3D_VT_PHYSICAL_FORMAT_BC5_UNORM;
	case T3D_VT_LOGICAL_CHANNEL_HEIGHT:
	default:
		return T3D_VT_PHYSICAL_FORMAT_R8_UNORM;
	}
}

static inline t3dVTChannelSemantic_t t3d_vt_logical_channel_semantic( const t3dVTLogicalChannel_t Channel )
{
	switch ( Channel ) {
	case T3D_VT_LOGICAL_CHANNEL_ALBEDO:
		return T3D_VT_CHANNEL_SEMANTIC_SRGB_ALBEDO;
	case T3D_VT_LOGICAL_CHANNEL_PBR:
		return T3D_VT_CHANNEL_SEMANTIC_TERRAIN_PBR;
	case T3D_VT_LOGICAL_CHANNEL_HEIGHT:
		return T3D_VT_CHANNEL_SEMANTIC_LINEAR_HEIGHT;
	case T3D_VT_LOGICAL_CHANNEL_NORMAL:
		return T3D_VT_CHANNEL_SEMANTIC_NORMAL_XY;
	case T3D_VT_LOGICAL_CHANNEL_ORM:
	default:
		return T3D_VT_CHANNEL_SEMANTIC_LINEAR_ORM;
	}
}

// VT-P06 runtime adapter: one logical V1 material allocation is flattened to
// three independent residency allocations. The meaning of slots 1/2 depends
// on the consumer; slot 0 is ALBEDO for both.
#define T3D_VT_RUNTIME_CHANNELS_PER_ALLOCATION 3u

static inline bool t3d_vt_runtime_slot_for_channel( const t3dVTConsumer_t Consumer, const t3dVTLogicalChannel_t Channel, uint32_t& rSlot )
{
	if ( Channel == T3D_VT_LOGICAL_CHANNEL_ALBEDO ) {
		rSlot = 0u;
		return true;
	}
	if ( Consumer == T3D_VT_CONSUMER_CLIPMAP ) {
		if ( Channel == T3D_VT_LOGICAL_CHANNEL_PBR ) {
			rSlot = 1u;
			return true;
		}
		if ( Channel == T3D_VT_LOGICAL_CHANNEL_HEIGHT ) {
			rSlot = 2u;
			return true;
		}
	} else if ( Consumer == T3D_VT_CONSUMER_COMPOSED ) {
		if ( Channel == T3D_VT_LOGICAL_CHANNEL_NORMAL ) {
			rSlot = 1u;
			return true;
		}
		if ( Channel == T3D_VT_LOGICAL_CHANNEL_PBR ) {
			rSlot = 2u;
			return true;
		}
	} else if ( Consumer == T3D_VT_CONSUMER_INSTANCE ) {
		if ( Channel == T3D_VT_LOGICAL_CHANNEL_NORMAL ) {
			rSlot = 1u;
			return true;
		}
		if ( Channel == T3D_VT_LOGICAL_CHANNEL_ORM ) {
			rSlot = 2u;
			return true;
		}
	}
	rSlot = T3D_VT_INVALID_SLOT;
	return false;
}

static inline uint32_t t3d_vt_runtime_allocation_base( const uint32_t nAllocationId )
{
	return nAllocationId * T3D_VT_RUNTIME_CHANNELS_PER_ALLOCATION;
}

static inline bool t3d_vt_logical_channel_for_runtime_slot( const t3dVTConsumer_t Consumer, const uint32_t nSlot,
                                                            t3dVTLogicalChannel_t& rChannel )
{
	if ( nSlot == 0u ) {
		rChannel = T3D_VT_LOGICAL_CHANNEL_ALBEDO;
		return true;
	}
	if ( Consumer == T3D_VT_CONSUMER_CLIPMAP ) {
		if ( nSlot == 1u ) {
			rChannel = T3D_VT_LOGICAL_CHANNEL_PBR;
			return true;
		}
		if ( nSlot == 2u ) {
			rChannel = T3D_VT_LOGICAL_CHANNEL_HEIGHT;
			return true;
		}
	} else if ( Consumer == T3D_VT_CONSUMER_COMPOSED ) {
		if ( nSlot == 1u ) {
			rChannel = T3D_VT_LOGICAL_CHANNEL_NORMAL;
			return true;
		}
		if ( nSlot == 2u ) {
			rChannel = T3D_VT_LOGICAL_CHANNEL_PBR;
			return true;
		}
	} else if ( Consumer == T3D_VT_CONSUMER_INSTANCE ) {
		if ( nSlot == 1u ) {
			rChannel = T3D_VT_LOGICAL_CHANNEL_NORMAL;
			return true;
		}
		if ( nSlot == 2u ) {
			rChannel = T3D_VT_LOGICAL_CHANNEL_ORM;
			return true;
		}
	}
	return false;
}

#define T3D_VT_LOGICAL_CHANNEL_BIT( channel ) ( 1u << ( uint32_t )( channel ) )
#define T3D_VT_CLIPMAP_ALLOWED_CHANNEL_MASK                                                                                                \
	( T3D_VT_LOGICAL_CHANNEL_BIT( T3D_VT_LOGICAL_CHANNEL_ALBEDO ) | T3D_VT_LOGICAL_CHANNEL_BIT( T3D_VT_LOGICAL_CHANNEL_PBR ) |             \
	  T3D_VT_LOGICAL_CHANNEL_BIT( T3D_VT_LOGICAL_CHANNEL_HEIGHT ) )
// Composed pages are BC1 albedo, BC5 normal and BC7 packed surface.
// The three slots share the existing V1 allocator and physical BC banks.
#define T3D_VT_COMPOSED_ALLOWED_CHANNEL_MASK                                                                                               \
	( T3D_VT_LOGICAL_CHANNEL_BIT( T3D_VT_LOGICAL_CHANNEL_ALBEDO ) | T3D_VT_LOGICAL_CHANNEL_BIT( T3D_VT_LOGICAL_CHANNEL_NORMAL ) |          \
	  T3D_VT_LOGICAL_CHANNEL_BIT( T3D_VT_LOGICAL_CHANNEL_PBR ) )
#define T3D_VT_COMPOSED_REQUIRED_CHANNEL_MASK T3D_VT_COMPOSED_ALLOWED_CHANNEL_MASK

#define T3D_VT_INSTANCE_ALLOWED_CHANNEL_MASK                                                                                               \
	( T3D_VT_LOGICAL_CHANNEL_BIT( T3D_VT_LOGICAL_CHANNEL_ALBEDO ) | T3D_VT_LOGICAL_CHANNEL_BIT( T3D_VT_LOGICAL_CHANNEL_NORMAL ) |          \
	  T3D_VT_LOGICAL_CHANNEL_BIT( T3D_VT_LOGICAL_CHANNEL_ORM ) )

static inline uint32_t t3d_vt_consumer_allowed_mask( const t3dVTConsumer_t consumer )
{
	switch ( consumer ) {
	case T3D_VT_CONSUMER_CLIPMAP:
		return T3D_VT_CLIPMAP_ALLOWED_CHANNEL_MASK;
	case T3D_VT_CONSUMER_INSTANCE:
		return T3D_VT_INSTANCE_ALLOWED_CHANNEL_MASK;
	case T3D_VT_CONSUMER_COMPOSED:
		return T3D_VT_COMPOSED_ALLOWED_CHANNEL_MASK;
	default:
		return 0u;
	}
}
static inline uint32_t t3d_vt_consumer_required_mask( const t3dVTConsumer_t consumer )
{
	switch ( consumer ) {
	case T3D_VT_CONSUMER_CLIPMAP:
		return T3D_VT_LOGICAL_CHANNEL_BIT( T3D_VT_LOGICAL_CHANNEL_ALBEDO ) | T3D_VT_LOGICAL_CHANNEL_BIT( T3D_VT_LOGICAL_CHANNEL_PBR );
	case T3D_VT_CONSUMER_INSTANCE:
		return T3D_VT_LOGICAL_CHANNEL_BIT( T3D_VT_LOGICAL_CHANNEL_ALBEDO );
	case T3D_VT_CONSUMER_COMPOSED:
		return T3D_VT_COMPOSED_REQUIRED_CHANNEL_MASK;
	default:
		return 0u;
	}
}

static inline t3dVTLogicalChannelFormat_t t3d_vt_logical_channel_format( const t3dVTLogicalChannel_t Channel )
{
	switch ( Channel ) {
	case T3D_VT_LOGICAL_CHANNEL_ALBEDO:
		return T3D_VT_LOGICAL_FORMAT_ALBEDO_BC1_SRGB;
	case T3D_VT_LOGICAL_CHANNEL_PBR:
		return T3D_VT_LOGICAL_FORMAT_PBR_BC7_UNORM;
	case T3D_VT_LOGICAL_CHANNEL_HEIGHT:
		return T3D_VT_LOGICAL_FORMAT_HEIGHT_R8_UNORM;
	case T3D_VT_LOGICAL_CHANNEL_NORMAL:
		return T3D_VT_LOGICAL_FORMAT_NORMAL_BC5_UNORM;
	case T3D_VT_LOGICAL_CHANNEL_ORM:
	default:
		return T3D_VT_LOGICAL_FORMAT_ORM_BC1_UNORM;
	}
}

#define T3D_VT_ALLOCATION_RUNTIME_QUALITY_MASK ( T3D_VT_ALLOCATION_QUALITY_BASE_MIP_MASK | T3D_VT_ALLOCATION_QUALITY_DISABLED )

static inline uint32_t t3d_vt_allocation_quality_base_mip( const uint16_t nFlags )
{
	return ( ( uint32_t )nFlags & T3D_VT_ALLOCATION_QUALITY_BASE_MIP_MASK ) >> T3D_VT_ALLOCATION_QUALITY_BASE_MIP_SHIFT;
}

static inline bool t3d_vt_allocation_quality_enabled( const uint16_t nFlags )
{
	return ( nFlags & T3D_VT_ALLOCATION_QUALITY_DISABLED ) == 0u;
}

// CPU fields at offsets 8/10 and 12/14 are consumed by GLSL as two packed
// uints: packedMip0Extent and packedMipCountFlags respectively.

// V1 uses one record per allocation/logical-channel pair. Index is
// allocation_id * T3D_VT_LOGICAL_CHANNEL_COUNT + logical_channel.

#define T3D_VT_PACK_INVALID_MIP 0xffu

// Native V1 serialized records shared by the pack parser and runtime. The
// channel-allocation directory remains a fixed allocation x logical-channel
// matrix; absent channels carry a zero record and allocate no pages/tail.

// CPU fields at offsets 12/14 are consumed by GLSL as packedResolvedMipFlags.

// GPU-only precomputed allocation mip layout. This is deliberately separate
// from the frozen 32-byte serialized allocation ABI. One allocation owns
// T3D_VT_MAX_MIPS contiguous entries; unused entries are zeroed.

// One tail metadata record addresses the same compact atlas rectangle in all
// four channel atlases. ChannelMask identifies authored payloads; absent
// channels resolve to the frozen neutral semantics without allocating pages.

// Debug-visible resolve result. A physical resolution has a valid slot and an
// invalid tail index. A terminal tail resolution has the inverse.

// The slot number selects the same bank/layer for every material channel.
// CPU 16-bit fields at offsets 16..23 are consumed by GLSL as packed words.

// CPU wantedMip/priority at offsets 4/6 are consumed by GLSL as one packed uint.

// Fixed 64-byte material ABI. Texture identity is represented only by stable
// virtual-texture allocation IDs; source Texture2D/RID identity is cook input.

static_assert( sizeof( t3dVTAllocation_t ) == 32u, "t3dVTAllocation_t ABI changed" );
static_assert( sizeof( t3dVTChannelAllocation_t ) == 20u, "t3dVTChannelAllocation_t v1 ABI changed" );
static_assert( sizeof( t3dVTPackAllocationRecord_t ) == 16u, "t3dVTPackAllocationRecord_t ABI changed" );
static_assert( sizeof( t3dVTPackPageRecord_t ) == 36u, "t3dVTPackPageRecord_t ABI changed" );
static_assert( sizeof( t3dVTPackTailRecord_t ) == 40u, "t3dVTPackTailRecord_t ABI changed" );
static_assert( sizeof( t3dVTPackQualityView_t ) == 20u, "t3dVTPackQualityView_t ABI changed" );
static_assert( sizeof( t3dVTPackFormatFamilyRecord_t ) == 8u, "t3dVTPackFormatFamilyRecord_t ABI changed" );
static_assert( offsetof( t3dVTAllocation_t, m_nBaseVirtualPage ) == 0u, "VT allocation base-page ABI changed" );
static_assert( offsetof( t3dVTAllocation_t, m_nMip0WidthPages ) == 8u, "VT allocation extent ABI changed" );
static_assert( offsetof( t3dVTAllocation_t, m_nMipCount ) == 12u, "VT allocation mip ABI changed" );
static_assert( offsetof( t3dVTAllocation_t, m_nTailIndex ) == 16u, "VT allocation tail ABI changed" );
static_assert( offsetof( t3dVTAllocation_t, m_nGeneration ) == 20u, "VT allocation generation ABI changed" );
static_assert( offsetof( t3dVTAllocation_t, m_nContentId ) == 24u, "VT allocation content ABI changed" );

static_assert( sizeof( t3dVTPageEntry_t ) == 16u, "t3dVTPageEntry_t ABI changed" );
static_assert( offsetof( t3dVTPageEntry_t, m_nResolvedMip ) == 12u, "VT page-entry packed ABI changed" );
static_assert( sizeof( t3dVTMipLayout_t ) == 8u, "t3dVTMipLayout_t GPU ABI changed" );
static_assert( offsetof( t3dVTMipLayout_t, m_nWidthPages ) == 4u, "VT mip-layout packed extent ABI changed" );

static_assert( sizeof( t3dVTTailMetadata_t ) == 32u, "t3dVTTailMetadata_t ABI changed" );
static_assert( offsetof( t3dVTTailMetadata_t, m_nWidthTexels ) == 8u, "VT tail extent ABI changed" );
static_assert( offsetof( t3dVTTailMetadata_t, m_nGeneration ) == 16u, "VT tail generation ABI changed" );
static_assert( offsetof( t3dVTTailMetadata_t, m_nChannelMask ) == 20u, "VT tail channel-mask ABI changed" );

static_assert( sizeof( t3dVTResolveResult_t ) == 32u, "t3dVTResolveResult_t ABI changed" );
static_assert( offsetof( t3dVTResolveResult_t, m_nRequestedMip ) == 20u, "VT resolve debug ABI changed" );
static_assert( offsetof( t3dVTResolveResult_t, m_nResolvedMip ) == 22u, "VT resolve packed mip ABI changed" );
static_assert( offsetof( t3dVTResolveResult_t, m_nFallbackDepth ) == 24u, "VT resolve fallback ABI changed" );
static_assert( offsetof( t3dVTResolveResult_t, m_nFlags ) == 26u, "VT resolve flags ABI changed" );
static_assert( offsetof( t3dVTResolveResult_t, m_nReserved ) == 28u, "VT resolve reserved ABI changed" );

static_assert( sizeof( t3dVTPhysicalSlot_t ) == 32u, "t3dVTPhysicalSlot_t ABI changed" );
static_assert( offsetof( t3dVTPhysicalSlot_t, m_nFlags ) == 16u, "VT physical-slot flags ABI changed" );
static_assert( offsetof( t3dVTPhysicalSlot_t, m_nPayloadId ) == 24u, "VT physical-slot payload ABI changed" );

static_assert( sizeof( t3dVTRequest_t ) == 8u, "t3dVTRequest_t ABI changed" );
static_assert( offsetof( t3dVTRequest_t, m_nWantedMip ) == 4u, "VT request packed ABI changed" );

static_assert( sizeof( t3dRenderingMaterialVTBindings_t ) == 8u, "VT material binding ABI changed" );
static_assert( sizeof( t3dRenderingMaterialGpu_t ) == 64u, "VT material GPU ABI changed" );
static_assert( T3D_VT_MATERIAL_HAS_SURFACE_TEXTURE == ( 1u << 16u ), "VT surface flag ABI changed" );
static_assert( T3D_VT_MATERIAL_SURFACE_TEXTURE_KNOWN == ( 1u << 17u ), "VT surface provenance ABI changed" );

inline bool t3d_vt_material_has_surface_texture( const uint32_t flags )
{
	return ( flags & T3D_VT_MATERIAL_HAS_SURFACE_TEXTURE ) != 0u || ( flags & T3D_VT_MATERIAL_SURFACE_TEXTURE_KNOWN ) == 0u;
}
static_assert( offsetof( t3dRenderingMaterialGpu_t, m_nVtAllocation ) == 0u, "VT material allocation ABI changed" );
static_assert( offsetof( t3dRenderingMaterialGpu_t, m_vBaseColor ) == 16u, "VT material base-color ABI changed" );
static_assert( offsetof( t3dRenderingMaterialGpu_t, m_vUvScaleBias ) == 32u, "VT material UV ABI changed" );
static_assert( offsetof( t3dRenderingMaterialGpu_t, m_nPackedEmission ) == 48u, "VT material emission ABI changed" );
static_assert( offsetof( t3dRenderingMaterialGpu_t, m_nSamplerClass ) == 56u, "VT material sampler ABI changed" );

// P21/P22 shared request-hash contract. The table is fixed-capacity and owner
// controlled. Hash entries never allocate or chain; bounded probing either
// merges the request or records an observable overflow.
// Version 1 remains the active hash ABI; the new grid uses version 2.
// Raster shaders switch only once their own source and reflection are verified.
#define T3D_VT_REQUEST_HASH_EMPTY_KEY 0xffffffffu
#define T3D_VT_REQUEST_HASH_PROBE_COUNT 8u
#define T3D_VT_REQUEST_HASH_SLOTS_PER_REQUEST 2u
#define T3D_VT_REQUEST_RECT_PAGE_CAP 16u

#define T3D_VT_FRAGMENT_FEEDBACK_ENABLED ( 1u << 0u )
#define T3D_VT_REQUEST_COUNTER_WORDS 16u

#define T3D_VT_REQUEST_PRODUCER_VISIBLE ( 1u << 0u )
#define T3D_VT_REQUEST_PRODUCER_FUTURE ( 1u << 1u )
#define T3D_VT_REQUEST_PRODUCER_FRAGMENT ( 1u << 2u )

#define T3D_VT_PREDICTION_CATEGORY_CLUTTER 0u
#define T3D_VT_PREDICTION_CATEGORY_GROUND_COVER 1u
#define T3D_VT_PREDICTION_CATEGORY_OCCLUDER 2u
#define T3D_VT_PREDICTION_CATEGORY_TERRAIN 3u

// Counter words 12..15 form the generation/frame header consumed with the
// compact request list. Keeping it in the same fixed block makes WG-D readback
// independent from shader-private hash state.

// Descriptor-invariant raster feedback control block bound at set 3/binding 4.
// Capacities are owner-selected cold values; raster code never resizes either
// request structure. flags bit 0 enables the miss-repair producer.

// 16-byte open-addressing entry. wantedMip and priority intentionally occupy
// separate 32-bit atomic words: same-page merges independently retain the
// finest wanted mip (atomicMin) and highest priority (atomicMax) without 64-bit
// shader atomics. producerMask is diagnostic only.

// Immutable draw-slot prediction metadata. WG-C can publish this beside its
// final draw-slot table without changing the draw ABI. P21 consumes the table
// by draw slot after GPU visibility has produced the final indirect counts.

// Shared 64-byte dispatch packet. Reset/compact consume the first uvec4 only;
// predictors additionally use camera motion and immutable resource counts.

static_assert( sizeof( t3dVTRequestHashEntry_t ) == 16u, "VT request hash entry ABI changed" );
static_assert( sizeof( t3dVTRequestGpuCounters_t ) == 64u, "VT request counter/header ABI changed" );
static_assert( sizeof( t3dVTFragmentFeedbackParams_t ) == 32u, "VT fragment feedback parameter ABI changed" );
static_assert( offsetof( t3dVTFragmentFeedbackParams_t, m_nHashCapacity ) == 0u, "VT fragment hash-capacity offset changed" );
static_assert( offsetof( t3dVTFragmentFeedbackParams_t, m_nAllocationCount ) == 4u, "VT fragment allocation-count offset changed" );
static_assert( offsetof( t3dVTFragmentFeedbackParams_t, m_nPageCount ) == 8u, "VT fragment page-count offset changed" );
static_assert( offsetof( t3dVTFragmentFeedbackParams_t, m_nFlags ) == 12u, "VT fragment flags offset changed" );
static_assert( offsetof( t3dVTFragmentFeedbackParams_t, m_nPredictedTerrainMaterials ) == 16u, "VT predicted material mask ABI changed" );
static_assert( offsetof( t3dVTRequestGpuCounters_t, m_nFrameId ) == 48u, "VT readback frame header offset changed" );
static_assert( sizeof( t3dVTPredictionMetadata_t ) == 64u, "VT prediction metadata ABI changed" );
static_assert( offsetof( t3dVTPredictionMetadata_t, m_nBaseVtAllocation ) == 32u, "VT prediction allocation ABI changed" );
static_assert( offsetof( t3dVTPredictionMetadata_t, m_nDrawSlot ) == 44u, "VT prediction draw-slot ABI changed" );
static_assert( offsetof( t3dVTPredictionMetadata_t, m_fTexelDensity ) == 48u, "VT prediction density ABI changed" );
static_assert( offsetof( t3dVTPredictionMetadata_t, m_nGeometryLod ) == 56u, "VT prediction packed LOD ABI changed" );
static_assert( offsetof( t3dVTPredictionMetadata_t, m_nFlags ) == 60u, "VT prediction flags ABI changed" );
static_assert( sizeof( t3dVTPredictionDispatch_t ) == 64u, "VT prediction dispatch ABI changed" );

static_assert( T3D_VT_SAMPLE_HEIGHT_TAIL_BINDING == 78u, "VT sample descriptor layout changed" );
static_assert( T3D_VT_FRAGMENT_FEEDBACK_DESCRIPTOR_SET == 3u, "VT fragment feedback descriptor set changed" );
static_assert( T3D_VT_FRAGMENT_FEEDBACK_DESCRIPTOR_COUNT == 5u, "VT fragment feedback descriptor count changed" );

#include <stdint.h>

// Residency requires neutral backing plus room for admission and retirement.
// Budget planning and residency admission must agree on this runtime minimum.
inline constexpr uint32_t T3D_VT_MIN_PHYSICAL_PAGE_SETS = 3u;

#endif // TERRAIN3D_VT_CONTRACT_H
