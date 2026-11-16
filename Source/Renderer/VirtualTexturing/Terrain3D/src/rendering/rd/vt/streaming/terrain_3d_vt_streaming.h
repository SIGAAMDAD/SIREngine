// Copyright © 2023-2026 Cory Petkovsek, Roope Palmroos, and Contributors.

#ifndef TERRAIN3D_VT_STREAMING_H
#define TERRAIN3D_VT_STREAMING_H

#include "Terrain3DTypes.h"
#include "Terrain3DConstants.h"

#include "rendering/rd/vt/streaming/terrain_3d_vt_residency.h"
#include "core/utility/terrain_3d_sync.h"

#include <stdint.h>

// WG-E owns the raw 16-bit request score. WG-D freezes only the CPU scheduler
// mapping: higher raw score is more urgent, while lower bucket index is better.

typedef struct t3dVTStreamingBuffers_s
{
	t3dVTStreamJob_t *pJobs;
	uint32_t nJobCapacity;
	uint8_t *pScratchBytes;
	uint32_t nScratchLaneCount;
	uint32_t nScratchLaneBytes;
	uint32_t *pScratchLaneOwner;
	Terrain3DAtomic<uint32_t> *pScratchLaneCompletion;
	uint16_t *pPageToJob;
	uint32_t nPageToJobCapacity;
	uint16_t *pFreeJobs;
	// P10 names are retained for ABI/source stability. Under P23 these arrays
	// are intrusive next/previous links for the six fixed scheduler buckets.
	uint16_t *pQueuedJobs;
	uint16_t *pJobQueuePosition;
	uint16_t *pReadyJobs;
	uint16_t *pMergedWantedMip;
	uint16_t *pMergedPriority;
} t3dVTStreamingBuffers_t;

typedef struct t3dVTStreamingState_s
{
	t3dVTResidencyState_t *pResidency;
	t3dVTStreamingBuffers_t Buffers;
	t3dVTStreamingLimits_t Limits;
	t3dVTStreamingStats_t Stats;
	uint16_t nPriorityBucketHead[T3D_VT_PRIORITY_BUCKET_COUNT];
	uint16_t nPriorityBucketTail[T3D_VT_PRIORITY_BUCKET_COUNT];
	uint32_t nPriorityBucketCount[T3D_VT_PRIORITY_BUCKET_COUNT];
	uint32_t nFreeJobCount;
	uint32_t nQueuedJobCount;
	uint32_t nReadyJobCount;
	uint32_t nInFlightIoJobCount;
	uint32_t nAdmissionFrame;
	uint32_t nAdmittedRequestCountThisFrame;
	uint32_t nUploadFrame;
	uint32_t nUploadedPageSetCountThisFrame;
	uint64_t nUploadedBytesThisFrame;
	bool bInitialized;
	bool bQuiescing;
} t3dVTStreamingState_t;

typedef bool ( *t3dVTStreamReadDecodeFn_t )( const t3dVTDatabaseView_t& pDatabase, const t3dVTStreamJob_t& pJob, uint8_t *pScratch,
                                             uint32_t pScratchCapacity, uint32_t& rDecodedBytes, void *pUser );

typedef bool ( *t3dVTUploadCommitFn_t )( t3dVTResidencyState_t& rResidency, const t3dVTUploadReservation_t& pReservation,
                                         const uint8_t *pDecodedBytes, uint32_t pDecodedByteCount, uint32_t pFrameId, void *pUser );

typedef bool ( *t3dVTUploadBatchBeginFn_t )( void *pUser );
typedef bool ( *t3dVTUploadBatchEndFn_t )( t3dVTResidencyState_t& rResidency, uint32_t pFrameId, void *pUser );

bool t3d_vt_streaming_initialize_cold( t3dVTResidencyState_t& rResidency, const t3dVTStreamingBuffers_t& pBuffers,
                                       const t3dVTStreamingLimits_t& pLimits, t3dVTStreamingState_t& rState );
// P16 compatibility adapter. New runtime owners should provide all budgets explicitly.
bool t3d_vt_streaming_initialize_cold( t3dVTResidencyState_t& rResidency, const t3dVTStreamingBuffers_t& pBuffers,
                                       uint64_t pUploadBudgetBytesPerFrame, t3dVTStreamingState_t& rState );
uint16_t t3d_vt_scheduler_priority_bucket_reference_debug( uint16_t pPriority );
t3dVTAdmissionStatus_t t3d_vt_streaming_admit_request_hot( t3dVTStreamingState_t& rState, const t3dVTRuntimeRequest_t& pRequest,
                                                           uint32_t pFrameId );
uint32_t t3d_vt_streaming_ingest_requests_hot( t3dVTStreamingState_t& rState, const t3dVTRuntimeRequest_t *pRequests,
                                               uint32_t pRequestCount, uint32_t pFrameId );
t3dVTReadbackConsumeStatus_t t3d_vt_streaming_ingest_readback_owner_hot( t3dVTStreamingState_t& rState,
                                                                         const t3dVTReadbackSlotHeader_t& pHeader,
                                                                         const uint8_t *pReadbackBytes, uint32_t pReadbackByteCount,
                                                                         t3dVTGpuCounters_t& rCounters, uint32_t& rAcceptedRequestCount );
// P20 compatibility adapter. P23 owners should use the explicit slot header overload.
uint32_t t3d_vt_streaming_ingest_readback_owner_hot( t3dVTStreamingState_t& rState, const uint8_t *pReadbackBytes,
                                                     uint32_t pReadbackByteCount, uint32_t pRequestCapacity, uint32_t pFrameId,
                                                     t3dVTGpuCounters_t& rCounters );
uint32_t t3d_vt_streaming_invalidate_queued_demand_hot( t3dVTStreamingState_t& rState );
bool t3d_vt_streaming_claim_job_owner_hot( t3dVTStreamingState_t& rState, uint32_t& rJobIndex );
// Owner thread stops admission/claims, then polls until all worker completions
// have been acquired. Never cancel a worker that may still touch package bytes.
bool t3d_vt_streaming_quiesce_package_warm( t3dVTStreamingState_t& rState );
void t3d_vt_streaming_resume_package_warm( t3dVTStreamingState_t& rState );
bool t3d_vt_streaming_cancel_claim_owner_hot( t3dVTStreamingState_t& rState, uint32_t pJobIndex );
bool t3d_vt_streaming_execute_job_async( t3dVTStreamingState_t& rState, uint32_t pJobIndex, t3dVTStreamReadDecodeFn_t pReadDecode,
                                         void *pUser );
uint64_t t3d_vt_streaming_service_uploads_hot( t3dVTStreamingState_t& rState, uint32_t pFrameId, t3dVTUploadCommitFn_t pUploadCommit,
                                               void *pUser );
uint64_t t3d_vt_streaming_service_uploads_batched_hot( t3dVTStreamingState_t& rState, uint32_t pFrameId,
                                                       t3dVTUploadBatchBeginFn_t pBatchBegin, t3dVTUploadCommitFn_t pUploadStage,
                                                       t3dVTUploadBatchEndFn_t pBatchEnd, void *pUser );
bool t3d_vt_decode_readback_reference_debug( const uint8_t *pReadbackBytes, uint32_t pReadbackByteCount, uint32_t pRequestCapacity,
                                             t3dVTGpuCounters_t& rCounters, uint32_t& rRequestsByteOffset, uint32_t& rRequestCount );

#endif // TERRAIN3D_VT_STREAMING_H
