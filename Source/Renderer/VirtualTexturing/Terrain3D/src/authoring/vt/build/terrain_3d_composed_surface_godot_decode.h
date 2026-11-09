#ifndef TERRAIN3D_COMPOSED_SURFACE_V1_GODOT_DECODE_H
#define TERRAIN3D_COMPOSED_SURFACE_V1_GODOT_DECODE_H
#include <stdint.h>
// Editor-only cold decoder; intended as t3dComposedSurfacePackDecodeFn.
bool t3d_composed_surface_pack_decode_godot_image( void *pUser, uint32_t nPhysicalFamily, const uint8_t *pEncoded, uint32_t nEncodedBytes,
                                                   uint8_t *pRgba, uint32_t nRgbaCapacity );
#endif
