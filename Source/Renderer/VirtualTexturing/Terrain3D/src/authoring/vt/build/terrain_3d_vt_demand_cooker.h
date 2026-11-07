// Copyright (c) 2023-2026 Terrain3D contributors.
#ifndef TERRAIN3D_VT_DEMAND_COOKER_H
#define TERRAIN3D_VT_DEMAND_COOKER_H

#include "Terrain3DTypes.h"
#include <godot_cpp/variant/packed_byte_array.hpp>

class Terrain3DData;
class Terrain3DAssets;

// Owner-thread offline cook. The final (post-MegaVT merge) V1 package is the
// source of stable keys and its content signature. No runtime scene traversal.
bool t3d_vt_demand_cook_asset_sidecar_cold( const Terrain3DData& rData, const godot::Ref<Terrain3DAssets>& rAssets,
                                            const godot::PackedByteArray& rFinalPackage, godot::PackedByteArray& rSidecar,
                                            godot::String& rPath, godot::String& rError, godot::PackedByteArray *pV1Sidecar = nullptr,
                                            godot::String *pV1Path = nullptr );

// Publish two independently validated files as one rollback-capable cook action.
// The package may stay in terrain's data directory; the demand sidecar follows the asset.
bool t3d_vt_demand_publish_pair_cold( const godot::String& rPackagePath, const godot::PackedByteArray& rPackage,
                                      const godot::String& rDemandPath, const godot::PackedByteArray& rDemand, godot::String& rError );
// The third artifact is optional. v2 is always published at its canonical
// path, so shipping runtimes can continue using their existing reader.
bool t3d_vt_demand_publish_triplet_cold( const godot::String& rPackagePath, const godot::PackedByteArray& rPackage,
                                         const godot::String& rDemandPath, const godot::PackedByteArray& rDemand,
                                         const godot::String& rV1Path, const godot::PackedByteArray& rV1, godot::String& rError );
// Atomic four-artifact publication for parity-approved SuperTerrain. The
// surface sidecar is mandatory; the v2/v1 demand indexes remain optional.
bool t3d_vt_demand_publish_quartet_cold( const godot::String& rPackagePath, const godot::PackedByteArray& rPackage,
                                         const godot::String& rDemandPath, const godot::PackedByteArray& rDemand,
                                         const godot::String& rV1Path, const godot::PackedByteArray& rV1, const godot::String& rSurfacePath,
                                         const godot::PackedByteArray& rSurface, godot::String& rError );
#endif
