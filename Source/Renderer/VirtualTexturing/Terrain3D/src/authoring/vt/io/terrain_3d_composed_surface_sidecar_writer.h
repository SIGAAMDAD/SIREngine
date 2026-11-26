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

// Editor-only write/replace boundary for a fully validated composed sidecar.
#ifndef TERRAIN3D_COMPOSED_SURFACE_SIDECAR_WRITER_H
#define TERRAIN3D_COMPOSED_SURFACE_SIDECAR_WRITER_H
#include <stdint.h>

// pDestination is a platform-native path supplied by the owner; serialized
// payload must already pass t3d_comp_surf_validate_sidecar_bytes(..., true).
// Caller serializes cooks per destination; the temporary path is exclusive.
bool t3d_comp_surf_write_sidecar_cold( const char *pDestination, const uint8_t *pBytes, uint64_t nBytes );
#endif
