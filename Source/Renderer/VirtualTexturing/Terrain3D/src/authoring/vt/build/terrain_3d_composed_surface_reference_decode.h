#ifndef TERRAIN3D_COMPOSED_SURFACE_V1_REFERENCE_DECODE_H
#define TERRAIN3D_COMPOSED_SURFACE_V1_REFERENCE_DECODE_H

#include <stdint.h>

// Cold-only deterministic scalar block decoder for BC1, BC5, R8, and BC7
// mode 6 (the mode emitted by the V1 reference encoder). Other BC7 modes are
// rejected; use the Godot image decoder for those. Invalid BC7 modes and
// invalid spans do not modify the destination buffer.
bool t3d_comp_surf_decode_reference_tile( uint32_t nFamily, const uint8_t *pEncoded, uint32_t nEncodedBytes, uint8_t *pRgba,
                                          uint32_t nRgbaCapacity );

#endif
