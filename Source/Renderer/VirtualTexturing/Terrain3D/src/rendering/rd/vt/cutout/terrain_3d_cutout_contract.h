// Copyright 2026 Terrain3D contributors.
#ifndef TERRAIN3D_CUTOUT_CONTRACT_H
#define TERRAIN3D_CUTOUT_CONTRACT_H

#include <stdint.h>
#include <stddef.h>

#define T3D_ST_CUTOUT_SIDE 128u
#define T3D_ST_CUTOUT_PAGE_SIDE 64u
#define T3D_ST_CUTOUT_PAGES_PER_REGION 4u
#define T3D_ST_CUTOUT_PLANE_WORDS 128u
#define T3D_ST_CUTOUT_PLANES 3u
#define T3D_ST_CUTOUT_PAGE_WORDS 384u
#define T3D_ST_CUTOUT_PAGE_BYTES 1536u
#define T3D_ST_CUTOUT_ENTRY_BYTES 16u
#define T3D_ST_CUTOUT_ENTRIES_BYTES 64u
#define T3D_ST_CUTOUT_MAX_BYTES ( 1024u * 4u * T3D_ST_CUTOUT_PAGE_BYTES )
#define T3D_ST_CUTOUT_INVALID 0xffffffffu
#define T3D_ST_CUTOUT_VALID 0x40000000u
#define T3D_ST_CUTOUT_UNIFORM 0x80000000u
#define T3D_ST_CUTOUT_HOLE_BIT 0x4u

struct t3dStCutoutEntry_t
{
	uint32_t m_nPhysicalSlot;
	uint32_t m_nPinnedWordOffset;
	uint32_t m_nGeneration;
	uint32_t m_nPackedState;
};
static_assert( sizeof( t3dStCutoutEntry_t ) == T3D_ST_CUTOUT_ENTRY_BYTES, "ST03 frozen entry" );

static inline uint8_t t3dStCutoutSourceValue( const uint32_t nControl, const uint8_t nAlpha )
{
	const uint32_t alpha2 = ( uint32_t( nAlpha ) * 3u + 127u ) / 255u;
	return uint8_t( uint32_t( ( nControl & T3D_ST_CUTOUT_HOLE_BIT ) == 0u ) | ( alpha2 << 1u ) );
}
static inline uint32_t t3dStCutoutRegionPageId( const uint32_t nRegion, const uint32_t nPageX, const uint32_t nPageY )
{
	return nRegion < 1024u && nPageX < 2u && nPageY < 2u ? nRegion * T3D_ST_CUTOUT_PAGES_PER_REGION + nPageY * 2u + nPageX
	                                                     : T3D_ST_CUTOUT_INVALID;
}
#endif // TERRAIN3D_CUTOUT_CONTRACT_H
