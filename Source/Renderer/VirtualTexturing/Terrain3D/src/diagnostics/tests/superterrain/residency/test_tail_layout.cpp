#include "rendering/rd/vt/io/terrain_3d_vt_compact_tail_layout.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <set>
#include <utility>

static void testLayout( const uint32_t seed )
{
	uint32_t next[3]{};
	std::set<std::pair<uint32_t, uint32_t>> occupied[3];
	for ( uint32_t i = 0u; i < 3072u; ++i ) {
		const uint32_t family = ( i * 7u + seed ) % 3u;
		const bool compact = ( i + seed ) % 11u != 0u;
		uint32_t x = 0u, y = 0u;
		assert( t3dVtPlaceCompactTailCold( next[family], compact, 64u, 128u, 64u, x, y ) );
		const uint32_t width = compact ? 64u : 128u;
		assert( x % 64u == 0u && y % 64u == 0u && x + width <= 8192u );
		for ( uint32_t row = y; row < y + 64u; row += 4u ) {
			for ( uint32_t col = x; col < x + width; col += 4u ) {
				assert( occupied[family].insert( { col, row } ).second );
			}
		}
	}
}

int main( void )
{
	for ( uint32_t seed : { 0u, 1u, 7u, 27u } )
		testLayout( seed );
	uint32_t index = 0u;
	for ( uint32_t i = 0u; i < 1024u; ++i ) {
		uint32_t x = 0u, y = 0u;
		assert( t3dVtPlaceCompactTailCold( index, true, 64u, 128u, 64u, x, y ) );
		assert( x == ( ( i / 2u ) % 64u ) * 128u + ( i % 2u ) * 64u );
		assert( y == ( ( i / 2u ) / 64u ) * 64u );
	}
	assert( index == 1024u );
	uint32_t badX = 55u, badY = 66u;
	index = UINT32_MAX;
	assert( !t3dVtPlaceCompactTailCold( index, true, 64u, 128u, 64u, badX, badY ) );
	assert( index == UINT32_MAX && badX == 55u && badY == 66u );
	index = 1u;
	assert( !t3dVtPlaceCompactTailCold( index, false, 0u, 128u, 64u, badX, badY ) );
	assert( index == 1u );
	std::puts( "ST02 production compact-tail placement, mixed families, 1024 roots, overflow: PASS" );
}
