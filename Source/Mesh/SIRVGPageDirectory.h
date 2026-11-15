/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#ifndef __SIRENGINE_SIRVGPAGEDIRECTORY_H__
#define __SIRENGINE_SIRVGPAGEDIRECTORY_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
    #pragma once
#endif
// clang-format on

#include <Mesh/SIRMeshPackSchema.h>

// This is a logical page directory, not a native GPU allocator or a second
// streaming scheduler. GRDG provides the immutable cooked source, Streaming
// issues demand and the native driver owns the sealed physical backing.
// Caller-owned fixed storage is used exclusively; no allocation occurs here.
typedef enum sirVGPageState_e : uint8_t
{
	SIR_VG_PAGE_MISSING = 0,
	SIR_VG_PAGE_UPLOADING = 1,
	SIR_VG_PAGE_RESIDENT = 2,
	SIR_VG_PAGE_RETIRING = 3
} sirVGPageState_t;

typedef enum sirVGResidencyResult_e : uint8_t
{
	SIR_VG_RESIDENCY_OK = 0,
	SIR_VG_RESIDENCY_INVALID_ARGUMENT = 1,
	SIR_VG_RESIDENCY_NOT_READY = 2,
	SIR_VG_RESIDENCY_BUSY = 3,
	SIR_VG_RESIDENCY_OUT_OF_BUDGET = 4,
	SIR_VG_RESIDENCY_STALE = 5,
	SIR_VG_RESIDENCY_BAD_SOURCE = 6,
	SIR_VG_RESIDENCY_EXHAUSTED = 7
} sirVGResidencyResult_t;

typedef struct sirVGPageSlot_s
{
	uint64_t nNativeOffset;
	uint64_t nBytes;
	uint64_t nCopyFence;
	uint64_t nComputeFence;
	uint64_t nGraphicsFence;
	uint64_t nLastGraphicsUseFence;
	uint32_t nGeneration;
	uint32_t nDeviceGeneration;
	uint32_t nRequestEpoch;
	uint32_t nReaders;
	sirVGPageState_t eState;
} sirVGPageSlot_t;

// Admission reserves native physical bytes, independent of compressed payload bytes.
// Draw offsets address the decoded page-local vertex/index buffers in cooked
// cluster order. A driver builds those buffers without changing this layout.
typedef struct sirVGClusterDrawLayout_s
{
	uint32_t nFirstIndex;
	uint32_t nIndexCount;
	uint32_t nFirstVertex;
	uint32_t nMaterialId;
} sirVGClusterDrawLayout_t;

typedef struct sirVGPageStorage_s
{
	sirVGPageSlot_t *pPages;
	sirVGClusterDrawLayout_t *pClusterDraws;
	uint32_t nPageCapacity;
	uint32_t nClusterCapacity;
} sirVGPageStorage_t;

typedef struct sirVGPageArena_s
{
	uint64_t nPhysicalBackingId;
	uint64_t nBackingCapacityBytes;
	uint64_t nResidentBudgetBytes;
	uint32_t nDeviceGeneration;
	uint32_t nRequestEpoch;
} sirVGPageArena_t;

typedef struct sirVGPageAdmission_s
{
	const void *pCookedBytes;
	uint64_t nCookedBytes;
	uint64_t nNativeBytes;
	uint64_t nPhysicalBackingId;
	uint64_t nNativeOffset;
	uint64_t nCopyFence;
	uint64_t nComputeFence;
	uint64_t nGraphicsFence;
	uint32_t nPage;
	uint32_t nDeviceGeneration;
	uint32_t nRequestEpoch;
} sirVGPageAdmission_t;

typedef struct sirVGPageCompletion_s
{
	uint64_t nCompletedCopyFence;
	uint64_t nCompletedComputeFence;
	uint64_t nCompletedGraphicsFence;
	uint32_t nPage;
	uint32_t nDeviceGeneration;
	uint32_t nRequestEpoch;
	bool bSuccess;
} sirVGPageCompletion_t;

// Native consumers bind this immutable subrange after an exact snapshot check.
// The physical backing identity is the one published by the owning driver.
typedef struct sirVGPageResidentView_s
{
	uint64_t nPhysicalBackingId;
	uint64_t nNativeOffset;
	uint64_t nNativeBytes;
	uint32_t nPageGeneration;
	uint32_t nDeviceGeneration;
	uint32_t nSnapshotGeneration;
} sirVGPageResidentView_t;

typedef struct sirVGPagePin_s
{
	uint32_t nPage;
	uint32_t nGeneration;
	uint32_t nDeviceGeneration;
} sirVGPagePin_t;

class SIRVGPageDirectory
{
public:
	SIRVGPageDirectory( void ) noexcept;
	SIRVGPageDirectory( const SIRVGPageDirectory& other ) = delete;
	SIRVGPageDirectory( SIRVGPageDirectory&& other ) = delete;
	~SIRVGPageDirectory( void ) noexcept;

	SIRVGPageDirectory& operator=( const SIRVGPageDirectory& other ) = delete;
	SIRVGPageDirectory& operator=( SIRVGPageDirectory&& other ) = delete;

	uint32_t getSnapshotGeneration( void ) const noexcept;
	uint32_t getDeviceGeneration( void ) const noexcept;
	uint32_t getRequestEpoch( void ) const noexcept;
	uint64_t getCommittedBytes( void ) const noexcept;
	uint64_t getBackingCapacityBytes( void ) const noexcept;
	uint64_t getBackingId( void ) const noexcept;
	bool isResident( uint32_t nPage ) const noexcept;
	bool isCurrentSnapshot( uint32_t nGeneration ) const noexcept;
	sirVGPageState_t getState( uint32_t nPage ) const noexcept;
	sirVGResidencyResult_t getClusterLayout( uint32_t nCluster, sirVGClusterDrawLayout_t *pLayout ) const noexcept;
	sirVGResidencyResult_t initialize( const SIRMeshPackSchema& schema, const sirVGPageStorage_t& storage,
	                                   const sirVGPageArena_t& arena ) noexcept;
	sirVGResidencyResult_t advanceEpoch( uint32_t nNextEpoch ) noexcept;
	sirVGResidencyResult_t admit( const sirVGPageAdmission_t& admission ) noexcept;
	sirVGResidencyResult_t complete( const sirVGPageCompletion_t& completion ) noexcept;
	// Admission reserves the native span before a command is submitted.
	// UINT64_MAX is used as the pending fence; only attachSubmission can
	// make it eligible for publication, after a real queue completion exists.
	sirVGResidencyResult_t attachSubmission( uint32_t nPage, uint32_t nRequestEpoch, uint64_t nCopyFence ) noexcept;
	sirVGResidencyResult_t abortAdmission( uint32_t nPage, uint32_t nRequestEpoch ) noexcept;
	sirVGResidencyResult_t getResidentView( uint32_t nPage, sirVGPageResidentView_t *pView ) const noexcept;
	sirVGResidencyResult_t findFreeSpan( uint64_t nNativeBytes, uint64_t nAlignment, uint64_t *pOffset ) const noexcept;
	sirVGResidencyResult_t acquire( uint32_t nPage, sirVGPagePin_t *pPin ) noexcept;
	sirVGResidencyResult_t release( const sirVGPagePin_t& pin ) noexcept;
	sirVGResidencyResult_t markSubmitted( uint32_t nPage, uint64_t nGraphicsFence ) noexcept;
	sirVGResidencyResult_t beginRetire( uint32_t nPage ) noexcept;
	sirVGResidencyResult_t finishRetire( uint32_t nPage, uint64_t nCompletedGraphicsFence ) noexcept;
	sirVGResidencyResult_t resetDevice( const sirVGPageArena_t& arena ) noexcept;
	sirVGResidencyResult_t shutdown( void ) noexcept;

protected:
private:
	bool isReady( void ) const noexcept;
	bool canReserve( const sirVGPageAdmission_t& admission ) const noexcept;
	bool hasLiveChildren( uint32_t nPage ) const noexcept;
	bool bumpSnapshot( void ) noexcept;
	void discardSlot( uint32_t nPage ) noexcept;

	const SIRMeshPackSchema *m_pSchema;
	sirVGPageSlot_t *m_pPages;
	sirVGClusterDrawLayout_t *m_pClusterDraws;
	sirVGPageArena_t m_Arena;
	uint64_t m_nCommittedBytes;
	uint32_t m_nPageCount;
	uint32_t m_nClusterCount;
	uint32_t m_nSnapshotGeneration;
};

#endif // __SIRENGINE_SIRVGPAGEDIRECTORY_H__
