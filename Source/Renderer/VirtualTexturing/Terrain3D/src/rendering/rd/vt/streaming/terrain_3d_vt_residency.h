// Copyright © 2023-2026 Cory Petkovsek, Roope Palmroos, and Contributors.

#ifndef TERRAIN3D_VT_RESIDENCY_H
#define TERRAIN3D_VT_RESIDENCY_H

#include "Terrain3DTypes.h"
#include "Terrain3DConstants.h"

#include "rendering/rd/vt/core/terrain_3d_vt_contract.h"
#include "Terrain3DTypes.h"
#include "Terrain3DConstants.h"

#include <stdint.h>
#include <EASTL/vector.h>

// Legacy channel-owned allocation ABI retained only for V1 pack compatibility.

static_assert( sizeof( t3dVTAllocationRecord_t ) == 32, "t3dVTAllocationRecord_t ABI mismatch" );
static_assert( sizeof( t3dVTPageTableEntry_t ) == 16, "t3dVTPageTableEntry_t ABI mismatch" );
static_assert( sizeof( t3dVTGpuCounters_t ) == 32, "t3dVTGpuCounters_t ABI mismatch" );
static_assert( sizeof( t3dVTRuntimeRequest_t ) == 8, "t3dVTRuntimeRequest_t ABI mismatch" );

#include <stdint.h>

// Sparse V1 residency uses the runtime-normalized allocation/page tables;
// physical storage remains format-family based and shared across logical channels.
// T3TV

// Runtime source metadata for the authored compact mip chain stored in a sparse
// V1 tail payload. The chain remains in its cooked horizontal layout.

void t3d_vt_default_residency_policy( t3dVTResidencyPolicy_t& rPolicy );
bool t3d_vt_validate_residency_policy( const t3dVTResidencyPolicy_t& pPolicy );
uint16_t t3d_vt_residency_category_reference_debug( const t3dVTDatabaseView_t& pDatabase, uint32_t pVirtualPage );
uint16_t t3d_vt_residency_temperature_reference_debug( const t3dVTResidencyState_t& pState, const t3dVTPhysicalSlot_t& pSlot,
                                                       uint32_t pFrameId );
bool t3d_vt_retirement_ready_reference_debug( const t3dVTResidencyState_t& pState, const t3dVTRetiredPageSet_t& pRetired,
                                              uint32_t pFrameId );

uint64_t t3d_vt_compute_pack_bytes( const t3dVTPackSource_t& pSource );
bool t3d_vt_write_pack_cold( const t3dVTPackSource_t& pSource, void *pDestination, uint64_t pDestinationBytes );
// The caller owns decoded records for the whole lifetime of rView. This prevents
// type-punning arbitrary serialized bytes into typed C++ arrays.
struct t3dVTLegacyDecodeStorage_t
{
	t3dVTFileHeader_t Header{};
	eastl::vector<t3dVTAllocationRecord_t> Allocations;
	eastl::vector<t3dVTPageFileRecord_t> Pages;
	eastl::vector<t3dVTPayloadRecord_t> Payloads;
};
bool t3d_vt_open_pack_cold( const void *pBytes, uint64_t pByteCount, t3dVTLegacyDecodeStorage_t& rStorage, t3dVTDatabaseView_t& rView );

bool t3d_vt_initialize_residency_cold( const t3dVTDatabaseView_t& pDatabase, const t3dVTResidencyBuffers_t& pBuffers,
                                       uint32_t pDatabaseGeneration, uint32_t pRetirementDelayFrames, t3dVTResidencyState_t& rState );
bool t3d_vt_commit_neutral_backing_warm( t3dVTResidencyState_t& rState, uint32_t pPhysicalSlot );
bool t3d_vt_finalize_neutral_publication_warm( t3dVTResidencyState_t& rState );
bool t3d_vt_finalize_package_publication_warm( t3dVTResidencyState_t& rState );
uint32_t t3d_vt_resolve_page_hot( t3dVTResidencyState_t& rState, uint32_t pVirtualPage, uint16_t& rResolvedMip );
bool t3d_vt_allocation_lookup_hot( const t3dVTResidencyState_t& pState, uint32_t pAllocationId, const t3dVTAllocation_t *& rAllocation );
bool t3d_vt_compute_page( const t3dVTAllocation_t& pAllocation, uint32_t pMip, uint32_t pPageX, uint32_t pPageY, uint32_t& rVirtualPage );
bool t3d_vt_resolve_allocation_page_hot( t3dVTResidencyState_t& rState, uint32_t pAllocationId, uint32_t pRequestedPage,
                                         uint16_t pRequestedMip, t3dVTResolveResult_t& rResult );
bool t3d_vt_resolve_allocation_page_reference_debug( const t3dVTResidencyState_t& rState, uint32_t pAllocationId, uint32_t pRequestedPage,
                                                     uint16_t pRequestedMip, t3dVTResolveResult_t& rResult );
bool t3d_vt_resolve_allocation_hot( t3dVTResidencyState_t& rState, uint32_t pAllocationId, uint16_t pRequestedMip, uint32_t pPageX,
                                    uint32_t pPageY, t3dVTResolveResult_t& rResult );
bool t3d_vt_touch_page_hot( t3dVTResidencyState_t& rState, uint32_t pVirtualPage, uint32_t pFrameId );
bool t3d_vt_reserve_upload_hot( t3dVTResidencyState_t& rState, uint32_t pVirtualPage, uint16_t pWantedMip, uint32_t pFrameId,
                                t3dVTUploadReservation_t& rReservation );
bool t3d_vt_can_commit_upload_hot( const t3dVTResidencyState_t& pState, const t3dVTUploadReservation_t& pReservation );
bool t3d_vt_commit_upload_hot( t3dVTResidencyState_t& rState, const t3dVTUploadReservation_t& pReservation, uint32_t pUploadedChannelMask,
                               uint32_t pFrameId );
bool t3d_vt_begin_eviction_hot( t3dVTResidencyState_t& rState, uint32_t pPhysicalSlot, uint32_t pVictimPage, uint32_t pFrameId );
bool t3d_vt_finalize_eviction_hot( t3dVTResidencyState_t& rState, uint32_t pPhysicalSlot, uint32_t pVictimPage, uint32_t pFrameId );
bool t3d_vt_service_retired_slots_hot( t3dVTResidencyState_t& rState, uint32_t pFrameId );
void t3d_vt_abort_upload_hot( t3dVTResidencyState_t& rState, const t3dVTUploadReservation_t& pReservation );
bool t3d_vt_validate_residency_reference_debug( const t3dVTResidencyState_t& pState );
bool t3d_vt_allocation_page_id( const t3dVTAllocation_t& pAllocation, uint32_t pMip, uint32_t pPageX, uint32_t pPageY,
                                uint32_t& rVirtualPage );

bool t3d_vt_predict_page_rect_reference_debug( const t3dVTAllocation_t& pAllocation, const float pUvMin[2], const float pUvMax[2],
                                               uint32_t pWantedMip, uint32_t pMaximumPages, t3dVTPredictedPageRect_t& rRect );

static_assert( sizeof( t3dVTFileHeader_t ) == 128, "t3dVTFileHeader_t ABI mismatch" );
static_assert( sizeof( t3dVTPageFileRecord_t ) == 32, "t3dVTPageFileRecord_t ABI mismatch" );
static_assert( sizeof( t3dVTPayloadRecord_t ) == 32, "t3dVTPayloadRecord_t ABI mismatch" );
static_assert( sizeof( t3dVTPhysicalSlot_t ) == 32, "P17 physical page-set slot ABI mismatch" );
static_assert( sizeof( t3dVTRetiredPageSet_t ) == 16, "P17 retirement ring record ABI mismatch" );
static_assert( sizeof( t3dVTUploadReservation_t ) == 24, "P17 upload reservation ABI mismatch" );

#endif // TERRAIN3D_VT_RESIDENCY_H
