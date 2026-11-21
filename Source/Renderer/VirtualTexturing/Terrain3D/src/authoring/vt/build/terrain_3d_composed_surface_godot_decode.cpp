// Godot cold authoring decoder; never called by the shipping raster thread.
#include "authoring/vt/build/terrain_3d_composed_surface_godot_decode.h"
#include "authoring/vt/build/terrain_3d_composed_surface_reference_decode.h"
#include "rendering/rd/vt/core/terrain_3d_vt_contract.h"
#include "data/formats/terrain_3d_vt_pack_format.h"
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <cstring>

bool t3d_composed_surface_pack_decode_godot_image( void *, uint32_t format, const uint8_t *bytes, uint32_t size, uint8_t *out,
                                                   uint32_t capacity )
{
	using godot::Image;
	using godot::PackedByteArray;
	using godot::Ref;
	if ( !bytes || !out || capacity != T3D_VT_PHYSICAL_TILE * T3D_VT_PHYSICAL_TILE * 4u ||
	     size != t3d_vt_pack_expected_physical_tile_bytes( format ) )
		return false;
	if ( format == T3D_VT_PHYSICAL_FORMAT_R8_UNORM || format == T3D_VT_PHYSICAL_FORMAT_BC1_UNORM ||
	     format == T3D_VT_PHYSICAL_FORMAT_BC5_UNORM )
		return t3d_comp_surf_decode_reference_tile( format, bytes, size, out, capacity );
	if ( format != T3D_VT_PHYSICAL_FORMAT_BC7_UNORM )
		return false;
	// The cold V1 reference encoder produces mode-6 BC7. Decode those
	// directly; retain Godot BPTC support for packs from other encoders.
	const uint32_t block_count = ( T3D_VT_PHYSICAL_TILE / 4u ) * ( T3D_VT_PHYSICAL_TILE / 4u );
	bool all_mode6 = true;
	for ( uint32_t i = 0u; i < block_count; ++i ) {
		if ( ( bytes[i * 16u] & 0x7fu ) != 0x40u ) {
			all_mode6 = false;
			break;
		}
	}
	if ( all_mode6 )
		return t3d_comp_surf_decode_reference_tile( format, bytes, size, out, capacity );
	const Image::Format godot_format = Image::FORMAT_BPTC_RGBA;
	PackedByteArray encoded;
	encoded.resize( int64_t( size ) );
	std::memcpy( encoded.ptrw(), bytes, size );
	Ref<Image> image =
	    Image::create_from_data( int32_t( T3D_VT_PHYSICAL_TILE ), int32_t( T3D_VT_PHYSICAL_TILE ), false, godot_format, encoded );
	if ( image.is_null() || image->is_empty() || image->decompress() != godot::OK )
		return false;
	image->convert( Image::FORMAT_RGBA8 );
	if ( image->get_format() != Image::FORMAT_RGBA8 )
		return false;
	PackedByteArray decoded = image->get_data();
	if ( uint64_t( decoded.size() ) != capacity )
		return false;
	std::memcpy( out, decoded.ptr(), capacity );
	return true;
}
