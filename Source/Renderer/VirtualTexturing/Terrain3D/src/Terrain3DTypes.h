// Copyright (c) 2023-2026 Cory Petkovsek, Roope Palmroos, and Terrain3D Contributors.

// Native-only selection of the donor Terrain3DTypes.h P1 POD/ABI contracts.

// Field lists, enum values and byte layouts are preserved verbatim from the donor.

#ifndef TERRAIN3D_VT_NATIVE_P1_TYPES_H

#define TERRAIN3D_VT_NATIVE_P1_TYPES_H

#include <stddef.h>

#include <stdint.h>

#include <string.h>

#include <limits.h>

#include <cstddef>

#define T3D_VT_CHANNEL_COUNT 5u

#define T3D_VT_LOGICAL_CHANNEL_COUNT 6u

#define T3D_VT_QUALITY_PROFILE_COUNT 6u

#define T3D_VT_MAX_MIPS 16u

#define T3D_VT_INVALID_ALLOCATION 0xffffffffu

#ifndef MIN

#define MIN( a, b ) ( ( a ) < ( b ) ? ( a ) : ( b ) )

#endif

#ifndef MAX

#define MAX( a, b ) ( ( a ) > ( b ) ? ( a ) : ( b ) )

#endif

#ifndef CLAMP

#define CLAMP( v, lo, hi ) ( MAX( ( lo ), MIN( ( v ), ( hi ) ) ) )

#endif

typedef enum t3dRenderingMaterialFlags_e : uint32_t
{
	T3D_VT_MATERIAL_ALPHA_SCISSOR = 1u << 0u,
	T3D_VT_MATERIAL_FOLIAGE = 1u << 1u,
	T3D_VT_MATERIAL_CAST_SHADOWS = 1u << 2u,
	T3D_VT_MATERIAL_RECEIVE_SHADOWS = 1u << 3u,
	T3D_VT_MATERIAL_TWO_SIDED = 1u << 4u,
	T3D_VT_MATERIAL_UNSHADED = 1u << 5u,
	T3D_VT_MATERIAL_VERTEX_COLOR = 1u << 6u,
	T3D_VT_MATERIAL_VERTEX_COLOR_SRGB = 1u << 7u,
	T3D_VT_MATERIAL_NORMAL_MAPPING = 1u << 8u,
	T3D_VT_MATERIAL_AO_ENABLED = 1u << 9u,
	T3D_VT_MATERIAL_HAS_HEIGHT = 1u << 10u,
	T3D_VT_MATERIAL_HAS_ALPHA = 1u << 11u,
	// Imported custom ShaderMaterial profile: ORM channels are authored as
	// absolute AO/roughness/metallic values rather than scalar multipliers.
	T3D_VT_MATERIAL_DIRECT_ORM = 1u << 12u,
	// m_nReserved low 16 bits hold albedo saturation in [0, 1.5].
	T3D_VT_MATERIAL_ALBEDO_SATURATION = 1u << 13u,
	// Known shared ShaderMaterial profile uses the authored stylized sun loop.
	T3D_VT_MATERIAL_STYLIZED_SUN = 1u << 14u,
	// VT-P06: material allocation is the base of a sparse V1 runtime channel triplet.
	T3D_VT_MATERIAL_LAYOUT = 1u << 15u,
	T3D_VT_MATERIAL_HAS_SURFACE_TEXTURE = 1u << 16u,
	// Old cooked records lack provenance and must retain surface sampling.
	T3D_VT_MATERIAL_SURFACE_TEXTURE_KNOWN = 1u << 17u,
} t3dRenderingMaterialFlags_t;

typedef enum t3dVTOuterCompression_e : uint32_t
{
	T3D_VT_OUTER_COMPRESSION_NONE = 0u,
	T3D_VT_OUTER_COMPRESSION_ZSTD = 1u,
} t3dVTOuterCompression_t;

typedef enum t3dVTCookWrapMode_e : uint32_t
{
	T3D_VT_COOK_WRAP_REPEAT = 0u,
	T3D_VT_COOK_WRAP_CLAMP = 1u,
} t3dVTCookWrapMode_t;

typedef enum t3dVTAllocationFlags_e : uint16_t
{
	T3D_VT_ALLOCATION_MATERIAL_UV = 1u << 0u,
	T3D_VT_ALLOCATION_MEGATEXTURE = 1u << 1u,
	T3D_VT_ALLOCATION_REPEAT = 1u << 2u,
	T3D_VT_ALLOCATION_CLAMP = 1u << 3u,
	T3D_VT_ALLOCATION_HAS_ALPHA = 1u << 4u,
	T3D_VT_ALLOCATION_HAS_HEIGHT = 1u << 5u,
	T3D_VT_ALLOCATION_PINNED = 1u << 6u,
	T3D_VT_ALLOCATION_SHIPPING_IMMUTABLE = 1u << 7u,
	T3D_VT_ALLOCATION_EDITOR_DYNAMIC = 1u << 8u,
	// VT-P07 runtime-only view selection. Serialized authoring flags keep these bits clear.
	T3D_VT_ALLOCATION_QUALITY_BASE_MIP_SHIFT = 9u,
	T3D_VT_ALLOCATION_QUALITY_BASE_MIP_MASK = 0x0fu << T3D_VT_ALLOCATION_QUALITY_BASE_MIP_SHIFT,
	T3D_VT_ALLOCATION_QUALITY_DISABLED = 1u << 13u,
	T3D_VT_ALLOCATION_ALBEDO_RGBA = 1u << 14u,
} t3dVTAllocationFlags_t;

typedef enum t3dVTPageFlags_e : uint16_t
{
	T3D_VT_PAGE_VALID = 1u << 0u,
	T3D_VT_PAGE_RESIDENT = 1u << 1u,
	T3D_VT_PAGE_PINNED = 1u << 2u,
	T3D_VT_PAGE_NEUTRAL = 1u << 3u,
	T3D_VT_PAGE_ROOT = 1u << 4u,
	T3D_VT_PAGE_TAIL_FALLBACK = 1u << 5u,
} t3dVTPageFlags_t;

typedef enum t3dVTChannel_e : uint32_t
{
	T3D_VT_CHANNEL_ALBEDO = 0u,
	T3D_VT_CHANNEL_NORMAL = 1u,
	T3D_VT_CHANNEL_SURFACE = 2u,
	T3D_VT_CHANNEL_HEIGHT = 3u,
	T3D_VT_CHANNEL_HEIGHT16 = 4u,
} t3dVTChannel_t;

typedef enum t3dVTChannelFormat_e : uint32_t
{
	T3D_VT_FORMAT_ALBEDO_BC1_SRGB = 0u,
	T3D_VT_FORMAT_NORMAL_BC5_UNORM = 1u,
	T3D_VT_FORMAT_SURFACE_BC7_UNORM = 2u,
	T3D_VT_FORMAT_HEIGHT_R8_UNORM = 3u,
} t3dVTChannelFormat_t;

typedef enum t3dVTLogicalChannel_e : uint32_t
{
	T3D_VT_LOGICAL_CHANNEL_ALBEDO = 0u,
	T3D_VT_LOGICAL_CHANNEL_PBR = 1u,
	T3D_VT_LOGICAL_CHANNEL_HEIGHT = 2u,
	T3D_VT_LOGICAL_CHANNEL_NORMAL = 3u,
	T3D_VT_LOGICAL_CHANNEL_ORM = 4u,
	T3D_VT_LOGICAL_CHANNEL_RESERVED_5 = 5u,
} t3dVTLogicalChannel_t;

typedef enum t3dVTLogicalChannelFormat_e : uint32_t
{
	T3D_VT_LOGICAL_FORMAT_ALBEDO_BC1_SRGB = 0u,
	T3D_VT_LOGICAL_FORMAT_PBR_BC7_UNORM = 1u,
	T3D_VT_LOGICAL_FORMAT_HEIGHT_R8_UNORM = 2u,
	T3D_VT_LOGICAL_FORMAT_NORMAL_BC5_UNORM = 3u,
	T3D_VT_LOGICAL_FORMAT_ORM_BC1_UNORM = 4u,
	T3D_VT_LOGICAL_FORMAT_RESERVED_5 = 5u,
} t3dVTLogicalChannelFormat_t;

typedef enum t3dVTPhysicalFormatFamily_e : uint32_t
{
	T3D_VT_PHYSICAL_FORMAT_BC1_UNORM = 0u,
	T3D_VT_PHYSICAL_FORMAT_BC7_UNORM = 1u,
	T3D_VT_PHYSICAL_FORMAT_BC5_UNORM = 2u,
	T3D_VT_PHYSICAL_FORMAT_R8_UNORM = 3u,
	T3D_VT_PHYSICAL_FORMAT_R16_UNORM = 4u,
	T3D_VT_PHYSICAL_FORMAT_COUNT = 5u,
} t3dVTPhysicalFormatFamily_t;

typedef enum t3dVTChannelSemantic_e : uint32_t
{
	T3D_VT_CHANNEL_SEMANTIC_SRGB_ALBEDO = 0u,
	T3D_VT_CHANNEL_SEMANTIC_TERRAIN_PBR = 1u,
	T3D_VT_CHANNEL_SEMANTIC_LINEAR_HEIGHT = 2u,
	T3D_VT_CHANNEL_SEMANTIC_NORMAL_XY = 3u,
	T3D_VT_CHANNEL_SEMANTIC_LINEAR_ORM = 4u,
	T3D_VT_CHANNEL_SEMANTIC_RESERVED_5 = 5u,
} t3dVTChannelSemantic_t;

typedef enum t3dVTConsumer_e : uint32_t
{
	T3D_VT_CONSUMER_CLIPMAP = 0u,
	T3D_VT_CONSUMER_INSTANCE = 1u,
	T3D_VT_CONSUMER_COMPOSED = 2u,
} t3dVTConsumer_t;

typedef enum t3dVTFixedProfile_e : uint32_t
{
	T3D_VT_FIXED_PROFILE_JUST_MAKE_IT_RUN = 0u,
	T3D_VT_FIXED_PROFILE_LOW = 1u,
	T3D_VT_FIXED_PROFILE_NORMAL = 2u,
	T3D_VT_FIXED_PROFILE_HIGH = 3u,
	T3D_VT_FIXED_PROFILE_VERY_HIGH = 4u,
	T3D_VT_FIXED_PROFILE_MAKE_MY_GPU_SCREAM = 5u,
	T3D_VT_FIXED_PROFILE_COUNT = T3D_VT_QUALITY_PROFILE_COUNT,
	T3D_VT_FIXED_PROFILE_INVALID = 0xffffffffu,
} t3dVTFixedProfile_t;

typedef enum t3dSurfaceTextureLayout_e
{
	SURFACE_TEXTURE_ORM = 0,
	SURFACE_TEXTURE_ROUGHNESS_R = 1,
	SURFACE_TEXTURE_ROUGHNESS_G = 2,
	SURFACE_TEXTURE_ROUGHNESS_B = 3,
	SURFACE_TEXTURE_ROUGHNESS_A = 4,
} t3dSurfaceTextureLayout_t;

#define T3D_VT_SURFACE_SOURCE_ORM SURFACE_TEXTURE_ORM

#define T3D_VT_SURFACE_SOURCE_ROUGHNESS_R SURFACE_TEXTURE_ROUGHNESS_R

#define T3D_VT_SURFACE_SOURCE_ROUGHNESS_G SURFACE_TEXTURE_ROUGHNESS_G

#define T3D_VT_SURFACE_SOURCE_ROUGHNESS_B SURFACE_TEXTURE_ROUGHNESS_B

#define T3D_VT_SURFACE_SOURCE_ROUGHNESS_A SURFACE_TEXTURE_ROUGHNESS_A

using t3dVTSurfaceSourceLayout_t = t3dSurfaceTextureLayout_t;

struct t3dVTEncodeTileRequest_s;

using t3dVTEncodeTileFn = bool ( * )( void *, const t3dVTEncodeTileRequest_s&, uint32_t& );

using t3dVTCookAllocateFn = void *( * )( void *, uint64_t, uint32_t );

using t3dVTCookFreeFn = void ( * )( void *, void * );

using t3dVTCookClusterBoundFn = uint64_t ( * )( void *, uint32_t, uint64_t );

using t3dVTCookClusterEncodeFn = bool ( * )( void *, uint32_t, const uint8_t *, uint32_t, uint8_t *, uint32_t, uint32_t& );

typedef struct t3dVTCookStagedPayload_s
{
	uint64_t m_nContentSignature;
	uint8_t *m_pBytes;
	uint32_t m_nBytes;
	uint32_t m_nFormat;
	uint32_t m_nFlags;
	uint32_t m_nChecksum;
} t3dVTCookStagedPayload_t;

typedef struct t3dVTCookRawTileCacheEntry_s
{
	uint64_t m_nHashA;
	uint64_t m_nHashB;
	uint32_t m_nRawBytes;
	uint32_t m_nFamily;
	uint32_t m_nChannel;
	uint32_t m_nPayloadId;
	uint32_t m_nFlags;
} t3dVTCookRawTileCacheEntry_t;

typedef struct t3dVTCookerSettings_s
{
	uint32_t m_nPageInterior;
	uint32_t m_nPageGutter;
	uint32_t m_nClusterTargetPages;
	uint32_t m_nClusterTargetBytes;
	uint32_t m_nOuterCompression;
	uint32_t m_nMipQuality;
	uint32_t m_nCompressionQuality;
} t3dVTCookerSettings_t;

typedef struct t3dVTEncodeTileRequest_s
{
	const uint8_t *m_pSourcePixels;
	uint32_t m_nSourceBytes;
	uint32_t m_nChannel;
	uint32_t m_nFormat;
	uint32_t m_nWidth;
	uint32_t m_nHeight;
	uint32_t m_nComponents;
	uint32_t m_nQuality;
	uint32_t m_nContractVersion;
	uint8_t m_nAlphaThreshold = 0u; // Zero disables cutoff-constrained encoding.
	uint8_t *m_pDestination;
	uint32_t m_nDestinationCapacity;
} t3dVTEncodeTileRequest_t;

typedef struct t3dVTEncoderInterface_s
{
	void *m_pUser;
	t3dVTEncodeTileFn m_pEncodeTile;
} t3dVTEncoderInterface_t;

typedef struct t3dVTCookAllocator_s
{
	void *m_pUser;
	t3dVTCookAllocateFn m_pAllocate;
	t3dVTCookFreeFn m_pFree;
} t3dVTCookAllocator_t;

typedef struct t3dVTCookClusterCodec_s
{
	void *m_pUser;
	t3dVTCookClusterBoundFn m_pBound;
	t3dVTCookClusterEncodeFn m_pEncode;
} t3dVTCookClusterCodec_t;

typedef struct t3dVTCookSparseStats_s
{
	uint32_t m_nAllocationCount;
	uint32_t m_nPageCount;
	uint32_t m_nTailCount;
	uint32_t m_nUniquePayloadCount;
	uint32_t m_nRawDedupHits;
	uint32_t m_nEncodeCalls;
	uint32_t m_nChannelPageCount[T3D_VT_LOGICAL_CHANNEL_COUNT];
	uint32_t m_nPhysicalFamilyPayloadCount[T3D_VT_PHYSICAL_FORMAT_COUNT];
	uint64_t m_nContentSignature;
} t3dVTCookSparseStats_t;

typedef struct t3dVTPackAllocationKeyRecord_s
{
	uint64_t m_nStableKey;
	uint32_t m_nAllocationId;
	uint32_t m_nContentId;
} t3dVTPackAllocationKeyRecord_t;

typedef struct t3dVTPackPayloadRecord_s
{
	uint64_t m_nContentSignature;
	uint32_t m_nFormat;
	uint32_t m_nClusterId;
	uint32_t m_nClusterOffset;
	uint32_t m_nStoredBytes;
	uint32_t m_nDecodedBytes;
	uint32_t m_nCodec;
	uint32_t m_nChecksum;
	uint32_t m_nFlags;
	uint32_t m_nReserved;
} t3dVTPackPayloadRecord_t;

typedef struct t3dVTPackClusterRecord_s
{
	uint64_t m_nPayloadDataOffset;
	uint64_t m_nContentSignature;
	uint32_t m_nStoredBytes;
	uint32_t m_nDecodedBytes;
	uint32_t m_nFirstPayload;
	uint32_t m_nPayloadCount;
	uint32_t m_nCodec;
	uint32_t m_nChecksum;
} t3dVTPackClusterRecord_t;

typedef struct t3dVTPackHeader_s
{
	uint32_t m_nMagic;
	uint32_t m_nVersion;
	uint32_t m_nHeaderBytes;
	uint32_t m_nFlags;
	uint32_t m_nAllocationCount;
	uint32_t m_nChannelAllocationCount;
	uint32_t m_nPageCount;
	uint32_t m_nPayloadCount;
	uint32_t m_nClusterCount;
	uint32_t m_nTailCount;
	uint32_t m_nChannelCount;
	uint32_t m_nInteriorTexels;
	uint32_t m_nGutterTexels;
	uint32_t m_nPhysicalTexels;
	uint32_t m_nPackageGeneration;
	uint32_t m_nHeaderChecksum;
	uint32_t m_nQualityViewCount;
	uint32_t m_nChannelPageCount[T3D_VT_LOGICAL_CHANNEL_COUNT];
	uint32_t m_nChannelTailCount[T3D_VT_LOGICAL_CHANNEL_COUNT];
	uint32_t m_nFormatFamilyCount;
	uint32_t m_nFullySupportedProfileMask;
	uint64_t m_nAllocationOffset;
	uint64_t m_nAllocationKeyOffset;
	uint64_t m_nChannelAllocationOffset;
	uint64_t m_nPageOffset;
	uint64_t m_nPayloadDirectoryOffset;
	uint64_t m_nClusterDirectoryOffset;
	uint64_t m_nTailMetadataOffset;
	uint64_t m_nQualityViewOffset;
	uint64_t m_nFormatFamilyOffset;
	uint64_t m_nPayloadDataOffset;
	uint64_t m_nPayloadDataBytes;
	uint64_t m_nContentSignature;
	uint64_t m_nPackageBytes;
	uint64_t m_nReserved[3];
} t3dVTPackHeader_t;

typedef struct t3dVTChannelAllocation_s
{
	uint32_t m_nBaseVirtualPage;
	uint32_t m_nPageCount;
	uint32_t m_nTailIndex;
	uint16_t m_nMip0WidthPages;
	uint16_t m_nMip0HeightPages;
	uint16_t m_nMipCount;
	uint16_t m_nFlags;
} t3dVTChannelAllocation_t;

typedef struct t3dVTPackAllocationRecord_s
{
	uint32_t m_nGeneration;
	uint32_t m_nContentId;
	uint16_t m_nFlags;
	uint16_t m_nChannelMask;
	uint8_t m_nHighestAvailableProfile;
	uint8_t m_nAvailableProfileMask;
	uint8_t m_nConsumer;
	uint8_t m_nReserved0;
} t3dVTPackAllocationRecord_t;

typedef struct t3dVTPackPageRecord_s
{
	uint32_t m_nVirtualPage;
	uint32_t m_nParentPage;
	uint32_t m_nAllocationId;
	uint32_t m_nPayloadId;
	uint32_t m_nGeneration;
	uint32_t m_nContentId;
	uint16_t m_nMip;
	uint16_t m_nFlags;
	uint16_t m_nPageX;
	uint16_t m_nPageY;
	uint16_t m_nChannel;
	uint16_t m_nReserved;
} t3dVTPackPageRecord_t;

typedef struct t3dVTPackTailRecord_s
{
	uint64_t m_nContentSignature;
	uint32_t m_nCanonicalAllocationId;
	uint32_t m_nPayloadId;
	uint32_t m_nContentId;
	uint16_t m_nFirstMip;
	uint16_t m_nMipCount;
	uint16_t m_nFirstMipWidth;
	uint16_t m_nFirstMipHeight;
	uint16_t m_nAtlasInteriorWidth;
	uint16_t m_nAtlasInteriorHeight;
	uint16_t m_nChannel;
	uint16_t m_nFlags;
	uint32_t m_nLayout;
} t3dVTPackTailRecord_t;

typedef struct t3dVTPackQualityView_s
{
	uint32_t m_nAllocationId;
	uint8_t m_nProfile;
	uint8_t m_nChannelMask;
	uint8_t m_nBaseMip[T3D_VT_LOGICAL_CHANNEL_COUNT];
	uint8_t m_nMaxMip[T3D_VT_LOGICAL_CHANNEL_COUNT];
} t3dVTPackQualityView_t;

typedef struct t3dVTPackFormatFamilyRecord_s
{
	uint8_t m_nLogicalChannel;
	uint8_t m_nPhysicalFamily;
	uint8_t m_nSemantic;
	uint8_t m_nFlags;
	uint32_t m_nReserved;
} t3dVTPackFormatFamilyRecord_t;

typedef struct t3dVTSerializedPackSource_s
{
	const t3dVTPackAllocationRecord_s *m_pAllocations;
	uint32_t m_nAllocationCount;
	const t3dVTPackAllocationKeyRecord_t *m_pAllocationKeys;
	const t3dVTChannelAllocation_s *m_pChannelAllocations;
	uint32_t m_nChannelAllocationCount;
	const t3dVTPackPageRecord_s *m_pPages;
	uint32_t m_nPageCount;
	const t3dVTPackPayloadRecord_t *m_pPayloads;
	uint32_t m_nPayloadCount;
	const t3dVTPackClusterRecord_t *m_pClusters;
	uint32_t m_nClusterCount;
	const t3dVTPackTailRecord_s *m_pTails;
	uint32_t m_nTailCount;
	const t3dVTPackQualityView_s *m_pQualityViews;
	uint32_t m_nQualityViewCount;
	const t3dVTPackFormatFamilyRecord_s *m_pFormatFamilies;
	uint32_t m_nFormatFamilyCount;
	const uint8_t *m_pPayloadData;
	uint64_t m_nPayloadDataBytes;
	uint32_t m_nPackageGeneration;
} t3dVTSerializedPackSource_t;

template <typename T>
struct t3dVTPackByteRecords_t
{
	const uint8_t *m_pBytes = nullptr;
	T operator[]( size_t nIndex ) const
	{
		T Record{};
		memcpy( &Record, m_pBytes + nIndex * sizeof( T ), sizeof( T ) );
		return Record;
	}
	bool operator==( std::nullptr_t ) const
	{
		return m_pBytes == nullptr;
	}
	bool operator!=( std::nullptr_t ) const
	{
		return m_pBytes != nullptr;
	}
};

struct t3dVTPackHeaderValue_t
{
	t3dVTPackHeader_t m_Value{};
	bool m_bValid = false;
	const t3dVTPackHeader_t *operator->() const
	{
		return &m_Value;
	}
	const t3dVTPackHeader_t& operator*() const
	{
		return m_Value;
	}
	bool operator==( std::nullptr_t ) const
	{
		return !m_bValid;
	}
	bool operator!=( std::nullptr_t ) const
	{
		return m_bValid;
	}
};

typedef struct t3dVTPackBaselineView_s
{
	t3dVTPackHeaderValue_t m_pHeader;
	t3dVTPackByteRecords_t<t3dVTPackAllocationRecord_t> m_pAllocations;
	t3dVTPackByteRecords_t<t3dVTPackAllocationKeyRecord_t> m_pAllocationKeys;
	t3dVTPackByteRecords_t<t3dVTChannelAllocation_t> m_pChannelAllocations;
	t3dVTPackByteRecords_t<t3dVTPackPageRecord_t> m_pPages;
	t3dVTPackByteRecords_t<t3dVTPackPayloadRecord_t> m_pPayloads;
	t3dVTPackByteRecords_t<t3dVTPackClusterRecord_t> m_pClusters;
	t3dVTPackByteRecords_t<t3dVTPackTailRecord_t> m_pTails;
	t3dVTPackByteRecords_t<t3dVTPackQualityView_t> m_pQualityViews;
	t3dVTPackByteRecords_t<t3dVTPackFormatFamilyRecord_t> m_pFormatFamilies;
	const uint8_t *m_pPayloadData = nullptr;
	uint32_t m_nAllocationCount = 0u;
	uint32_t m_nChannelAllocationCount = 0u;
	uint32_t m_nPageCount = 0u;
	uint32_t m_nPayloadCount = 0u;
	uint32_t m_nClusterCount = 0u;
	uint32_t m_nTailCount = 0u;
	uint32_t m_nQualityViewCount = 0u;
	uint32_t m_nFormatFamilyCount = 0u;
	uint64_t m_nPayloadDataBytes = 0u;
} t3dVTPackBaselineView_t;

typedef struct t3dVTCookImageView_s
{
	const uint8_t *m_pPixels;
	uint32_t m_nWidth;
	uint32_t m_nHeight;
	uint32_t m_nComponents;
	uint32_t m_nRowBytes;
} t3dVTCookImageView_t;

typedef struct t3dVTMipLevelLayout_s
{
	uint32_t m_nWidth;
	uint32_t m_nHeight;
	uint64_t m_nOffset;
	uint64_t m_nBytes;
} t3dVTMipLevelLayout_t;

typedef struct t3dVTReferenceBitWriter_s
{
	uint8_t *m_pBytes;
	uint32_t m_nBit;
} t3dVTReferenceBitWriter_t;

typedef struct t3dVTFixedProfileDesc_s
{
	uint32_t m_nExtent[T3D_VT_LOGICAL_CHANNEL_COUNT];
	uint32_t m_nSupportedChannelMask;
} t3dVTFixedProfileDesc_t;

typedef struct t3dVTAllocation_s
{
	uint32_t m_nBaseVirtualPage;
	uint32_t m_nPageCount;
	uint16_t m_nMip0WidthPages;
	uint16_t m_nMip0HeightPages;
	uint16_t m_nMipCount;
	uint16_t m_nFlags;
	uint32_t m_nTailIndex;
	uint32_t m_nGeneration;
	uint32_t m_nContentId;
	uint32_t m_nReserved;
} t3dVTAllocation_t;

typedef struct t3dVTPageEntry_s
{
	uint32_t m_nPhysicalSlot;
	uint32_t m_nParentPage;
	uint32_t m_nGeneration;
	uint16_t m_nResolvedMip;
	uint16_t m_nFlags;
} t3dVTPageEntry_t;

typedef struct t3dVTMipLayout_s
{
	uint32_t m_nBaseVirtualPage;
	uint16_t m_nWidthPages;
	uint16_t m_nHeightPages;
} t3dVTMipLayout_t;

typedef struct t3dVTTailMetadata_s
{
	uint32_t m_nAtlasX;
	uint32_t m_nAtlasY;
	uint16_t m_nWidthTexels;
	uint16_t m_nHeightTexels;
	uint16_t m_nFirstMip;
	uint16_t m_nMipCount;
	uint32_t m_nGeneration;
	uint32_t m_nChannelMask;
	uint32_t m_nContentId;
	uint32_t m_nFlags;
} t3dVTTailMetadata_t;

typedef struct t3dVTRequestHashEntry_s
{
	uint32_t m_nVirtualPage;
	uint32_t m_nWantedMip;
	uint32_t m_nPriority;
	uint32_t m_nProducerMask;
} t3dVTRequestHashEntry_t;

typedef struct t3dVTRequestGpuCounters_s
{
	uint32_t m_nCompactedRequestCount;
	uint32_t m_nRawAttemptCount;
	uint32_t m_nUniqueAcceptedCount;
	uint32_t m_nSameKeyMergeCount;
	uint32_t m_nProbeOverflowCount;
	uint32_t m_nRequestListOverflowCount;
	uint32_t m_nVisiblePredictionCount;
	uint32_t m_nFuturePredictionCount;
	uint32_t m_nMipCoarsenCount;
	uint32_t m_nBaseAllocationCount;
	uint32_t m_nOverlayAllocationCount;
	uint32_t m_nFragmentMissCount;
	uint32_t m_nFrameId;
	uint32_t m_nGeneration;
	uint32_t m_nHashCapacity;
	uint32_t m_nRequestCapacity;
} t3dVTRequestGpuCounters_t;

typedef struct t3dVTFragmentFeedbackParams_s
{
	uint32_t m_nHashCapacity;
	uint32_t m_nAllocationCount;
	uint32_t m_nPageCount;
	uint32_t m_nFlags;
	uint32_t m_nPredictedTerrainMaterials; // one bit per material layer, never a runtime allocation ID
	uint32_t m_nReserved0;
	uint32_t m_nReserved1;
	uint32_t m_nReserved2;
} t3dVTFragmentFeedbackParams_t;

typedef struct t3dVTPredictionMetadata_s
{
	float m_vWorldSphere[4];
	float m_vUvBounds[4];
	uint32_t m_nBaseVtAllocation;
	uint32_t m_nOverlayVtAllocation;
	uint32_t m_nMaterialId;
	uint32_t m_nDrawSlot;
	float m_fTexelDensity;
	float m_fObjectScale;
	uint16_t m_nGeometryLod;
	uint8_t m_nCategory;
	int8_t m_nMipBias;
	uint32_t m_nFlags;
} t3dVTPredictionMetadata_t;

typedef struct t3dVTPredictionDispatch_s
{
	uint32_t m_nHashCapacity;
	uint32_t m_nRequestCapacity;
	uint32_t m_nFrameId;
	uint32_t m_nGeneration;
	float m_vCameraLinearVelocity[3];
	float m_fPredictionHorizon;
	float m_fCameraAngularSpeed;
	float m_fViewportWidth;
	float m_fViewportHeight;
	float m_fFuturePriorityScale;
	uint32_t m_nDrawSlotCount;
	uint32_t m_nAllocationCount;
	uint32_t m_nPageCount;
	uint32_t m_nFlags;
} t3dVTPredictionDispatch_t;

typedef struct t3dVTResolveResult_s
{
	uint32_t m_nAllocation;
	uint32_t m_nRequestedPage;
	uint32_t m_nResolvedPage;
	uint32_t m_nPhysicalSlot;
	uint32_t m_nTailIndex;
	uint16_t m_nRequestedMip;
	uint16_t m_nResolvedMip;
	uint16_t m_nFallbackDepth;
	uint16_t m_nFlags;
	uint32_t m_nReserved;
} t3dVTResolveResult_t;

typedef struct t3dVTPhysicalSlot_s
{
	uint32_t m_nVirtualPage;
	uint32_t m_nGeneration;
	uint32_t m_nLastVisibleFrame;
	uint32_t m_nLastRequestedFrame;
	uint16_t m_nFlags;
	uint16_t m_nClockAge;
	uint16_t m_nPinCount;
	uint16_t m_nRetireFrame;
	uint32_t m_nPayloadId;
	uint32_t m_nReserved;
} t3dVTPhysicalSlot_t;

typedef struct t3dVTRequest_s
{
	uint32_t m_nVirtualPage;
	uint16_t m_nWantedMip;
	uint16_t m_nPriority;
} t3dVTRequest_t;

typedef struct t3dRenderingMaterialVTBindings_s
{
	uint32_t m_nVtAllocation;
	uint32_t m_nOverlayVtAllocation;
} t3dRenderingMaterialVTBindings_t;

typedef struct t3dRenderingMaterialGpu_s
{
	uint32_t m_nVtAllocation;
	uint32_t m_nOverlayVtAllocation;
	uint32_t m_nFlags;
	uint32_t m_nPackedScalars0;
	float m_vBaseColor[4];
	float m_vUvScaleBias[4];
	uint32_t m_nPackedEmission;
	uint32_t m_nPackedScalars1;
	uint32_t m_nSamplerClass;
	uint32_t m_nReserved;
} t3dRenderingMaterialGpu_t;

typedef struct t3dVTMipChainLayout_s
{
	t3dVTMipLevelLayout_t m_Levels[T3D_VT_MAX_MIPS];
	uint32_t m_nMipCount;
	uint32_t m_nComponents;
	uint64_t m_nTotalBytes;
} t3dVTMipChainLayout_t;

typedef struct t3dVTCookPreparedChannel_s
{
	const uint8_t *m_pMipBytes;
	t3dVTMipChainLayout_t m_Layout;
	uint32_t m_nTargetExtent;
	uint32_t m_nReserved0;
} t3dVTCookPreparedChannel_t;

typedef struct t3dVTCookedSparsePackage_s
{
	t3dVTCookAllocator_t m_Allocator;
	t3dVTPackAllocationRecord_t *m_pAllocations;
	t3dVTPackAllocationKeyRecord_t *m_pAllocationKeys;
	t3dVTChannelAllocation_t *m_pChannelAllocations;
	t3dVTPackPageRecord_t *m_pPages;
	t3dVTPackPayloadRecord_t *m_pPayloads;
	t3dVTPackClusterRecord_t *m_pClusters;
	t3dVTPackTailRecord_t *m_pTails;
	t3dVTPackQualityView_t *m_pQualityViews;
	uint8_t *m_pPayloadData;
	t3dVTPackFormatFamilyRecord_t m_FormatFamilies[T3D_VT_LOGICAL_CHANNEL_COUNT];
	uint32_t m_nAllocationCount;
	uint32_t m_nChannelAllocationCount;
	uint32_t m_nPageCount;
	uint32_t m_nPayloadCount;
	uint32_t m_nClusterCount;
	uint32_t m_nTailCount;
	uint32_t m_nQualityViewCount;
	uint32_t m_nFullySupportedProfileMask;
	uint32_t m_nPackageGeneration;
	uint64_t m_nPayloadDataBytes;
	t3dVTCookSparseStats_t m_Stats;
} t3dVTCookedSparsePackage_t;

typedef struct t3dVTCookSparseSource_s
{
	uint64_t m_nStableKey;
	t3dVTCookImageView_t m_Channels[T3D_VT_LOGICAL_CHANNEL_COUNT];
	uint64_t m_nSourceFingerprint[T3D_VT_LOGICAL_CHANNEL_COUNT];
	t3dVTCookPreparedChannel_t m_PreparedChannels[T3D_VT_LOGICAL_CHANNEL_COUNT];
	uint32_t m_nAuthoredChannelMask;
	uint32_t m_nOrmSourceLayout;
	uint32_t m_nWrapMode;
	uint32_t m_nConsumer;
	uint16_t m_nAllocationFlags;
	uint8_t m_bAlphaScissor;
	uint8_t m_nAlphaThreshold;
	uint32_t m_nGenerationSeed;
} t3dVTCookSparseSource_t;

// Original Terrain3D runtime pack POD/allocator contracts. See donor Terrain3DTypes.h.
#define T3D_VT_RESIDENCY_TERRAIN 0u
#define T3D_VT_RUNTIME_DATABASE_PACK 2u
typedef void *( *t3dVTPackRuntimeAllocateFn_t )( void *pUser, size_t nBytes, size_t nAlignment );
typedef void ( *t3dVTPackRuntimeFreeFn_t )( void *pUser, void *pMemory );
typedef struct t3dVTPackRuntimeAllocator_s
{
	void *pUser;
	t3dVTPackRuntimeAllocateFn_t pAllocate;
	t3dVTPackRuntimeFreeFn_t pFree;
} t3dVTPackRuntimeAllocator_t;

typedef struct t3dVTQualityRuntimeStats_s
{
	uint32_t nRequestedProfile;
	uint32_t nPackageFullySupportedProfileMask;
	uint32_t nFallbackAllocationCount;
	uint32_t nAllocationsByHighestProfile[T3D_VT_QUALITY_PROFILE_COUNT];
	uint32_t nAllocationsByResolvedProfile[T3D_VT_QUALITY_PROFILE_COUNT];
} t3dVTQualityRuntimeStats_t;

typedef struct t3dVTFileHeader_s
{
	uint32_t nMagic;
	uint32_t nVersion;
	uint32_t nHeaderBytes;
	uint32_t nFlags;
	uint32_t nAllocationCount;
	uint32_t nPageCount;
	uint32_t nPayloadCount;
	uint32_t nChannelCount;
	uint64_t nAllocationOffset;
	uint64_t nPageOffset;
	uint64_t nPayloadDirectoryOffset;
	uint64_t nPayloadDataOffset;
	uint64_t nPayloadDataBytes;
	uint64_t nContentSignature;
	uint32_t nInteriorTexels;
	uint32_t nGutterTexels;
	uint32_t nPhysicalTexels;
	uint32_t nReserved0;
	uint64_t nReserved[4];
} t3dVTFileHeader_t;

typedef struct t3dVTPageFileRecord_s
{
	uint32_t nVirtualPage;
	uint32_t nParentPage;
	uint32_t nAllocationId;
	uint32_t nPayloadId;
	uint16_t nMip;
	uint16_t nChannel;
	uint16_t nFlags;
	uint16_t nReserved0;
	uint32_t nGeneration;
	uint32_t nReserved1;
} t3dVTPageFileRecord_t;

typedef struct t3dVTPayloadRecord_s
{
	uint64_t nOffset;
	uint32_t nCompressedBytes;
	uint32_t nDecodedBytes;
	uint32_t nCodec;
	uint32_t nChecksum;
	uint32_t nFlags;
	uint32_t nReserved;
} t3dVTPayloadRecord_t;

typedef struct t3dVTRuntimePayloadRecord_s
{
	uint64_t nOffset;
	uint32_t nBytes;
	uint32_t nChannel;
	uint32_t nContentId;
	uint32_t nChecksum;
} t3dVTRuntimePayloadRecord_t;

typedef struct t3dVTRuntimeTailSourceRecord_s
{
	uint32_t nPayloadId[T3D_VT_CHANNEL_COUNT];
	uint16_t nAtlasInteriorWidth;
	uint16_t nAtlasInteriorHeight;
} t3dVTRuntimeTailSourceRecord_t;

typedef struct t3dVTAllocationRecord_s
{
	uint32_t nBaseVirtualPage;
	uint32_t nPageCount;
	uint32_t nMip0WidthPages;
	uint32_t nMip0HeightPages;
	uint16_t nMipCount;
	uint16_t nChannel;
	uint16_t nFlags;
	uint16_t nReserved0;
	uint32_t nRootPage;
	uint32_t nReserved1;
} t3dVTAllocationRecord_t;

typedef struct t3dVTPackSource_s
{
	const t3dVTAllocationRecord_s *pAllocations;
	uint32_t nAllocationCount;
	const t3dVTPageFileRecord_t *pPages;
	uint32_t nPageCount;
	const t3dVTPayloadRecord_t *pPayloads;
	uint32_t nPayloadCount;
	const uint8_t *pPayloadData;
	uint64_t nPayloadDataBytes;
} t3dVTPackSource_t;

struct t3dVTFileSource_s;
typedef struct t3dVTDatabaseView_s
{
	const t3dVTFileSource_s *pFileSource;
	const t3dVTFileHeader_t *pHeader;
	const t3dVTAllocationRecord_s *pAllocations;
	const t3dVTPageFileRecord_t *pPages;
	const t3dVTPayloadRecord_t *pPayloads;
	const uint8_t *pPayloadData;
	uint64_t nFileBytes;
	const t3dVTAllocation_t *pRuntimeAllocations;
	const t3dVTTailMetadata_t *pRuntimeTailMetadata;
	const t3dVTRuntimeTailSourceRecord_t *pRuntimeTailSources;
	const t3dVTRuntimePayloadRecord_t *pRuntimePayloads;
	uint32_t nRuntimePayloadCount;
	uint32_t nRuntimeTailCount;
	t3dVTPackByteRecords_t<t3dVTPackAllocationRecord_t> pV1Allocations;
	t3dVTPackByteRecords_t<t3dVTChannelAllocation_t> pV1ChannelAllocations;
	t3dVTPackByteRecords_t<t3dVTPackPageRecord_t> pV1Pages;
	t3dVTPackByteRecords_t<t3dVTPackTailRecord_t> pV1Tails;
	t3dVTPackByteRecords_t<t3dVTPackQualityView_t> pV1QualityViews;
	t3dVTPackByteRecords_t<t3dVTPackFormatFamilyRecord_t> pV1FormatFamilies;
	uint32_t nV1ChannelAllocationCount;
	uint32_t nV1PageCount;
	uint32_t nV1TailCount;
	uint32_t nV1QualityViewCount;
	uint32_t nV1FormatFamilyCount;
	uint32_t nV1FullySupportedProfileMask;
	uint32_t nPackageGeneration;
	uint32_t nRuntimeKind;
	uint64_t nPackageContentSignature;
} t3dVTDatabaseView_t;

typedef struct t3dVTPackRuntimeStorage_s
{
	t3dVTFileSource_s *pFileSource;
	void *pOwnedBlock;
	uint64_t nOwnedBytes;
	uint8_t *pPackageBytes;
	uint64_t nPackageBytes;
	t3dVTPackBaselineView_t PackViewV1;
	t3dVTDatabaseView_t Database;
	t3dVTFileHeader_t *pRuntimeHeader;
	t3dVTAllocationRecord_t *pLegacyAllocations;
	t3dVTPageFileRecord_t *pLegacyPages;
	t3dVTPayloadRecord_t *pLegacyPayloads;
	t3dVTAllocation_t *pRuntimeAllocations;
	t3dVTRuntimePayloadRecord_t *pRuntimePayloads;
	t3dVTTailMetadata_t *pRuntimeTails;
	t3dVTRuntimeTailSourceRecord_t *pRuntimeTailSources;
} t3dVTPackRuntimeStorage_t;

#endif // TERRAIN3D_VT_NATIVE_P1_TYPES_H
