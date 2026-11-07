// Copyright © 2023-2026 Cory Petkovsek, Roope Palmroos, and Contributors.

#ifndef TERRAIN3D_VT_REFERENCE_ENCODER_H
#define TERRAIN3D_VT_REFERENCE_ENCODER_H

#include "authoring/vt/build/terrain_3d_vt_cooker.h"

// Cook implementation revision, independent of the frozen V1 format/binding ABI.
#define T3D_VT_REFERENCE_BC1_CODEC_REVISION 2u
#define T3D_VT_REFERENCE_ALBEDO_CODEC_REVISION 3u

void t3d_vt_reference_encoder_make_interface( t3dVTEncoderInterface_t& rEncoder );

#endif // TERRAIN3D_VT_REFERENCE_ENCODER_H
