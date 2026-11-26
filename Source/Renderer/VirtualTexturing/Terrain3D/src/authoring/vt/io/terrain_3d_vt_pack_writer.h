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
