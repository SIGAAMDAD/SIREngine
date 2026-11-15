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

#ifndef __SIRENGINE_SIRTERRAINCOMPOSEDSURFACERECOOKBRIDGE_H__
#define __SIRENGINE_SIRTERRAINCOMPOSEDSURFACERECOOKBRIDGE_H__

#include <SIRPCH.hpp>
#include <Renderer/VirtualTexturing/Terrain3D/src/authoring/vt/integration/terrain_3d_composed_surface_edit_journal.h>
#include <Renderer/VirtualTexturing/Terrain3D/src/authoring/vt/build/terrain_3d_composed_surface_cooker.h>
#include <Renderer/VirtualTexturing/Terrain3D/src/authoring/vt/integration/terrain_3d_composed_surface_editor_cook.h>

typedef struct sirTerrainComposedDirtyRequest_s
{
	t3dComposedSurfaceEditRegion_t region;
	t3dComposedSurfaceEditBounds_t bounds;
	uint32_t nTexelsPerMeter;
	uint64_t nSourceFingerprint;
	uint64_t nTopologyGeneration;
} sirTerrainComposedDirtyRequest_t;

typedef struct sirTerrainComposedRecookPageList_s
{
	const t3dComposedSurfaceCookPage_t *pPages;
	uint32_t nPageCount;
	uint64_t nRegionKey;
	uint32_t nMip0Width;
	uint32_t nMip0Height;
} sirTerrainComposedRecookPageList_t;

typedef struct sirTerrainComposedRecookBuffers_s
{
	const uint8_t *pPreviousFile;
	uint64_t nPreviousBytes;
	uint8_t *pDirtyFlags;
	uint32_t nDirtyCapacity;
	uint8_t *pScratch;
	uint32_t nScratchBytes;
	uint8_t *pOutput;
	uint64_t nOutputCapacity;
} sirTerrainComposedRecookBuffers_t;

typedef struct sirTerrainComposedRecookRequest_s
{
	const t3dComposedSurfaceEditTicket_t *pTicket;
	sirTerrainComposedRecookPageList_t pages;
	sirTerrainComposedRecookBuffers_t buffers;
	t3dComposedSurfaceCookSource_t source;
	const char *pszDestination;
} sirTerrainComposedRecookRequest_t;

typedef struct sirTerrainComposedSidecarLoadRequest_s
{
	const char *pszPath;
	uint8_t *pStorage;
	uint64_t nCapacity;
} sirTerrainComposedSidecarLoadRequest_t;

typedef enum sirTerrainComposedRecookResult_e : uint8_t
{
	SIR_TERRAIN_COMPOSED_RECOOK_SUCCESS = 0,
	SIR_TERRAIN_COMPOSED_RECOOK_INVALID_INPUT,
	SIR_TERRAIN_COMPOSED_RECOOK_STALE,
	SIR_TERRAIN_COMPOSED_RECOOK_FAILED,
	SIR_TERRAIN_COMPOSED_RECOOK_PUBLISH_FAILED
} sirTerrainComposedRecookResult_t;

// Native Terrain owner of the original Terrain3D CS07 edit journal.
// All journal calls, source snapshots, and file publication occur under the
// caller's editor/terrain authoring lock. Recook buffers are caller-owned.
// Failed/stale publication leaves the dirty ticket available for retry.
class SIRTerrainComposedSurfaceRecookBridge final
{
public:
	static bool loadPreviousSidecar( const sirTerrainComposedSidecarLoadRequest_t& request, uint64_t& nLoaded ) noexcept;
	void reset( void ) noexcept;
	bool dirtyRegion( const sirTerrainComposedDirtyRequest_t& request ) noexcept;
	bool invalidateRegion( const t3dComposedSurfaceEditRegion_t& region, uint64_t nFingerprint, uint64_t nTopologyGeneration ) noexcept;
	bool forgetRegion( uint32_t nRegionIndex, uint64_t nTopologyGeneration ) noexcept;
	bool claim( uint32_t nRegionIndex, t3dComposedSurfaceEditTicket_t& ticket ) noexcept;
	void cancel( const t3dComposedSurfaceEditTicket_t& ticket ) noexcept;
	sirTerrainComposedRecookResult_t recookAndPublish( const sirTerrainComposedRecookRequest_t& request ) noexcept;
	bool cookEditorAndPublish( const t3dComposedSurfaceEditorCookRequest_t& request,
	                           t3dComposedSurfaceEditorCookResult_t& result ) noexcept;
	const t3dComposedSurfaceEditStats_t& stats( void ) const noexcept;
	const t3dComposedSurfaceEditRecord_t *region( uint32_t nRegionIndex ) const noexcept;

protected:
private:
	Terrain3DComposedSurfaceEditJournal m_journal;
};

#endif // __SIRENGINE_SIRTERRAINCOMPOSEDSURFACERECOOKBRIDGE_H__
