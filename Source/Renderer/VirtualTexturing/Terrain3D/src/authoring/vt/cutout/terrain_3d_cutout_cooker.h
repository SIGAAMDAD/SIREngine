// Copyright 2026 Terrain3D contributors.
#ifndef TERRAIN3D_CUTOUT_COOKER_H
#define TERRAIN3D_CUTOUT_COOKER_H

#include <stdint.h>
#include <vector>
#include <stddef.h>

struct t3dStCutoutCookInput_t
{
	// Exact R16 height is not needed for masks. Control carries RF bit-pattern
	// as uint32 little-endian, not numerically converted float values.
	const uint8_t *m_pControlLe = nullptr;  // 128x128x4
	const uint8_t *m_pColorRgba8 = nullptr; // 128x128x4
	uint32_t m_nRegionIndex = 0u;
	uint32_t m_nGeneration = 0u;
	uint64_t m_nSourceSignature = 0u;
	uint64_t m_nRecipeSignature = 0u;
};

class Terrain3DCutoutCooker
{
public:
	// Serialize full header and exact payload in a private candidate. Writes
	// rOutput only on success, leaving last-good output available on failure.
	static bool cookRegionCold( const t3dStCutoutCookInput_t& rSource, std::vector<uint8_t>& rOutput );
};

#endif // TERRAIN3D_CUTOUT_COOKER_H
