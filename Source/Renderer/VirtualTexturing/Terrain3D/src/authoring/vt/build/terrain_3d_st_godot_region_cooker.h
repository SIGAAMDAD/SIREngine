/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

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
