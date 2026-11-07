// Copyright © 2023-2026 Cory Petkovsek, Roope Palmroos, and Contributors.

#include "rendering/rd/vt/runtime/terrain_3d_vt_fixed_profile.h"

#define T3D_VT_MASK( channel ) T3D_VT_LOGICAL_CHANNEL_BIT( channel )

static uint32_t t3d_vt_fixed_profile_consumer_allowed_mask( const t3dVTConsumer_t consumer )
{
	return t3d_vt_consumer_allowed_mask( consumer );
}
static uint32_t t3d_vt_fixed_profile_consumer_required_mask( const t3dVTConsumer_t consumer )
{
	return t3d_vt_consumer_required_mask( consumer );
}

static uint32_t t3d_vt_fixed_profile_clamp_source_extent( const uint32_t nExtent )
{
	return nExtent > T3D_VT_MAX_AUTHORED_EXTENT ? T3D_VT_MAX_AUTHORED_EXTENT : nExtent;
}

static const t3dVTFixedProfileDesc_t s_VtFixedProfiles[T3D_VT_FIXED_PROFILE_COUNT] = {
    // Just Make It Run: Albedo 256, PBR/Normal 128, no Height/ORM.
    { { 256u, 128u, 128u, 128u, 0u, 0u },
	  T3D_VT_MASK( T3D_VT_LOGICAL_CHANNEL_ALBEDO ) | T3D_VT_MASK( T3D_VT_LOGICAL_CHANNEL_PBR ) |
	      T3D_VT_MASK( T3D_VT_LOGICAL_CHANNEL_NORMAL ) | T3D_VT_MASK( T3D_VT_LOGICAL_CHANNEL_HEIGHT ) },
    // Low: Albedo 512, 256 secondary maps, no Height.
    { { 512u, 256u, 256u, 256u, 256u, 0u },
	  T3D_VT_MASK( T3D_VT_LOGICAL_CHANNEL_ALBEDO ) | T3D_VT_MASK( T3D_VT_LOGICAL_CHANNEL_PBR ) |
	      T3D_VT_MASK( T3D_VT_LOGICAL_CHANNEL_NORMAL ) | T3D_VT_MASK( T3D_VT_LOGICAL_CHANNEL_ORM ) |
	      T3D_VT_MASK( T3D_VT_LOGICAL_CHANNEL_HEIGHT ) },
    // Normal: Albedo 1024, 512 secondary maps.
    { { 1024u, 512u, 512u, 512u, 512u, 0u }, T3D_VT_LOGICAL_CHANNEL_MASK_ALL },
    // High: Albedo 2048, 1024 secondary maps.
    { { 2048u, 1024u, 1024u, 1024u, 1024u, 0u }, T3D_VT_LOGICAL_CHANNEL_MASK_ALL },
    // Very High: Albedo 4096, 2048 secondary maps.
    { { 4096u, 2048u, 2048u, 2048u, 2048u, 0u }, T3D_VT_LOGICAL_CHANNEL_MASK_ALL },
    // Make My GPU SCREAM: all channels at 4096.
    { { 4096u, 4096u, 4096u, 4096u, 4096u, 0u }, T3D_VT_LOGICAL_CHANNEL_MASK_ALL },
};

const t3dVTFixedProfileDesc_t *t3d_vt_fixed_profile_get( const t3dVTFixedProfile_t Profile )
{
	if ( ( uint32_t )( Profile ) >= T3D_VT_FIXED_PROFILE_COUNT ) {
		return NULL;
	}
	return &s_VtFixedProfiles[( uint32_t )( Profile )];
}

uint32_t t3d_vt_fixed_profile_channel_extent( const t3dVTFixedProfile_t Profile, const t3dVTLogicalChannel_t Channel )
{
	const t3dVTFixedProfileDesc_t *const pProfile = t3d_vt_fixed_profile_get( Profile );
	if ( pProfile == NULL || ( uint32_t )( Channel ) >= T3D_VT_LOGICAL_CHANNEL_COUNT ) {
		return 0u;
	}
	return pProfile->m_nExtent[( uint32_t )( Channel )];
}

uint32_t t3d_vt_fixed_profile_mip0_pages( const t3dVTFixedProfile_t Profile, const t3dVTLogicalChannel_t Channel )
{
	const uint32_t nExtent = t3d_vt_fixed_profile_channel_extent( Profile, Channel );
	if ( nExtent == 0u ) {
		return 0u;
	}
	return ( nExtent + T3D_VT_PAGE_INTERIOR - 1u ) / T3D_VT_PAGE_INTERIOR;
}

bool t3d_vt_fixed_profile_quality_base_mip( const t3dVTFixedProfile_t HighestProfile, const t3dVTFixedProfile_t ViewProfile,
                                            const t3dVTLogicalChannel_t Channel, const uint32_t nMipCount, uint8_t& rBaseMip )
{
	rBaseMip = T3D_VT_PACK_INVALID_MIP;
	const uint32_t nTopExtent = t3d_vt_fixed_profile_channel_extent( HighestProfile, Channel );
	const uint32_t nTargetExtent = t3d_vt_fixed_profile_channel_extent( ViewProfile, Channel );
	if ( nTopExtent == 0u || nTargetExtent == 0u || nTargetExtent > nTopExtent || nMipCount == 0u ) {
		return false;
	}
	uint32_t nExtent = nTopExtent;
	uint32_t nMip = 0u;
	while ( nExtent > nTargetExtent && nMip + 1u < nMipCount ) {
		nExtent = nExtent > 1u ? nExtent >> 1u : 1u;
		++nMip;
	}
	if ( nExtent != nTargetExtent || nMip >= nMipCount || nMip > 0xffu ) {
		return false;
	}
	rBaseMip = ( uint8_t )nMip;
	return true;
}

bool t3d_vt_fixed_profile_validate_extent( const t3dVTFixedProfile_t Profile, const t3dVTLogicalChannel_t Channel, const uint32_t nWidth,
                                           const uint32_t nHeight )
{
	const uint32_t nExtent = t3d_vt_fixed_profile_channel_extent( Profile, Channel );
	return nExtent > 0u && nWidth == nExtent && nHeight == nExtent;
}

uint32_t t3d_vt_fixed_profile_clipmap_mask( const t3dVTFixedProfile_t Profile, const bool bHeightEnabled )
{
	const t3dVTFixedProfileDesc_t *const pProfile = t3d_vt_fixed_profile_get( Profile );
	if ( pProfile == NULL ) {
		return 0u;
	}
	uint32_t nMask = T3D_VT_MASK( T3D_VT_LOGICAL_CHANNEL_ALBEDO ) | T3D_VT_MASK( T3D_VT_LOGICAL_CHANNEL_PBR );
	if ( bHeightEnabled && pProfile->m_nExtent[T3D_VT_LOGICAL_CHANNEL_HEIGHT] > 0u ) {
		nMask |= T3D_VT_MASK( T3D_VT_LOGICAL_CHANNEL_HEIGHT );
	}
	return nMask & pProfile->m_nSupportedChannelMask & T3D_VT_CLIPMAP_ALLOWED_CHANNEL_MASK;
}

uint32_t t3d_vt_fixed_profile_instance_mask( const t3dVTFixedProfile_t Profile, const bool bNormalEnabled, const bool bOrmEnabled )
{
	const t3dVTFixedProfileDesc_t *const pProfile = t3d_vt_fixed_profile_get( Profile );
	if ( pProfile == NULL ) {
		return 0u;
	}
	uint32_t nMask = T3D_VT_MASK( T3D_VT_LOGICAL_CHANNEL_ALBEDO );
	if ( bNormalEnabled && pProfile->m_nExtent[T3D_VT_LOGICAL_CHANNEL_NORMAL] > 0u ) {
		nMask |= T3D_VT_MASK( T3D_VT_LOGICAL_CHANNEL_NORMAL );
	}
	if ( bOrmEnabled && pProfile->m_nExtent[T3D_VT_LOGICAL_CHANNEL_ORM] > 0u ) {
		nMask |= T3D_VT_MASK( T3D_VT_LOGICAL_CHANNEL_ORM );
	}
	return nMask & pProfile->m_nSupportedChannelMask & T3D_VT_INSTANCE_ALLOWED_CHANNEL_MASK;
}

bool t3d_vt_fixed_profile_source_supports( const t3dVTFixedProfile_t Profile, const t3dVTConsumer_t Consumer,
                                           const uint32_t nAuthoredChannelMask, const uint32_t nSourceExtent[T3D_VT_LOGICAL_CHANNEL_COUNT] )
{
	const t3dVTFixedProfileDesc_t *const pProfile = t3d_vt_fixed_profile_get( Profile );
	const uint32_t nAllowedMask = t3d_vt_fixed_profile_consumer_allowed_mask( Consumer );
	const uint32_t nRequiredMask = t3d_vt_fixed_profile_consumer_required_mask( Consumer );
	if ( pProfile == NULL || nSourceExtent == NULL || nAllowedMask == 0u || nRequiredMask == 0u ) {
		return false;
	}
	if ( ( nAuthoredChannelMask & ~nAllowedMask ) != 0u || ( nAuthoredChannelMask & nRequiredMask ) != nRequiredMask ) {
		return false;
	}
	for ( uint32_t iChannel = 0u; iChannel < T3D_VT_LOGICAL_CHANNEL_COUNT; ++iChannel ) {
		const uint32_t nBit = T3D_VT_MASK( iChannel );
		if ( ( nAllowedMask & nBit ) == 0u || ( nAuthoredChannelMask & nBit ) == 0u ) {
			continue;
		}
		const uint32_t nTargetExtent = pProfile->m_nExtent[iChannel];
		if ( nTargetExtent == 0u ) {
			continue;
		}
		const uint32_t nSource = t3d_vt_fixed_profile_clamp_source_extent( nSourceExtent[iChannel] );
		if ( nSource < nTargetExtent ) {
			return false;
		}
	}
	return true;
}

t3dVTFixedProfile_t t3d_vt_fixed_profile_highest_supported( const t3dVTConsumer_t Consumer, const uint32_t nAuthoredChannelMask,
                                                            const uint32_t nSourceExtent[T3D_VT_LOGICAL_CHANNEL_COUNT] )
{
	if ( nSourceExtent == NULL ) {
		return T3D_VT_FIXED_PROFILE_INVALID;
	}
	for ( uint32_t iProfile = T3D_VT_FIXED_PROFILE_COUNT; iProfile > 0u; --iProfile ) {
		const t3dVTFixedProfile_t Profile = ( t3dVTFixedProfile_t )( iProfile - 1u );
		if ( t3d_vt_fixed_profile_source_supports( Profile, Consumer, nAuthoredChannelMask, nSourceExtent ) ) {
			return Profile;
		}
	}
	return T3D_VT_FIXED_PROFILE_INVALID;
}

#undef T3D_VT_MASK
