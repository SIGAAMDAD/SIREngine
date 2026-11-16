/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
Copyright (c) 2023-2026 Cory Petkovsek, Roope Palmroos, and Terrain3D Contributors

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#ifndef __SIRENGINE_SIRTERRAINVIRTUALTEXTUREINCREMENTALCOOKER_H__
#define __SIRENGINE_SIRTERRAINVIRTUALTEXTUREINCREMENTALCOOKER_H__

#include <SIRPCH.hpp>
#include <Core/Jobs/SIRTaskTypes.h>
#include <Terrain/Editor/SIRTerrainVirtualTextureCookBridge.h>

class SIRTaskScheduler;

#define SIR_TERRAIN_VT_REGION_COUNT 1024u
#define SIR_TERRAIN_VT_MAX_PUBLICATION_REGIONS 32u

// The owner captures immutable source images and provides output buffers before
// submission. They must remain alive until the task reaches a terminal state
// and pollCompletion() releases its handle. No GPU or Godot work is performed.
typedef struct sirTerrainVTIncrementalWork_s
{
	sirTerrainVTCookPublication_t publication;
} sirTerrainVTIncrementalWork_t;

typedef struct sirTerrainVTCacheRestore_s
{
	const char *pszPackagePath;
	const char *pszFingerprintPath;
	uint8_t *pStorage;
	uint64_t nStorageCapacity;
	uint64_t nFingerprint;
	uint32_t nRegionIndex;
	uint32_t nGeneration;
	uint32_t nExtent;
} sirTerrainVTCacheRestore_t;

// Native translation of the donor's .t3dvt + .source rollback pair.
// Every path, including staging/backup paths, is supplied by the Terrain
// persistence owner. Destinations must be distinct and on the same volume.
typedef struct sirTerrainVTCachePublication_s
{
	const char *pszPackagePath;
	const char *pszFingerprintPath;
	const char *pszPackageStaging;
	const char *pszFingerprintStaging;
	const char *pszPackageBackup;
	const char *pszFingerprintBackup;
	const uint8_t *pPackage;
	uint64_t nPackageBytes;
	uint64_t nFingerprint;
	uint32_t nRegionIndex;
	uint32_t nGeneration;
	uint32_t nExtent;
} sirTerrainVTCachePublication_t;

typedef struct sirTerrainVTIncrementalRegion_s
{
	uint64_t nRetryAfterUsec;
	uint32_t nGeneration;
	uint8_t nRetryAttempts;
	bool bDirty;
	bool bFailed;
	bool bCacheRestoreAttempted;
} sirTerrainVTIncrementalRegion_t;

typedef struct sirTerrainVTIncrementalStats_s
{
	uint64_t nSubmitted;
	uint64_t nPublished;
	uint64_t nRetried;
	uint64_t nRejected;
	uint64_t nRestored;
	uint64_t nCacheRejected;
	uint32_t nDirty;
} sirTerrainVTIncrementalStats_t;

typedef enum sirTerrainVTIncrementalCompletion_e : uint8_t
{
	SIR_TERRAIN_VT_INCREMENTAL_IDLE = 0,
	SIR_TERRAIN_VT_INCREMENTAL_RUNNING,
	SIR_TERRAIN_VT_INCREMENTAL_PUBLISHED,
	SIR_TERRAIN_VT_INCREMENTAL_RETRY,
	SIR_TERRAIN_VT_INCREMENTAL_STALE
} sirTerrainVTIncrementalCompletion_t;

// Owner-thread controller for the donor's bounded authoring worker, retry and
// cache-restore policy. SIRTaskScheduler owns execution and cancellation;
// SIRTerrain owns edited generations, capture lifetimes and disk publication.
class SIRTerrainVirtualTextureIncrementalCooker final
{
public:
	SIRTerrainVirtualTextureIncrementalCooker( void ) noexcept;
	~SIRTerrainVirtualTextureIncrementalCooker();
	SIRTerrainVirtualTextureIncrementalCooker( const SIRTerrainVirtualTextureIncrementalCooker& ) = delete;
	SIRTerrainVirtualTextureIncrementalCooker& operator=( const SIRTerrainVirtualTextureIncrementalCooker& ) = delete;

	bool initialize( SIRTaskScheduler *pScheduler ) noexcept;
	bool markDirty( uint32_t nRegionIndex, uint32_t nGeneration ) noexcept;
	bool selectReadyRegion( uint64_t nNowUsec, uint32_t& nRegionIndex, uint32_t& nGeneration ) noexcept;
	bool submit( const sirTerrainVTIncrementalWork_t& work ) noexcept;
	sirTerrainVTIncrementalCompletion_t pollCompletion( uint64_t nNowUsec ) noexcept;
	bool restoreCached( const sirTerrainVTCacheRestore_t& request, uint64_t& nLoaded ) noexcept;
	static bool publishCached( const sirTerrainVTCachePublication_t& request ) noexcept;
	void cancelInFlight( void ) noexcept;
	void retryFailed( void ) noexcept;
	void shutdown( void ) noexcept;
	const sirTerrainVTIncrementalStats_t& stats( void ) const noexcept;
	const sirTerrainVTIncrementalRegion_t *region( uint32_t nRegionIndex ) const noexcept;

protected:
private:
	typedef struct cancelContext_s
	{
		SIRTerrainVirtualTextureIncrementalCooker *pOwner;
		sirVirtualTextureRegionCancelFn pfnOriginal;
		void *pOriginalUser;
	} cancelContext_t;
	static sirTaskResult_t workerEntry( void *pContext, sirTaskHandle_t task ) noexcept;
	static bool workerCanceled( void *pContext ) noexcept;
	static bool frozenSourceCurrent( void *pContext, uint32_t nRegionIndex, uint32_t nGeneration ) noexcept;
	bool submittedGenerationsCurrent( void ) const noexcept;
	void completeRegions( bool bPublished, uint64_t nNowUsec ) noexcept;
	void failRegion( uint32_t nRegionIndex, uint64_t nNowUsec ) noexcept;
	void releaseActiveTask( void ) noexcept;

	SIRTaskScheduler *m_pScheduler;
	sirTerrainVTIncrementalWork_t m_work;
	sirVirtualTextureRegionCookRequest_t m_capturedRegions[SIR_TERRAIN_VT_MAX_PUBLICATION_REGIONS];
	cancelContext_t m_cancelContexts[SIR_TERRAIN_VT_MAX_PUBLICATION_REGIONS];
	sirTerrainVTIncrementalRegion_t m_regions[SIR_TERRAIN_VT_REGION_COUNT];
	sirTerrainVTIncrementalStats_t m_stats;
	sirTaskHandle_t m_task;
	sirTaskHandle_t m_workerTask;
	uint32_t m_nNextRegion;
	uint32_t m_nActiveCount;
	uint32_t m_activeIndices[SIR_TERRAIN_VT_MAX_PUBLICATION_REGIONS];
	uint32_t m_activeGenerations[SIR_TERRAIN_VT_MAX_PUBLICATION_REGIONS];
	bool m_bActive;
	bool m_bWorkerSucceeded;
	bool m_bCancelRequested;
};

#endif // __SIRENGINE_SIRTERRAINVIRTUALTEXTUREINCREMENTALCOOKER_H__
