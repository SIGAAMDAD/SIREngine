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

// Copyright © 2023-2026 Cory Petkovsek, Roope Palmroos, and Contributors.

#ifndef TERRAIN3D_VT_REFERENCE_ENCODER_H
#define TERRAIN3D_VT_REFERENCE_ENCODER_H

#include "authoring/vt/build/terrain_3d_vt_cooker.h"

// Cook implementation revision, independent of the frozen V1 format/binding ABI.
#define T3D_VT_REFERENCE_BC1_CODEC_REVISION 2u
#define T3D_VT_REFERENCE_ALBEDO_CODEC_REVISION 3u

void t3d_vt_reference_encoder_make_interface( t3dVTEncoderInterface_t& rEncoder );

#endif // TERRAIN3D_VT_REFERENCE_ENCODER_H
