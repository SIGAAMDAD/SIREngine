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

#ifndef __SIRENGINE_SIRMESHCACHE_H__
#define __SIRENGINE_SIRMESHCACHE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

// CPU-only foundation. No GRDG, physics, graphics API, or allocator dependency.
// All methods must run on the owning thread. Source pointers remain immutable
// and alive through the caller-supplied final-release callback.
typedef enum sirMeshResult_e : uint8_t
{
	SIR_MESH_OK = 0,
	SIR_MESH_INVALID_ARGUMENT = 1,
	SIR_MESH_NOT_READY = 2,
	SIR_MESH_OUT_OF_CAPACITY = 3,
	SIR_MESH_DUPLICATE = 4,
	SIR_MESH_NOT_FOUND = 5,
	SIR_MESH_STALE_HANDLE = 6,
	SIR_MESH_RETIRED = 7,
	SIR_MESH_BUSY = 8
} sirMeshResult_t;

typedef struct sirMeshKey_s
{
	uint64_t nAssetId;
	uint64_t nRevision;
	uint32_t nVariant;
	uint32_t nReserved;
} sirMeshKey_t;

// Runtime-only capability: never serialize a handle in SIRPack. The owner ID
// identifies a successful cache initialization epoch, not an authored asset.
typedef struct sirMeshHandle_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
	uint64_t nOwnerId;
} sirMeshHandle_t;

typedef struct sirMeshGeometrySource_s
{
	sirMeshKey_t key;
	const float *pPositionsXYZ;
	const uint32_t *pTriangleIndices;
	uint32_t nVertexCount;
	uint32_t nIndexCount;
	void *pSourcePin;
	void ( *pfnReleaseSourcePin )( void *pSourcePin ) noexcept;
} sirMeshGeometrySource_t;

typedef struct sirMeshGeometryRead_s
{
	const float *pPositionsXYZ;
	const uint32_t *pTriangleIndices;
	uint32_t nVertexCount;
	uint32_t nIndexCount;
	sirMeshKey_t key;
} sirMeshGeometryRead_t;

typedef struct sirMeshCacheSlot_s sirMeshCacheSlot_t;

// Sole writer of logical slot/generation/borrow metadata. A source pin is
// transferred on successful insertion, and released exactly once after all
// leases retire. This class never owns or copies the underlying vertex bytes.
// Shared device backing integration types.
// These allow mesh geometry to be backed by a shared physical GPU allocation
// that can be borrowed by multiple subsystems (render, physics, audio, nav).
typedef struct sirMeshDeviceBackingDesc_s
{
	uint32_t nPhysicalDeviceId;
	uint32_t nDeviceGeneration;
	uint32_t nStorageLayoutId;
	uint32_t nSupportedUsageMask;
	uint32_t nFormatId;
	uint32_t nAlignment;
	uint64_t nVertexCount;
	uint64_t nIndexCount;
	uint32_t nProviderId;
	uint32_t nReserved;
} sirMeshDeviceBackingDesc_t;

typedef struct sirMeshDeviceBackingReservation_s
{
	SIRDerivedResourceHandle hBacking;
	uint64_t nPhysicalBackingId;
	bool bNeedsNativeCreation;
	uint8_t m_szReserved[7];
} sirMeshDeviceBackingReservation_t;

class SIRResourceDerivedRegistry;
class SIRResourceGraph;

class SIRMeshCache
{
public:
	SIRMeshCache( void ) noexcept;
	SIRMeshCache( const SIRMeshCache& other ) = delete;
	SIRMeshCache( SIRMeshCache&& other ) = delete;
	~SIRMeshCache( void ) noexcept;

	SIRMeshCache& operator=( const SIRMeshCache& other ) = delete;
	SIRMeshCache& operator=( SIRMeshCache&& other ) = delete;

	uint32_t getCapacity( void ) const noexcept;
	uint32_t getResidentCount( void ) const noexcept;
	uint32_t getBorrowCount( void ) const noexcept;

	static uint64_t requiredBytes( uint32_t nCapacity ) noexcept;
	static uint32_t requiredAlignment( void ) noexcept;

	sirMeshResult_t initialize( void *pStorage, uint64_t nStorageBytes, uint32_t nCapacity ) noexcept;
	sirMeshResult_t insert( const sirMeshGeometrySource_t *pSource, sirMeshHandle_t *pHandle ) noexcept;
	sirMeshResult_t find( const sirMeshKey_t *pKey, sirMeshHandle_t *pHandle ) const noexcept;
	sirMeshResult_t acquire( sirMeshHandle_t handle, sirMeshGeometryRead_t *pRead ) noexcept;
	sirMeshResult_t release( sirMeshHandle_t handle ) noexcept;
	sirMeshResult_t retire( sirMeshHandle_t handle ) noexcept;
	sirMeshResult_t shutdown( void ) noexcept;

	// Shared device backing integration.
	// These methods integrate with SIRResourceDerivedRegistry for
	// one-canonical-asset and one-physical-GPU-backing across all systems.
	sirMeshResult_t reserveSharedDeviceBacking(
	    SIRResourceDerivedRegistry& registry,
	    SIRResourceGraph& graph,
	    const sirMeshKey_t* pKey,
	    const sirMeshDeviceBackingDesc_t& desc,
	    sirMeshDeviceBackingReservation_t* pReservation ) noexcept;

protected:
private:
	bool isValidHandle( sirMeshHandle_t handle ) const noexcept;
	void finalizeSlot( uint32_t iSlot ) noexcept;

	sirMeshCacheSlot_t *m_pSlots;
	uint32_t m_nCapacity;
	uint32_t m_nResidentCount;
	uint32_t m_nBorrowCount;
	uint32_t m_nNextGeneration;
	uint64_t m_nOwnerId;
	bool m_bReady;
	bool m_bShuttingDown;
};

#endif // __SIRENGINE_SIRMESHCACHE_H__
