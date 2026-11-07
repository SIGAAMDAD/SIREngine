#include "authoring/vt/build/terrain_3d_composed_surface_reference_decode.h"
#include "rendering/rd/vt/core/terrain_3d_vt_contract.h"
#include "data/formats/terrain_3d_vt_pack_format.h"

#include <cstring>

static uint16_t t3d_comp_surf_u16( const uint8_t *p )
{
	return uint16_t( uint16_t( p[0] ) | ( uint16_t( p[1] ) << 8u ) );
}

static void t3d_comp_surf_rgb565( uint16_t color, uint8_t *out )
{
	const uint32_t r = ( color >> 11u ) & 31u;
	const uint32_t g = ( color >> 5u ) & 63u;
	const uint32_t b = color & 31u;
	out[0] = uint8_t( ( r << 3u ) | ( r >> 2u ) );
	out[1] = uint8_t( ( g << 2u ) | ( g >> 4u ) );
	out[2] = uint8_t( ( b << 3u ) | ( b >> 2u ) );
}

static void t3d_comp_surf_bc1( const uint8_t *src, uint8_t *dst, uint32_t stride )
{
	uint8_t palette[4][4]{};
	const uint16_t a = t3d_comp_surf_u16( src );
	const uint16_t b = t3d_comp_surf_u16( src + 2u );
	t3d_comp_surf_rgb565( a, palette[0] );
	t3d_comp_surf_rgb565( b, palette[1] );
	palette[0][3] = palette[1][3] = 255u;
	for ( uint32_t c = 0u; c < 3u; ++c ) {
		if ( a > b ) {
			palette[2][c] = uint8_t( ( 2u * palette[0][c] + palette[1][c] ) / 3u );
			palette[3][c] = uint8_t( ( palette[0][c] + 2u * palette[1][c] ) / 3u );
		} else {
			palette[2][c] = uint8_t( ( palette[0][c] + palette[1][c] ) / 2u );
		}
	}
	palette[2][3] = 255u;
	palette[3][3] = a > b ? 255u : 0u;
	const uint32_t bits = uint32_t( src[4] ) | ( uint32_t( src[5] ) << 8u ) | ( uint32_t( src[6] ) << 16u ) | ( uint32_t( src[7] ) << 24u );
	for ( uint32_t y = 0u; y < 4u; ++y ) {
		for ( uint32_t x = 0u; x < 4u; ++x ) {
			const uint32_t index = ( bits >> ( 2u * ( 4u * y + x ) ) ) & 3u;
			std::memcpy( dst + y * stride + x * 4u, palette[index], 4u );
		}
	}
}

static void t3d_comp_surf_bc4( const uint8_t *src, uint8_t *dst, uint32_t stride )
{
	uint8_t palette[8]{};
	palette[0] = src[0];
	palette[1] = src[1];
	if ( src[0] > src[1] ) {
		for ( uint32_t i = 2u; i < 8u; ++i )
			palette[i] = uint8_t( ( ( 8u - i ) * src[0] + ( i - 1u ) * src[1] ) / 7u );
	} else {
		for ( uint32_t i = 2u; i < 6u; ++i )
			palette[i] = uint8_t( ( ( 6u - i ) * src[0] + ( i - 1u ) * src[1] ) / 5u );
		palette[6] = 0u;
		palette[7] = 255u;
	}
	uint64_t bits = 0u;
	for ( uint32_t i = 0u; i < 6u; ++i )
		bits |= uint64_t( src[2u + i] ) << ( 8u * i );
	for ( uint32_t y = 0u; y < 4u; ++y ) {
		for ( uint32_t x = 0u; x < 4u; ++x ) {
			const uint32_t pixel = 4u * y + x;
			dst[y * stride + x * 4u] = palette[( bits >> ( 3u * pixel ) ) & 7u];
		}
	}
}

// The shipping V1 reference BC7 encoder writes mode 6, with one RGBA
// partition, per-endpoint p-bits and a three-bit anchor index at texel zero.
// Parse LSB-first as written by t3d_vt_reference_encode_bc7_mode6_block().
static uint32_t t3d_comp_surf_read_bits( const uint8_t *src, uint32_t& position, uint32_t count )
{
	uint32_t value = 0u;
	for ( uint32_t i = 0u; i < count; ++i, ++position ) {
		value |= uint32_t( ( src[position >> 3u] >> ( position & 7u ) ) & 1u ) << i;
	}
	return value;
}

static bool t3d_comp_surf_decode_bc7_mode6( const uint8_t *src, uint8_t *dst, uint32_t stride )
{
	static const uint8_t weights[16] = { 0u, 4u, 9u, 13u, 17u, 21u, 26u, 30u, 34u, 38u, 43u, 47u, 51u, 55u, 60u, 64u };
	uint32_t position = 0u;
	if ( t3d_comp_surf_read_bits( src, position, 7u ) != 0x40u )
		return false;
	uint32_t endpoints[2][4]{};
	for ( uint32_t c = 0u; c < 4u; ++c ) {
		endpoints[0][c] = t3d_comp_surf_read_bits( src, position, 7u );
		endpoints[1][c] = t3d_comp_surf_read_bits( src, position, 7u );
	}
	const uint32_t p0 = t3d_comp_surf_read_bits( src, position, 1u );
	const uint32_t p1 = t3d_comp_surf_read_bits( src, position, 1u );
	for ( uint32_t c = 0u; c < 4u; ++c ) {
		endpoints[0][c] = ( endpoints[0][c] << 1u ) | p0;
		endpoints[1][c] = ( endpoints[1][c] << 1u ) | p1;
	}
	for ( uint32_t i = 0u; i < 16u; ++i ) {
		const uint32_t index = t3d_comp_surf_read_bits( src, position, i == 0u ? 3u : 4u );
		const uint32_t weight = weights[index];
		uint8_t *const pixel = dst + ( i / 4u ) * stride + ( i % 4u ) * 4u;
		for ( uint32_t c = 0u; c < 4u; ++c ) {
			pixel[c] = uint8_t( ( ( 64u - weight ) * endpoints[0][c] + weight * endpoints[1][c] + 32u ) >> 6u );
		}
	}
	return position == 128u;
}

bool t3d_comp_surf_decode_reference_tile( uint32_t family, const uint8_t *src, uint32_t bytes, uint8_t *dst, uint32_t capacity )
{
	const uint32_t width = T3D_VT_PHYSICAL_TILE;
	const uint32_t expected_output = width * width * 4u;
	if ( !src || !dst || capacity != expected_output || bytes != t3d_vt_pack_expected_physical_tile_bytes( family ) )
		return false;
	if ( family == T3D_VT_PHYSICAL_FORMAT_R8_UNORM ) {
		for ( uint32_t i = 0u; i < width * width; ++i ) {
			dst[4u * i] = src[i];
			dst[4u * i + 1u] = dst[4u * i + 2u] = 0u;
			dst[4u * i + 3u] = 255u;
		}
		return true;
	}
	if ( family != T3D_VT_PHYSICAL_FORMAT_BC1_UNORM && family != T3D_VT_PHYSICAL_FORMAT_BC5_UNORM &&
	     family != T3D_VT_PHYSICAL_FORMAT_BC7_UNORM )
		return false;
	const uint32_t stride = width * 4u;
	const uint32_t blocks = width / 4u;
	const uint32_t block_bytes = family == T3D_VT_PHYSICAL_FORMAT_BC1_UNORM ? 8u : 16u;
	if ( family == T3D_VT_PHYSICAL_FORMAT_BC7_UNORM ) {
		// Reject mixed/unsupported modes before touching the output. Other
		// BC7 modes are handled by the Godot decoder in the authoring adapter.
		for ( uint32_t i = 0u; i < blocks * blocks; ++i ) {
			if ( ( src[i * 16u] & 0x7fu ) != 0x40u )
				return false;
		}
	}
	for ( uint32_t y = 0u; y < blocks; ++y ) {
		for ( uint32_t x = 0u; x < blocks; ++x ) {
			const uint8_t *block = src + ( y * blocks + x ) * block_bytes;
			uint8_t *output = dst + y * 4u * stride + x * 16u;
			if ( family == T3D_VT_PHYSICAL_FORMAT_BC1_UNORM ) {
				t3d_comp_surf_bc1( block, output, stride );
			} else if ( family == T3D_VT_PHYSICAL_FORMAT_BC7_UNORM ) {
				if ( !t3d_comp_surf_decode_bc7_mode6( block, output, stride ) )
					return false;
			} else {
				for ( uint32_t py = 0u; py < 4u; ++py ) {
					for ( uint32_t px = 0u; px < 4u; ++px ) {
						uint8_t *pixel = output + py * stride + px * 4u;
						pixel[0] = pixel[1] = pixel[2] = 0u;
						pixel[3] = 255u;
					}
				}
				t3d_comp_surf_bc4( block, output, stride );
				t3d_comp_surf_bc4( block + 8u, output + 1u, stride );
			}
		}
	}
	return true;
}
