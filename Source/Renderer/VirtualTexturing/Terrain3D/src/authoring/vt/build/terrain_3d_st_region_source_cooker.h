// Copyright 2026 Terrain3D contributors.
#ifndef TERRAIN3D_ST_REGION_SOURCE_COOKER_H
#define TERRAIN3D_ST_REGION_SOURCE_COOKER_H
#include <stdint.h>
#include <vector>

// Cold snapshot of canonical authored 128x128 images. Pointers are borrowed
// for the duration of cookCold; every source must already be fully resident.
struct t3dStAuthoredRegionSource_t
{
	const uint8_t *m_pHeightR16 = nullptr;
	const uint8_t *m_pControlRF = nullptr;
	const uint8_t *m_pColorRgba8 = nullptr;
	uint32_t m_nRegionIndex = 0u;
	uint32_t m_nComposedAllocation = 0u;
	uint32_t m_nComposedGeneration = 0u;
};

class Terrain3DStRegionSourceCooker
{
public:
	// Produces the atomic height+cutout companion of a verified COMPOSED V1
	// package. A failed bake never changes rOutput.
	static bool cookCold( const t3dStAuthoredRegionSource_t *pInputs, uint32_t nCount, uint32_t nVtPackageGeneration,
	                      std::vector<uint8_t>& rOutput );
};
#endif // TERRAIN3D_ST_REGION_SOURCE_COOKER_H
