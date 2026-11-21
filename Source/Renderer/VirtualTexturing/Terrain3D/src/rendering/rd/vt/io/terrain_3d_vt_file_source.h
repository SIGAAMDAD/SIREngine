// File-backed V1 VT payload source. One worker owns the mutable read cursor.
#ifndef TERRAIN3D_VT_FILE_SOURCE_H
#define TERRAIN3D_VT_FILE_SOURCE_H

#include "Terrain3DTypes.h"
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/string.hpp>
#include <stdio.h>

struct t3dVTFileSource_s
{
	FILE *pNativeFile = NULL;
	godot::Ref<godot::FileAccess> m_File;
	uint64_t nFileBytes = 0u;
	uint64_t nPayloadOffset = 0u;
	uint64_t nPayloadBytes = 0u;
	uint64_t nSignature = 0u;
	uint32_t nGeneration = 0u;
	uint64_t nReadCount = 0u;
};

bool t3d_vt_file_source_open_cold( const godot::String& rPath, t3dVTFileSource_s& rSource, godot::PackedByteArray& rMetadata,
                                   bool bStrictHash = true );
// Only a single I/O worker may use the handle concurrently. CPU-cold tail reads
// run before worker launch; publication quiesces before replacing the source.
bool t3d_vt_file_source_read_payload( t3dVTFileSource_s& rSource, uint64_t nOffset, uint8_t *pDestination, uint32_t nLength );
void t3d_vt_file_source_close( t3dVTFileSource_s& rSource );

#endif
