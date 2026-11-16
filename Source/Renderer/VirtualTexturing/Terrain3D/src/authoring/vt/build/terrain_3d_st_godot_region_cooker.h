// Copyright 2026 Terrain3D contributors.
#ifndef TERRAIN3D_ST_GODOT_REGION_COOKER_H
#define TERRAIN3D_ST_GODOT_REGION_COOKER_H
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>
class Terrain3DData;

// Exact source image snapshot, packaged with the final approved V1 COMPOSED
// allocation generation. No source images are synthesized on the GPU.
bool t3d_st_cook_region_package_cold( Terrain3DData& rData, const godot::PackedByteArray& rFinalV1, godot::PackedByteArray& rRegionPackage,
                                      godot::String& rError );
#endif // TERRAIN3D_ST_GODOT_REGION_COOKER_H
