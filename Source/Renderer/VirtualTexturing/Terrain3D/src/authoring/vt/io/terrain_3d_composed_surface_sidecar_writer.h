// Editor-only write/replace boundary for a fully validated composed sidecar.
#ifndef TERRAIN3D_COMPOSED_SURFACE_SIDECAR_WRITER_H
#define TERRAIN3D_COMPOSED_SURFACE_SIDECAR_WRITER_H
#include <stdint.h>

// pDestination is a platform-native path supplied by the owner; serialized
// payload must already pass t3d_comp_surf_validate_sidecar_bytes(..., true).
// Caller serializes cooks per destination; the temporary path is exclusive.
bool t3d_comp_surf_write_sidecar_cold( const char *pDestination, const uint8_t *pBytes, uint64_t nBytes );
#endif
