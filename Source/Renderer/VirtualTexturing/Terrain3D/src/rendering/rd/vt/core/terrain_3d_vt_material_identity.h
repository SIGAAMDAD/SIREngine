// Copyright © 2023-2026 Cory Petkovsek, Roope Palmroos, and Contributors.

#ifndef TERRAIN3D_VT_MATERIAL_IDENTITY_H
#define TERRAIN3D_VT_MATERIAL_IDENTITY_H

#include <godot_cpp/variant/char_string.hpp>
#include <godot_cpp/variant/string.hpp>

#include "core/utility/terrain_3d_hash_bytes.h"

#include <stdint.h>

static inline uint64_t t3d_vt_material_stable_key_from_resource_path( const godot::String& rPath )
{
	if ( rPath.is_empty() ) {
		return 0u;
	}
	const godot::CharString Path = rPath.utf8();
	if ( Path.length() <= 0 ) {
		return 0u;
	}
	const uint64_t nKey = t3dHashStableBytes( Path.get_data(), ( uint64_t )Path.length() );
	return nKey == 0u ? 1u : nKey;
}

static inline uint64_t t3d_vt_material_stable_key_from_owner_role( const godot::String& rOwnerIdentity, const godot::String& rRoleIdentity )
{
	if ( rOwnerIdentity.is_empty() || rRoleIdentity.is_empty() ) {
		return 0u;
	}
	const godot::String identity = godot::String( "terrain3d-vt-material:" ) + rOwnerIdentity + godot::String( ":" ) + rRoleIdentity;
	return t3d_vt_material_stable_key_from_resource_path( identity );
}

#endif // TERRAIN3D_VT_MATERIAL_IDENTITY_H
