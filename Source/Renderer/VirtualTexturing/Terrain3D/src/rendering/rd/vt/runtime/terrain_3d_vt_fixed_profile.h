// Copyright © 2023-2026 Cory Petkovsek, Roope Palmroos, and Contributors.

#ifndef TERRAIN3D_VT_FIXED_PROFILE_H
#define TERRAIN3D_VT_FIXED_PROFILE_H

#include "Terrain3DTypes.h"

#include "rendering/rd/vt/core/terrain_3d_vt_contract.h"

#include <stdint.h>

#define T3D_VT_MAX_AUTHORED_EXTENT 4096u

const t3dVTFixedProfileDesc_t *t3d_vt_fixed_profile_get( t3dVTFixedProfile_t Profile );
uint32_t t3d_vt_fixed_profile_channel_extent( t3dVTFixedProfile_t Profile, t3dVTLogicalChannel_t Channel );
uint32_t t3d_vt_fixed_profile_mip0_pages( t3dVTFixedProfile_t Profile, t3dVTLogicalChannel_t Channel );
bool t3d_vt_fixed_profile_quality_base_mip( t3dVTFixedProfile_t HighestProfile, t3dVTFixedProfile_t ViewProfile,
                                            t3dVTLogicalChannel_t Channel, uint32_t nMipCount, uint8_t& rBaseMip );
bool t3d_vt_fixed_profile_validate_extent( t3dVTFixedProfile_t Profile, t3dVTLogicalChannel_t Channel, uint32_t nWidth, uint32_t nHeight );
uint32_t t3d_vt_fixed_profile_clipmap_mask( t3dVTFixedProfile_t Profile, bool bHeightEnabled );
uint32_t t3d_vt_fixed_profile_instance_mask( t3dVTFixedProfile_t Profile, bool bNormalEnabled, bool bOrmEnabled );
bool t3d_vt_fixed_profile_source_supports( t3dVTFixedProfile_t Profile, t3dVTConsumer_t Consumer, uint32_t nAuthoredChannelMask,
                                           const uint32_t nSourceExtent[T3D_VT_LOGICAL_CHANNEL_COUNT] );
t3dVTFixedProfile_t t3d_vt_fixed_profile_highest_supported( t3dVTConsumer_t Consumer, uint32_t nAuthoredChannelMask,
                                                            const uint32_t nSourceExtent[T3D_VT_LOGICAL_CHANNEL_COUNT] );

#endif // TERRAIN3D_VT_FIXED_PROFILE_H
