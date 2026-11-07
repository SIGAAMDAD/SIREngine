/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
Copyright (c) 2023-2026 Cory Petkovsek, Roope Palmroos, and Terrain3D contributors

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Renderer/VirtualTexturing/SIRVirtualTextureFixedProfile.h>

// Exact native data and selection policy from Terrain3D's
// rendering/rd/vt/runtime/terrain_3d_vt_fixed_profile.cpp.
static constexpr uint32_t SIR_VT_CLIPMAP_CHANNEL_MASK = ( 1U << 0 ) | ( 1U << 1 ) | ( 1U << 2 );
static constexpr uint32_t SIR_VT_INSTANCE_CHANNEL_MASK = ( 1U << 0 ) | ( 1U << 3 ) | ( 1U << 4 );
static constexpr uint32_t SIR_VT_COMPOSED_CHANNEL_MASK = ( 1U << 0 ) | ( 1U << 1 ) | ( 1U << 3 );
static constexpr uint32_t SIR_VT_PROFILE_CHANNEL_MASK_ALL = 0x1FU;

static constexpr sirVirtualTextureFixedProfile_t s_szFixedProfiles[SIR_VT_PACK_QUALITY_PROFILES] = {
    { { 256, 128, 128, 128, 0, 0 }, ( 1U << 0 ) | ( 1U << 1 ) | ( 1U << 2 ) | ( 1U << 3 ) },
    { { 512, 256, 256, 256, 256, 0 }, SIR_VT_PROFILE_CHANNEL_MASK_ALL },
    { { 1024, 512, 512, 512, 512, 0 }, SIR_VT_PROFILE_CHANNEL_MASK_ALL },
    { { 2048, 1024, 1024, 1024, 1024, 0 }, SIR_VT_PROFILE_CHANNEL_MASK_ALL },
    { { 4096, 2048, 2048, 2048, 2048, 0 }, SIR_VT_PROFILE_CHANNEL_MASK_ALL },
    { { 4096, 4096, 4096, 4096, 4096, 0 }, SIR_VT_PROFILE_CHANNEL_MASK_ALL } };

static uint32_t sirVTFixedProfileAllowedMask( sirVirtualTextureProfileConsumer_t eConsumer ) noexcept
{
	switch ( eConsumer ) {
	case SIR_VT_PROFILE_CLIPMAP:
		return SIR_VT_CLIPMAP_CHANNEL_MASK;
	case SIR_VT_PROFILE_INSTANCE:
		return SIR_VT_INSTANCE_CHANNEL_MASK;
	case SIR_VT_PROFILE_COMPOSED:
		return SIR_VT_COMPOSED_CHANNEL_MASK;
	default:
		return 0;
	}
}

static uint32_t sirVTFixedProfileRequiredMask( sirVirtualTextureProfileConsumer_t eConsumer ) noexcept
{
	switch ( eConsumer ) {
	case SIR_VT_PROFILE_CLIPMAP:
		return ( 1U << 0 ) | ( 1U << 1 );
	case SIR_VT_PROFILE_INSTANCE:
		return 1U << 0;
	case SIR_VT_PROFILE_COMPOSED:
		return SIR_VT_COMPOSED_CHANNEL_MASK;
	default:
		return 0;
	}
}

const sirVirtualTextureFixedProfile_t *sirVTFixedProfileGet( uint32_t nProfile ) noexcept
{
	return nProfile < SIR_VT_PACK_QUALITY_PROFILES ? &s_szFixedProfiles[nProfile] : nullptr;
}

uint32_t sirVTFixedProfileExtent( uint32_t nProfile, uint32_t nChannel ) noexcept
{
	const sirVirtualTextureFixedProfile_t *pProfile = sirVTFixedProfileGet( nProfile );
	return pProfile != nullptr && nChannel < SIR_VT_PACK_CHANNEL_COUNT ? pProfile->szExtent[nChannel] : 0;
}

uint32_t sirVTFixedProfileMip0Pages( uint32_t nProfile, uint32_t nChannel ) noexcept
{
	const uint32_t nExtent = sirVTFixedProfileExtent( nProfile, nChannel );
	return nExtent == 0 ? 0 : 1U + ( nExtent - 1U ) / SIR_VT_PAGE_INTERIOR;
}

bool sirVTFixedProfileQualityBaseMip( const sirVirtualTextureProfileMipRequest_t& request, uint8_t *pBaseMip ) noexcept
{
	if ( pBaseMip == nullptr ) {
		return false;
	}
	*pBaseMip = UINT8_MAX;
	const uint32_t nTop = sirVTFixedProfileExtent( request.nHighestProfile, request.nChannel );
	const uint32_t nTarget = sirVTFixedProfileExtent( request.nViewProfile, request.nChannel );
	if ( nTop == 0 || nTarget == 0 || nTarget > nTop || request.nMipCount == 0 ) {
		return false;
	}
	uint32_t nExtent = nTop;
	uint32_t nMip = 0;
	while ( nExtent > nTarget && nMip + 1U < request.nMipCount ) {
		nExtent = nExtent > 1 ? nExtent >> 1U : 1U;
		++nMip;
	}
	if ( nExtent != nTarget || nMip >= request.nMipCount || nMip > UINT8_MAX ) {
		return false;
	}
	*pBaseMip = ( uint8_t )nMip;
	return true;
}

bool sirVTFixedProfileValidateExtent( uint32_t nProfile, uint32_t nChannel, uint32_t nWidth, uint32_t nHeight ) noexcept
{
	const uint32_t nExtent = sirVTFixedProfileExtent( nProfile, nChannel );
	return nExtent != 0 && nWidth == nExtent && nHeight == nExtent;
}

uint32_t sirVTFixedProfileClipmapMask( uint32_t nProfile, bool bHeightEnabled ) noexcept
{
	const sirVirtualTextureFixedProfile_t *pProfile = sirVTFixedProfileGet( nProfile );
	if ( pProfile == nullptr ) {
		return 0;
	}
	uint32_t nMask = ( 1U << 0 ) | ( 1U << 1 );
	if ( bHeightEnabled && pProfile->szExtent[2] > 0 ) {
		nMask |= 1U << 2;
	}
	return nMask & pProfile->nSupportedChannelMask & SIR_VT_CLIPMAP_CHANNEL_MASK;
}

uint32_t sirVTFixedProfileInstanceMask( uint32_t nProfile, bool bNormalEnabled, bool bOrmEnabled ) noexcept
{
	const sirVirtualTextureFixedProfile_t *pProfile = sirVTFixedProfileGet( nProfile );
	if ( pProfile == nullptr ) {
		return 0;
	}
	uint32_t nMask = 1U << 0;
	if ( bNormalEnabled && pProfile->szExtent[3] > 0 ) {
		nMask |= 1U << 3;
	}
	if ( bOrmEnabled && pProfile->szExtent[4] > 0 ) {
		nMask |= 1U << 4;
	}
	return nMask & pProfile->nSupportedChannelMask & SIR_VT_INSTANCE_CHANNEL_MASK;
}

bool sirVTFixedProfileSourceSupports( uint32_t nProfile, sirVirtualTextureProfileConsumer_t eConsumer, uint32_t nAuthoredChannelMask,
                                      const uint32_t *pSourceExtent ) noexcept
{
	const sirVirtualTextureFixedProfile_t *pProfile = sirVTFixedProfileGet( nProfile );
	const uint32_t nAllowed = sirVTFixedProfileAllowedMask( eConsumer );
	const uint32_t nRequired = sirVTFixedProfileRequiredMask( eConsumer );
	if ( pProfile == nullptr || pSourceExtent == nullptr || nAllowed == 0 || nRequired == 0 || ( nAuthoredChannelMask & ~nAllowed ) != 0 ||
	     ( nAuthoredChannelMask & nRequired ) != nRequired ) {
		return false;
	}
	for ( uint32_t iChannel = 0; iChannel < SIR_VT_PACK_CHANNEL_COUNT; ++iChannel ) {
		const uint32_t nBit = 1U << iChannel;
		if ( ( nAllowed & nBit ) == 0 || ( nAuthoredChannelMask & nBit ) == 0 ) {
			continue;
		}
		const uint32_t nTarget = pProfile->szExtent[iChannel];
		if ( nTarget == 0 ) {
			continue;
		}
		const uint32_t nSource =
		    pSourceExtent[iChannel] > SIR_VT_MAX_AUTHORED_EXTENT ? SIR_VT_MAX_AUTHORED_EXTENT : pSourceExtent[iChannel];
		if ( nSource < nTarget ) {
			return false;
		}
	}
	return true;
}

uint32_t sirVTFixedProfileHighestSupported( sirVirtualTextureProfileConsumer_t eConsumer, uint32_t nAuthoredChannelMask,
                                            const uint32_t *pSourceExtent ) noexcept
{
	if ( pSourceExtent == nullptr ) {
		return SIR_VT_PROFILE_INVALID;
	}
	for ( uint32_t nIndex = SIR_VT_PACK_QUALITY_PROFILES; nIndex > 0; --nIndex ) {
		if ( sirVTFixedProfileSourceSupports( nIndex - 1U, eConsumer, nAuthoredChannelMask, pSourceExtent ) ) {
			return nIndex - 1U;
		}
	}
	return SIR_VT_PROFILE_INVALID;
}
