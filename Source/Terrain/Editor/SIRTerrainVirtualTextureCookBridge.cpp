/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
Copyright (c) 2023-2026 Cory Petkovsek, Roope Palmroos, and Terrain3D Contributors

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#include <Terrain/Editor/SIRTerrainVirtualTextureCookBridge.h>
#include <Renderer/VirtualTexturing/SIRVirtualTextureCooker.h>
#include <string.h>

sirTerrainVTCookResult_t SIRTerrainVirtualTextureCookBridge::cookAndPublish( const sirTerrainVTCookPublication_t& request ) noexcept
{
	sirVirtualTextureCookOutput_t *const pPackage = request.pPackage;
	if ( pPackage == nullptr || request.pszDestination == nullptr || request.pszStaging == nullptr || request.pszDestination[0] == '\0' ||
	     request.pszStaging[0] == '\0' || strcmp( request.pszDestination, request.pszStaging ) == 0 ||
	     ( ( request.pfnBeginCommit == nullptr ) != ( request.pfnEndCommit == nullptr ) ) ) {
		return SIR_TERRAIN_VT_COOK_INVALID_INPUT;
	}
	pPackage->nWritten = 0u;
	if ( request.batch.pfnIsCurrent == nullptr || request.batch.pRegions == nullptr ) {
		return SIR_TERRAIN_VT_COOK_INVALID_INPUT;
	}
	for ( uint32_t i = 0u; i < request.batch.nRegionCount; ++i ) {
		const sirVirtualTextureRegionCookRequest_t& region = request.batch.pRegions[i];
		if ( !request.batch.pfnIsCurrent( request.batch.pCurrentUser, region.nRegionIndex, region.nGeneration ) ) {
			return SIR_TERRAIN_VT_COOK_STALE_REVISION;
		}
	}
	if ( !SIRVirtualTextureRegionCooker::cookBatch( request.batch, *pPackage ) ) {
		return SIR_TERRAIN_VT_COOK_FAILED;
	}
	for ( uint32_t i = 0u; i < request.batch.nRegionCount; ++i ) {
		const sirVirtualTextureRegionCookRequest_t& region = request.batch.pRegions[i];
		if ( !request.batch.pfnIsCurrent( request.batch.pCurrentUser, region.nRegionIndex, region.nGeneration ) ) {
			pPackage->nWritten = 0u;
			return SIR_TERRAIN_VT_COOK_STALE_REVISION;
		}
	}
	if ( request.pfnBeginCommit != nullptr && !request.pfnBeginCommit( request.pCommitUser ) ) {
		pPackage->nWritten = 0u;
		return SIR_TERRAIN_VT_COOK_PUBLISH_FAILED;
	}
	// All authored source generations are rechecked inside the editor owner's
	// short commit lock. Never publish a cook overtaken by a later edit.
	bool bCurrent = true;
	for ( uint32_t i = 0u; i < request.batch.nRegionCount; ++i ) {
		const sirVirtualTextureRegionCookRequest_t& region = request.batch.pRegions[i];
		if ( !request.batch.pfnIsCurrent( request.batch.pCurrentUser, region.nRegionIndex, region.nGeneration ) ) {
			bCurrent = false;
			break;
		}
	}
	const bool bPublished = bCurrent && SIRVirtualTextureCooker::publishAtomic( request.pszDestination, request.pszStaging, *pPackage );
	if ( request.pfnEndCommit != nullptr ) {
		request.pfnEndCommit( request.pCommitUser );
	}
	if ( !bPublished ) {
		pPackage->nWritten = 0u;
		return bCurrent ? SIR_TERRAIN_VT_COOK_PUBLISH_FAILED : SIR_TERRAIN_VT_COOK_STALE_REVISION;
	}
	return SIR_TERRAIN_VT_COOK_SUCCESS;
}
