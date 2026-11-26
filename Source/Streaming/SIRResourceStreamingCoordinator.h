/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIR_RESOURCESTREAMINGCOORDINATOR_H__
#define __SIR_RESOURCESTREAMINGCOORDINATOR_H__

#include <Resources/SIRResourceGraph.h>
#include <Streaming/Pack/SIRPackMountService.h>
#include <Streaming/Pack/SIRCompressionRegistry.h>
#include <Core/IO/SIRAsyncIOService.h>
#include <Core/IO/SIRIOBufferPool.h>
#include <Streaming/SIRStreamingUnitResolver.h>
#include <Streaming/SIRStreamingCoordinator.h>

typedef struct sirStreamingBudget_s
{
	uint64_t nIoBytes;
	uint32_t nTransfers;
	uint32_t nDecodeJobs;
	uint64_t nDecodeBytes;
	uint32_t nPublicationOperations;
	uint32_t nRetirementOperations;
	uint64_t nTick;
	uint64_t nMaximumPrefetchedBytes;
} sirStreamingBudget_t;

typedef struct sirResourceStreamingOperation_s sirResourceStreamingOperation_t;
typedef struct sirResourceStreamingConfig_s
{
	SIRResourceGraph *pGraph;
	SIRTaskScheduler *pScheduler;
	SIRPackMountService *pMount;
	SIRStreamingUnitResolver *pResolver;
	SIRAsyncIOService *pIo;
	SIRIOBufferPool *pBufferPool;
	SIRCompressionRegistry *pCodecs;
	SIRStreamingCoordinator *pCoordinator;
	sirResourceStreamingOperation_t *pOperations;
	uint32_t nOperationCapacity;
	sirStreamingChunkDemand_t *pPlanChunks;
	uint32_t nPlanChunkCapacity;
	sirResourceVerifiedChunk_t *pPublicationChunks;
	uint32_t nPublicationChunkCapacity;
	SIRResourceDependency *pPublicationDependencies;
	uint32_t nPublicationDependencyCapacity;
	uint32_t nResidentDomain;
} sirResourceStreamingConfig_t;

class SIRResourceStreamingCoordinator final
{
public:
	SIRResourceStreamingCoordinator( void ) noexcept;
	~SIRResourceStreamingCoordinator( void ) noexcept;
	SIRResourceStreamingCoordinator( const SIRResourceStreamingCoordinator& ) = delete;
	SIRResourceStreamingCoordinator& operator=( const SIRResourceStreamingCoordinator& ) = delete;

	sirStreamingResult_t initialize( const sirResourceStreamingConfig_t& config ) noexcept;
	sirStreamingResult_t prepare( sirStreamingTicket_t ticket, const sirStreamingDemand_t& demand ) noexcept;
	sirStreamingResult_t activate( sirStreamingTicket_t ticket ) noexcept;
	sirStreamingResult_t retire( sirStreamingTicket_t ticket ) noexcept;
	sirStreamingResult_t cancel( sirStreamingTicket_t ticket ) noexcept;
	sirStreamingResult_t update( const sirStreamingBudget_t& budget ) noexcept;
	sirStreamingResult_t shutdown( void ) noexcept;

private:
	sirResourceStreamingOperation_t *m_pOperations;
	uint32_t m_nCapacity;
	sirResourceStreamingConfig_t *m_pConfig;
	bool m_bInitialized;
	bool m_bStopping;

	sirStreamingResult_t materialize( sirResourceStreamingOperation_t& operation, const sirStreamingBudget_t& budget ) noexcept;
	void fail( sirResourceStreamingOperation_t& operation ) noexcept;
};

#endif // __SIR_RESOURCESTREAMINGCOORDINATOR_H__
