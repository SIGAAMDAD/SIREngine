// Copyright © 2023-2026 Cory Petkovsek, Roope Palmroos, and Contributors.

#ifndef TERRAIN3D_VT_RESOURCE_PLAN_H
#define TERRAIN3D_VT_RESOURCE_PLAN_H

#include "Terrain3DTypes.h"
#include "Terrain3DConstants.h"

#include "rendering/rd/vt/core/terrain_3d_vt_contract.h"
#include "rendering/rd/world/core/terrain_3d_world_system_limits.h"

#include <stdint.h>

// GPU-P04: production VT cache is specialized to one sampler2DArray bank per channel.
// Physical-slot ABI remains bank:layer; bank is required to be zero in this specialization.

t3dVTGpuProfile_t t3d_vt_make_development_gpu_profile();
t3dVTGpuProfile_t t3d_vt_make_runtime_gpu_profile( const t3dWorldResourceLimits_t& rWorldLimits );
t3dVTGpuProfile_t t3d_vt_make_preview_gpu_profile();
bool t3d_vt_validate_gpu_profile( const t3dVTGpuProfile_t& pProfile );
bool t3d_vt_compute_bank_layout( uint32_t pPageCount, uint32_t pMaxArrayLayers, t3dVTBankLayout_t& rLayout );
bool t3d_vt_compute_resource_plan( const t3dWorldLimits_t& pLimits, const t3dVTGpuProfile_t& pProfile, t3dVTGpuResourcePlan_t& rPlan,
                                   uint32_t nActiveTailAtlasHeight = 0u );

// Reduce physical residency to fit a GPU resource cap, preserving the complete
// virtual address space and mandatory tail/table storage. Failure is transactional.
bool t3d_vt_fit_resource_plan_to_budget( t3dWorldLimits_t& rLimits, const t3dVTGpuProfile_t& rProfile, uint64_t budget_bytes,
                                         t3dVTGpuResourcePlan_t& rPlan, uint32_t active_tail_height = 0u );

// Full slots include the authored mip chain and gutters, not just mip zero.
bool t3d_vt_compute_active_tail_height_cold( const t3dVTTailMetadata_t *pTails, uint32_t nTailCount, uint32_t& rHeight );

#endif // TERRAIN3D_VT_RESOURCE_PLAN_H
