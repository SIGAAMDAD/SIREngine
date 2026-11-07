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

#include <Renderer/Graph/SIRFrameGraphNative.h>

typedef struct sirFrameGraphDeviceBridge_s
{
	SIRRenderingDevice *pDevice;
	sirFrameGraphDeviceRecordFn_t pRecord;
	void *pContext;
} sirFrameGraphDeviceBridge_t;

static uint32_t sirFrameGraphDeviceQuery( void *pContext, sirGraphicsABICapabilities_t *pCaps ) noexcept
{
	sirFrameGraphDeviceBridge_t *pBridge = ( sirFrameGraphDeviceBridge_t * )pContext;
	if ( pBridge == nullptr || pCaps == nullptr || pCaps->nSize < sizeof( *pCaps ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirGraphicsCapabilities_t caps = {};
	const sirGraphicsResult_t eResult = pBridge->pDevice->queryCapabilities( &caps );
	if ( eResult != SIR_GRAPHICS_SUCCESS ) {
		return eResult;
	}
	*pCaps = {};
	pCaps->nSize = sizeof( *pCaps );
	pCaps->nDeviceGeneration = caps.nDeviceGeneration;
	pCaps->nEnabledQueues = caps.nQueueMask;
	pCaps->nEnabledFeatures = caps.nFeatureMask;
	pCaps->nBackend = caps.eBackend;
	return SIR_GRAPHICS_SUCCESS;
}

static uint32_t sirFrameGraphDeviceReset( void *pCommands ) noexcept
{
	return pCommands != nullptr ? ( ( SIRGraphicsCommandList * )pCommands )->reset() : SIR_GRAPHICS_INVALID_ARGUMENT;
}

static uint32_t sirFrameGraphDeviceBarrier( void *pCommands, const sirGraphicsBarrier_t *pBarriers, uint32_t nCount ) noexcept
{
	return pCommands != nullptr ? ( ( SIRGraphicsCommandList * )pCommands )->barrier( pBarriers, nCount ) : SIR_GRAPHICS_INVALID_ARGUMENT;
}

static uint32_t sirFrameGraphDeviceClose( void *pCommands ) noexcept
{
	return pCommands != nullptr ? ( ( SIRGraphicsCommandList * )pCommands )->close() : SIR_GRAPHICS_INVALID_ARGUMENT;
}

static uint32_t sirFrameGraphDeviceAcquire( void *pContext, uint32_t nQueue, sirGraphicsABICommandList_t *pCommands ) noexcept
{
	sirFrameGraphDeviceBridge_t *pBridge = ( sirFrameGraphDeviceBridge_t * )pContext;
	if ( pBridge == nullptr || pCommands == nullptr || pCommands->nSize < sizeof( *pCommands ) || nQueue >= SIR_GRAPHICS_QUEUE_COUNT ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	SIRGraphicsCommandList *pNative = nullptr;
	const sirGraphicsResult_t eAcquire = pBridge->pDevice->acquireCommands( ( sirGraphicsQueue_t )nQueue, &pNative );
	if ( eAcquire != SIR_GRAPHICS_SUCCESS ) {
		return eAcquire;
	}
	if ( pNative == nullptr ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	*pCommands = {};
	pCommands->nSize = sizeof( *pCommands );
	pCommands->nQueue = ( uint32_t )pNative->getQueue();
	pCommands->nDeviceGeneration = pBridge->pDevice->getDeviceGeneration();
	pCommands->pCommands = pNative;
	pCommands->pReset = sirFrameGraphDeviceReset;
	pCommands->pBarrier = sirFrameGraphDeviceBarrier;
	pCommands->pClose = sirFrameGraphDeviceClose;
	return SIR_GRAPHICS_SUCCESS;
}

static uint32_t sirFrameGraphDeviceSubmit( void *pContext, uint32_t nQueue, const sirGraphicsABISubmission_t *pBatch,
										   sirGraphicsCompletion_t *pCompletion ) noexcept
{
	sirFrameGraphDeviceBridge_t *pBridge = ( sirFrameGraphDeviceBridge_t * )pContext;
	if ( pBridge == nullptr || pBatch == nullptr || pCompletion == nullptr || pBatch->nSize < sizeof( *pBatch ) ||
		 pBatch->nCommandListCount > SIR_FRAMEGRAPH_MAX_PASSES || ( pBatch->nCommandListCount != 0 && pBatch->ppCommandLists == nullptr ) ||
		 ( pBatch->nWaitCount != 0 && pBatch->pWaits == nullptr ) || nQueue >= SIR_GRAPHICS_QUEUE_COUNT ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	SIRGraphicsCommandList *szCommands[SIR_FRAMEGRAPH_MAX_PASSES] = {};
	for ( uint32_t iCommand = 0; iCommand < pBatch->nCommandListCount; ++iCommand ) {
		if ( pBatch->ppCommandLists[iCommand] == nullptr ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		szCommands[iCommand] = ( SIRGraphicsCommandList * )pBatch->ppCommandLists[iCommand];
	}
	sirGraphicsSubmission_t submission = {};
	submission.ppCommandLists = szCommands;
	submission.nCommandListCount = pBatch->nCommandListCount;
	submission.pWaits = pBatch->pWaits;
	submission.nWaitCount = pBatch->nWaitCount;
	submission.pAcquisitions = pBatch->pAcquisitions;
	submission.nAcquisitionCount = pBatch->nAcquisitionCount;
	return pBridge->pDevice->submit( ( sirGraphicsQueue_t )nQueue, submission, pCompletion );
}

static sirGraphicsResult_t sirFrameGraphDeviceRecord( uint32_t nPass, sirGraphicsABICommandList_t *pCommands, void *pContext ) noexcept
{
	sirFrameGraphDeviceBridge_t *pBridge = ( sirFrameGraphDeviceBridge_t * )pContext;
	if ( pBridge == nullptr || pCommands == nullptr || pCommands->pCommands == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	return pBridge->pRecord( nPass, ( SIRGraphicsCommandList * )pCommands->pCommands, pBridge->pContext );
}

sirGraphicsResult_t SIRFrameGraph::submitDevice( const sirFrameGraphCompiled_t& compiled, const sirFrameGraphDeviceInput_t& input,
												 sirFrameGraphNativeOutput_t *pOutput ) const noexcept
{
	if ( input.pDevice == nullptr || input.pRecord == nullptr ) {
		if ( pOutput != nullptr ) {
			*pOutput = {};
		}
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirFrameGraphDeviceBridge_t bridge = { input.pDevice, input.pRecord, input.pContext };
	sirGraphicsABIDevice_t device = {};
	device.nSize = sizeof( device );
	device.pDevice = &bridge;
	device.pQueryCapabilities = sirFrameGraphDeviceQuery;
	device.pAcquireCommands = sirFrameGraphDeviceAcquire;
	device.pSubmit = sirFrameGraphDeviceSubmit;
	const sirFrameGraphNativeInput_t native = { &device, input.pResources, input.nResourceCount, sirFrameGraphDeviceRecord, &bridge };
	return submitNative( compiled, native, pOutput );
}
