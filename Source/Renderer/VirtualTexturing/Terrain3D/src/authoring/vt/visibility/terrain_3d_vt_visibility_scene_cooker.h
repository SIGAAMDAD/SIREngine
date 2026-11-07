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
