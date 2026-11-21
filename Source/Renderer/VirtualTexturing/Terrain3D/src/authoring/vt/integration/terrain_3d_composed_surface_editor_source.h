// Editor-only, non-authoritative V1 composed candidate bake from real terrain data.
#ifndef TERRAIN3D_COMPOSED_SURFACE_EDITOR_SOURCE_H
#define TERRAIN3D_COMPOSED_SURFACE_EDITOR_SOURCE_H

#include "Terrain3DTypes.h"
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <stdint.h>

class Terrain3DData;
class Terrain3DAssets;
class Terrain3DMaterial;
class Terrain3DTerrainMegaVT;
class Terrain3DComposedSurfaceEditJournal;

typedef struct t3dCompSurfEditorCandidateResult_s
{
	bool m_bSaved = false;
	uint32_t m_nPages = 0u;
	uint32_t m_nDecodedTiles = 0u;
	uint32_t m_nReusedPages = 0u;
	// Reference-preview metadata, never accepted as canonical seam parity.
	uint32_t m_nBoundaryClampedPages = 0u;
	uint64_t m_nWrittenBytes = 0u;
	uint64_t m_nSourceFingerprint = 0u;
	godot::String m_error;
	godot::String m_path;
} t3dCompSurfEditorCandidateResult_t;

// Produces .t3sc.candidate, which is NEVER used for live GPU residency.
// Explicit editor bake only; all buffers and Godot source samples are cold-owned.
bool t3d_comp_surf_cook_editor_candidate_cold( const godot::PackedByteArray& rSourcePack, Terrain3DData& rData,
                                               const godot::Ref<Terrain3DAssets>& rAssets, const godot::Ref<Terrain3DMaterial>& rMaterial,
                                               const Terrain3DTerrainMegaVT& rMegaVT, const Terrain3DComposedSurfaceEditJournal& rEdits,
                                               const godot::String& rDataDirectory, t3dCompSurfEditorCandidateResult_t& rResult );

#endif
