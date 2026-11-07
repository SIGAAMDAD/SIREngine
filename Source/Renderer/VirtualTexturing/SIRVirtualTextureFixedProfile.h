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

#ifndef __SIRENGINE_SIRVIRTUALTEXTUREFIXEDPROFILE_H__
#define __SIRENGINE_SIRVIRTUALTEXTUREFIXEDPROFILE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/VirtualTexturing/SIRVirtualTexturePackage.h>

static constexpr uint32_t SIR_VT_MAX_AUTHORED_EXTENT = 4096U;
static constexpr uint32_t SIR_VT_PROFILE_INVALID = UINT32_MAX;

typedef enum sirVirtualTextureProfileConsumer_e : uint32_t
{
	SIR_VT_PROFILE_CLIPMAP = 0U,
	SIR_VT_PROFILE_INSTANCE = 1U,
	SIR_VT_PROFILE_COMPOSED = 2U
} sirVirtualTextureProfileConsumer_t;

typedef struct sirVirtualTextureFixedProfile_s
{
	uint32_t szExtent[SIR_VT_PACK_CHANNEL_COUNT];
	uint32_t nSupportedChannelMask;
} sirVirtualTextureFixedProfile_t;

typedef struct sirVirtualTextureProfileMipRequest_s
{
	uint32_t nHighestProfile;
	uint32_t nViewProfile;
	uint32_t nChannel;
	uint32_t nMipCount;
} sirVirtualTextureProfileMipRequest_t;

// The six frozen Terrain3D V1 quality levels have no dynamic resolution policy.
// Channels 0-4 represent albedo, PBR, height, normal and ORM; channel 5 is reserved.
const sirVirtualTextureFixedProfile_t *sirVTFixedProfileGet( uint32_t nProfile ) noexcept;
uint32_t sirVTFixedProfileExtent( uint32_t nProfile, uint32_t nChannel ) noexcept;
uint32_t sirVTFixedProfileMip0Pages( uint32_t nProfile, uint32_t nChannel ) noexcept;
bool sirVTFixedProfileQualityBaseMip( const sirVirtualTextureProfileMipRequest_t& request, uint8_t *pBaseMip ) noexcept;
bool sirVTFixedProfileValidateExtent( uint32_t nProfile, uint32_t nChannel, uint32_t nWidth, uint32_t nHeight ) noexcept;
uint32_t sirVTFixedProfileClipmapMask( uint32_t nProfile, bool bHeightEnabled ) noexcept;
uint32_t sirVTFixedProfileInstanceMask( uint32_t nProfile, bool bNormalEnabled, bool bOrmEnabled ) noexcept;
bool sirVTFixedProfileSourceSupports( uint32_t nProfile, sirVirtualTextureProfileConsumer_t eConsumer, uint32_t nAuthoredChannelMask,
                                      const uint32_t *pSourceExtent ) noexcept;
uint32_t sirVTFixedProfileHighestSupported( sirVirtualTextureProfileConsumer_t eConsumer, uint32_t nAuthoredChannelMask,
                                            const uint32_t *pSourceExtent ) noexcept;

#endif // __SIRENGINE_SIRVIRTUALTEXTUREFIXEDPROFILE_H__
