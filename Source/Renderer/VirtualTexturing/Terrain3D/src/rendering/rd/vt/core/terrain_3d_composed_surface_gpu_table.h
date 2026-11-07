// CS05 independent, versioned composed-page GPU lookup. No Sparse VT V1 ABI changes.
#ifndef TERRAIN3D_COMPOSED_SURFACE_GPU_TABLE_H
#define TERRAIN3D_COMPOSED_SURFACE_GPU_TABLE_H

#include "rendering/rd/vt/core/terrain_3d_composed_surface_contract.h"
#include <stdint.h>
#include <string.h>

#define T3D_COMP_SURF_GPU_LOOKUP_PROBES 16u
#define T3D_COMP_SURF_GPU_MAP_EMPTY 0u
#define T3D_COMP_SURF_GPU_MAP_RESIDENT 1u
#define T3D_COMP_SURF_GPU_MAP_TOMBSTONE 2u

// GLSL std430 equivalent: 12 uint32 words, 48 bytes. World-page coordinates
// use the same signed floor(world_texel / 128) convention as the shader.
struct t3dComposedSurfaceGpuEntry_t
{
	int32_t m_nWorldPageX;
	int32_t m_nWorldPageY;
	uint32_t m_nMip;
	uint32_t m_nPhysicalSlot;
	uint32_t m_nGeneration;
	uint32_t m_nDeviceGeneration;
	uint32_t m_nRevisionLo;
	uint32_t m_nRevisionHi;
	uint32_t m_nState;
	uint32_t m_nChannelMask;
	uint32_t m_nReserved0;
	uint32_t m_nReserved1;
};
static_assert( sizeof( t3dComposedSurfaceGpuEntry_t ) == 48u, "CS05 GPU std430 ABI" );

inline uint32_t t3d_comp_surf_gpu_hash( const int32_t x, const int32_t y, const uint32_t mip )
{
	uint32_t h = uint32_t( x ) * 0x9e3779b9u;
	h ^= uint32_t( y ) * 0x85ebca6bu;
	h ^= mip * 0xc2b2ae35u;
	h ^= h >> 16u;
	h *= 0x7feb352du;
	h ^= h >> 15u;
	return h;
}

// Single render-owner writer. The external array must be allocated and
// accounted at a cold profile transition; no allocation in the hot path.
class Terrain3DComposedSurfaceGpuTable
{
public:
	bool initializeCold( t3dComposedSurfaceGpuEntry_t *pEntries, uint32_t capacity, uint32_t deviceGeneration )
	{
		if ( !pEntries || capacity < 64u || ( capacity & ( capacity - 1u ) ) || !deviceGeneration )
			return false;
		m_pEntries = pEntries;
		m_nCapacity = capacity;
		m_nDeviceGeneration = deviceGeneration;
		memset( m_pEntries, 0, sizeof( *m_pEntries ) * capacity );
		return true;
	}
	void resetCold( uint32_t deviceGeneration )
	{
		m_nDeviceGeneration = deviceGeneration;
		if ( m_pEntries )
			memset( m_pEntries, 0, sizeof( *m_pEntries ) * m_nCapacity );
	}
	uint32_t capacity() const
	{
		return m_nCapacity;
	}
	uint32_t deviceGeneration() const
	{
		return m_nDeviceGeneration;
	}
	const t3dComposedSurfaceGpuEntry_t *entries() const
	{
		return m_pEntries;
	}

	uint32_t lookup( int32_t x, int32_t y, uint32_t mip ) const
	{
		if ( !m_pEntries || !m_nDeviceGeneration )
			return T3D_COMP_SURF_INVALID_SLOT;
		const uint32_t start = t3d_comp_surf_gpu_hash( x, y, mip ) & ( m_nCapacity - 1u );
		for ( uint32_t i = 0; i < T3D_COMP_SURF_GPU_LOOKUP_PROBES; ++i ) {
			const uint32_t index = ( start + i ) & ( m_nCapacity - 1u );
			const t3dComposedSurfaceGpuEntry_t& entry = m_pEntries[index];
			if ( entry.m_nState == T3D_COMP_SURF_GPU_MAP_EMPTY )
				return T3D_COMP_SURF_INVALID_SLOT;
			if ( entry.m_nState == T3D_COMP_SURF_GPU_MAP_RESIDENT && entry.m_nWorldPageX == x && entry.m_nWorldPageY == y &&
			     entry.m_nMip == mip )
				return index;
		}
		return T3D_COMP_SURF_INVALID_SLOT;
	}
	bool stageResident( int32_t x, int32_t y, uint32_t mip, const t3dComposedSurfacePageMapping_t& mapping, uint32_t& rDirtyIndex,
	                    t3dComposedSurfaceGpuEntry_t *pPrevious = nullptr )
	{
		rDirtyIndex = T3D_COMP_SURF_INVALID_SLOT;
		if ( !m_pEntries || mip != 0u || !t3d_comp_surf_validate_mapping( mapping, m_nDeviceGeneration, mapping.m_nCompositionRevision ) )
			return false;
		const uint32_t start = t3d_comp_surf_gpu_hash( x, y, mip ) & ( m_nCapacity - 1u );
		uint32_t firstTombstone = T3D_COMP_SURF_INVALID_SLOT;
		for ( uint32_t i = 0; i < T3D_COMP_SURF_GPU_LOOKUP_PROBES; ++i ) {
			const uint32_t index = ( start + i ) & ( m_nCapacity - 1u );
			t3dComposedSurfaceGpuEntry_t& entry = m_pEntries[index];
			if ( entry.m_nState == T3D_COMP_SURF_GPU_MAP_RESIDENT && entry.m_nWorldPageX == x && entry.m_nWorldPageY == y &&
			     entry.m_nMip == mip ) {
				// Identity revision replacement must be explicitly retired first.
				return false;
			}
			if ( entry.m_nState == T3D_COMP_SURF_GPU_MAP_TOMBSTONE && firstTombstone == T3D_COMP_SURF_INVALID_SLOT )
				firstTombstone = index;
			if ( entry.m_nState == T3D_COMP_SURF_GPU_MAP_EMPTY ) {
				const uint32_t dest = firstTombstone != T3D_COMP_SURF_INVALID_SLOT ? firstTombstone : index;
				if ( pPrevious )
					*pPrevious = m_pEntries[dest];
				publishAt( dest, x, y, mip, mapping );
				rDirtyIndex = dest;
				return true;
			}
		}
		if ( firstTombstone == T3D_COMP_SURF_INVALID_SLOT )
			return false;
		if ( pPrevious )
			*pPrevious = m_pEntries[firstTombstone];
		publishAt( firstTombstone, x, y, mip, mapping );
		rDirtyIndex = firstTombstone;
		return true;
	}
	// Restore exactly the hash slot occupied before a failed GPU write.
	// Match the published entry so a delayed rollback cannot erase a newer
	// generation or a different insertion that reused this slot.
	bool rollbackStagedResident( uint32_t index, const t3dComposedSurfaceGpuEntry_t& staged, const t3dComposedSurfaceGpuEntry_t& previous )
	{
		if ( !m_pEntries || index >= m_nCapacity || staged.m_nState != T3D_COMP_SURF_GPU_MAP_RESIDENT ||
		     ( previous.m_nState != T3D_COMP_SURF_GPU_MAP_EMPTY && previous.m_nState != T3D_COMP_SURF_GPU_MAP_TOMBSTONE ) ||
		     memcmp( &m_pEntries[index], &staged, sizeof( staged ) ) != 0 )
			return false;
		m_pEntries[index] = previous;
		return true;
	}
	bool restoreRetirementAfterFailedWrite( uint32_t index, const t3dComposedSurfaceGpuEntry_t& previous )
	{
		if ( !m_pEntries || index >= m_nCapacity || m_pEntries[index].m_nState != T3D_COMP_SURF_GPU_MAP_TOMBSTONE ||
		     previous.m_nState != T3D_COMP_SURF_GPU_MAP_RESIDENT )
			return false;
		m_pEntries[index] = previous;
		return true;
	}
	bool retireSlot( uint32_t physicalSlot, uint32_t& rDirtyIndex )
	{
		rDirtyIndex = T3D_COMP_SURF_INVALID_SLOT;
		if ( !m_pEntries )
			return false;
		for ( uint32_t i = 0; i < m_nCapacity; ++i ) {
			t3dComposedSurfaceGpuEntry_t& entry = m_pEntries[i];
			if ( entry.m_nState == T3D_COMP_SURF_GPU_MAP_RESIDENT && entry.m_nPhysicalSlot == physicalSlot ) {
				entry.m_nState = T3D_COMP_SURF_GPU_MAP_TOMBSTONE;
				entry.m_nChannelMask = 0u;
				entry.m_nPhysicalSlot = T3D_COMP_SURF_INVALID_SLOT;
				rDirtyIndex = i;
				return true;
			}
		}
		return false;
	}

private:
	void publishAt( uint32_t index, int32_t x, int32_t y, uint32_t mip, const t3dComposedSurfacePageMapping_t& mapping )
	{
		t3dComposedSurfaceGpuEntry_t& entry = m_pEntries[index];
		entry = {};
		entry.m_nWorldPageX = x;
		entry.m_nWorldPageY = y;
		entry.m_nMip = mip;
		entry.m_nPhysicalSlot = mapping.m_nPhysicalSlot;
		entry.m_nGeneration = mapping.m_nGeneration;
		entry.m_nDeviceGeneration = mapping.m_nDeviceGeneration;
		entry.m_nRevisionLo = uint32_t( mapping.m_nCompositionRevision );
		entry.m_nRevisionHi = uint32_t( mapping.m_nCompositionRevision >> 32u );
		entry.m_nChannelMask = mapping.m_nResidentChannels;
		entry.m_nState = T3D_COMP_SURF_GPU_MAP_RESIDENT;
	}
	t3dComposedSurfaceGpuEntry_t *m_pEntries = nullptr;
	uint32_t m_nCapacity = 0u;
	uint32_t m_nDeviceGeneration = 0u;
};

#endif
