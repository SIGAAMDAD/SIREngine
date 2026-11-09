// Copyright 2026 Terrain3D contributors.
#ifndef TERRAIN3D_VT_ANCESTOR_HINT_H
#define TERRAIN3D_VT_ANCESTOR_HINT_H

#include "Terrain3DTypes.h"
#include "rendering/rd/vt/core/terrain_3d_vt_contract.h"
#include <stdint.h>

// Derived transient GPU metadata, never written into cooked VT packages.
// Record zero is the publication header; page P is stored at P + 1.
#define T3D_VT_HINT_ENABLED 0x54334831u
#define T3D_VT_HINT_FLAG_PHYSICAL 1u
#define T3D_VT_HINT_MAX_REBUILD_PER_FRAME 512u
#define T3D_VT_HINT_MAX_UPLOAD_RUNS_PER_FRAME 16u

typedef struct t3dVTAncestorHint_s
{
	uint32_t nResolvedPage;
	uint32_t nPhysicalSlot;
	uint32_t nAllocationGeneration;
	uint32_t nPackedMipDepthFlags;
} t3dVTAncestorHint_t;
static_assert( sizeof( t3dVTAncestorHint_t ) == 16u, "VT ancestor hint std430 ABI" );

static inline t3dVTAncestorHint_t t3d_vt_invalid_ancestor_hint()
{
	t3dVTAncestorHint_t hint{};
	hint.nResolvedPage = T3D_VT_INVALID_PAGE;
	hint.nPhysicalSlot = T3D_VT_INVALID_SLOT;
	return hint;
}

// Pure oracle: only return a physical resident ancestor within the original
// bounded walk. Tail/invalid cases deliberately use the production resolver.
static inline bool t3d_vt_build_ancestor_hint_reference( const t3dVTAllocation_t& allocation, const t3dVTPageTableEntry_t *pages,
                                                         uint32_t pageCount, uint32_t requestedPage, uint32_t requestedMip,
                                                         t3dVTAncestorHint_t& hint )
{
	hint = t3d_vt_invalid_ancestor_hint();
	if ( pages == NULL || allocation.m_nPageCount == 0u || allocation.m_nMipCount == 0u || allocation.m_nMipCount > T3D_VT_MAX_MIPS ||
	     requestedMip >= allocation.m_nMipCount || ( allocation.m_nFlags & T3D_VT_ALLOCATION_QUALITY_DISABLED ) != 0u ||
	     allocation.m_nBaseVirtualPage >= pageCount || allocation.m_nPageCount > pageCount - allocation.m_nBaseVirtualPage )
		return false;
	const uint32_t end = allocation.m_nBaseVirtualPage + allocation.m_nPageCount;
	if ( requestedPage < allocation.m_nBaseVirtualPage || requestedPage >= end )
		return false;
	const uint32_t cap = allocation.m_nMipCount - requestedMip < T3D_VT_ANCESTOR_PROBE_CAP ? allocation.m_nMipCount - requestedMip
	                                                                                       : T3D_VT_ANCESTOR_PROBE_CAP;
	uint32_t page = requestedPage;
	for ( uint32_t depth = 0u; depth < cap; ++depth ) {
		if ( page < allocation.m_nBaseVirtualPage || page >= end )
			return false;
		const t3dVTPageTableEntry_t& entry = pages[page];
		if ( ( entry.nFlags & T3D_VT_PAGE_VALID ) == 0u || entry.nGeneration != allocation.m_nGeneration )
			return false;
		if ( ( entry.nFlags & T3D_VT_PAGE_RESIDENT ) != 0u && entry.nPhysicalSlot != T3D_VT_INVALID_SLOT ) {
			hint.nResolvedPage = page;
			hint.nPhysicalSlot = entry.nPhysicalSlot;
			hint.nAllocationGeneration = allocation.m_nGeneration;
			hint.nPackedMipDepthFlags = ( requestedMip & 0xffu ) | ( ( uint32_t( entry.nResolvedMip ) & 0xffu ) << 8u ) |
			                            ( ( depth & 0xffu ) << 16u ) | ( T3D_VT_HINT_FLAG_PHYSICAL << 24u );
			return true;
		}
		if ( ( entry.nFlags & T3D_VT_PAGE_TAIL_FALLBACK ) != 0u || entry.nParentPage == T3D_VT_INVALID_PAGE )
			return false;
		page = entry.nParentPage;
	}
	return false;
}

#endif
