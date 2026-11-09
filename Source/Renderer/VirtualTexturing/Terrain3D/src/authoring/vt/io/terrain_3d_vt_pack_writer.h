// Editor-only frozen VT V1 writer API.
#ifndef TERRAIN3D_VT_PACK_WRITER_H
#define TERRAIN3D_VT_PACK_WRITER_H
#include "data/formats/terrain_3d_vt_pack_format.h"

bool t3d_vt_pack_validate_source( const t3dVTSerializedPackSource_t& rSource );
uint64_t t3d_vt_pack_compute_metadata_bytes( const t3dVTSerializedPackSource_t& rSource );
uint64_t t3d_vt_pack_compute_bytes( const t3dVTSerializedPackSource_t& rSource );
bool t3d_vt_pack_write( const t3dVTSerializedPackSource_t& rSource, void *pDestination, uint64_t nDestinationBytes,
                        uint64_t& rWrittenBytes );
#endif // TERRAIN3D_VT_PACK_WRITER_H
