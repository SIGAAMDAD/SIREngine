// Compact COMPOSED V1 root placement. Cold package importer only.
// This helper is independent of Godot to support deterministic footprint tests.
#ifndef TERRAIN3D_VT_COMPACT_TAIL_LAYOUT_H
#define TERRAIN3D_VT_COMPACT_TAIL_LAYOUT_H

#include <stdint.h>
#include <limits.h>

static inline bool t3dVtPlaceCompactTailCold( uint32_t& rNextHalf, const bool bCompact, const uint32_t nSlotsPerRow,
                                              const uint32_t nSlotWidth, const uint32_t nSlotHeight, uint32_t& rAtlasX, uint32_t& rAtlasY )
{
	if ( nSlotsPerRow == 0u || nSlotWidth == 0u || ( nSlotWidth & 1u ) || nSlotHeight == 0u || nSlotWidth > UINT32_MAX / nSlotsPerRow ) {
		return false;
	}
	uint32_t next = rNextHalf;
	if ( !bCompact && ( next & 1u ) ) {
		if ( next == UINT32_MAX )
			return false;
		++next;
	}
	const uint32_t count = bCompact ? 1u : 2u;
	if ( next > UINT32_MAX - count )
		return false;
	const uint32_t halfIndex = next;
	const uint32_t fullSlot = halfIndex >> 1u;
	const uint64_t y = uint64_t( fullSlot / nSlotsPerRow ) * nSlotHeight;
	if ( y > uint64_t( UINT32_MAX - nSlotHeight ) )
		return false;
	rAtlasX = ( fullSlot % nSlotsPerRow ) * nSlotWidth + ( bCompact ? ( halfIndex & 1u ) * ( nSlotWidth >> 1u ) : 0u );
	rAtlasY = uint32_t( y );
	rNextHalf = next + count;
	return true;
}

#endif // TERRAIN3D_VT_COMPACT_TAIL_LAYOUT_H
