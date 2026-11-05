/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIRENGINE_SIRTERRAINRESOURCEBRIDGE_H__
#define __SIRENGINE_SIRTERRAINRESOURCEBRIDGE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Resources/SIRResourceGraph.h>
#include <Terrain/Data/SIRTerrainHeightfieldView.h>

constexpr uint32_t SIR_TERRAIN_HEIGHTFIELD_RESOURCE_TYPE = UINT32_C( 0x54484631 );
constexpr uint32_t SIR_TERRAIN_HEIGHTFIELD_FORMAT = UINT32_C( 0x00010001 );

typedef struct sirTerrainRegionIdentity_s
{
	uint64_t nAssetId;
	uint32_t nCookedVariantId;
	int32_t iRegionX;
	int32_t iRegionZ;
} sirTerrainRegionIdentity_t;

typedef struct sirTerrainRegionPublishRequest_s
{
	sirTerrainRegionIdentity_t identity;
	uint32_t nStagingDomain;
	uint64_t nExpectedGraphRevision;
	uint64_t nMinimumTerrainRevision;
} sirTerrainRegionPublishRequest_t;

// Terrain has no independent CPU residency or staging owner. Source bytes are
// validated before GRDG admission; a read scope owns the resulting view lifetime.
class SIRTerrainResourceBridge
{
public:
	static SIRResourceKey getKey( const sirTerrainRegionIdentity_t& identity ) noexcept;

	static SIRResourceResult publish( SIRResourceGraph& graph, sirTerrainConstByteSpan_t source,
	                                  const sirTerrainRegionPublishRequest_t& request, SIRResourceHandle& handle ) noexcept;
	static SIRResourceResult open( const SIRResourceGraph& graph, const SIRResourceReadScope& scope,
	                               const sirTerrainRegionIdentity_t& identity, sirTerrainR16View_t& view ) noexcept;

protected:
private:
	static SIRResourceContentKey fingerprint( sirTerrainConstByteSpan_t source ) noexcept;
};

#endif // __SIRENGINE_SIRTERRAINRESOURCEBRIDGE_H__
