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

#ifndef __SIR_VGCONTRACTS_H__
#define __SIR_VGCONTRACTS_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Mesh/SIRVGFramePlan.h>
#include <Resources/SIRResourceTypes.h>
#include <Resources/SIRProviderCompletion.h>
#include <Renderer/SIRGraphicsDriverABI.h>

#include <type_traits>

// Mesh-owned cross-system contract. GRDG is the sole source publisher; World
// and Streaming own demand; each native provider owns its physical backing.
// These views do not grant publication, allocation, or submission authority.
static constexpr uint32_t SIR_VG_CONTRACT_VERSION = 1;

enum class SIRVGContractResult : uint8_t {
	SIR_VG_CONTRACT_OK = 0,
	SIR_VG_CONTRACT_INVALID_ARGUMENT,
	SIR_VG_CONTRACT_STALE_REVISION,
	SIR_VG_CONTRACT_STALE_GENERATION,
	SIR_VG_CONTRACT_NOT_READY,
	SIR_VG_CONTRACT_UNSUPPORTED,
	SIR_VG_CONTRACT_VERSION_MISMATCH
};

// A typed SIRResourceKey is the canonical serialized identity. The explicit
// mesh representation revision is the GRDG published content revision.
// nResourceTypeId is supplied by the registered GRDG resource type owner.
typedef struct sirVGResourceBinding_s
{
	sirMeshKey_t keyMesh;
	SIRResourceKey keyResource;
	uint64_t nPublishedRevision;
	uint32_t nContractVersion;
	uint32_t nResourceTypeId;
} sirVGResourceBinding_t;

enum class SIRVGPageState : uint32_t {
	SIR_VG_PAGE_UNLOADED = 0,
	SIR_VG_PAGE_REQUESTED = 1,
	SIR_VG_PAGE_PREPARING = 2,
	SIR_VG_PAGE_RESIDENT = 3,
	SIR_VG_PAGE_RETIRING = 4
};

// Immutable generation-scoped observation, not an allocator or page table.
// The source pin and provider-owned native backing must remain live until all
// readers and required provider lanes have completed. Root fallback must be
// admitted before any non-root page can be selected for rendering.
typedef struct sirVGPageObservation_s
{
	sirVGResourceBinding_t resource;
	sirProviderCompletionSet_t required;
	sirProviderCompletionSet_t observed;
	uint64_t nPageId;
	uint32_t nPageGeneration;
	uint32_t nSourcePinGeneration;
	uint32_t nDeviceGeneration;
	uint32_t nManifestGeneration;
	uint32_t nRequestEpoch;
	SIRVGPageState eState;
	uint8_t bRootFallbackResident;
	uint8_t bSourcePinned;
	uint8_t szReserved[2];
} sirVGPageObservation_t;

// Describes a capability checked on the same device generation as the frame.
// CPU indexed draws are the universal baseline; GPU acceleration is optional.
enum class SIRVGExecutionTier : uint32_t {
	SIR_VG_EXECUTION_CPU_INDEXED = 1,
	SIR_VG_EXECUTION_GPU_INDIRECT = 2,
	SIR_VG_EXECUTION_GPU_MESH = 3
};

typedef struct sirVGExecutionChoice_s
{
	uint32_t nContractVersion;
	uint32_t nDeviceGeneration;
	SIRVGExecutionTier eTier;
	uint32_t nPipelineModel;
} sirVGExecutionChoice_t;

SIRVGContractResult sirVGBindResource( const sirMeshKey_t& mesh, const SIRResourceKey& resource, uint64_t nPublishedRevision,
                                         sirVGResourceBinding_t *pBinding ) noexcept;
SIRVGContractResult sirVGValidateResourceBinding( const sirVGResourceBinding_t& binding ) noexcept;
SIRVGContractResult sirVGValidatePageObservation( const sirVGPageObservation_t& page, const sirVGFramePlan_t& plan ) noexcept;
SIRVGContractResult sirVGSelectExecution( const sirGraphicsABICapabilities_t& device, const sirGraphicsABIPipelineModelCaps_t& model,
                                            sirVGExecutionChoice_t *pChoice ) noexcept;
SIRVGContractResult sirVGEncodeIndexedDraw( const sirVGDrawRange_t& range, const sirVGExecutionChoice_t& choice,
                                              sirGraphicsABIDrawIndexed_t *pNativeDraw ) noexcept;

static_assert( sizeof( sirMeshKey_t ) == 32 && sizeof( sirMeshHandle_t ) == 16, "mesh identities changed" );
static_assert( sizeof( SIRResourceKey ) == 16 && sizeof( sirProviderCompletionSet_t ) == 48, "GRDG/provider ABI changed" );
static_assert( sizeof( sirGraphicsABIDrawIndexed_t ) == 24, "graphics draw ABI changed" );
static_assert( offsetof( sirGraphicsABIDrawIndexed_t, nVertexOffset ) == 12, "base vertex offset changed" );
static_assert( offsetof( sirGraphicsABIDrawIndexed_t, nFirstInstance ) == 16, "first instance offset changed" );
static_assert( std::is_standard_layout<sirVGResourceBinding_t>::value && std::is_trivially_copyable<sirVGPageObservation_t>::value,
               "portable views must be POD" );

#endif // __SIR_VGCONTRACTS_H__
