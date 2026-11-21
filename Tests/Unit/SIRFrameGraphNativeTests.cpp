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
#include <stdio.h>

typedef struct sirFrameGraphNativeMock_s
{
	uint32_t nGeneration;
	uint32_t nQueueMask;
	uint32_t nSubmitted;
	uint32_t nBarrierCount;
	uint32_t nTransitionCount;
	uint32_t nUavCount;
	uint32_t nRecordCount;
	uint32_t nWaitCount;
	uint32_t nFailSubmit;
	uint32_t nFailRecord;
	uint32_t nBadCompletion;
	uint64_t szNextCompletion[SIR_GRAPHICS_QUEUE_COUNT];
} sirFrameGraphNativeMock_t;

static uint32_t mockQuery( void *pDevice, sirGraphicsABICapabilities_t *pCaps ) noexcept
{
	sirFrameGraphNativeMock_t *pMock = ( sirFrameGraphNativeMock_t * )pDevice;
	if ( pCaps == nullptr || pCaps->nSize < sizeof( *pCaps ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pCaps = {};
	pCaps->nSize = sizeof( *pCaps );
	pCaps->nDeviceGeneration = pMock->nGeneration;
	pCaps->nEnabledQueues = pMock->nQueueMask;
	return SIR_GRAPHICS_SUCCESS;
}

static uint32_t mockReset( void *pCommands ) noexcept
{
	return pCommands != nullptr ? SIR_GRAPHICS_SUCCESS : SIR_GRAPHICS_INVALID_ARGUMENT;
}

static uint32_t mockBarrier( void *pCommands, const sirGraphicsBarrier_t *pBarriers, uint32_t nCount ) noexcept
{
	sirFrameGraphNativeMock_t *pMock = ( sirFrameGraphNativeMock_t * )pCommands;
	if ( pMock == nullptr || pBarriers == nullptr || nCount == 0 || nCount > 32 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t iBarrier = 0; iBarrier < nCount; ++iBarrier ) {
		const sirGraphicsBarrier_t& barrier = pBarriers[iBarrier];
		if ( barrier.hResource.nDeviceGeneration != pMock->nGeneration || barrier.hResource.nGeneration == 0 ||
		     ( barrier.hResource.eType == SIR_GRAPHICS_HANDLE_BUFFER && ( barrier.nMipCount != 0 || barrier.nLayerCount != 0 ) ) ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( barrier.eType == SIR_GRAPHICS_BARRIER_TRANSITION ) {
			++pMock->nTransitionCount;
		} else if ( barrier.eType == SIR_GRAPHICS_BARRIER_UAV ) {
			++pMock->nUavCount;
		} else {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
		++pMock->nBarrierCount;
	}
	return SIR_GRAPHICS_SUCCESS;
}

static uint32_t mockClose( void *pCommands ) noexcept
{
	return pCommands != nullptr ? SIR_GRAPHICS_SUCCESS : SIR_GRAPHICS_INVALID_ARGUMENT;
}

static uint32_t mockAcquire( void *pDevice, uint32_t nQueue, sirGraphicsABICommandList_t *pCommands ) noexcept
{
	sirFrameGraphNativeMock_t *pMock = ( sirFrameGraphNativeMock_t * )pDevice;
	if ( pCommands == nullptr || pCommands->nSize < sizeof( *pCommands ) || nQueue >= SIR_GRAPHICS_QUEUE_COUNT ||
	     ( pMock->nQueueMask & ( 1U << nQueue ) ) == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pCommands = {};
	pCommands->nSize = sizeof( *pCommands );
	pCommands->nDeviceGeneration = pMock->nGeneration;
	pCommands->nQueue = nQueue;
	pCommands->pCommands = pDevice;
	pCommands->pReset = mockReset;
	pCommands->pBarrier = mockBarrier;
	pCommands->pClose = mockClose;
	return SIR_GRAPHICS_SUCCESS;
}

static uint32_t mockSubmit( void *pDevice, uint32_t nQueue, const sirGraphicsABISubmission_t *pBatch,
                            sirGraphicsCompletion_t *pCompletion ) noexcept
{
	sirFrameGraphNativeMock_t *pMock = ( sirFrameGraphNativeMock_t * )pDevice;
	if ( pBatch == nullptr || pCompletion == nullptr || pBatch->nSize != sizeof( *pBatch ) || pBatch->nCommandListCount != 1 ||
	     pBatch->ppCommandLists == nullptr || pBatch->ppCommandLists[0] != pDevice || nQueue >= SIR_GRAPHICS_QUEUE_COUNT ||
	     ( pMock->nQueueMask & ( 1U << nQueue ) ) == 0 || ( pBatch->nWaitCount != 0 && pBatch->pWaits == nullptr ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t iWait = 0; iWait < pBatch->nWaitCount; ++iWait ) {
		const sirGraphicsCompletion_t& wait = pBatch->pWaits[iWait];
		if ( wait.nDeviceGeneration != pMock->nGeneration || wait.eQueue >= SIR_GRAPHICS_QUEUE_COUNT || wait.eQueue == nQueue ||
		     wait.nValue == 0 || wait.nValue != pMock->szNextCompletion[wait.eQueue] ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		++pMock->nWaitCount;
	}
	if ( pMock->nFailSubmit != 0 && pMock->nSubmitted + 1 == pMock->nFailSubmit ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	++pMock->nSubmitted;
	*pCompletion = {};
	pCompletion->nDeviceGeneration = pMock->nGeneration;
	pCompletion->eQueue = ( sirGraphicsQueue_t )nQueue;
	pCompletion->nValue = ++pMock->szNextCompletion[nQueue];
	if ( pMock->nBadCompletion != 0 && pMock->nSubmitted == pMock->nBadCompletion ) {
		pCompletion->nDeviceGeneration++;
	}
	return SIR_GRAPHICS_SUCCESS;
}

static sirGraphicsResult_t mockRecord( uint32_t nPass, sirGraphicsABICommandList_t *pCommands, void *pContext ) noexcept
{
	sirFrameGraphNativeMock_t *pMock = ( sirFrameGraphNativeMock_t * )pContext;
	if ( pCommands == nullptr || pCommands->pCommands != pContext || nPass != pMock->nRecordCount ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	++pMock->nRecordCount;
	return pMock->nFailRecord != 0 && pMock->nRecordCount == pMock->nFailRecord ? SIR_GRAPHICS_UNSUPPORTED : SIR_GRAPHICS_SUCCESS;
}

static sirGraphicsABIDevice_t makeDevice( sirFrameGraphNativeMock_t *pMock ) noexcept
{
	sirGraphicsABIDevice_t device = {};
	device.nSize = sizeof( device );
	device.pDevice = pMock;
	device.pQueryCapabilities = mockQuery;
	device.pAcquireCommands = mockAcquire;
	device.pSubmit = mockSubmit;
	return device;
}

static bool makeGraph( SIRFrameGraph *pGraph, sirFrameGraphCompiled_t *pCompiled, bool bCrossQueue ) noexcept
{
	sirFrameGraphResource_t resource = {};
	resource.eType = SIR_GRAPHICS_HANDLE_BUFFER;
	resource.eInitialState = SIR_GRAPHICS_STATE_UNDEFINED;
	resource.eInitialQueue = SIR_GRAPHICS_QUEUE_DIRECT;
	resource.nMipLevels = 1;
	resource.nArrayLayers = 1;
	uint32_t nResource = 0;
	uint32_t nPass = 0;
	if ( pGraph->addResource( resource, &nResource ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirFrameGraphPass_t pass = {};
	pass.eQueue = bCrossQueue ? SIR_GRAPHICS_QUEUE_COPY : SIR_GRAPHICS_QUEUE_DIRECT;
	if ( pGraph->addPass( pass, &nPass ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	pass.eQueue = SIR_GRAPHICS_QUEUE_DIRECT;
	if ( pGraph->addPass( pass, &nPass ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirFrameGraphAccess_t access = {};
	access.nResource = nResource;
	access.nMipCount = 1;
	access.nLayerCount = 1;
	access.bWrite = true;
	access.eState = bCrossQueue ? SIR_GRAPHICS_STATE_COPY_DESTINATION : SIR_GRAPHICS_STATE_SHADER_WRITE;
	if ( pGraph->addAccess( access ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	access.nPass = 1;
	access.eState = bCrossQueue ? SIR_GRAPHICS_STATE_SHADER_READ : SIR_GRAPHICS_STATE_SHADER_WRITE;
	access.bWrite = !bCrossQueue;
	if ( pGraph->addAccess( access ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirGraphicsCapabilities_t caps = {};
	caps.nDeviceGeneration = 7;
	caps.nQueueMask = ( 1U << SIR_GRAPHICS_QUEUE_DIRECT ) | ( 1U << SIR_GRAPHICS_QUEUE_COPY );
	caps.nFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_COPY_QUEUE;
	return pGraph->compile( caps, pCompiled ) == SIR_GRAPHICS_SUCCESS;
}

static bool testNativeExecution( bool bCrossQueue, uint32_t nFailRecord, uint32_t nFailSubmit, uint32_t nBadCompletion ) noexcept
{
	SIRFrameGraph graph;
	sirFrameGraphCompiled_t compiled = {};
	if ( !makeGraph( &graph, &compiled, bCrossQueue ) ) {
		return false;
	}
	sirFrameGraphNativeMock_t mock = {};
	mock.nGeneration = 7;
	mock.nQueueMask = ( 1U << SIR_GRAPHICS_QUEUE_DIRECT ) | ( 1U << SIR_GRAPHICS_QUEUE_COPY );
	mock.nFailRecord = nFailRecord;
	mock.nFailSubmit = nFailSubmit;
	mock.nBadCompletion = nBadCompletion;
	sirGraphicsABIDevice_t device = makeDevice( &mock );
	const sirGraphicsHandle_t handles[] = { { 8, 1, 7, SIR_GRAPHICS_HANDLE_BUFFER } };
	sirFrameGraphNativeInput_t input = { &device, handles, 1, mockRecord, &mock };
	sirFrameGraphNativeOutput_t output = {};
	const sirGraphicsResult_t eResult = graph.submitNative( compiled, input, &output );
	if ( nBadCompletion != 0 ) {
		return eResult == SIR_GRAPHICS_DRIVER_ERROR && output.bUnknownCompletion && output.nSubmittedPassCount == 0;
	}
	if ( nFailRecord != 0 ) {
		return eResult == SIR_GRAPHICS_UNSUPPORTED && output.nSubmittedPassCount == 1 &&
		       output.completion.szQueueValues[bCrossQueue ? SIR_GRAPHICS_QUEUE_COPY : SIR_GRAPHICS_QUEUE_DIRECT] == 1;
	}
	if ( nFailSubmit != 0 ) {
		return eResult == SIR_GRAPHICS_DEVICE_LOST && output.nSubmittedPassCount == 1 && mock.nSubmitted == 1 &&
		       output.bUnknownCompletion && output.completion.nQueueMask != 0;
	}
	return eResult == SIR_GRAPHICS_SUCCESS && output.nSubmittedPassCount == 2 && mock.nSubmitted == 2 && mock.nRecordCount == 2 &&
	       mock.nBarrierCount == compiled.nTransitionCount && mock.nWaitCount == ( bCrossQueue ? 1U : 0U ) &&
	       mock.nTransitionCount == ( bCrossQueue ? 2U : 1U ) && mock.nUavCount == ( bCrossQueue ? 0U : 1U ) && !output.bUnknownCompletion;
}

static bool testPreflightRejectsStaleHandle( void ) noexcept
{
	SIRFrameGraph graph;
	sirFrameGraphCompiled_t compiled = {};
	if ( !makeGraph( &graph, &compiled, true ) ) {
		return false;
	}
	sirFrameGraphNativeMock_t mock = {};
	mock.nGeneration = 7;
	mock.nQueueMask = ( 1U << SIR_GRAPHICS_QUEUE_DIRECT ) | ( 1U << SIR_GRAPHICS_QUEUE_COPY );
	sirGraphicsABIDevice_t device = makeDevice( &mock );
	const sirGraphicsHandle_t handles[] = { { 8, 1, 6, SIR_GRAPHICS_HANDLE_BUFFER } };
	sirFrameGraphNativeInput_t input = { &device, handles, 1, mockRecord, &mock };
	sirFrameGraphNativeOutput_t output = {};
	return graph.submitNative( compiled, input, &output ) == SIR_GRAPHICS_INVALID_HANDLE && mock.nSubmitted == 0 &&
	       output.nSubmittedPassCount == 0 && output.completion.nQueueMask == 0;
}

static bool testRejectsUnsupportedSameStateBarrier( void ) noexcept
{
	SIRFrameGraph graph;
	uint32_t nResource = 0;
	uint32_t nPass = 0;
	sirFrameGraphResource_t resource = {};
	resource.eType = SIR_GRAPHICS_HANDLE_BUFFER;
	resource.eInitialState = SIR_GRAPHICS_STATE_UNDEFINED;
	resource.eInitialQueue = SIR_GRAPHICS_QUEUE_COPY;
	resource.nMipLevels = 1;
	resource.nArrayLayers = 1;
	if ( graph.addResource( resource, &nResource ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirFrameGraphPass_t pass = {};
	pass.eQueue = SIR_GRAPHICS_QUEUE_COPY;
	if ( graph.addPass( pass, &nPass ) != SIR_GRAPHICS_SUCCESS || graph.addPass( pass, &nPass ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirFrameGraphAccess_t access = {};
	access.nResource = nResource;
	access.nMipCount = 1;
	access.nLayerCount = 1;
	access.eState = SIR_GRAPHICS_STATE_COPY_DESTINATION;
	access.bWrite = true;
	if ( graph.addAccess( access ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	access.nPass = 1;
	if ( graph.addAccess( access ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirGraphicsCapabilities_t caps = {};
	caps.nDeviceGeneration = 7;
	caps.nQueueMask = ( 1U << SIR_GRAPHICS_QUEUE_COPY ) | ( 1U << SIR_GRAPHICS_QUEUE_DIRECT );
	caps.nFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_COPY_QUEUE;
	sirFrameGraphCompiled_t compiled = {};
	if ( graph.compile( caps, &compiled ) != SIR_GRAPHICS_SUCCESS || compiled.nTransitionCount < 2 ) {
		return false;
	}
	sirFrameGraphNativeMock_t mock = {};
	mock.nGeneration = 7;
	mock.nQueueMask = caps.nQueueMask;
	sirGraphicsABIDevice_t device = makeDevice( &mock );
	const sirGraphicsHandle_t handles[] = { { 8, 1, 7, SIR_GRAPHICS_HANDLE_BUFFER } };
	sirFrameGraphNativeInput_t input = { &device, handles, 1, mockRecord, &mock };
	sirFrameGraphNativeOutput_t output = {};
	return graph.submitNative( compiled, input, &output ) == SIR_GRAPHICS_UNSUPPORTED && mock.nSubmitted == 0 &&
	       output.nSubmittedPassCount == 0 && !output.bUnknownCompletion;
}

static bool testUnusedTransientUnbound( void ) noexcept
{
	SIRFrameGraph graph;
	sirFrameGraphResource_t resource = {};
	resource.eType = SIR_GRAPHICS_HANDLE_BUFFER;
	resource.eInitialState = SIR_GRAPHICS_STATE_UNDEFINED;
	resource.eInitialQueue = SIR_GRAPHICS_QUEUE_DIRECT;
	resource.nMipLevels = 1;
	resource.nArrayLayers = 1;
	uint32_t nResource = 0;
	uint32_t nPass = 0;
	if ( graph.addResource( resource, &nResource ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirFrameGraphPass_t pass = {};
	pass.eQueue = SIR_GRAPHICS_QUEUE_DIRECT;
	pass.bSideEffect = true;
	if ( graph.addPass( pass, &nPass ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirGraphicsCapabilities_t caps = {};
	caps.nDeviceGeneration = 7;
	caps.nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	caps.nFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS;
	sirFrameGraphCompiled_t compiled = {};
	if ( graph.compile( caps, &compiled ) != SIR_GRAPHICS_SUCCESS || compiled.szFirstUse[0] != SIR_FRAMEGRAPH_INVALID_INDEX ) {
		return false;
	}
	sirFrameGraphNativeMock_t mock = {};
	mock.nGeneration = 7;
	mock.nQueueMask = caps.nQueueMask;
	sirGraphicsABIDevice_t device = makeDevice( &mock );
	const sirGraphicsHandle_t handles[] = { {} };
	sirFrameGraphNativeInput_t input = { &device, handles, 1, mockRecord, &mock };
	sirFrameGraphNativeOutput_t output = {};
	return graph.submitNative( compiled, input, &output ) == SIR_GRAPHICS_SUCCESS && output.nSubmittedPassCount == 1 &&
	       output.completion.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT] == 1 && mock.nBarrierCount == 0;
}

int main( void )
{
	if ( !testNativeExecution( true, 0, 0, 0 ) || !testNativeExecution( false, 0, 0, 0 ) || !testNativeExecution( true, 2, 0, 0 ) ||
	     !testNativeExecution( true, 0, 2, 0 ) || !testNativeExecution( true, 0, 0, 1 ) || !testPreflightRejectsStaleHandle() ||
	     !testRejectsUnsupportedSameStateBarrier() || !testUnusedTransientUnbound() ) {
		fputs( "FrameGraph native lowering FAILED\n", stderr );
		return 1;
	}
	puts( "FrameGraph native lowering PASS" );
	return 0;
}
