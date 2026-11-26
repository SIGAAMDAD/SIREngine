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

#ifndef TERRAIN3D_VT_VISIBILITY_SCENE_COOKER_H
#define TERRAIN3D_VT_VISIBILITY_SCENE_COOKER_H

#include "authoring/vt/visibility/terrain_3d_vt_visibility_aggregator.h"
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>

class Terrain3DData;

// T03 authoring-only entry. Builds conservative PARTIAL sampled terrain from
// authored height/control; HLOD meshes and exact UV occupancy arrive in T05/6.
// GPU unavailability is a benign cold failure; the v2 index stays authoritative.
bool t3d_vt_visibility_cook_cold( const Terrain3DData& rData, const uint64_t *pStableKeys, uint32_t nKeys, uint64_t nPackageSignature,
                                  uint64_t nAssetHash, godot::PackedByteArray& rV1, t3dVTVisibilityAggregateStats_t& rStats,
                                  godot::String& rError );

#endif
