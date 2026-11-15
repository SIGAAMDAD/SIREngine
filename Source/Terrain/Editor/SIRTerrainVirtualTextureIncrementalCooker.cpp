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

#include <Terrain/Editor/SIRTerrainVirtualTextureIncrementalCooker.h>
#include <Core/Jobs/SIRTaskScheduler.h>
#include <Core/Platform/SIRPlatformFile.h>
#include <Renderer/VirtualTexturing/SIRVirtualTextureRegionCooker.h>
#include <Renderer/VirtualTexturing/Terrain3D/src/data/formats/terrain_3d_vt_pack_format.h>
#include <Renderer/VirtualTexturing/Terrain3D/src/rendering/rd/vt/core/terrain_3d_vt_contract.h>
#include <string.h>

namespace
{
// The donor schedules transient failures at 250ms, 500ms, 1s, 2s, 4s,
// then 8s. A new authored generation resets this sequence immediately.
static constexpr uint64_t SIR_TERRAIN_VT_RETRY_FIRST_USEC = 250000u;
static constexpr uint32_t SIR_TERRAIN_VT_RETRY_LIMIT = 6u;
static constexpr uint64_t SIR_TERRAIN_VT_SOURCE_MAGIC = 0x3152435354563344ull;

bool readSourceFingerprint( const char *pszPath, uint64_t nFingerprint ) noexcept
{
	if ( pszPath == nullptr || pszPath[0] == '\0' ) {
		return false;
	}
	SIRPlatformFile file;
	if ( file.open( pszPath, SIR_FILE_OPEN_READ ) != SIR_FILE_OK ) {
		return false;
	}
	uint64_t nBytes = 0u;
	if ( file.getSize( &nBytes ) != SIR_FILE_OK || nBytes != 16u ) {
		return false;
	}
	uint8_t bytes[16]{};
	uint64_t nRead = 0u;
	if ( file.readAt( 0u, bytes, 16u, &nRead ) != SIR_FILE_OK || nRead != 16u ) {
		return false;
	}
	uint64_t nMagic = 0u;
	uint64_t nStoredFingerprint = 0u;
	for ( uint32_t i = 0u; i < 8u; ++i ) {
		nMagic |= ( uint64_t )bytes[i] << ( i * 8u );
		nStoredFingerprint |= ( uint64_t )bytes[i + 8u] << ( i * 8u );
	}
	return nMagic == SIR_TERRAIN_VT_SOURCE_MAGIC && nStoredFingerprint == nFingerprint;
}

bool validCachePayload( const sirTerrainVTCacheRestore_t& request, const uint8_t *pBytes, uint64_t nBytes ) noexcept
{
	t3dVTPackBaselineView_t view{};
	if ( pBytes == nullptr || !t3d_vt_pack_open_baseline_view( pBytes, nBytes, view ) || view.m_nAllocationCount != 1u ||
	     view.m_nChannelAllocationCount < T3D_VT_LOGICAL_CHANNEL_COUNT ) {
		return false;
	}
	const t3dVTPackAllocationRecord_t& allocation = view.m_pAllocations[0];
	if ( view.m_pAllocationKeys[0].m_nStableKey != t3d_vt_terrain_region_stable_key( request.nRegionIndex ) ||
	     allocation.m_nGeneration != request.nGeneration || allocation.m_nConsumer != T3D_VT_CONSUMER_CLIPMAP ) {
		return false;
	}
	const uint32_t nFlags = T3D_VT_ALLOCATION_MEGATEXTURE | T3D_VT_ALLOCATION_CLAMP;
	if ( ( allocation.m_nFlags & nFlags ) != nFlags ) {
		return false;
	}
	const uint32_t nPages = request.nExtent / T3D_VT_PAGE_INTERIOR;
	const t3dVTChannelAllocation_t& albedo = view.m_pChannelAllocations[T3D_VT_LOGICAL_CHANNEL_ALBEDO];
	const t3dVTChannelAllocation_t& pbr = view.m_pChannelAllocations[T3D_VT_LOGICAL_CHANNEL_PBR];
	return albedo.m_nMip0WidthPages == nPages && albedo.m_nMip0HeightPages == nPages && pbr.m_nMip0WidthPages == nPages / 2u &&
	       pbr.m_nMip0HeightPages == nPages / 2u;
}

bool filePresent( const char *pszPath, bool& bPresent ) noexcept
{
	sirFileInfo_t info{};
	const sirFileResult_t result = SIRPlatformFile::getInfo( pszPath, &info );
	bPresent = result == SIR_FILE_OK && info.bRegularFile;
	return ( result == SIR_FILE_NOT_FOUND ) || bPresent;
}

bool writeStaged( const char *pszPath, const void *pBytes, uint64_t nBytes ) noexcept
{
	SIRPlatformFile file;
	if ( file.open( pszPath, SIR_FILE_OPEN_WRITE | SIR_FILE_OPEN_CREATE | SIR_FILE_OPEN_EXCLUSIVE ) != SIR_FILE_OK ) {
		return false;
	}
	uint64_t nTransferred = 0u;
	const bool bWritten = file.writeAt( 0u, pBytes, nBytes, &nTransferred ) == SIR_FILE_OK && nTransferred == nBytes;
	const bool bFlushed = bWritten && file.flush() == SIR_FILE_OK;
	const bool bClosed = file.close() == SIR_FILE_OK;
	if ( !bWritten || !bFlushed || !bClosed ) {
		( void )SIRPlatformFile::remove( pszPath );
		return false;
	}
	return true;
}

bool distinctCachePaths( const sirTerrainVTCachePublication_t& request ) noexcept
{
	const char *paths[6] = { request.pszPackagePath,        request.pszFingerprintPath, request.pszPackageStaging,
	                         request.pszFingerprintStaging, request.pszPackageBackup,   request.pszFingerprintBackup };
	for ( uint32_t i = 0u; i < 6u; ++i ) {
		if ( paths[i] == nullptr || paths[i][0] == '\0' ) {
			return false;
		}
		for ( uint32_t j = 0u; j < i; ++j ) {
			if ( strcmp( paths[i], paths[j] ) == 0 ) {
				return false;
			}
		}
	}
	return true;
}

bool openCachedPackage( const sirTerrainVTCacheRestore_t& request, uint64_t& nLoaded ) noexcept
{
	if ( request.pszPackagePath == nullptr || request.pszPackagePath[0] == '\0' || request.pStorage == nullptr ||
	     request.nStorageCapacity < T3D_VT_PACK_HEADER_BYTES ||
	     !readSourceFingerprint( request.pszFingerprintPath, request.nFingerprint ) ) {
		return false;
	}
	SIRPlatformFile file;
	if ( file.open( request.pszPackagePath, SIR_FILE_OPEN_READ ) != SIR_FILE_OK ) {
		return false;
	}
	uint64_t nBytes = 0u;
	if ( file.getSize( &nBytes ) != SIR_FILE_OK || nBytes < T3D_VT_PACK_HEADER_BYTES || nBytes > request.nStorageCapacity ) {
		return false;
	}
	uint64_t nRead = 0u;
	if ( file.readAt( 0u, request.pStorage, nBytes, &nRead ) != SIR_FILE_OK || nRead != nBytes ) {
		return false;
	}
	if ( !validCachePayload( request, request.pStorage, nBytes ) ) {
		return false;
	}
	nLoaded = nBytes;
	return true;
}
} // namespace

SIRTerrainVirtualTextureIncrementalCooker::SIRTerrainVirtualTextureIncrementalCooker( void ) noexcept
    : m_pScheduler( nullptr ), m_work{}, m_capturedRegions{}, m_cancelContexts{}, m_regions{}, m_stats{}, m_task{}, m_workerTask{},
      m_nNextRegion( 0u ), m_nActiveCount( 0u ), m_activeIndices{}, m_activeGenerations{}, m_bActive( false ), m_bWorkerSucceeded( false ),
      m_bCancelRequested( false )
{
}

SIRTerrainVirtualTextureIncrementalCooker::~SIRTerrainVirtualTextureIncrementalCooker()
{
	shutdown();
}

bool SIRTerrainVirtualTextureIncrementalCooker::initialize( SIRTaskScheduler *pScheduler ) noexcept
{
	if ( m_bActive || pScheduler == nullptr ) {
		return false;
	}
	m_pScheduler = pScheduler;
	return true;
}

bool SIRTerrainVirtualTextureIncrementalCooker::markDirty( uint32_t nRegionIndex, uint32_t nGeneration ) noexcept
{
	if ( nRegionIndex >= SIR_TERRAIN_VT_REGION_COUNT || nGeneration == 0u ) {
		return false;
	}
	sirTerrainVTIncrementalRegion_t& entry = m_regions[nRegionIndex];
	if ( entry.nGeneration != nGeneration ) {
		entry.nGeneration = nGeneration;
		entry.nRetryAttempts = 0u;
		entry.nRetryAfterUsec = 0u;
		entry.bFailed = false;
		entry.bCacheRestoreAttempted = false;
	}
	if ( !entry.bDirty ) {
		++m_stats.nDirty;
	}
	entry.bDirty = true;
	return true;
}

bool SIRTerrainVirtualTextureIncrementalCooker::selectReadyRegion( uint64_t nNowUsec, uint32_t& nRegionIndex,
                                                                   uint32_t& nGeneration ) noexcept
{
	for ( uint32_t i = 0u; i < SIR_TERRAIN_VT_REGION_COUNT; ++i ) {
		const uint32_t index = ( m_nNextRegion + i ) % SIR_TERRAIN_VT_REGION_COUNT;
		const sirTerrainVTIncrementalRegion_t& entry = m_regions[index];
		if ( !entry.bDirty || entry.bFailed || entry.nRetryAfterUsec > nNowUsec ) {
			continue;
		}
		bool bAlreadySubmitted = false;
		for ( uint32_t j = 0u; j < m_nActiveCount; ++j ) {
			if ( m_activeIndices[j] == index ) {
				bAlreadySubmitted = true;
				break;
			}
		}
		if ( bAlreadySubmitted ) {
			continue;
		}
		m_nNextRegion = ( index + 1u ) % SIR_TERRAIN_VT_REGION_COUNT;
		nRegionIndex = index;
		nGeneration = entry.nGeneration;
		return true;
	}
	return false;
}

bool SIRTerrainVirtualTextureIncrementalCooker::submit( const sirTerrainVTIncrementalWork_t& work ) noexcept
{
	if ( m_pScheduler == nullptr || m_bActive || work.publication.pPackage == nullptr || work.publication.batch.pRegions == nullptr ||
	     work.publication.batch.pSurfaces == nullptr || work.publication.batch.pCookScratch == nullptr ||
	     work.publication.batch.nRegionCount == 0u || work.publication.batch.nRegionCount > SIR_TERRAIN_VT_MAX_PUBLICATION_REGIONS ||
	     work.publication.batch.nSourceCapacity < work.publication.batch.nRegionCount || work.publication.batch.pfnIsCurrent == nullptr ||
	     work.publication.pPackage->pBytes == nullptr || work.publication.pPackage->nCapacity == 0u ) {
		return false;
	}
	const uint32_t nCount = work.publication.batch.nRegionCount;
	for ( uint32_t i = 0u; i < nCount; ++i ) {
		const sirVirtualTextureRegionCookRequest_t& region = work.publication.batch.pRegions[i];
		if ( region.nRegionIndex >= SIR_TERRAIN_VT_REGION_COUNT || region.nGeneration == 0u ) {
			return false;
		}
		const sirTerrainVTIncrementalRegion_t& entry = m_regions[region.nRegionIndex];
		if ( !entry.bDirty || entry.bFailed || entry.nGeneration != region.nGeneration ||
		     !work.publication.batch.pfnIsCurrent( work.publication.batch.pCurrentUser, region.nRegionIndex, region.nGeneration ) ) {
			return false;
		}
		for ( uint32_t j = 0u; j < i; ++j ) {
			if ( work.publication.batch.pRegions[j].nRegionIndex == region.nRegionIndex ) {
				return false;
			}
		}
	}
	m_work = work;
	m_nActiveCount = nCount;
	for ( uint32_t i = 0u; i < nCount; ++i ) {
		m_capturedRegions[i] = work.publication.batch.pRegions[i];
		m_cancelContexts[i] = { this, m_capturedRegions[i].pfnShouldCancel, m_capturedRegions[i].pCancelUser };
		m_capturedRegions[i].pfnShouldCancel = &SIRTerrainVirtualTextureIncrementalCooker::workerCanceled;
		m_capturedRegions[i].pCancelUser = &m_cancelContexts[i];
		m_activeIndices[i] = m_capturedRegions[i].nRegionIndex;
		m_activeGenerations[i] = m_capturedRegions[i].nGeneration;
	}
	m_work.publication.batch.pRegions = m_capturedRegions;
	m_bWorkerSucceeded = false;
	m_bCancelRequested = false;
	const sirTaskBatchTask_t task{ &SIRTerrainVirtualTextureIncrementalCooker::workerEntry, this, SIR_TASK_PRIORITY_BACKGROUND,
	                               SIR_TASK_EXEC_CPU };
	if ( !m_pScheduler->submit( task, &m_task ) ) {
		m_work = {};
		m_nActiveCount = 0u;
		return false;
	}
	m_bActive = true;
	++m_stats.nSubmitted;
	return true;
}

bool SIRTerrainVirtualTextureIncrementalCooker::frozenSourceCurrent( void *pContext, uint32_t nRegionIndex, uint32_t nGeneration ) noexcept
{
	( void )pContext;
	( void )nRegionIndex;
	( void )nGeneration;
	// Worker reads an immutable authoring capture. The owner validates the live
	// generation again after worker completion and under the publication lock.
	return true;
}

bool SIRTerrainVirtualTextureIncrementalCooker::workerCanceled( void *pContext ) noexcept
{
	const cancelContext_t *const pCancel = ( const cancelContext_t * )pContext;
	if ( pCancel->pfnOriginal != nullptr && pCancel->pfnOriginal( pCancel->pOriginalUser ) ) {
		return true;
	}
	return pCancel->pOwner->m_pScheduler->isCancellationRequested( pCancel->pOwner->m_workerTask );
}

sirTaskResult_t SIRTerrainVirtualTextureIncrementalCooker::workerEntry( void *pContext, sirTaskHandle_t task ) noexcept
{
	SIRTerrainVirtualTextureIncrementalCooker *const pCooker = ( SIRTerrainVirtualTextureIncrementalCooker * )pContext;
	pCooker->m_workerTask = task;
	sirVirtualTextureRegionBatchRequest_t batch = pCooker->m_work.publication.batch;
	batch.pfnIsCurrent = &SIRTerrainVirtualTextureIncrementalCooker::frozenSourceCurrent;
	batch.pCurrentUser = nullptr;
	pCooker->m_bWorkerSucceeded = SIRVirtualTextureRegionCooker::cookBatch( batch, *pCooker->m_work.publication.pPackage );
	return pCooker->m_bWorkerSucceeded ? SIR_TASK_RESULT_SUCCESS : SIR_TASK_RESULT_FAILURE;
}

bool SIRTerrainVirtualTextureIncrementalCooker::submittedGenerationsCurrent( void ) const noexcept
{
	const sirVirtualTextureRegionBatchRequest_t& batch = m_work.publication.batch;
	for ( uint32_t i = 0u; i < m_nActiveCount; ++i ) {
		const uint32_t index = m_activeIndices[i];
		if ( m_regions[index].nGeneration != m_activeGenerations[i] ||
		     !batch.pfnIsCurrent( batch.pCurrentUser, index, m_activeGenerations[i] ) ) {
			return false;
		}
	}
	return true;
}

void SIRTerrainVirtualTextureIncrementalCooker::failRegion( uint32_t nRegionIndex, uint64_t nNowUsec ) noexcept
{
	sirTerrainVTIncrementalRegion_t& entry = m_regions[nRegionIndex];
	if ( entry.nRetryAttempts >= SIR_TERRAIN_VT_RETRY_LIMIT ) {
		entry.bFailed = true;
		entry.nRetryAfterUsec = 0u;
		return;
	}
	const uint64_t delay = SIR_TERRAIN_VT_RETRY_FIRST_USEC << entry.nRetryAttempts;
	++entry.nRetryAttempts;
	entry.nRetryAfterUsec = nNowUsec > UINT64_MAX - delay ? UINT64_MAX : nNowUsec + delay;
	++m_stats.nRetried;
}

void SIRTerrainVirtualTextureIncrementalCooker::completeRegions( bool bPublished, uint64_t nNowUsec ) noexcept
{
	for ( uint32_t i = 0u; i < m_nActiveCount; ++i ) {
		const uint32_t index = m_activeIndices[i];
		sirTerrainVTIncrementalRegion_t& entry = m_regions[index];
		if ( entry.nGeneration != m_activeGenerations[i] ) {
			continue;
		}
		if ( bPublished ) {
			if ( entry.bDirty ) {
				--m_stats.nDirty;
			}
			entry.bDirty = false;
			entry.bFailed = false;
			entry.nRetryAttempts = 0u;
			entry.nRetryAfterUsec = 0u;
		} else {
			failRegion( index, nNowUsec );
		}
	}
}

void SIRTerrainVirtualTextureIncrementalCooker::releaseActiveTask( void ) noexcept
{
	( void )m_pScheduler->releaseTask( m_task );
	m_work = {};
	m_task = {};
	m_workerTask = {};
	m_nActiveCount = 0u;
	m_bActive = false;
	m_bWorkerSucceeded = false;
	m_bCancelRequested = false;
}

sirTerrainVTIncrementalCompletion_t SIRTerrainVirtualTextureIncrementalCooker::pollCompletion( uint64_t nNowUsec ) noexcept
{
	if ( !m_bActive || m_pScheduler == nullptr ) {
		return SIR_TERRAIN_VT_INCREMENTAL_IDLE;
	}
	const sirTaskState_t state = m_pScheduler->getState( m_task );
	if ( state == SIR_TASK_QUEUED || state == SIR_TASK_RUNNING || state == SIR_TASK_BLOCKED ) {
		return SIR_TERRAIN_VT_INCREMENTAL_RUNNING;
	}
	const bool bCurrent = submittedGenerationsCurrent();
	const bool bWorkerSuccess = !m_bCancelRequested && state == SIR_TASK_COMPLETED && m_bWorkerSucceeded;
	const sirTerrainVTCookResult_t published =
	    bWorkerSuccess && bCurrent ? SIRTerrainVirtualTextureCookBridge::publishPrepared( m_work.publication ) : SIR_TERRAIN_VT_COOK_FAILED;
	const bool bPublished = published == SIR_TERRAIN_VT_COOK_SUCCESS;
	completeRegions( bPublished, nNowUsec );
	if ( bPublished ) {
		++m_stats.nPublished;
	} else {
		++m_stats.nRejected;
	}
	releaseActiveTask();
	if ( bPublished ) {
		return SIR_TERRAIN_VT_INCREMENTAL_PUBLISHED;
	}
	return bCurrent && !m_bCancelRequested ? SIR_TERRAIN_VT_INCREMENTAL_RETRY : SIR_TERRAIN_VT_INCREMENTAL_STALE;
}

bool SIRTerrainVirtualTextureIncrementalCooker::restoreCached( const sirTerrainVTCacheRestore_t& request, uint64_t& nLoaded ) noexcept
{
	nLoaded = 0u;
	if ( request.nRegionIndex >= SIR_TERRAIN_VT_REGION_COUNT || request.nGeneration == 0u ||
	     ( request.nExtent != 256u && request.nExtent != 512u ) ) {
		return false;
	}
	sirTerrainVTIncrementalRegion_t& entry = m_regions[request.nRegionIndex];
	if ( entry.bCacheRestoreAttempted || entry.bDirty || ( entry.nGeneration != 0u && entry.nGeneration != request.nGeneration ) ) {
		return false;
	}
	entry.bCacheRestoreAttempted = true;
	if ( !openCachedPackage( request, nLoaded ) ) {
		++m_stats.nCacheRejected;
		markDirty( request.nRegionIndex, request.nGeneration );
		entry.bCacheRestoreAttempted = true;
		return false;
	}
	entry.nGeneration = request.nGeneration;
	++m_stats.nRestored;
	return true;
}

bool SIRTerrainVirtualTextureIncrementalCooker::publishCached( const sirTerrainVTCachePublication_t& request ) noexcept
{
	if ( !distinctCachePaths( request ) || request.pPackage == nullptr || request.nPackageBytes < T3D_VT_PACK_HEADER_BYTES ||
	     request.nRegionIndex >= SIR_TERRAIN_VT_REGION_COUNT || request.nGeneration == 0u ||
	     ( request.nExtent != 256u && request.nExtent != 512u ) ) {
		return false;
	}
	sirTerrainVTCacheRestore_t identity{};
	identity.nRegionIndex = request.nRegionIndex;
	identity.nGeneration = request.nGeneration;
	identity.nExtent = request.nExtent;
	if ( !validCachePayload( identity, request.pPackage, request.nPackageBytes ) ) {
		return false;
	}
	bool bPackagePrevious = false;
	bool bFingerprintPrevious = false;
	bool bPackageBackupExists = false;
	bool bFingerprintBackupExists = false;
	if ( !filePresent( request.pszPackagePath, bPackagePrevious ) || !filePresent( request.pszFingerprintPath, bFingerprintPrevious ) ||
	     !filePresent( request.pszPackageBackup, bPackageBackupExists ) ||
	     !filePresent( request.pszFingerprintBackup, bFingerprintBackupExists ) || bPackageBackupExists || bFingerprintBackupExists ) {
		return false;
	}
	uint8_t sourceSignature[16]{};
	for ( uint32_t i = 0u; i < 8u; ++i ) {
		sourceSignature[i] = ( uint8_t )( SIR_TERRAIN_VT_SOURCE_MAGIC >> ( i * 8u ) );
		sourceSignature[i + 8u] = ( uint8_t )( request.nFingerprint >> ( i * 8u ) );
	}
	if ( !writeStaged( request.pszFingerprintStaging, sourceSignature, sizeof( sourceSignature ) ) ) {
		return false;
	}
	if ( !writeStaged( request.pszPackageStaging, request.pPackage, request.nPackageBytes ) ) {
		( void )SIRPlatformFile::remove( request.pszFingerprintStaging );
		return false;
	}
	bool bBackedPackage = false;
	bool bBackedFingerprint = false;
	bool bInstalledPackage = false;
	bool bInstalledFingerprint = false;
	if ( bPackagePrevious ) {
		bBackedPackage = SIRPlatformFile::renameReplace( request.pszPackagePath, request.pszPackageBackup ) == SIR_FILE_OK;
	}
	if ( !bPackagePrevious || bBackedPackage ) {
		if ( bFingerprintPrevious ) {
			bBackedFingerprint = SIRPlatformFile::renameReplace( request.pszFingerprintPath, request.pszFingerprintBackup ) == SIR_FILE_OK;
		}
		if ( !bFingerprintPrevious || bBackedFingerprint ) {
			bInstalledPackage = SIRPlatformFile::renameReplace( request.pszPackageStaging, request.pszPackagePath ) == SIR_FILE_OK;
			if ( bInstalledPackage ) {
				bInstalledFingerprint =
				    SIRPlatformFile::renameReplace( request.pszFingerprintStaging, request.pszFingerprintPath ) == SIR_FILE_OK;
			}
		}
	}
	if ( !bInstalledPackage || !bInstalledFingerprint ) {
		if ( bInstalledPackage ) {
			( void )SIRPlatformFile::remove( request.pszPackagePath );
		}
		if ( bInstalledFingerprint ) {
			( void )SIRPlatformFile::remove( request.pszFingerprintPath );
		}
		if ( bBackedPackage ) {
			( void )SIRPlatformFile::renameReplace( request.pszPackageBackup, request.pszPackagePath );
		}
		if ( bBackedFingerprint ) {
			( void )SIRPlatformFile::renameReplace( request.pszFingerprintBackup, request.pszFingerprintPath );
		}
		( void )SIRPlatformFile::remove( request.pszPackageStaging );
		( void )SIRPlatformFile::remove( request.pszFingerprintStaging );
		return false;
	}
	if ( bBackedPackage ) {
		( void )SIRPlatformFile::remove( request.pszPackageBackup );
	}
	if ( bBackedFingerprint ) {
		( void )SIRPlatformFile::remove( request.pszFingerprintBackup );
	}
	return true;
}

void SIRTerrainVirtualTextureIncrementalCooker::cancelInFlight( void ) noexcept
{
	if ( m_bActive && m_pScheduler != nullptr ) {
		m_bCancelRequested = true;
		( void )m_pScheduler->cancel( m_task );
	}
}

void SIRTerrainVirtualTextureIncrementalCooker::retryFailed( void ) noexcept
{
	for ( uint32_t i = 0u; i < SIR_TERRAIN_VT_REGION_COUNT; ++i ) {
		sirTerrainVTIncrementalRegion_t& entry = m_regions[i];
		if ( entry.bDirty ) {
			entry.nRetryAttempts = 0u;
			entry.nRetryAfterUsec = 0u;
			entry.bFailed = false;
		}
	}
}

void SIRTerrainVirtualTextureIncrementalCooker::shutdown( void ) noexcept
{
	if ( m_bActive && m_pScheduler != nullptr ) {
		cancelInFlight();
		// Scheduler ownership is preserved until the worker has stopped touching
		// borrowed terrain snapshots. Shutdown is an editor owner-thread call.
		( void )m_pScheduler->waitTask( m_task );
		releaseActiveTask();
	}
	m_pScheduler = nullptr;
}

const sirTerrainVTIncrementalStats_t& SIRTerrainVirtualTextureIncrementalCooker::stats( void ) const noexcept
{
	return m_stats;
}

const sirTerrainVTIncrementalRegion_t *SIRTerrainVirtualTextureIncrementalCooker::region( uint32_t nRegionIndex ) const noexcept
{
	return nRegionIndex < SIR_TERRAIN_VT_REGION_COUNT ? &m_regions[nRegionIndex] : nullptr;
}
