/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIRENGINE_SIRVGCONTRACTS_H__
#define __SIRENGINE_SIRVGCONTRACTS_H__

#include <SIRPCH.hpp>

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

typedef enum sirVGContractResult_e : uint8_t
{
	SIR_VG_CONTRACT_OK = 0,
	SIR_VG_CONTRACT_INVALID_ARGUMENT,
	SIR_VG_CONTRACT_STALE_REVISION,
	SIR_VG_CONTRACT_STALE_GENERATION,
	SIR_VG_CONTRACT_NOT_READY,
	SIR_VG_CONTRACT_UNSUPPORTED,
	SIR_VG_CONTRACT_VERSION_MISMATCH
} sirVGContractResult_t;

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

typedef enum sirVGPageState_e : uint32_t
{
	SIR_VG_PAGE_UNLOADED = 0,
	SIR_VG_PAGE_REQUESTED = 1,
	SIR_VG_PAGE_PREPARING = 2,
	SIR_VG_PAGE_RESIDENT = 3,
	SIR_VG_PAGE_RETIRING = 4
} sirVGPageState_t;

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
	sirVGPageState_t eState;
	uint8_t bRootFallbackResident;
	uint8_t bSourcePinned;
	uint8_t szReserved[2];
} sirVGPageObservation_t;

// Describes a capability checked on the same device generation as the frame.
// CPU indexed draws are the universal baseline; GPU acceleration is optional.
typedef enum sirVGExecutionTier_e : uint32_t
{
	SIR_VG_EXECUTION_CPU_INDEXED = 1,
	SIR_VG_EXECUTION_GPU_INDIRECT = 2,
	SIR_VG_EXECUTION_GPU_MESH = 3
} sirVGExecutionTier_t;

typedef struct sirVGExecutionChoice_s
{
	uint32_t nContractVersion;
	uint32_t nDeviceGeneration;
	sirVGExecutionTier_t eTier;
	uint32_t nPipelineModel;
} sirVGExecutionChoice_t;

sirVGContractResult_t sirVGBindResource( const sirMeshKey_t& mesh, const SIRResourceKey& resource, uint64_t nPublishedRevision,
                                         sirVGResourceBinding_t *pBinding ) noexcept;
sirVGContractResult_t sirVGValidateResourceBinding( const sirVGResourceBinding_t& binding ) noexcept;
sirVGContractResult_t sirVGValidatePageObservation( const sirVGPageObservation_t& page, const sirVGFramePlan_t& plan ) noexcept;
sirVGContractResult_t sirVGSelectExecution( const sirGraphicsABICapabilities_t& device, const sirGraphicsABIPipelineModelCaps_t& model,
                                            sirVGExecutionChoice_t *pChoice ) noexcept;
sirVGContractResult_t sirVGEncodeIndexedDraw( const sirVGDrawRange_t& range, const sirVGExecutionChoice_t& choice,
                                              sirGraphicsABIDrawIndexed_t *pNativeDraw ) noexcept;

static_assert( sizeof( sirMeshKey_t ) == 24 && sizeof( sirMeshHandle_t ) == 16, "mesh identities changed" );
static_assert( sizeof( SIRResourceKey ) == 16 && sizeof( sirProviderCompletionSet_t ) == 48, "GRDG/provider ABI changed" );
static_assert( sizeof( sirGraphicsABIDrawIndexed_t ) == 24, "graphics draw ABI changed" );
static_assert( offsetof( sirGraphicsABIDrawIndexed_t, nVertexOffset ) == 12, "base vertex offset changed" );
static_assert( offsetof( sirGraphicsABIDrawIndexed_t, nFirstInstance ) == 16, "first instance offset changed" );
static_assert( std::is_standard_layout<sirVGResourceBinding_t>::value && std::is_trivially_copyable<sirVGPageObservation_t>::value,
               "portable views must be POD" );

#endif // __SIRENGINE_SIRVGCONTRACTS_H__
