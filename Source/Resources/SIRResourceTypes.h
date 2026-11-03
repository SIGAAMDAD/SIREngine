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

#ifndef __SIRENGINE_SIRRESOURCETYPES_H__
#define __SIRENGINE_SIRRESOURCETYPES_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <stdint.h>

// These are data-representation categories, not rendering/physics/audio classes.
typedef enum sirResourceKind_e : uint8_t
{
	SIR_RESOURCE_KIND_BLOB = 0,
	SIR_RESOURCE_KIND_IMAGE,
	SIR_RESOURCE_KIND_GEOMETRY,
	SIR_RESOURCE_KIND_FIELD,
	SIR_RESOURCE_KIND_TABLE,
	SIR_RESOURCE_KIND_GRAPH,
	SIR_RESOURCE_KIND_HIERARCHY,
	SIR_RESOURCE_KIND_TIME_SERIES,
	SIR_RESOURCE_KIND_SAMPLE_STREAM,
	SIR_RESOURCE_KIND_COMPOSITION
} sirResourceKind_t;
typedef sirResourceKind_t SIRResourceKind;

typedef enum sirResourceState_e : uint8_t
{
	SIR_RESOURCE_STATE_UNLOADED = 0,
	SIR_RESOURCE_STATE_REQUESTED,
	SIR_RESOURCE_STATE_STAGING,
	SIR_RESOURCE_STATE_RESIDENT,
	SIR_RESOURCE_STATE_EVICTING,
	SIR_RESOURCE_STATE_FAILED
} sirResourceState_t;
typedef sirResourceState_t SIRResourceState;

typedef enum sirResourceResult_e : uint8_t
{
	SIR_RESOURCE_RESULT_SUCCESS = 0,
	SIR_RESOURCE_RESULT_INVALID_ARGUMENT,
	SIR_RESOURCE_RESULT_INVALID_HANDLE,
	SIR_RESOURCE_RESULT_TYPE_MISMATCH,
	SIR_RESOURCE_RESULT_NOT_RESIDENT,
	SIR_RESOURCE_RESULT_MISSING_CHUNK,
	SIR_RESOURCE_RESULT_DEPENDENCY_MISSING,
	SIR_RESOURCE_RESULT_DEPENDENCY_CYCLE,
	SIR_RESOURCE_RESULT_OUT_OF_BUDGET,
	SIR_RESOURCE_RESULT_INVALID_FORMAT,
	SIR_RESOURCE_RESULT_WRITER_VIOLATION,
	SIR_RESOURCE_RESULT_BUSY,
	SIR_RESOURCE_RESULT_FAILED,
	SIR_RESOURCE_RESULT_STALE_REVISION,
	SIR_RESOURCE_RESULT_JOURNAL_OVERFLOW
} sirResourceResult_t;
typedef sirResourceResult_t SIRResourceResult;

typedef enum sirResourceEdgeKind_e : uint8_t
{
	SIR_RESOURCE_EDGE_KIND_REQUIRED = 0,
	SIR_RESOURCE_EDGE_KIND_OPTIONAL,
	SIR_RESOURCE_EDGE_KIND_STREAMED
} sirResourceEdgeKind_t;
typedef sirResourceEdgeKind_t SIRResourceEdgeKind;

// A stable cooked identity. Variant is a cooked platform/config variant, not a second CPU owner.
typedef struct sirResourceKey_s
{
	uint64_t m_nAssetId;
	uint32_t m_nTypeId;
	uint32_t m_nVariantId;
} sirResourceKey_t;
typedef sirResourceKey_t SIRResourceKey;

// Process-local and generation-safe: never serialize this in place of SIRResourceKey.
typedef struct sirResourceHandle_s
{
	uint32_t m_nSlot;
	uint32_t m_nGeneration;
	uint32_t m_nTypeId;
	uint32_t m_nReserved;
} sirResourceHandle_t;
typedef sirResourceHandle_t SIRResourceHandle;

typedef struct sirResourceChunkId_s
{
	uint32_t m_nValue;
} sirResourceChunkId_t;
typedef sirResourceChunkId_t SIRResourceChunkId;

typedef struct sirResourceByteRange_s
{
	uint64_t m_nOffset;
	uint64_t m_nSize;
} sirResourceByteRange_t;
typedef sirResourceByteRange_t SIRResourceByteRange;

// Borrowed memory. Valid only while the relevant read scope is alive.
typedef struct sirResourceConstBytes_s
{
	const uint8_t *m_pData;
	uint64_t m_nSize;
} sirResourceConstBytes_t;
typedef sirResourceConstBytes_t SIRResourceConstBytes;

// Unpublished staging bytes. Never exposed through a published resource view.
typedef struct sirResourceMutableBytes_s
{
	uint8_t *m_pData;
	uint64_t m_nSize;
} sirResourceMutableBytes_t;
typedef sirResourceMutableBytes_t SIRResourceMutableBytes;

typedef struct sirResourceDependency_s
{
	SIRResourceKey m_Target;
	SIRResourceEdgeKind m_eKind;
	uint8_t m_szReserved[7];
} sirResourceDependency_t;
typedef sirResourceDependency_t SIRResourceDependency;

typedef struct sirResourceInfo_s
{
	SIRResourceKey m_Key;
	uint64_t m_nRevision;
	uint64_t m_nLogicalBytes;
	uint32_t m_nFormatId;
	uint32_t m_nChunkCount;
	uint32_t m_nDependencyCount;
	SIRResourceKind m_eKind;
	SIRResourceState m_eState;
	uint16_t m_nSchemaVersion;
} sirResourceInfo_t;
typedef sirResourceInfo_t SIRResourceInfo;

typedef struct sirResourceContentKey_s
{
	uint64_t m_nHashLo;
	uint64_t m_nHashHi;
	uint32_t m_nFormatId;
	uint32_t m_nEncodingId;
} sirResourceContentKey_t;
typedef sirResourceContentKey_t SIRResourceContentKey;

typedef struct sirResourceChunkInfo_s
{
	SIRResourceChunkId m_Id;
	uint32_t m_nAlignment;
	uint64_t m_nBytes;
	SIRResourceContentKey m_ContentKey;
} sirResourceChunkInfo_t;
typedef sirResourceChunkInfo_t SIRResourceChunkInfo;

typedef struct sirResourcePinHandle_s
{
	uint32_t m_nSlot;
	uint32_t m_nGeneration;
} sirResourcePinHandle_t;
typedef sirResourcePinHandle_t SIRResourcePinHandle;

typedef struct sirResourceStagingHandle_s
{
	uint32_t m_nSlot;
	uint32_t m_nGeneration;
} sirResourceStagingHandle_t;
typedef sirResourceStagingHandle_t SIRResourceStagingHandle;

typedef struct sirResourceChunkPublication_s
{
	SIRResourceChunkId m_Id;
	SIRResourceContentKey m_ContentKey;
	SIRResourceStagingHandle m_Staging;
	bool m_bReuseExisting; // Attach an existing identical chunk without allocating.
} sirResourceChunkPublication_t;
typedef sirResourceChunkPublication_t SIRResourceChunkPublication;

typedef struct sirResourcePublication_s
{
	SIRResourceKey m_Key;
	SIRResourceKind m_eKind;
	uint16_t m_nSchemaVersion;
	uint32_t m_nFormatId;
	const SIRResourceChunkPublication *m_pChunks;
	uint32_t m_nChunkCount;
	const SIRResourceDependency *m_pDependencies;
	uint32_t m_nDependencyCount;
} sirResourcePublication_t;
typedef sirResourcePublication_t SIRResourcePublication;

// Opaque provider-native objects are registered with the same resource identity/revision.
typedef struct sirDerivedResourceKey_s
{
	SIRResourceHandle m_Resource;
	uint64_t m_nRevision;
	uint32_t m_nProviderId;
	uint32_t m_nRepresentationId;
	uint32_t m_nVariantId;
	uint32_t m_nReserved;
} sirDerivedResourceKey_t;
typedef sirDerivedResourceKey_t SIRDerivedResourceKey;

typedef enum sirMemorySpace_e : uint8_t
{
	SIR_MEMORY_SPACE_CPU = 0,
	SIR_MEMORY_SPACE_DEVICE,
	SIR_MEMORY_SPACE_EXTERNAL
} sirMemorySpace_t;
typedef sirMemorySpace_t SIRMemorySpace;

typedef struct sirDerivedResourceAccounting_s
{
	SIRMemorySpace m_eSpace;
	uint32_t m_nDomainId;
	uint64_t m_nCommittedBytes;
	uint64_t m_nResidentBytes;
	uint64_t m_nSourceRevision;
} sirDerivedResourceAccounting_t;
typedef sirDerivedResourceAccounting_t SIRDerivedResourceAccounting;

static_assert( sizeof( SIRResourceKey ) == 16, "Resource keys must remain compact" );
static_assert( sizeof( SIRResourceHandle ) == 16, "Resource handles must remain generation-safe" );

#endif // __SIRENGINE_SIRRESOURCETYPES_H__
