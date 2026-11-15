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

#ifndef __SIRENGINE_SIRTERRAINVIRTUALTEXTURECOOKBRIDGE_H__
#define __SIRENGINE_SIRTERRAINVIRTUALTEXTURECOOKBRIDGE_H__

#include <Renderer/VirtualTexturing/SIRVirtualTextureRegionCooker.h>

typedef enum sirTerrainVTCookResult_e : uint8_t
{
	SIR_TERRAIN_VT_COOK_SUCCESS = 0,
	SIR_TERRAIN_VT_COOK_INVALID_INPUT,
	SIR_TERRAIN_VT_COOK_STALE_REVISION,
	SIR_TERRAIN_VT_COOK_FAILED,
	SIR_TERRAIN_VT_COOK_PUBLISH_FAILED
} sirTerrainVTCookResult_t;

typedef bool ( *sirTerrainVTBeginCommitFn )( void *pUser );
typedef void ( *sirTerrainVTEndCommitFn )( void *pUser );

typedef struct sirTerrainVTCookPublication_s
{
	sirVirtualTextureRegionBatchRequest_t batch;
	sirVirtualTextureCookOutput_t *pPackage;
	const char *pszDestination;
	const char *pszStaging;
	// Optional short commit lock, matching the original donor editor transaction.
	// When absent, the caller must hold its terrain publication lock.
	sirTerrainVTBeginCommitFn pfnBeginCommit;
	sirTerrainVTEndCommitFn pfnEndCommit;
	void *pCommitUser;
} sirTerrainVTCookPublication_t;

// Terrain/editor owns the authoring revision, publication lock, source image
// lifetime and output buffers. This owns no world, image, or renderer cache.
// Call on the editor owner thread. With commit callbacks, the final revision
// check and disk replace run under the short Terrain-owned publication lock.
// Without them, the caller must hold that lock for the whole operation.
class SIRTerrainVirtualTextureCookBridge final
{
public:
	static sirTerrainVTCookResult_t cookAndPublish( const sirTerrainVTCookPublication_t& request ) noexcept;

protected:
private:
};

#endif // __SIRENGINE_SIRTERRAINVIRTUALTEXTURECOOKBRIDGE_H__
