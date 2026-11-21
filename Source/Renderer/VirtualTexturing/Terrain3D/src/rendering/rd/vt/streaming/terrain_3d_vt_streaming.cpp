#include "rendering/rd/core/terrain_3d_gpu_transfer_budget.h"
// Copyright © 2023-2026 Cory Petkovsek, Roope Palmroos, and Contributors.

#include "Terrain3DConstants.h"
#include "rendering/rd/vt/streaming/terrain_3d_vt_streaming.h"
#include "core/utility/terrain_3d_sync.h"

#include <string.h>

uint16_t t3d_vt_scheduler_priority_bucket_reference_debug( const uint16_t pPriority )
{
	if ( pPriority >= T3D_VT_PRIORITY_CRITICAL_MIN ) {
		return T3D_VT_PRIORITY_CRITICAL;
	}
	if ( pPriority >= T3D_VT_PRIORITY_NEAR_MIN ) {
		return T3D_VT_PRIORITY_NEAR;
	}
	if ( pPriority >= T3D_VT_PRIORITY_NORMAL_MIN ) {
		return T3D_VT_PRIORITY_NORMAL;
	}
	if ( pPriority >= T3D_VT_PRIORITY_PREFETCH_MIN ) {
		return T3D_VT_PRIORITY_PREFETCH;
	}
	if ( pPriority >= T3D_VT_PRIORITY_TRANSITION_MIN ) {
		return T3D_VT_PRIORITY_TRANSITION;
	}
	return T3D_VT_PRIORITY_BACKGROUND;
}

static uint32_t t3d_vt_stream_find_free_lane( const t3dVTStreamingState_t& pState )
{
	for ( uint32_t iLane = 0u; iLane < pState.Buffers.nScratchLaneCount; ++iLane ) {
		if ( pState.Buffers.pScratchLaneOwner[iLane] == T3D_VT_INVALID_ID ) {
			return iLane;
		}
	}
	return T3D_VT_INVALID_ID;
}

static bool t3d_vt_stream_queue_insert( t3dVTStreamingState_t& rState, const uint32_t pJobIndex )
{
	if ( pJobIndex >= rState.Buffers.nJobCapacity ) {
		return false;
	}
	t3dVTStreamJob_t& Job = rState.Buffers.pJobs[pJobIndex];
	const uint16_t nBucket = t3d_vt_scheduler_priority_bucket_reference_debug( Job.nPriority );
	if ( nBucket >= T3D_VT_PRIORITY_BUCKET_COUNT || rState.nPriorityBucketCount[nBucket] >= rState.Buffers.nJobCapacity ) {
		return false;
	}
	const uint16_t nTail = rState.nPriorityBucketTail[nBucket];
	Job.nPriorityBucket = nBucket;
	rState.Buffers.pQueuedJobs[pJobIndex] = T3D_VT_INVALID_JOB16;
	rState.Buffers.pJobQueuePosition[pJobIndex] = nTail;
	if ( nTail == T3D_VT_INVALID_JOB16 ) {
		rState.nPriorityBucketHead[nBucket] = ( uint16_t )pJobIndex;
	} else {
		rState.Buffers.pQueuedJobs[nTail] = ( uint16_t )pJobIndex;
	}
	rState.nPriorityBucketTail[nBucket] = ( uint16_t )pJobIndex;
	++rState.nPriorityBucketCount[nBucket];
	++rState.nQueuedJobCount;
	if ( rState.nPriorityBucketCount[nBucket] > rState.Stats.nPriorityBucketHighWater[nBucket] ) {
		rState.Stats.nPriorityBucketHighWater[nBucket] = rState.nPriorityBucketCount[nBucket];
	}
	return true;
}

static void t3d_vt_stream_queue_remove( t3dVTStreamingState_t& rState, const uint32_t pJobIndex )
{
	if ( pJobIndex >= rState.Buffers.nJobCapacity ) {
		return;
	}
	t3dVTStreamJob_t& Job = rState.Buffers.pJobs[pJobIndex];
	const uint16_t nBucket = Job.nPriorityBucket;
	if ( nBucket >= T3D_VT_PRIORITY_BUCKET_COUNT || rState.nPriorityBucketCount[nBucket] == 0u ) {
		return;
	}
	const uint16_t nNext = rState.Buffers.pQueuedJobs[pJobIndex];
	const uint16_t nPrevious = rState.Buffers.pJobQueuePosition[pJobIndex];
	if ( nPrevious == T3D_VT_INVALID_JOB16 ) {
		if ( rState.nPriorityBucketHead[nBucket] != pJobIndex ) {
			return;
		}
		rState.nPriorityBucketHead[nBucket] = nNext;
	} else {
		rState.Buffers.pQueuedJobs[nPrevious] = nNext;
	}
	if ( nNext == T3D_VT_INVALID_JOB16 ) {
		if ( rState.nPriorityBucketTail[nBucket] != pJobIndex ) {
			return;
		}
		rState.nPriorityBucketTail[nBucket] = nPrevious;
	} else {
		rState.Buffers.pJobQueuePosition[nNext] = nPrevious;
	}
	rState.Buffers.pQueuedJobs[pJobIndex] = T3D_VT_INVALID_JOB16;
	rState.Buffers.pJobQueuePosition[pJobIndex] = T3D_VT_INVALID_JOB16;
	--rState.nPriorityBucketCount[nBucket];
	--rState.nQueuedJobCount;
}

static uint32_t t3d_vt_stream_find_existing( const t3dVTStreamingState_t& pState, const uint32_t pVirtualPage )
{
	if ( pVirtualPage >= pState.Buffers.nPageToJobCapacity ) {
		return T3D_VT_INVALID_ID;
	}
	const uint16_t nJob = pState.Buffers.pPageToJob[pVirtualPage];
	return nJob == T3D_VT_INVALID_JOB16 ? T3D_VT_INVALID_ID : ( uint32_t )nJob;
}

static uint32_t t3d_vt_stream_pop_free_job( t3dVTStreamingState_t& rState )
{
	if ( rState.nFreeJobCount == 0u ) {
		return T3D_VT_INVALID_ID;
	}
	return rState.Buffers.pFreeJobs[--rState.nFreeJobCount];
}

static uint32_t t3d_vt_stream_find_replaceable( const t3dVTStreamingState_t& pState, const uint16_t pPriority )
{
	if ( pState.nQueuedJobCount == 0u ) {
		return T3D_VT_INVALID_ID;
	}
	const uint16_t nIncomingBucket = t3d_vt_scheduler_priority_bucket_reference_debug( pPriority );
	for ( int32_t nBucket = ( int32_t )T3D_VT_PRIORITY_BACKGROUND; nBucket > ( int32_t )nIncomingBucket; --nBucket ) {
		const uint16_t nJob = pState.nPriorityBucketTail[nBucket];
		if ( nJob != T3D_VT_INVALID_JOB16 ) {
			return nJob;
		}
	}
	return T3D_VT_INVALID_ID;
}

static void t3d_vt_stream_ready_remove( t3dVTStreamingState_t& rState, const uint32_t pReadyIndex )
{
	if ( pReadyIndex >= rState.nReadyJobCount ) {
		return;
	}
	if ( pReadyIndex + 1u < rState.nReadyJobCount ) {
		memmove( rState.Buffers.pReadyJobs + pReadyIndex, rState.Buffers.pReadyJobs + pReadyIndex + 1u,
		         ( rState.nReadyJobCount - pReadyIndex - 1u ) * sizeof( uint16_t ) );
	}
	--rState.nReadyJobCount;
}

static void t3d_vt_stream_free_job( t3dVTStreamingState_t& rState, const uint32_t pJobIndex )
{
	if ( pJobIndex >= rState.Buffers.nJobCapacity ) {
		return;
	}
	t3dVTStreamJob_t& Job = rState.Buffers.pJobs[pJobIndex];
	if ( Job.nState == T3D_VT_STREAM_QUEUED ) {
		t3d_vt_stream_queue_remove( rState, pJobIndex );
	}
	if ( Job.nVirtualPage < rState.Buffers.nPageToJobCapacity && rState.Buffers.pPageToJob[Job.nVirtualPage] == pJobIndex ) {
		rState.Buffers.pPageToJob[Job.nVirtualPage] = T3D_VT_INVALID_JOB16;
	}
	if ( Job.nScratchLane != T3D_VT_INVALID_ID && Job.nScratchLane < rState.Buffers.nScratchLaneCount ) {
		rState.Buffers.pScratchLaneOwner[Job.nScratchLane] = T3D_VT_INVALID_ID;
	}
	memset( &Job, 0, sizeof( Job ) );
	Job.nScratchLane = T3D_VT_INVALID_ID;
	Job.nPriorityBucket = T3D_VT_PRIORITY_BACKGROUND;
	Job.nState = T3D_VT_STREAM_FREE;
	rState.Buffers.pQueuedJobs[pJobIndex] = T3D_VT_INVALID_JOB16;
	rState.Buffers.pJobQueuePosition[pJobIndex] = T3D_VT_INVALID_JOB16;
	rState.Buffers.pMergedWantedMip[pJobIndex] = 0xffffu;
	rState.Buffers.pMergedPriority[pJobIndex] = 0u;
	if ( rState.nFreeJobCount < rState.Buffers.nJobCapacity ) {
		rState.Buffers.pFreeJobs[rState.nFreeJobCount++] = ( uint16_t )pJobIndex;
	}
	if ( rState.Stats.nActiveJobCount != 0u ) {
		--rState.Stats.nActiveJobCount;
	}
}

static bool t3d_vt_stream_has_required_storage( const t3dVTResidencyState_t& rResidency, const t3dVTStreamingBuffers_t& pBuffers )
{
	return rResidency.bInitialized && rResidency.pDatabase != NULL && rResidency.pDatabase->pHeader != NULL && pBuffers.pJobs != NULL &&
	       pBuffers.nJobCapacity != 0u && pBuffers.nJobCapacity <= 0xffffu && pBuffers.pScratchBytes != NULL &&
	       pBuffers.pScratchLaneOwner != NULL && pBuffers.pScratchLaneCompletion != NULL && pBuffers.pPageToJob != NULL &&
	       pBuffers.nPageToJobCapacity >= rResidency.pDatabase->pHeader->nPageCount && pBuffers.pFreeJobs != NULL &&
	       pBuffers.pQueuedJobs != NULL && pBuffers.pJobQueuePosition != NULL && pBuffers.pReadyJobs != NULL &&
	       pBuffers.pMergedWantedMip != NULL && pBuffers.pMergedPriority != NULL && pBuffers.nScratchLaneCount != 0u &&
	       pBuffers.nScratchLaneCount <= T3D_VT_MAX_STREAM_SCRATCH_LANES && pBuffers.nScratchLaneBytes != 0u;
}

static bool t3d_vt_stream_has_valid_limits( const t3dVTStreamingBuffers_t& pBuffers, const t3dVTStreamingLimits_t& pLimits )
{
	return pLimits.nMaxUploadBytesPerFrame != 0u && pLimits.nMaxPageSetsPerFrame != 0u && pLimits.nMaxInFlightIoJobs != 0u &&
	       pLimits.nMaxInFlightIoJobs <= pBuffers.nScratchLaneCount && pLimits.nMaxAdmittedRequestsPerFrame != 0u &&
	       pLimits.nMaxAdmittedRequestsPerFrame <= pBuffers.nJobCapacity;
}

bool t3d_vt_streaming_initialize_cold( t3dVTResidencyState_t& rResidency, const t3dVTStreamingBuffers_t& pBuffers,
                                       const t3dVTStreamingLimits_t& pLimits, t3dVTStreamingState_t& rState )
{
	memset( &rState, 0, sizeof( rState ) );
	if ( !t3d_vt_stream_has_required_storage( rResidency, pBuffers ) ) {
		return false;
	}
	if ( !t3d_vt_stream_has_valid_limits( pBuffers, pLimits ) ) {
		return false;
	}
	rState.pResidency = &rResidency;
	rState.Buffers = pBuffers;
	rState.Limits = pLimits;
	rState.nAdmissionFrame = T3D_VT_INVALID_ID;
	rState.nUploadFrame = T3D_VT_INVALID_ID;
	for ( uint32_t nBucket = 0u; nBucket < T3D_VT_PRIORITY_BUCKET_COUNT; ++nBucket ) {
		rState.nPriorityBucketHead[nBucket] = T3D_VT_INVALID_JOB16;
		rState.nPriorityBucketTail[nBucket] = T3D_VT_INVALID_JOB16;
	}
	for ( uint32_t iPage = 0u; iPage < pBuffers.nPageToJobCapacity; ++iPage ) {
		pBuffers.pPageToJob[iPage] = T3D_VT_INVALID_JOB16;
	}
	for ( uint32_t iJob = 0u; iJob < pBuffers.nJobCapacity; ++iJob ) {
		memset( &pBuffers.pJobs[iJob], 0, sizeof( t3dVTStreamJob_t ) );
		pBuffers.pJobs[iJob].nScratchLane = T3D_VT_INVALID_ID;
		pBuffers.pJobs[iJob].nPriorityBucket = T3D_VT_PRIORITY_BACKGROUND;
		pBuffers.pQueuedJobs[iJob] = T3D_VT_INVALID_JOB16;
		pBuffers.pJobQueuePosition[iJob] = T3D_VT_INVALID_JOB16;
		pBuffers.pMergedWantedMip[iJob] = 0xffffu;
		pBuffers.pMergedPriority[iJob] = 0u;
		pBuffers.pFreeJobs[iJob] = ( uint16_t )( pBuffers.nJobCapacity - 1u - iJob );
	}
	rState.nFreeJobCount = pBuffers.nJobCapacity;
	for ( uint32_t iLane = 0u; iLane < pBuffers.nScratchLaneCount; ++iLane ) {
		pBuffers.pScratchLaneOwner[iLane] = T3D_VT_INVALID_ID;
		pBuffers.pScratchLaneCompletion[iLane].store( T3D_VT_STREAM_COMPLETION_IDLE, T3D_MEMORY_ORDER_RELAXED );
	}
	rState.bInitialized = true;
	return true;
}

bool t3d_vt_streaming_initialize_cold( t3dVTResidencyState_t& rResidency, const t3dVTStreamingBuffers_t& pBuffers,
                                       const uint64_t pUploadBudgetBytesPerFrame, t3dVTStreamingState_t& rState )
{
	t3dVTStreamingLimits_t Limits = {};
	Limits.nMaxUploadBytesPerFrame = pUploadBudgetBytesPerFrame;
	Limits.nMaxPageSetsPerFrame = pBuffers.nJobCapacity;
	Limits.nMaxInFlightIoJobs = pBuffers.nScratchLaneCount;
	Limits.nMaxAdmittedRequestsPerFrame = pBuffers.nJobCapacity;
	return t3d_vt_streaming_initialize_cold( rResidency, pBuffers, Limits, rState );
}

static void t3d_vt_stream_begin_admission_frame( t3dVTStreamingState_t& rState, const uint32_t pFrameId )
{
	if ( rState.nAdmissionFrame != pFrameId ) {
		rState.nAdmissionFrame = pFrameId;
		rState.nAdmittedRequestCountThisFrame = 0u;
	}
}

static bool t3d_vt_stream_prepare_job_identity( t3dVTStreamingState_t& rState, const t3dVTRuntimeRequest_t& pRequest,
                                                const uint32_t pFrameId, t3dVTStreamJob_t& rJob )
{
	const t3dVTDatabaseView_t *const pDatabase = rState.pResidency->pDatabase;
	if ( pDatabase == NULL || pDatabase->pHeader == NULL || pDatabase->pPages == NULL || pDatabase->pPayloads == NULL ||
	     pRequest.nVirtualPage >= pDatabase->pHeader->nPageCount ) {
		return false;
	}
	const t3dVTPageFileRecord_t& Page = pDatabase->pPages[pRequest.nVirtualPage];
	if ( Page.nPayloadId >= pDatabase->pHeader->nPayloadCount || Page.nAllocationId >= pDatabase->pHeader->nAllocationCount ) {
		return false;
	}
	const t3dVTPayloadRecord_t& Payload = pDatabase->pPayloads[Page.nPayloadId];
	if ( Payload.nCompressedBytes == 0u || Payload.nDecodedBytes == 0u || Payload.nCompressedBytes > rState.Buffers.nScratchLaneBytes ||
	     Payload.nDecodedBytes > rState.Buffers.nScratchLaneBytes || Payload.nOffset > pDatabase->pHeader->nPayloadDataBytes ||
	     Payload.nCompressedBytes > pDatabase->pHeader->nPayloadDataBytes - Payload.nOffset ) {
		++rState.Stats.nOversizedReadCount;
		return false;
	}
	const t3dVTPageTableEntry_t& Entry = rState.pResidency->Buffers.pPageTable[pRequest.nVirtualPage];
	memset( &rJob, 0, sizeof( rJob ) );
	rJob.nReadOffset = Payload.nOffset;
	rJob.nReadBytes = Payload.nCompressedBytes;
	rJob.nExpectedDecodedBytes = Payload.nDecodedBytes;
	rJob.nVirtualPage = pRequest.nVirtualPage;
	rJob.nAllocationId = Page.nAllocationId;
	rJob.nPayloadId = Page.nPayloadId;
	rJob.nContentId = Page.nPayloadId;
	rJob.nPageGeneration = Entry.nGeneration;
	rJob.nAllocationGeneration = Page.nGeneration;
	rJob.nDatabaseGeneration = rState.pResidency->nDatabaseGeneration;
	rJob.nRequestFrame = pFrameId;
	rJob.nScratchLane = T3D_VT_INVALID_ID;
	rJob.nWantedMip = pRequest.nWantedMip;
	rJob.nPriority = pRequest.nPriority;
	rJob.nPriorityBucket = t3d_vt_scheduler_priority_bucket_reference_debug( pRequest.nPriority );
	rJob.nChannel = Page.nChannel;
	rJob.nState = T3D_VT_STREAM_QUEUED;
	rJob.nResultCode = T3D_VT_STREAM_RESULT_OK;
	return true;
}

static void t3d_vt_stream_record_admission_status( t3dVTStreamingState_t& rState, const t3dVTAdmissionStatus_t pStatus )
{
	if ( pStatus < T3D_VT_ADMISSION_STATUS_COUNT ) {
		++rState.Stats.nAdmissionStatusCount[pStatus];
	}
}

t3dVTAdmissionStatus_t t3d_vt_streaming_admit_request_hot( t3dVTStreamingState_t& rState, const t3dVTRuntimeRequest_t& pRequest,
                                                           const uint32_t pFrameId )
{
	if ( rState.bQuiescing ) {
		return T3D_VT_ADMISSION_DEFERRED_FRAME_BUDGET;
	}
	if ( !rState.bInitialized || rState.pResidency == NULL || rState.pResidency->pDatabase == NULL ||
	     rState.pResidency->pDatabase->pHeader == NULL || pRequest.nVirtualPage >= rState.pResidency->pDatabase->pHeader->nPageCount ) {
		++rState.Stats.nDroppedRequestCount;
		t3d_vt_stream_record_admission_status( rState, T3D_VT_ADMISSION_INVALID_PAGE );
		return T3D_VT_ADMISSION_INVALID_PAGE;
	}
	t3d_vt_stream_begin_admission_frame( rState, pFrameId );
	const t3dVTPageTableEntry_t& Entry = rState.pResidency->Buffers.pPageTable[pRequest.nVirtualPage];
	if ( ( Entry.nFlags & T3D_VT_PAGE_RESIDENT ) != 0u ) {
		t3d_vt_touch_page_hot( *rState.pResidency, pRequest.nVirtualPage, pFrameId );
		++rState.Stats.nPageHitCount;
		++rState.Stats.nResidentRequestCount;
		t3d_vt_stream_record_admission_status( rState, T3D_VT_ADMISSION_ALREADY_RESIDENT );
		return T3D_VT_ADMISSION_ALREADY_RESIDENT;
	}
	++rState.Stats.nPageMissCount;

	uint32_t nJob = t3d_vt_stream_find_existing( rState, pRequest.nVirtualPage );
	if ( nJob != T3D_VT_INVALID_ID ) {
		t3dVTStreamJob_t& Job = rState.Buffers.pJobs[nJob];
		const uint16_t nOldPriority = rState.Buffers.pMergedPriority[nJob];
		const uint16_t nOldWantedMip = rState.Buffers.pMergedWantedMip[nJob];
		const uint16_t nMergedPriority = pRequest.nPriority > nOldPriority ? pRequest.nPriority : nOldPriority;
		const uint16_t nMergedWantedMip = pRequest.nWantedMip < nOldWantedMip ? pRequest.nWantedMip : nOldWantedMip;
		rState.Buffers.pMergedPriority[nJob] = nMergedPriority;
		rState.Buffers.pMergedWantedMip[nJob] = nMergedWantedMip;
		if ( Job.nState == T3D_VT_STREAM_QUEUED && ( nMergedPriority != Job.nPriority || nMergedWantedMip != Job.nWantedMip ) ) {
			const uint16_t nOldBucket = Job.nPriorityBucket;
			const uint16_t nNewBucket = t3d_vt_scheduler_priority_bucket_reference_debug( nMergedPriority );
			if ( nNewBucket != nOldBucket ) {
				t3d_vt_stream_queue_remove( rState, nJob );
			}
			Job.nPriority = nMergedPriority;
			Job.nWantedMip = nMergedWantedMip;
			if ( nNewBucket != nOldBucket && !t3d_vt_stream_queue_insert( rState, nJob ) ) {
				t3d_vt_stream_free_job( rState, nJob );
				++rState.Stats.nDroppedRequestCount;
				t3d_vt_stream_record_admission_status( rState, T3D_VT_ADMISSION_DROPPED_QUEUE_FULL );
				return T3D_VT_ADMISSION_DROPPED_QUEUE_FULL;
			}
			if ( nNewBucket < nOldBucket ) {
				++rState.Stats.nPriorityPromotionCount;
			}
		}
		++rState.Stats.nDuplicateMergeCount;
		t3d_vt_stream_record_admission_status( rState, T3D_VT_ADMISSION_MERGED_EXISTING );
		return T3D_VT_ADMISSION_MERGED_EXISTING;
	}

	if ( rState.nAdmittedRequestCountThisFrame >= rState.Limits.nMaxAdmittedRequestsPerFrame ) {
		++rState.Stats.nAdmissionBudgetDeferredCount;
		++rState.Stats.nBudgetDeferredCount;
		t3d_vt_stream_record_admission_status( rState, T3D_VT_ADMISSION_DEFERRED_FRAME_BUDGET );
		return T3D_VT_ADMISSION_DEFERRED_FRAME_BUDGET;
	}
	nJob = t3d_vt_stream_pop_free_job( rState );
	if ( nJob == T3D_VT_INVALID_ID ) {
		nJob = t3d_vt_stream_find_replaceable( rState, pRequest.nPriority );
		if ( nJob == T3D_VT_INVALID_ID ) {
			++rState.Stats.nDroppedRequestCount;
			t3d_vt_stream_record_admission_status( rState, T3D_VT_ADMISSION_DROPPED_QUEUE_FULL );
			return T3D_VT_ADMISSION_DROPPED_QUEUE_FULL;
		}
		t3d_vt_stream_free_job( rState, nJob );
		nJob = t3d_vt_stream_pop_free_job( rState );
		++rState.Stats.nReplacedQueuedJobCount;
	}
	t3dVTStreamJob_t& Job = rState.Buffers.pJobs[nJob];
	if ( !t3d_vt_stream_prepare_job_identity( rState, pRequest, pFrameId, Job ) ) {
		memset( &Job, 0, sizeof( Job ) );
		Job.nScratchLane = T3D_VT_INVALID_ID;
		Job.nPriorityBucket = T3D_VT_PRIORITY_BACKGROUND;
		Job.nState = T3D_VT_STREAM_FREE;
		rState.Buffers.pFreeJobs[rState.nFreeJobCount++] = ( uint16_t )nJob;
		++rState.Stats.nDroppedRequestCount;
		t3d_vt_stream_record_admission_status( rState, T3D_VT_ADMISSION_INVALID_PAYLOAD );
		return T3D_VT_ADMISSION_INVALID_PAYLOAD;
	}
	rState.Buffers.pMergedWantedMip[nJob] = pRequest.nWantedMip;
	rState.Buffers.pMergedPriority[nJob] = pRequest.nPriority;
	++rState.Stats.nActiveJobCount;
	if ( rState.Stats.nActiveJobCount > rState.Stats.nQueueHighWater ) {
		rState.Stats.nQueueHighWater = rState.Stats.nActiveJobCount;
	}
	rState.Buffers.pPageToJob[pRequest.nVirtualPage] = ( uint16_t )nJob;
	if ( !t3d_vt_stream_queue_insert( rState, nJob ) ) {
		t3d_vt_stream_free_job( rState, nJob );
		++rState.Stats.nDroppedRequestCount;
		t3d_vt_stream_record_admission_status( rState, T3D_VT_ADMISSION_DROPPED_QUEUE_FULL );
		return T3D_VT_ADMISSION_DROPPED_QUEUE_FULL;
	}
	++rState.nAdmittedRequestCountThisFrame;
	++rState.Stats.nQueuedCount;
	t3d_vt_stream_record_admission_status( rState, T3D_VT_ADMISSION_ACCEPTED_NEW );
	return T3D_VT_ADMISSION_ACCEPTED_NEW;
}

uint32_t t3d_vt_streaming_ingest_requests_hot( t3dVTStreamingState_t& rState, const t3dVTRuntimeRequest_t *pRequests,
                                               const uint32_t pRequestCount, const uint32_t pFrameId )
{
	if ( !rState.bInitialized || pRequests == NULL ) {
		return 0u;
	}
	t3d_vt_stream_begin_admission_frame( rState, pFrameId );
	uint32_t nAccepted = 0u;
	for ( uint32_t iRequest = 0u; iRequest < pRequestCount; ++iRequest ) {
		if ( t3d_vt_streaming_admit_request_hot( rState, pRequests[iRequest], pFrameId ) == T3D_VT_ADMISSION_ACCEPTED_NEW ) {
			++nAccepted;
		}
	}
	return nAccepted;
}

t3dVTReadbackConsumeStatus_t t3d_vt_streaming_ingest_readback_owner_hot( t3dVTStreamingState_t& rState,
                                                                         const t3dVTReadbackSlotHeader_t& pHeader,
                                                                         const uint8_t *pReadbackBytes, const uint32_t pReadbackByteCount,
                                                                         t3dVTGpuCounters_t& rCounters, uint32_t& rAcceptedRequestCount )
{
	memset( &rCounters, 0, sizeof( rCounters ) );
	rAcceptedRequestCount = 0u;
	if ( !rState.bInitialized || rState.pResidency == NULL || pReadbackBytes == NULL || pHeader.nRequestCapacity == 0u ||
	     pHeader.nReadbackBytes == 0u || pHeader.nReadbackBytes != pReadbackByteCount ) {
		++rState.Stats.nInvalidReadbackCount;
		return T3D_VT_READBACK_INVALID;
	}
	if ( pHeader.nDatabaseGeneration != rState.pResidency->nDatabaseGeneration ) {
		++rState.Stats.nStaleReadbackCount;
		return T3D_VT_READBACK_STALE_DATABASE;
	}
	uint32_t nRequestOffset = 0u;
	uint32_t nRequestCount = 0u;
	if ( !t3d_vt_decode_readback_reference_debug( pReadbackBytes, pReadbackByteCount, pHeader.nRequestCapacity, rCounters, nRequestOffset,
	                                              nRequestCount ) ) {
		++rState.Stats.nInvalidReadbackCount;
		return T3D_VT_READBACK_INVALID;
	}
	t3d_vt_stream_begin_admission_frame( rState, pHeader.nFrameId );
	rAcceptedRequestCount = 0u;
	for ( uint32_t iRequest = 0u; iRequest < nRequestCount; ++iRequest ) {
		t3dVTRuntimeRequest_t Request{};
		memcpy( &Request, pReadbackBytes + nRequestOffset + ( size_t )iRequest * sizeof( Request ), sizeof( Request ) );
		const t3dVTAdmissionStatus_t Status = t3d_vt_streaming_admit_request_hot( rState, Request, pHeader.nFrameId );
		if ( Status == T3D_VT_ADMISSION_ACCEPTED_NEW ) {
			++rAcceptedRequestCount;
		}
	}
	return T3D_VT_READBACK_CONSUMED;
}

uint32_t t3d_vt_streaming_ingest_readback_owner_hot( t3dVTStreamingState_t& rState, const uint8_t *pReadbackBytes,
                                                     const uint32_t pReadbackByteCount, const uint32_t pRequestCapacity,
                                                     const uint32_t pFrameId, t3dVTGpuCounters_t& rCounters )
{
	t3dVTReadbackSlotHeader_t Header = {};
	Header.nFrameId = pFrameId;
	Header.nDatabaseGeneration = rState.pResidency != NULL ? rState.pResidency->nDatabaseGeneration : 0u;
	Header.nRequestCapacity = pRequestCapacity;
	Header.nReadbackBytes = pReadbackByteCount;
	uint32_t nAcceptedRequestCount = 0u;
	return t3d_vt_streaming_ingest_readback_owner_hot( rState, Header, pReadbackBytes, pReadbackByteCount, rCounters,
	                                                   nAcceptedRequestCount ) == T3D_VT_READBACK_CONSUMED
	           ? nAcceptedRequestCount
			   : 0u;
}

uint32_t t3d_vt_streaming_invalidate_queued_demand_hot( t3dVTStreamingState_t& rState )
{
	if ( !rState.bInitialized ) {
		return 0u;
	}
	uint32_t nCancelled = 0u;
	for ( uint32_t iJob = 0u; iJob < rState.Buffers.nJobCapacity; ++iJob ) {
		if ( rState.Buffers.pJobs[iJob].nState != T3D_VT_STREAM_QUEUED ) {
			continue;
		}
		t3d_vt_stream_free_job( rState, iJob );
		++nCancelled;
	}
	rState.nAdmissionFrame = T3D_VT_INVALID_ID;
	rState.nAdmittedRequestCountThisFrame = 0u;
	return nCancelled;
}

bool t3d_vt_streaming_claim_job_owner_hot( t3dVTStreamingState_t& rState, uint32_t& rJobIndex )
{
	rJobIndex = T3D_VT_INVALID_ID;
	if ( !rState.bInitialized || rState.bQuiescing || rState.nQueuedJobCount == 0u ) {
		return false;
	}
	if ( rState.nInFlightIoJobCount >= rState.Limits.nMaxInFlightIoJobs ) {
		++rState.Stats.nIoBudgetDeferredCount;
		++rState.Stats.nBudgetDeferredCount;
		return false;
	}
	const uint32_t nLane = t3d_vt_stream_find_free_lane( rState );
	if ( nLane == T3D_VT_INVALID_ID ) {
		return false;
	}
	uint32_t nJob = T3D_VT_INVALID_ID;
	uint16_t nBucket = T3D_VT_PRIORITY_BACKGROUND;
	for ( uint16_t iBucket = 0u; iBucket < T3D_VT_PRIORITY_BUCKET_COUNT; ++iBucket ) {
		if ( rState.nPriorityBucketHead[iBucket] != T3D_VT_INVALID_JOB16 ) {
			nJob = rState.nPriorityBucketHead[iBucket];
			nBucket = iBucket;
			break;
		}
	}
	if ( nJob == T3D_VT_INVALID_ID ) {
		return false;
	}
	t3d_vt_stream_queue_remove( rState, nJob );
	t3dVTStreamJob_t& Job = rState.Buffers.pJobs[nJob];
	Job.nState = T3D_VT_STREAM_WORKING;
	Job.nScratchLane = nLane;
	rState.Buffers.pScratchLaneOwner[nLane] = nJob;
	rState.Buffers.pScratchLaneCompletion[nLane].store( T3D_VT_STREAM_COMPLETION_IDLE, T3D_MEMORY_ORDER_RELAXED );
	++rState.nInFlightIoJobCount;
	if ( rState.nInFlightIoJobCount > rState.Stats.nInFlightIoHighWater ) {
		rState.Stats.nInFlightIoHighWater = rState.nInFlightIoJobCount;
	}
	++rState.Stats.nPriorityBucketClaimCount[nBucket];
	++rState.Stats.nClaimedCount;
	rJobIndex = nJob;
	return true;
}

bool t3d_vt_streaming_cancel_claim_owner_hot( t3dVTStreamingState_t& rState, const uint32_t pJobIndex )
{
	if ( !rState.bInitialized || pJobIndex >= rState.Buffers.nJobCapacity ) {
		return false;
	}
	t3dVTStreamJob_t& Job = rState.Buffers.pJobs[pJobIndex];
	if ( Job.nState != T3D_VT_STREAM_WORKING || Job.nScratchLane >= rState.Buffers.nScratchLaneCount ||
	     rState.Buffers.pScratchLaneOwner[Job.nScratchLane] != pJobIndex ) {
		return false;
	}
	rState.Buffers.pScratchLaneOwner[Job.nScratchLane] = T3D_VT_INVALID_ID;
	rState.Buffers.pScratchLaneCompletion[Job.nScratchLane].store( T3D_VT_STREAM_COMPLETION_IDLE, T3D_MEMORY_ORDER_RELAXED );
	if ( rState.nInFlightIoJobCount != 0u ) {
		--rState.nInFlightIoJobCount;
	}
	Job.nScratchLane = T3D_VT_INVALID_ID;
	Job.nPriority = rState.Buffers.pMergedPriority[pJobIndex];
	Job.nWantedMip = rState.Buffers.pMergedWantedMip[pJobIndex];
	Job.nState = T3D_VT_STREAM_QUEUED;
	return t3d_vt_stream_queue_insert( rState, pJobIndex );
}

bool t3d_vt_streaming_execute_job_async( t3dVTStreamingState_t& rState, const uint32_t pJobIndex, t3dVTStreamReadDecodeFn_t pReadDecode,
                                         void *pUser )
{
	if ( !rState.bInitialized || pReadDecode == NULL || pJobIndex >= rState.Buffers.nJobCapacity ) {
		return false;
	}
	t3dVTStreamJob_t& Job = rState.Buffers.pJobs[pJobIndex];
	if ( Job.nState != T3D_VT_STREAM_WORKING || Job.nScratchLane >= rState.Buffers.nScratchLaneCount || Job.nReadBytes == 0u ||
	     Job.nExpectedDecodedBytes == 0u || Job.nReadBytes > rState.Buffers.nScratchLaneBytes ||
	     Job.nExpectedDecodedBytes > rState.Buffers.nScratchLaneBytes ) {
		return false;
	}
	uint8_t *pScratch = rState.Buffers.pScratchBytes + ( uint64_t )Job.nScratchLane * rState.Buffers.nScratchLaneBytes;
	uint32_t nDecodedBytes = 0u;
	const bool bOk = pReadDecode( *rState.pResidency->pDatabase, Job, pScratch, rState.Buffers.nScratchLaneBytes, nDecodedBytes, pUser );
	if ( !bOk || nDecodedBytes == 0u || nDecodedBytes > rState.Buffers.nScratchLaneBytes ) {
		Job.nDecodedBytes = 0u;
		Job.nResultCode = T3D_VT_STREAM_RESULT_READ_FAILED;
		rState.Buffers.pScratchLaneCompletion[Job.nScratchLane].store( T3D_VT_STREAM_COMPLETION_FAILED, T3D_MEMORY_ORDER_RELEASE );
		return false;
	}
	if ( nDecodedBytes != Job.nExpectedDecodedBytes ) {
		Job.nDecodedBytes = 0u;
		Job.nResultCode = T3D_VT_STREAM_RESULT_OUTPUT_SIZE_MISMATCH;
		rState.Buffers.pScratchLaneCompletion[Job.nScratchLane].store( T3D_VT_STREAM_COMPLETION_FAILED, T3D_MEMORY_ORDER_RELEASE );
		return false;
	}
	Job.nDecodedBytes = nDecodedBytes;
	Job.nResultCode = T3D_VT_STREAM_RESULT_OK;
	rState.Buffers.pScratchLaneCompletion[Job.nScratchLane].store( T3D_VT_STREAM_COMPLETION_READY, T3D_MEMORY_ORDER_RELEASE );
	return true;
}

static void t3d_vt_stream_collect_completions_owner_hot( t3dVTStreamingState_t& rState )
{
	for ( uint32_t iLane = 0u; iLane < rState.Buffers.nScratchLaneCount; ++iLane ) {
		const uint32_t nJob = rState.Buffers.pScratchLaneOwner[iLane];
		if ( nJob == T3D_VT_INVALID_ID || nJob >= rState.Buffers.nJobCapacity ) {
			continue;
		}
		t3dVTStreamJob_t& Job = rState.Buffers.pJobs[nJob];
		if ( Job.nState != T3D_VT_STREAM_WORKING || Job.nScratchLane != iLane ) {
			continue;
		}
		const uint32_t nCompletion = rState.Buffers.pScratchLaneCompletion[iLane].load( T3D_MEMORY_ORDER_ACQUIRE );
		if ( nCompletion == T3D_VT_STREAM_COMPLETION_IDLE ) {
			continue;
		}
		rState.Buffers.pScratchLaneCompletion[iLane].store( T3D_VT_STREAM_COMPLETION_IDLE, T3D_MEMORY_ORDER_RELAXED );
		if ( rState.nInFlightIoJobCount != 0u ) {
			--rState.nInFlightIoJobCount;
		}
		Job.nState = nCompletion == T3D_VT_STREAM_COMPLETION_READY ? T3D_VT_STREAM_READY : T3D_VT_STREAM_FAILED;
		if ( Job.nState == T3D_VT_STREAM_READY ) {
			++rState.Stats.nCompletedCount;
			const t3dVTDatabaseView_t *const pDatabase = rState.pResidency != NULL ? rState.pResidency->pDatabase : NULL;
			if ( pDatabase != NULL && pDatabase->pHeader != NULL && pDatabase->pPayloads != NULL &&
			     Job.nPayloadId < pDatabase->pHeader->nPayloadCount ) {
				rState.Stats.nIoBytesTotal += pDatabase->pPayloads[Job.nPayloadId].nCompressedBytes;
			}
		} else {
			++rState.Stats.nDecodeFailureCount;
		}
		if ( rState.nReadyJobCount < rState.Buffers.nJobCapacity ) {
			rState.Buffers.pReadyJobs[rState.nReadyJobCount++] = ( uint16_t )nJob;
		}
	}
}

static void t3d_vt_stream_begin_upload_frame( t3dVTStreamingState_t& rState, const uint32_t pFrameId )
{
	if ( rState.nUploadFrame != pFrameId ) {
		rState.nUploadFrame = pFrameId;
		rState.nUploadedPageSetCountThisFrame = 0u;
		rState.nUploadedBytesThisFrame = 0u;
	}
}

bool t3d_vt_streaming_quiesce_package_warm( t3dVTStreamingState_t& rState )
{
	if ( !rState.bInitialized ) {
		return false;
	}
	rState.bQuiescing = true;
	( void )t3d_vt_streaming_invalidate_queued_demand_hot( rState );
	t3d_vt_stream_collect_completions_owner_hot( rState );
	if ( rState.nInFlightIoJobCount != 0u ) {
		return false;
	}
	for ( uint32_t i = 0u; i < rState.nReadyJobCount; ++i ) {
		t3d_vt_stream_free_job( rState, rState.Buffers.pReadyJobs[i] );
	}
	rState.nReadyJobCount = 0u;
	for ( uint32_t i = 0u; i < rState.Buffers.nScratchLaneCount; ++i ) {
		if ( rState.Buffers.pScratchLaneOwner[i] != T3D_VT_INVALID_ID ||
		     rState.Buffers.pScratchLaneCompletion[i].load( T3D_MEMORY_ORDER_ACQUIRE ) != T3D_VT_STREAM_COMPLETION_IDLE ) {
			return false;
		}
	}
	return true;
}

void t3d_vt_streaming_resume_package_warm( t3dVTStreamingState_t& rState )
{
	rState.bQuiescing = false;
}

static uint32_t t3d_vt_stream_validate_completion_identity( const t3dVTStreamingState_t& pState, const t3dVTStreamJob_t& pJob )
{
	if ( pState.pResidency == NULL || pState.pResidency->pDatabase == NULL || pState.pResidency->pDatabase->pHeader == NULL ||
	     pJob.nDatabaseGeneration != pState.pResidency->nDatabaseGeneration ) {
		return T3D_VT_STREAM_RESULT_STALE_DATABASE;
	}
	const t3dVTDatabaseView_t& Database = *pState.pResidency->pDatabase;
	if ( pJob.nVirtualPage >= Database.pHeader->nPageCount || pJob.nVirtualPage >= pState.pResidency->Buffers.nPageTableCapacity ||
	     pJob.nPageGeneration != pState.pResidency->Buffers.pPageTable[pJob.nVirtualPage].nGeneration ) {
		return T3D_VT_STREAM_RESULT_STALE_PAGE;
	}
	const t3dVTPageFileRecord_t& Page = Database.pPages[pJob.nVirtualPage];
	if ( Page.nAllocationId != pJob.nAllocationId || Page.nPayloadId != pJob.nPayloadId ||
	     Page.nGeneration != pJob.nAllocationGeneration ) {
		return T3D_VT_STREAM_RESULT_STALE_ALLOCATION;
	}
	return T3D_VT_STREAM_RESULT_OK;
}

static bool t3d_vt_stream_ready_before( const t3dVTStreamingState_t& pState, const uint32_t pJobA, const uint32_t pJobB )
{
	const uint16_t nPriorityA = pState.Buffers.pMergedPriority[pJobA];
	const uint16_t nPriorityB = pState.Buffers.pMergedPriority[pJobB];
	const uint16_t nBucketA = t3d_vt_scheduler_priority_bucket_reference_debug( nPriorityA );
	const uint16_t nBucketB = t3d_vt_scheduler_priority_bucket_reference_debug( nPriorityB );
	if ( nBucketA != nBucketB ) {
		return nBucketA < nBucketB;
	}
	if ( nPriorityA != nPriorityB ) {
		return nPriorityA > nPriorityB;
	}
	return pState.Buffers.pJobs[pJobA].nRequestFrame < pState.Buffers.pJobs[pJobB].nRequestFrame;
}

static uint32_t t3d_vt_stream_best_ready_index( const t3dVTStreamingState_t& pState, const uint32_t pStartIndex )
{
	uint32_t nBestIndex = pStartIndex;
	for ( uint32_t iReady = pStartIndex + 1u; iReady < pState.nReadyJobCount; ++iReady ) {
		if ( t3d_vt_stream_ready_before( pState, pState.Buffers.pReadyJobs[iReady], pState.Buffers.pReadyJobs[nBestIndex] ) ) {
			nBestIndex = iReady;
		}
	}
	return nBestIndex;
}

uint64_t t3d_vt_streaming_service_uploads_batched_hot( t3dVTStreamingState_t& rState, const uint32_t pFrameId,
                                                       t3dVTUploadBatchBeginFn_t pBatchBegin, t3dVTUploadCommitFn_t pUploadStage,
                                                       t3dVTUploadBatchEndFn_t pBatchEnd, void *pUser )
{
	if ( !rState.bInitialized || rState.bQuiescing || pUploadStage == NULL || ( pBatchBegin == NULL ) != ( pBatchEnd == NULL ) ) {
		return 0u;
	}
	t3d_vt_stream_collect_completions_owner_hot( rState );
	t3d_vt_stream_begin_upload_frame( rState, pFrameId );
	const uint64_t nUploadedBefore = rState.nUploadedBytesThisFrame;
	const bool bBatched = pBatchBegin != NULL;
	if ( bBatched && !pBatchBegin( pUser ) ) {
		return 0u;
	}
	uint32_t iReady = 0u;
	uint32_t nStagedPageSets = 0u;
	uint64_t nStagedBytes = 0u;
	while ( iReady < rState.nReadyJobCount ) {
		const uint32_t nBestIndex = t3d_vt_stream_best_ready_index( rState, iReady );
		if ( nBestIndex != iReady ) {
			const uint16_t nSwap = rState.Buffers.pReadyJobs[iReady];
			rState.Buffers.pReadyJobs[iReady] = rState.Buffers.pReadyJobs[nBestIndex];
			rState.Buffers.pReadyJobs[nBestIndex] = nSwap;
		}
		const uint32_t nJob = rState.Buffers.pReadyJobs[iReady];
		t3dVTStreamJob_t& Job = rState.Buffers.pJobs[nJob];
		if ( Job.nState == T3D_VT_STREAM_FAILED ) {
			t3d_vt_stream_ready_remove( rState, iReady );
			t3d_vt_stream_free_job( rState, nJob );
			continue;
		}
		if ( Job.nState != T3D_VT_STREAM_READY ) {
			t3d_vt_stream_ready_remove( rState, iReady );
			continue;
		}
		const uint32_t nIdentityResult = t3d_vt_stream_validate_completion_identity( rState, Job );
		if ( nIdentityResult != T3D_VT_STREAM_RESULT_OK ||
		     ( rState.pResidency->Buffers.pPageTable[Job.nVirtualPage].nFlags & T3D_VT_PAGE_RESIDENT ) != 0u ) {
			Job.nResultCode = nIdentityResult;
			++rState.Stats.nStaleCompletionCount;
			t3d_vt_stream_ready_remove( rState, iReady );
			t3d_vt_stream_free_job( rState, nJob );
			continue;
		}
		if ( rState.nUploadedPageSetCountThisFrame + nStagedPageSets >= rState.Limits.nMaxPageSetsPerFrame ) {
			++rState.Stats.nPageSetBudgetDeferredCount;
			++rState.Stats.nBudgetDeferredCount;
			++iReady;
			continue;
		}
		const uint64_t nFrameBytes = rState.nUploadedBytesThisFrame + nStagedBytes;
		if ( nFrameBytes >= rState.Limits.nMaxUploadBytesPerFrame ||
		     Job.nDecodedBytes > rState.Limits.nMaxUploadBytesPerFrame - nFrameBytes ) {
			++rState.Stats.nUploadByteBudgetDeferredCount;
			++rState.Stats.nBudgetDeferredCount;
			++iReady;
			continue;
		}
		// One admission for the complete physical tile and page-table update.
		// Denial leaves the ready job and its scratch lane intact for a later frame.
		t3dGpuTransferTicket_t transfer_ticket;
		if ( !Terrain3DGpuTransferBudget::get().reserve( T3D_GPU_TRANSFER_VT, ( uint64_t )Job.nDecodedBytes + 64u, 2u, transfer_ticket ) ) {
			++rState.Stats.nBudgetDeferredCount;
			++iReady;
			continue;
		}
		t3dVTUploadReservation_t Reservation = {};
		const uint16_t nMergedWantedMip = rState.Buffers.pMergedWantedMip[nJob];
		if ( !t3d_vt_reserve_upload_hot( *rState.pResidency, Job.nVirtualPage, nMergedWantedMip, pFrameId, Reservation ) ) {
			Terrain3DGpuTransferBudget::get().refund( transfer_ticket, transfer_ticket.bytes(), transfer_ticket.calls() );
			++iReady;
			continue;
		}
		const uint8_t *pScratch = rState.Buffers.pScratchBytes + ( uint64_t )Job.nScratchLane * rState.Buffers.nScratchLaneBytes;
		if ( !pUploadStage( *rState.pResidency, Reservation, pScratch, Job.nDecodedBytes, pFrameId, pUser ) ) {
			// Failed GPU commit can have submitted a partial copy: charge its full estimate.
			t3d_vt_abort_upload_hot( *rState.pResidency, Reservation );
			break;
		}
		if ( bBatched ) {
			Job.nState = T3D_VT_STREAM_STAGED;
			++nStagedPageSets;
			nStagedBytes += Job.nDecodedBytes;
			++iReady;
			continue;
		}
		rState.nUploadedBytesThisFrame += Job.nDecodedBytes;
		++rState.nUploadedPageSetCountThisFrame;
		++rState.Stats.nUploadedPageCount;
		rState.Stats.nUploadedBytes += Job.nDecodedBytes;
		t3d_vt_stream_ready_remove( rState, iReady );
		t3d_vt_stream_free_job( rState, nJob );
	}
	if ( bBatched ) {
		if ( !pBatchEnd( *rState.pResidency, pFrameId, pUser ) ) {
			for ( uint32_t i = 0u; i < rState.nReadyJobCount; ++i ) {
				t3dVTStreamJob_t& Job = rState.Buffers.pJobs[rState.Buffers.pReadyJobs[i]];
				if ( Job.nState == T3D_VT_STREAM_STAGED ) {
					Job.nState = T3D_VT_STREAM_READY;
				}
			}
			return rState.nUploadedBytesThisFrame - nUploadedBefore;
		}
		rState.nUploadedBytesThisFrame += nStagedBytes;
		rState.nUploadedPageSetCountThisFrame += nStagedPageSets;
		uint32_t i = 0u;
		while ( i < rState.nReadyJobCount ) {
			const uint32_t nJob = rState.Buffers.pReadyJobs[i];
			t3dVTStreamJob_t& Job = rState.Buffers.pJobs[nJob];
			if ( Job.nState != T3D_VT_STREAM_STAGED ) {
				++i;
				continue;
			}
			++rState.Stats.nUploadedPageCount;
			rState.Stats.nUploadedBytes += Job.nDecodedBytes;
			t3d_vt_stream_ready_remove( rState, i );
			t3d_vt_stream_free_job( rState, nJob );
		}
	}
	return rState.nUploadedBytesThisFrame - nUploadedBefore;
}

uint64_t t3d_vt_streaming_service_uploads_hot( t3dVTStreamingState_t& rState, const uint32_t pFrameId, t3dVTUploadCommitFn_t pUploadCommit,
                                               void *pUser )
{
	return t3d_vt_streaming_service_uploads_batched_hot( rState, pFrameId, NULL, pUploadCommit, NULL, pUser );
}

bool t3d_vt_decode_readback_reference_debug( const uint8_t *pReadbackBytes, const uint32_t pReadbackByteCount,
                                             const uint32_t pRequestCapacity, t3dVTGpuCounters_t& rCounters, uint32_t& rRequestsByteOffset,
                                             uint32_t& rRequestCount )
{
	memset( &rCounters, 0, sizeof( rCounters ) );
	rRequestsByteOffset = 0u;
	rRequestCount = 0u;
	if ( pReadbackBytes == NULL || pReadbackByteCount < sizeof( t3dVTGpuCounters_t ) ) {
		return false;
	}
	memcpy( &rCounters, pReadbackBytes, sizeof( rCounters ) );
	rRequestCount = rCounters.nRequestCount < pRequestCapacity ? rCounters.nRequestCount : pRequestCapacity;
	const uint64_t nRequired = sizeof( t3dVTGpuCounters_t ) + ( uint64_t )rRequestCount * sizeof( t3dVTRuntimeRequest_t );
	if ( nRequired > pReadbackByteCount ) {
		return false;
	}
	rRequestsByteOffset = sizeof( t3dVTGpuCounters_t );
	return true;
}
