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

static sirGraphicsResult_t sirFrameGraphConvertTransition( const sirFrameGraphTransition_t& transition,
														   const sirFrameGraphResource_t& resource, const sirGraphicsHandle_t& hNative,
														   sirGraphicsBarrier_t *pBarrier ) noexcept
{
	if ( pBarrier == nullptr || hNative.nGeneration == 0 || hNative.nDeviceGeneration == 0 || hNative.eType != resource.eType ||
		 transition.nMipCount == 0 || transition.nLayerCount == 0 ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	sirGraphicsBarrier_t barrier = {};
	// A UAV barrier is valid only for an in-queue shader-write hazard.
	// The shared ABI has no general same-state/ownership-only memory barrier.
	if ( transition.eBefore == transition.eAfter &&
		 ( transition.eAfter != SIR_GRAPHICS_STATE_SHADER_WRITE || transition.eFromQueue != transition.eToQueue ) ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	barrier.eType = transition.eBefore == transition.eAfter ? SIR_GRAPHICS_BARRIER_UAV : SIR_GRAPHICS_BARRIER_TRANSITION;
	barrier.hResource = hNative;
	barrier.eBefore = transition.eBefore;
	barrier.eAfter = transition.eAfter;
	if ( resource.eType == SIR_GRAPHICS_HANDLE_TEXTURE ) {
		barrier.nBaseMip = transition.nBaseMip;
		barrier.nMipCount = transition.nMipCount;
		barrier.nBaseLayer = transition.nBaseLayer;
		barrier.nLayerCount = transition.nLayerCount;
		if ( !sirGraphicsTextureBarrierRangeValid( barrier, resource.nMipLevels, resource.nArrayLayers ) ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	} else if ( transition.nBaseMip != 0 || transition.nBaseLayer != 0 || transition.nMipCount != 1 || transition.nLayerCount != 1 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pBarrier = barrier;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRFrameGraph::submitNative( const sirFrameGraphCompiled_t& compiled, const sirFrameGraphNativeInput_t& input,
												 sirFrameGraphNativeOutput_t *pOutput ) const noexcept
{
	if ( pOutput == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pOutput = {};
	const sirGraphicsABIDevice_t *pDevice = input.pDevice;
	if ( pDevice == nullptr || pDevice->nSize < sizeof( sirGraphicsABIDevice_t ) || pDevice->pDevice == nullptr ||
		 pDevice->pQueryCapabilities == nullptr || pDevice->pAcquireCommands == nullptr || pDevice->pSubmit == nullptr ||
		 input.pRecord == nullptr || input.nResourceCount != m_nResourceCount ||
		 ( input.nResourceCount != 0 && input.pResources == nullptr ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirGraphicsABICapabilities_t caps = {};
	caps.nSize = sizeof( caps );
	const sirGraphicsResult_t eCapabilities = ( sirGraphicsResult_t )pDevice->pQueryCapabilities( pDevice->pDevice, &caps );
	if ( eCapabilities != SIR_GRAPHICS_SUCCESS ) {
		return eCapabilities;
	}
	if ( caps.nSize < sizeof( caps ) || caps.nDeviceGeneration == 0 ||
		 ( caps.nEnabledQueues & ( 1U << SIR_GRAPHICS_QUEUE_DIRECT ) ) == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}

	sirFrameGraphSubmissionPlan_t plan = {};
	const sirGraphicsResult_t ePlan = buildSubmissionPlan( compiled, &plan );
	if ( ePlan != SIR_GRAPHICS_SUCCESS ) {
		return ePlan;
	}
	for ( uint32_t iPass = 0; iPass < plan.nPassCount; ++iPass ) {
		if ( ( caps.nEnabledQueues & ( 1U << plan.szPasses[iPass].eQueue ) ) == 0 ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
	}
	for ( uint32_t iResource = 0; iResource < m_nResourceCount; ++iResource ) {
		// Culled or never-used transients do not require physical allocation.
		if ( compiled.szFirstUse[iResource] == SIR_FRAMEGRAPH_INVALID_INDEX ) {
			continue;
		}
		const sirGraphicsHandle_t& hNative = input.pResources[iResource];
		if ( hNative.eType != m_szResources[iResource].eType || hNative.nGeneration == 0 ||
			 hNative.nDeviceGeneration != caps.nDeviceGeneration ) {
			return SIR_GRAPHICS_INVALID_HANDLE;
		}
		if ( m_szResources[iResource].bImported && ( hNative.nSlot != m_szResources[iResource].hImported.nSlot ||
													 hNative.nGeneration != m_szResources[iResource].hImported.nGeneration ||
													 hNative.nDeviceGeneration != m_szResources[iResource].hImported.nDeviceGeneration ) ) {
			return SIR_GRAPHICS_INVALID_HANDLE;
		}
	}
	// Validate every native barrier before acquiring or submitting commands.
	for ( uint32_t iTransition = 0; iTransition < compiled.nTransitionCount; ++iTransition ) {
		const sirFrameGraphTransition_t& transition = compiled.szTransitions[iTransition];
		sirGraphicsBarrier_t barrier = {};
		const sirGraphicsResult_t eConvert = sirFrameGraphConvertTransition( transition, m_szResources[transition.nResource],
																			 input.pResources[transition.nResource], &barrier );
		if ( eConvert != SIR_GRAPHICS_SUCCESS ) {
			return eConvert;
		}
	}

	pOutput->completion.nDeviceGeneration = caps.nDeviceGeneration;
	sirGraphicsCompletion_t szCompletions[SIR_FRAMEGRAPH_MAX_PASSES] = {};
	for ( uint32_t iOrder = 0; iOrder < plan.nPassCount; ++iOrder ) {
		const sirFrameGraphSubmissionPass_t& pass = plan.szPasses[iOrder];
		sirGraphicsABICommandList_t commands = {};
		commands.nSize = sizeof( commands );
		const sirGraphicsResult_t eAcquire =
			( sirGraphicsResult_t )pDevice->pAcquireCommands( pDevice->pDevice, ( uint32_t )pass.eQueue, &commands );
		if ( eAcquire != SIR_GRAPHICS_SUCCESS ) {
			return eAcquire;
		}
		if ( commands.nSize < sizeof( commands ) || commands.nDeviceGeneration != caps.nDeviceGeneration ||
			 commands.nQueue != ( uint32_t )pass.eQueue || commands.pCommands == nullptr || commands.pReset == nullptr ||
			 commands.pBarrier == nullptr || commands.pClose == nullptr ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		const sirGraphicsResult_t eReset = ( sirGraphicsResult_t )commands.pReset( commands.pCommands );
		if ( eReset != SIR_GRAPHICS_SUCCESS ) {
			return eReset;
		}
		sirGraphicsBarrier_t szBarriers[SIR_FRAMEGRAPH_MAX_TRANSITIONS] = {};
		for ( uint32_t iBarrier = 0; iBarrier < pass.nBarrierCount; ++iBarrier ) {
			const uint32_t iTransition = plan.szBarrierIndices[pass.nBarrierBegin + iBarrier];
			const sirFrameGraphTransition_t& transition = compiled.szTransitions[iTransition];
			const sirGraphicsResult_t eConvert = sirFrameGraphConvertTransition(
				transition, m_szResources[transition.nResource], input.pResources[transition.nResource], &szBarriers[iBarrier] );
			if ( eConvert != SIR_GRAPHICS_SUCCESS ) {
				return eConvert;
			}
		}
		for ( uint32_t iBatch = 0; iBatch < pass.nBarrierCount; iBatch += 32 ) {
			const uint32_t nBatch = pass.nBarrierCount - iBatch < 32 ? pass.nBarrierCount - iBatch : 32;
			const sirGraphicsResult_t eBarriers =
				( sirGraphicsResult_t )commands.pBarrier( commands.pCommands, szBarriers + iBatch, nBatch );
			if ( eBarriers != SIR_GRAPHICS_SUCCESS ) {
				return eBarriers;
			}
		}
		const sirGraphicsResult_t eRecord = input.pRecord( pass.nPass, &commands, input.pContext );
		if ( eRecord != SIR_GRAPHICS_SUCCESS ) {
			return eRecord;
		}
		const sirGraphicsResult_t eClose = ( sirGraphicsResult_t )commands.pClose( commands.pCommands );
		if ( eClose != SIR_GRAPHICS_SUCCESS ) {
			return eClose;
		}
		sirGraphicsCompletion_t szWaits[SIR_FRAMEGRAPH_MAX_DEPENDENCIES] = {};
		uint32_t nWaitCount = 0;
		for ( uint32_t iWait = 0; iWait < pass.nWaitCount; ++iWait ) {
			const sirFrameGraphSubmissionWait_t& wait = plan.szWaits[pass.nWaitBegin + iWait];
			uint32_t iProducer = 0;
			while ( iProducer < iOrder && plan.szPasses[iProducer].nPass != wait.nProducerPass ) {
				++iProducer;
			}
			if ( iProducer >= iOrder || szCompletions[iProducer].nValue == 0 || szCompletions[iProducer].eQueue != wait.eProducerQueue ) {
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			}
			szWaits[nWaitCount++] = szCompletions[iProducer];
		}
		const void *pNative = commands.pCommands;
		sirGraphicsABISubmission_t batch = {};
		batch.nSize = sizeof( batch );
		batch.nCommandListCount = 1;
		batch.ppCommandLists = &pNative;
		batch.pWaits = nWaitCount != 0 ? szWaits : nullptr;
		batch.nWaitCount = nWaitCount;
		sirGraphicsCompletion_t completion = {};
		const sirGraphicsResult_t eSubmit =
			( sirGraphicsResult_t )pDevice->pSubmit( pDevice->pDevice, ( uint32_t )pass.eQueue, &batch, &completion );
		if ( eSubmit != SIR_GRAPHICS_SUCCESS ) {
			// A driver can execute commands before its fence signal fails.
			// Never permit callers to interpret an absent token as retirement.
			pOutput->bUnknownCompletion = true;
			return eSubmit;
		}
		if ( completion.nDeviceGeneration != caps.nDeviceGeneration || completion.eQueue != pass.eQueue || completion.nValue == 0 ||
			 completion.nValue <= pOutput->completion.szQueueValues[pass.eQueue] ) {
			pOutput->bUnknownCompletion = true;
			return SIR_GRAPHICS_DRIVER_ERROR;
		}
		szCompletions[iOrder] = completion;
		pOutput->completion.nQueueMask |= ( 1U << pass.eQueue );
		pOutput->completion.szQueueValues[pass.eQueue] = completion.nValue;
		++pOutput->nSubmittedPassCount;
	}
	return SIR_GRAPHICS_SUCCESS;
}
