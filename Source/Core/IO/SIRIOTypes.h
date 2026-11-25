#ifndef __SIR_IOTYPES_H__
#define __SIR_IOTYPES_H__

#include <Core/SIRPCH.h>

typedef struct sirIOHandle_s { uint32_t nSlot; uint32_t nGeneration; } sirIOHandle_t;
typedef struct sirIOBufferHandle_s { uint32_t nSlot; uint32_t nGeneration; } sirIOBufferHandle_t;
typedef enum sirIOResult_e : uint8_t
{
	SIR_IO_OK = 0, SIR_IO_INVALID_ARGUMENT, SIR_IO_OUT_OF_CAPACITY, SIR_IO_NOT_READY, SIR_IO_STALE_HANDLE,
	SIR_IO_CANCELLED, SIR_IO_TRUNCATED, SIR_IO_FAILED, SIR_IO_BUSY
} sirIOResult_t;
typedef struct sirIOBytes_s { uint8_t *pData; uint64_t nSize; } sirIOBytes_t;
typedef struct sirIOBufferLease_s { sirIOBufferHandle_t handle; sirIOBytes_t bytes; } sirIOBufferLease_t;
typedef sirIOResult_t ( *sirIOReadAtCallback_t )( void *pSource, uint64_t nOffset, sirIOBytes_t destination, uint64_t *pTransferred ) noexcept;
typedef struct sirIORequest_s
{
	void *pSource;
	sirIOReadAtCallback_t pReadAt;
	sirIOBytes_t destination;
	uint64_t nOffset;
	uint64_t nBytes;
	uint64_t nDeadlineTick;
	uint8_t nPriority;
} sirIORequest_t;
typedef struct sirIOCompletion_s
{
	sirIOHandle_t handle;
	sirIOResult_t eResult;
	uint64_t nTransferred;
} sirIOCompletion_t;
typedef struct sirIOBatch_s { const sirIORequest_t *pRequests; uint32_t nCount; } sirIOBatch_t;
typedef struct sirIOBufferPoolConfig_s
{
	void *pStorage;
	uint64_t nStorageBytes;
	sirIOBufferLease_t *pLeaseStorage;
	uint32_t nLeaseCapacity;
	uint32_t nMaxAlignment;
} sirIOBufferPoolConfig_t;
typedef struct sirIOPlanInput_s
{
	const void *pSource;
	uint64_t nWorkId;
	uint64_t nOffset;
	uint64_t nBytes;
	uint64_t nDeadlineTick;
	uint8_t nPriority;
} sirIOPlanInput_t;
typedef struct sirIOPlannerConfig_s
{
	sirIOPlanInput_t *pPending;
	uint32_t nPendingCapacity;
	uint32_t nMaxMergeGap;
	uint64_t nMaxMergedBytes;
} sirIOPlannerConfig_t;
typedef struct sirIOBatchBudget_s { uint64_t nBytes; uint32_t nRequests; uint64_t nTick; } sirIOBatchBudget_t;
typedef struct sirIOPhysicalRange_s
{
	const void *pSource;
	uint64_t nOffset;
	uint64_t nBytes;
	uint64_t nWorkId;
} sirIOPhysicalRange_t;
typedef struct sirIOPlannedBatch_s
{
	sirIOPhysicalRange_t *pRanges;
	uint32_t nCapacity;
	uint32_t nCount;
	sirIOHandle_t handle;
} sirIOPlannedBatch_t;

#endif // __SIR_IOTYPES_H__
