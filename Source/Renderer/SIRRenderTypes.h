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

#ifndef __SIRENGINE_SIRRENDERTYPES_H__
#define __SIRENGINE_SIRRENDERTYPES_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRGraphicsTypes.h>

// ADDITIVE SOURCE-LEVEL CONTRACT. Existing SIRGraphicsTypes.h is authoritative
// for enum values, 16-byte graphics handles, result codes, memory classes and
// pipeline-era recipes. This header defines only missing RHI concepts.
// These source structures are NOT a new binary module ABI revision.

// Buffer texturing is missing from the current shared usage/binding enums.
// Reserve additive values for their one-time canonical migration.
constexpr uint32_t SIR_RENDER_USAGE_TEXEL = 1U << 10;
constexpr sirGraphicsBindingType_t SIR_RENDER_BINDING_TEXEL_BUFFER = static_cast<sirGraphicsBindingType_t>( 5U );

// Reserved extension handle tags. Integrate into sirGraphicsHandleType_t
// when binary ABI and native type validators are migrated together.
constexpr sirGraphicsHandleType_t SIR_RENDER_HANDLE_QUERY_POOL = static_cast<sirGraphicsHandleType_t>( 8U );
constexpr sirGraphicsHandleType_t SIR_RENDER_HANDLE_ACCELERATION_STRUCTURE = static_cast<sirGraphicsHandleType_t>( 9U );

typedef enum sirRenderLoadOp_e : uint32_t
{
	SIR_RENDER_LOAD_OP_LOAD = 0,
	SIR_RENDER_LOAD_OP_CLEAR = 1,
	SIR_RENDER_LOAD_OP_DONT_CARE = 2
} sirRenderLoadOp_t;

typedef enum sirRenderStoreOp_e : uint32_t
{
	SIR_RENDER_STORE_OP_STORE = 0,
	SIR_RENDER_STORE_OP_DONT_CARE = 1
} sirRenderStoreOp_t;

typedef enum sirRenderAspect_e : uint32_t
{
	SIR_RENDER_ASPECT_COLOR = 1U << 0,
	SIR_RENDER_ASPECT_DEPTH = 1U << 1,
	SIR_RENDER_ASPECT_STENCIL = 1U << 2
} sirRenderAspect_t;

typedef enum sirRenderStage_e : uint64_t
{
	SIR_RENDER_STAGE_TOP = 1ULL << 0,
	SIR_RENDER_STAGE_INDIRECT = 1ULL << 1,
	SIR_RENDER_STAGE_VERTEX_INPUT = 1ULL << 2,
	SIR_RENDER_STAGE_VERTEX_SHADER = 1ULL << 3,
	SIR_RENDER_STAGE_FRAGMENT_SHADER = 1ULL << 4,
	SIR_RENDER_STAGE_EARLY_DEPTH = 1ULL << 5,
	SIR_RENDER_STAGE_LATE_DEPTH = 1ULL << 6,
	SIR_RENDER_STAGE_COLOR_OUTPUT = 1ULL << 7,
	SIR_RENDER_STAGE_COMPUTE_SHADER = 1ULL << 8,
	SIR_RENDER_STAGE_TRANSFER = 1ULL << 9,
	SIR_RENDER_STAGE_HOST = 1ULL << 10,
	SIR_RENDER_STAGE_MESH_SHADER = 1ULL << 11,
	SIR_RENDER_STAGE_RAY_TRACING = 1ULL << 12,
	SIR_RENDER_STAGE_BOTTOM = 1ULL << 13,
	SIR_RENDER_STAGE_ALL_GRAPHICS = 1ULL << 14,
	SIR_RENDER_STAGE_ALL_COMMANDS = 1ULL << 15
} sirRenderStage_t;

typedef enum sirRenderAccess_e : uint64_t
{
	SIR_RENDER_ACCESS_INDIRECT_READ = 1ULL << 0,
	SIR_RENDER_ACCESS_VERTEX_INDEX_READ = 1ULL << 1,
	SIR_RENDER_ACCESS_UNIFORM_READ = 1ULL << 2,
	SIR_RENDER_ACCESS_SHADER_SAMPLED_READ = 1ULL << 3,
	SIR_RENDER_ACCESS_SHADER_STORAGE_READ = 1ULL << 4,
	SIR_RENDER_ACCESS_SHADER_STORAGE_WRITE = 1ULL << 5,
	SIR_RENDER_ACCESS_COLOR_ATTACHMENT_READ = 1ULL << 6,
	SIR_RENDER_ACCESS_COLOR_ATTACHMENT_WRITE = 1ULL << 7,
	SIR_RENDER_ACCESS_DEPTH_STENCIL_READ = 1ULL << 8,
	SIR_RENDER_ACCESS_DEPTH_STENCIL_WRITE = 1ULL << 9,
	SIR_RENDER_ACCESS_TRANSFER_READ = 1ULL << 10,
	SIR_RENDER_ACCESS_TRANSFER_WRITE = 1ULL << 11,
	SIR_RENDER_ACCESS_HOST_READ = 1ULL << 12,
	SIR_RENDER_ACCESS_HOST_WRITE = 1ULL << 13,
	SIR_RENDER_ACCESS_MEMORY_READ = 1ULL << 14,
	SIR_RENDER_ACCESS_MEMORY_WRITE = 1ULL << 15,
	SIR_RENDER_ACCESS_ACCELERATION_STRUCTURE_READ = 1ULL << 16,
	SIR_RENDER_ACCESS_ACCELERATION_STRUCTURE_WRITE = 1ULL << 17
} sirRenderAccess_t;

typedef enum sirRenderBarrierPhase_e : uint32_t
{
	SIR_RENDER_BARRIER_PHASE_FULL = 0,
	SIR_RENDER_BARRIER_PHASE_RELEASE = 1,
	SIR_RENDER_BARRIER_PHASE_ACQUIRE = 2
} sirRenderBarrierPhase_t;

typedef enum sirRenderQueryType_e : uint32_t
{
	SIR_RENDER_QUERY_TIMESTAMP = 0,
	SIR_RENDER_QUERY_OCCLUSION = 1,
	SIR_RENDER_QUERY_PIPELINE_STATISTICS = 2
} sirRenderQueryType_t;

typedef enum sirRenderAccelerationStructureType_e : uint32_t
{
	SIR_RENDER_ACCELERATION_STRUCTURE_BOTTOM_LEVEL = 0,
	SIR_RENDER_ACCELERATION_STRUCTURE_TOP_LEVEL = 1
} sirRenderAccelerationStructureType_t;

typedef enum sirRenderMemoryResidency_e : uint32_t
{
	SIR_RENDER_MEMORY_RESIDENCY_UNKNOWN = 0,
	SIR_RENDER_MEMORY_RESIDENCY_RESIDENT = 1,
	SIR_RENDER_MEMORY_RESIDENCY_EVICTED = 2
} sirRenderMemoryResidency_t;

// For a texture view, ranges name mip/layer intervals and aspects. Buffer
// views use sirRenderBufferRange_t. Counts must be nonzero.
typedef struct sirRenderSubresourceRange_s
{
	uint32_t nBaseMip;
	uint32_t nMipCount;
	uint32_t nBaseLayer;
	uint32_t nLayerCount;
	uint32_t nAspectMask;
} sirRenderSubresourceRange_t;

typedef struct sirRenderBufferRange_s
{
	uint64_t nOffsetBytes;
	uint64_t nSizeBytes;
} sirRenderBufferRange_t;

// hView and hResolveView are borrowed views. A framebuffer owns no texture
// memory. Load/store operations are per attachment, not per texture.
typedef struct sirRenderAttachmentDesc_s
{
	sirGraphicsHandle_t hView;
	sirGraphicsHandle_t hResolveView;
	sirRenderLoadOp_t eLoad;
	sirRenderStoreOp_t eStore;
	float szClearColor[4];
	uint32_t nAspectMask;
} sirRenderAttachmentDesc_t;

// Depth and stencil have distinct load/store semantics on modern APIs.
typedef struct sirRenderDepthStencilAttachmentDesc_s
{
	sirGraphicsHandle_t hView;
	sirRenderLoadOp_t eDepthLoad;
	sirRenderStoreOp_t eDepthStore;
	sirRenderLoadOp_t eStencilLoad;
	sirRenderStoreOp_t eStencilStore;
	float fClearDepth;
	uint32_t nClearStencil;
} sirRenderDepthStencilAttachmentDesc_t;

typedef struct sirRenderFramebufferDesc_s
{
	const sirRenderAttachmentDesc_t *pColors;
	uint32_t nColorCount;
	sirRenderDepthStencilAttachmentDesc_t depthStencil;
	uint32_t nWidth;
	uint32_t nHeight;
	uint32_t nLayers;
	uint32_t nSampleCount;
} sirRenderFramebufferDesc_t;

// Extends the original simplified sampler state for comparison sampling,
// border color and LOD bias. Migration needs a canonical createSampler input.
typedef struct sirRenderSamplerDesc_s
{
	sirGraphicsSamplerDesc_t base;
	sirGraphicsCompareOp_t eCompare;
	float szBorderColor[4];
	float fLodBias;
	bool bComparisonEnabled;
	bool bUnnormalizedCoordinates;
} sirRenderSamplerDesc_t;

// Typed aspect selection for a view; avoids depth/stencil ambiguity in the
// existing source view descriptor. Do not duplicate the backing allocation.
typedef struct sirRenderTextureViewDesc_s
{
	sirGraphicsViewDesc_t base;
	uint32_t nAspectMask;
} sirRenderTextureViewDesc_t;

// Shader data is borrowed for synchronous creation. Persistent backend caches
// or shader libraries must hold a separately owned, canonical cooked artifact.
typedef struct sirRenderShaderDesc_s
{
	sirGraphicsShaderCode_t code;
	uint32_t nStageMask;
	uint32_t nRequiredFeatures;
	uint64_t nReflectionHash;
} sirRenderShaderDesc_t;

// Binding records are borrowed for creation only. Native implementations
// validate and copy durable metadata before returning success.
typedef struct sirRenderBindingLayoutDesc_s
{
	const sirGraphicsBindingDesc_t *pBindings;
	uint32_t nBindingCount;
	uint64_t nSignatureHash;
} sirRenderBindingLayoutDesc_t;

// One logical dependency. Native drivers derive layouts, access masks and
// queue ownership operations from this shared representation. A queue change
// must be paired with a real ownership transfer where required by the API.
typedef struct sirRenderBarrierDesc_s
{
	sirGraphicsBarrierType_t eType;
	sirRenderBarrierPhase_t ePhase;
	sirGraphicsHandle_t hResource;
	sirGraphicsHandle_t hAliasedResource;
	sirGraphicsState_t eBefore;
	sirGraphicsState_t eAfter;
	sirGraphicsQueue_t eSourceQueue;
	sirGraphicsQueue_t eDestinationQueue;
	uint64_t nSourceStages;
	uint64_t nDestinationStages;
	uint64_t nSourceAccess;
	uint64_t nDestinationAccess;
	sirRenderSubresourceRange_t subresources;
	sirRenderBufferRange_t buffer;
} sirRenderBarrierDesc_t;

typedef struct sirRenderTextureCopyDesc_s
{
	sirGraphicsHandle_t hSource;
	sirGraphicsHandle_t hDestination;
	sirRenderSubresourceRange_t source;
	sirRenderSubresourceRange_t destination;
	uint32_t nSourceX;
	uint32_t nSourceY;
	uint32_t nSourceZ;
	uint32_t nDestinationX;
	uint32_t nDestinationY;
	uint32_t nDestinationZ;
	uint32_t nWidth;
	uint32_t nHeight;
	uint32_t nDepth;
} sirRenderTextureCopyDesc_t;

// Engine-canonical indirect argument memory layouts, independent of native
// API call structures. Strides and offsets are validated by the device.
typedef struct sirRenderDrawIndirectArgs_s
{
	uint32_t nVertexCount;
	uint32_t nInstanceCount;
	uint32_t nFirstVertex;
	uint32_t nFirstInstance;
} sirRenderDrawIndirectArgs_t;

typedef struct sirRenderDrawIndexedIndirectArgs_s
{
	uint32_t nIndexCount;
	uint32_t nInstanceCount;
	uint32_t nFirstIndex;
	int32_t nVertexOffset;
	uint32_t nFirstInstance;
} sirRenderDrawIndexedIndirectArgs_t;

typedef struct sirRenderDispatchIndirectArgs_s
{
	uint32_t nGroupCountX;
	uint32_t nGroupCountY;
	uint32_t nGroupCountZ;
} sirRenderDispatchIndirectArgs_t;

typedef struct sirRenderIndirectDrawDesc_s
{
	sirGraphicsHandle_t hArgumentBuffer;
	sirGraphicsHandle_t hCountBuffer;
	uint64_t nArgumentOffsetBytes;
	uint64_t nCountOffsetBytes;
	uint32_t nMaximumDrawCount;
	uint32_t nStrideBytes;
	bool bIndexed;
	bool bUseCountBuffer;
} sirRenderIndirectDrawDesc_t;

typedef struct sirRenderIndirectDispatchDesc_s
{
	sirGraphicsHandle_t hArgumentBuffer;
	uint64_t nArgumentOffsetBytes;
} sirRenderIndirectDispatchDesc_t;

typedef struct sirRenderQueryPoolDesc_s
{
	sirRenderQueryType_t eType;
	uint32_t nQueryCount;
	uint32_t nStatisticsMask;
} sirRenderQueryPoolDesc_t;

typedef struct sirRenderQueryResolveDesc_s
{
	sirGraphicsHandle_t hQueryPool;
	sirGraphicsHandle_t hDestinationBuffer;
	uint32_t nFirstQuery;
	uint32_t nQueryCount;
	uint64_t nDestinationOffsetBytes;
	uint64_t nStrideBytes;
	bool bWaitForResults;
	bool bWithAvailability;
} sirRenderQueryResolveDesc_t;

typedef struct sirRenderMemoryRequirements_s
{
	uint64_t nSizeBytes;
	uint64_t nAlignmentBytes;
	uint64_t nHeapClassMask;
	uint64_t nGranularityBytes;
	bool bDedicatedRequired;
	bool bAliasingSupported;
} sirRenderMemoryRequirements_t;

typedef struct sirRenderMemoryStatus_s
{
	sirGraphicsHandle_t hResource;
	sirRenderMemoryResidency_t eResidency;
	uint64_t nCommittedBytes;
	uint64_t nResidentBytes;
	bool bExternalBacking;
} sirRenderMemoryStatus_t;

typedef struct sirRenderQueueCapabilities_s
{
	sirGraphicsQueue_t eQueue;
	bool bTimestampSupported;
	bool bConcurrentSubmission;
	double fNanosecondsPerTimestampTick;
} sirRenderQueueCapabilities_t;

typedef struct sirRenderExtendedCapabilities_s
{
	sirGraphicsCapabilities_t base;
	uint64_t nUniformBufferOffsetAlignment;
	uint64_t nStorageBufferOffsetAlignment;
	uint64_t nTexelBufferOffsetAlignment;
	uint64_t nTextureCopyRowAlignment;
	uint64_t nShaderBindingTableAlignment;
	uint64_t nShaderBindingRecordAlignment;
	uint32_t nShaderGroupHandleBytes;
	uint32_t nMaximumQueryPools;
	uint32_t nMaximumRayRecursionDepth;
} sirRenderExtendedCapabilities_t;

// Optional ray-tracing contracts. The backing allocation is an existing
// graphics buffer; acceleration structures and SBT records borrow it.
typedef struct sirRenderAccelerationStructureDesc_s
{
	sirRenderAccelerationStructureType_t eType;
	sirGraphicsHandle_t hBackingBuffer;
	sirRenderBufferRange_t range;
	uint32_t nBuildFlags;
	uint32_t nGeometryCount;
} sirRenderAccelerationStructureDesc_t;

typedef struct sirRenderShaderBindingRegion_s
{
	uint64_t nOffsetBytes;
	uint64_t nSizeBytes;
	uint64_t nStrideBytes;
} sirRenderShaderBindingRegion_t;

typedef struct sirRenderShaderBindingTableDesc_s
{
	sirGraphicsHandle_t hBackingBuffer;
	sirRenderShaderBindingRegion_t rayGeneration;
	sirRenderShaderBindingRegion_t miss;
	sirRenderShaderBindingRegion_t hit;
	sirRenderShaderBindingRegion_t callable;
} sirRenderShaderBindingTableDesc_t;

typedef struct sirRenderTraceRaysDesc_s
{
	sirGraphicsHandle_t hPipeline;
	sirRenderShaderBindingTableDesc_t table;
	uint32_t nWidth;
	uint32_t nHeight;
	uint32_t nDepth;
} sirRenderTraceRaysDesc_t;

static_assert( sizeof( sirRenderDrawIndirectArgs_t ) == 16, "Indirect draw args are exactly 16 bytes" );
static_assert( sizeof( sirRenderDrawIndexedIndirectArgs_t ) == 20, "Indirect indexed args are exactly 20 bytes" );
static_assert( sizeof( sirRenderDispatchIndirectArgs_t ) == 12, "Indirect dispatch args are exactly 12 bytes" );
static_assert( sizeof( sirGraphicsHandle_t ) == 16, "RHI uses the existing 16-byte handle" );
static_assert( sizeof( sirGraphicsCompletion_t ) == 16, "RHI uses the existing 16-byte completion token" );

#endif // __SIRENGINE_SIRRENDERTYPES_H__
