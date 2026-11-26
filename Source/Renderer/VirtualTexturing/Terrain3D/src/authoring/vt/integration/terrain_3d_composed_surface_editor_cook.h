/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

// CS14: editor-owner publication transaction for a validated V1 composed cook.
#ifndef TERRAIN3D_COMPOSED_SURFACE_EDITOR_COOK_H
#define TERRAIN3D_COMPOSED_SURFACE_EDITOR_COOK_H

#include "authoring/vt/build/terrain_3d_composed_surface_cold_pipeline.h"
#include "authoring/vt/integration/terrain_3d_composed_surface_edit_journal.h"

// The supplied lock must serialize terrain edits, material/source replacement,
// the final sidecar rename and journal acceptance on the editor owner thread.
// Begin/End do not run during composition; only during the short commit phase.
typedef bool ( *t3dComposedSurfaceEditorCookBeginCommitFn )( void *pUser );
typedef void ( *t3dComposedSurfaceEditorCookEndCommitFn )( void *pUser );

struct t3dComposedSurfaceEditorCookRequest_t
{
	t3dComposedSurfaceColdCookRequest_t m_cook;
	uint32_t m_nRegionIndex;
	uint64_t m_nJournalFingerprint;
	uint64_t m_nJournalTopologyGeneration;
	uint64_t m_nRegionKey;
	uint32_t m_nMip0Width;
	uint32_t m_nMip0Height;
	uint8_t *m_pDirtyFlags;
	uint32_t m_nDirtyCapacity;
	t3dComposedSurfaceEditorCookBeginCommitFn m_pBeginCommit;
	t3dComposedSurfaceEditorCookEndCommitFn m_pEndCommit;
	void *m_pCommitUser;
};

enum class SIRComposedSurfaceEditorCookStatus : int32_t {
	T3D_COMPOSED_SURFACE_EDITOR_COOK_SUCCESS = 0,
	T3D_COMPOSED_SURFACE_EDITOR_COOK_INVALID_INPUT,
	T3D_COMPOSED_SURFACE_EDITOR_COOK_NOT_DIRTY,
	T3D_COMPOSED_SURFACE_EDITOR_COOK_STALE,
	T3D_COMPOSED_SURFACE_EDITOR_COOK_COOK_FAILED,
	T3D_COMPOSED_SURFACE_EDITOR_COOK_COMMIT_FAILED
};

struct t3dComposedSurfaceEditorCookResult_t
{
	SIRComposedSurfaceEditorCookStatus m_status;
	t3dComposedSurfaceColdCookResult_t m_cook;
	uint64_t m_nClaimRevision;
};

// Caller retains source/previous files, page footprints, authored layers,
// decoder/encoder and bounded scratch for the entire synchronous cold call.
// Never invoked from an edit callback, render thread or frame service.
// On failure, dirty journal state remains eligible for retry.
bool t3d_composed_surface_editor_cook_cold( Terrain3DComposedSurfaceEditJournal& rJournal,
                                            const t3dComposedSurfaceEditorCookRequest_t& rRequest,
                                            t3dComposedSurfaceEditorCookResult_t& rResult );
#endif
