// Copyright (c) 2023-2026 Terrain3D contributors.
// IO06: optional precomputed material/mip demand. No allocations on query.
#ifndef TERRAIN3D_VT_SPATIAL_DEMAND_H
#define TERRAIN3D_VT_SPATIAL_DEMAND_H

#include <stdint.h>
#include <vector>
#include "authoring/vt/visibility/terrain_3d_vt_visibility_types.h"

#define T3D_VT_DEMAND_MAGIC 0x44563354u // T3VD, little endian
#define T3D_VT_DEMAND_VERSION 1u        // historical read-only v1
#define T3D_VT_DEMAND_STABLE_MATERIAL_FORMAT_VERSION 2u
#define T3D_VT_DEMAND_MAX_CELLS 4096u
#define T3D_VT_DEMAND_MAX_RECORDS_PER_CELL 64u
#define T3D_VT_DEMAND_MAX_RECORDS 65536u
#define T3D_VT_DEMAND_MRU_SLOTS 4u

typedef struct t3dVTDemandHeader_s
{
	uint32_t nMagic;
	uint32_t nVersion;
	uint32_t nCellMeters;
	uint32_t nCellCount;
	uint32_t nRecordCount;
	uint32_t nReserved;
	uint64_t nPackageSignature;
	uint64_t nPayloadHash;
} t3dVTDemandHeader_t;

typedef struct t3dVTDemandCell_s
{
	int32_t nX;
	int32_t nZ;
	uint32_t nFirstRecord;
	uint16_t nCount;
	uint16_t nReserved;
} t3dVTDemandCell_t;

typedef struct t3dVTDemandRecord_s
{
	uint32_t nAllocation;
	uint16_t nMip;
	uint16_t nCoverage; // 1..65535; highest coverage first within a cell
	uint16_t nUvMinX;
	uint16_t nUvMinY;
	uint16_t nUvMaxX;
	uint16_t nUvMaxY;
	uint8_t nViewMask; // 8 horizontal octants; 255 means any viewpoint
	uint8_t nReserved0;
	uint16_t nReserved1;
} t3dVTDemandRecord_t;

// Cooked v2 records use stable material identity, never a transient allocation index.
// Bit 0 of flags: partial/unknown static coverage; retain discovery fallback.
typedef struct t3dVTDemandStableMaterialHeader_s
{
	t3dVTDemandHeader_t Base;
	uint64_t nAssetIdentityHash;
	int32_t nOriginCellX;
	int32_t nOriginCellZ;
	uint32_t nRecordStride;
	uint32_t nFlags;
} t3dVTDemandStableMaterialHeader_t;
typedef struct t3dVTDemandStableMaterialRecord_s
{
	uint64_t nStableMaterialKey;
	t3dVTDemandRecord_t Footprint;
	uint32_t nFlags;
} t3dVTDemandStableMaterialRecord_t;
static_assert( sizeof( t3dVTDemandStableMaterialHeader_t ) == 64u, "Demand V2 header ABI changed" );
static_assert( sizeof( t3dVTDemandStableMaterialRecord_t ) == 32u, "Demand V2 record ABI changed" );

// Common deterministic serializer; authoring computes cells and stable keys from baked regions.
bool t3d_vt_demand_write_stable_material( const t3dVTDemandCell_t *pCells, uint32_t nCells,
                                          const t3dVTDemandStableMaterialRecord_t *pRecords, uint32_t nRecords, uint64_t nPackageSignature,
                                          uint64_t nAssetIdentityHash, std::vector<uint8_t>& rBytes );

static_assert( sizeof( t3dVTDemandHeader_t ) == 40u, "Demand file header ABI changed" );
static_assert( sizeof( t3dVTDemandCell_t ) == 16u, "Demand cell ABI changed" );
static_assert( sizeof( t3dVTDemandRecord_t ) == 20u, "Demand record ABI changed" );

uint64_t t3d_vt_demand_hash( const uint8_t *pBytes, uint64_t nBytes );

class Terrain3DVTSpatialDemand
{
public:
	bool loadCold( const uint8_t *pBytes, uint64_t nBytes );
	void resetCold();
	bool isValidFor( uint64_t nSignature ) const;
	bool bindStableKeysCold( const uint64_t *pKeys, const uint32_t *pRuntimeAllocations, uint32_t nCount );
	bool hasBoundPredictor() const;
	uint32_t getVersion() const;
	uint64_t getAssetIdentityHash() const;
	uint32_t getMappedRecordCount() const;
	bool hasCoveredCellHot( int32_t nX, int32_t nZ );
	uint32_t getCellMeters() const;
	uint32_t getCellCount() const;
	// Returns at most 64 records from the MRU. Never allocates or touches disk.
	const t3dVTDemandRecord_t *findCellHot( int32_t nX, int32_t nZ, uint32_t& rCount );

private:
	struct CachedCell
	{
		int32_t nX = 0;
		int32_t nZ = 0;
		uint32_t nCount = 0;
		uint64_t nLastAccess = 0u;
		t3dVTDemandRecord_t Records[T3D_VT_DEMAND_MAX_RECORDS_PER_CELL]{};
	};
	std::vector<uint8_t> m_bytes;
	std::vector<uint32_t> m_boundAllocations;
	uint32_t m_nHeaderBytes = 0u;
	uint32_t m_nRecordStride = 0u;
	uint32_t m_nMappedRecords = 0u;
	t3dVTDemandHeader_t m_header{};
	CachedCell m_cache[T3D_VT_DEMAND_MRU_SLOTS]{};
	uint64_t m_accessClock = 0u;
};
// T04: immutable, directory-only T3VD v1 index. Bundle bodies are never
// resident here; the worker reads/decompresses individual 4x4-cell groups.
struct t3dVTDemandDirectoryCell_t
{
	int32_t nX = 0, nZ = 0;
	uint32_t nBundle = 0u, nByteOffset = 0u, nCount = 0u, nFlags = 0u;
};
struct t3dVTDemandDirectoryBundle_t
{
	int32_t nX = 0, nZ = 0;
	uint64_t nOffset = 0u, nHash = 0u;
	uint32_t nEncodedBytes = 0u, nDecodedBytes = 0u, nCodec = 0u, nCount = 0u;
};
class Terrain3DVTDemandDirectory
{
public:
	// Accept only the header and directories, not the bundle payloads.
	bool loadCold( const uint8_t *pDirectory, uint64_t nDirectoryBytes, uint64_t nFileBytes, uint64_t nExpectedPackage,
	               uint64_t nExpectedAsset );
	void resetCold();
	bool findCellHot( int32_t nX, int32_t nZ, uint32_t& rCellIndex ) const;
	bool validFor( uint64_t nPackage ) const;
	const t3dVTDemandVisibilityCookConfig_t& config() const
	{
		return m_config;
	}
	const std::vector<t3dVTDemandDirectoryCell_t>& cells() const
	{
		return m_cells;
	}
	const std::vector<t3dVTDemandDirectoryBundle_t>& bundles() const
	{
		return m_bundles;
	}
	uint64_t fileBytes() const
	{
		return m_nFileBytes;
	}

private:
	t3dVTDemandVisibilityCookConfig_t m_config{};
	std::vector<t3dVTDemandDirectoryCell_t> m_cells;
	std::vector<t3dVTDemandDirectoryBundle_t> m_bundles;
	uint64_t m_nFileBytes = 0u;
};

// Worker-only decode. Each output buffer is preallocated at cold bind; no
// allocations or Godot resources are touched by the codec.
bool t3d_vt_demand_decode_bundle( const uint8_t *pEncoded, uint32_t nEncodedBytes, const t3dVTDemandDirectoryBundle_t& rBundle,
                                  uint8_t *pScratch, uint32_t nScratchBytes, t3dVTDemandBakedVisibilityRecord_t *pRecords,
                                  uint32_t nRecordCapacity );

// Nominal bake-to-runtime projection scaling. The caller handles per-allocation
// quality-base mip and physical page budget separately.
uint16_t t3d_vt_demand_select_mip( const t3dVTDemandBakedVisibilityRecord_t& rRecord, const t3dVTDemandVisibilityCookConfig_t& rConfig,
                                   uint32_t nRuntimeTextureSize, uint32_t nRuntimeViewHeight, float fRuntimeFovDegrees,
                                   uint32_t nRuntimeMipCount );

#endif
