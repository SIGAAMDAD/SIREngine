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

#ifndef TERRAIN3D_COMPOSED_SURFACE_V1_GODOT_DECODE_H
#define TERRAIN3D_COMPOSED_SURFACE_V1_GODOT_DECODE_H
#include <stdint.h>
// Editor-only cold decoder; intended as t3dComposedSurfacePackDecodeFn.
bool t3d_composed_surface_pack_decode_godot_image( void *pUser, uint32_t nPhysicalFamily, const uint8_t *pEncoded, uint32_t nEncodedBytes,
                                                   uint8_t *pRgba, uint32_t nRgbaCapacity );
#endif
