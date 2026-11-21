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

#include <Renderer/SIRGraphicsEraRHI.h>

static sirGraphicsResult_t sirEraCheckHandle( sirGraphicsHandle_t handle, sirGraphicsHandleType_t eType, uint32_t nGeneration ) noexcept
{
	if ( handle.eType != eType || handle.nGeneration == 0 || handle.nDeviceGeneration != nGeneration ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRGraphicsEraRHI::admitPipeline( const sirGraphicsABIPipeline_t& pipeline,
                                                      const sirGraphicsABIPipelineModelCaps_t& caps ) noexcept
{
	if ( pipeline.nModel == SIR_GRAPHICS_PIPELINE_MODEL_MODERN ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	return sirGraphicsValidateABIPipeline( &pipeline, &caps );
}

sirGraphicsResult_t SIRGraphicsEraRHI::admitBatch( const sirGraphicsEraBatch_t& batch,
                                                   const sirGraphicsABIPipelineModelCaps_t& caps ) noexcept
{
	const sirGraphicsResult_t eCaps = sirGraphicsValidateABIPipelineModelCaps( &caps );
	if ( eCaps != SIR_GRAPHICS_SUCCESS ) {
		return eCaps;
	}
	if ( batch.eModel != SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE && batch.eModel != SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( caps.nModel != batch.eModel || caps.nExecutionModel != SIR_GRAPHICS_EXECUTION_STATEFUL_SERIAL ||
	     ( caps.nQueueMask & ( 1U << SIR_GRAPHICS_QUEUE_DIRECT ) ) == 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( batch.nDeviceGeneration == 0 || batch.nDeviceGeneration != caps.nDeviceGeneration ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( batch.pPass == nullptr || batch.pViewport == nullptr || batch.pScissor == nullptr ||
	     ( batch.nDrawCount != 0 && batch.pDraws == nullptr ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( ( batch.pPass->nColorCount == 0 && batch.pPass->hDepthView.eType == SIR_GRAPHICS_HANDLE_NONE ) ||
	     batch.pPass->nColorCount > caps.nMaximumColorAttachments ||
	     ( batch.pPass->nColorCount != 0 && batch.pPass->pColors == nullptr ) ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	for ( uint32_t i = 0; i < batch.pPass->nColorCount; ++i ) {
		if ( sirEraCheckHandle( batch.pPass->pColors[i].hView, SIR_GRAPHICS_HANDLE_VIEW, batch.nDeviceGeneration ) !=
		     SIR_GRAPHICS_SUCCESS ) {
			return SIR_GRAPHICS_INVALID_HANDLE;
		}
	}
	if ( batch.pPass->hDepthView.eType != SIR_GRAPHICS_HANDLE_NONE &&
	     sirEraCheckHandle( batch.pPass->hDepthView, SIR_GRAPHICS_HANDLE_VIEW, batch.nDeviceGeneration ) != SIR_GRAPHICS_SUCCESS ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	for ( uint32_t i = 0; i < batch.nDrawCount; ++i ) {
		const sirGraphicsEraDraw_t& draw = batch.pDraws[i];
		if ( draw.nElementCount == 0 || draw.nStreamCount == 0 || draw.nIndexed > 1 || draw.nIndex32 > 1 ||
		     draw.nFirstElement > UINT32_MAX - draw.nElementCount || ( draw.nStreamCount != 0 && draw.pStreams == nullptr ) ||
		     ( draw.nBindingCount != 0 && draw.pBindings == nullptr ) ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( draw.nStreamCount > caps.nMaximumVertexStreams || draw.nBindingCount > caps.nMaximumBindings ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
		if ( batch.eModel == SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION && draw.nBindingCount != 0 ) {
			// Fixed recipes use their native stage state, not modern descriptor sets.
			return SIR_GRAPHICS_UNSUPPORTED;
		}
		if ( sirEraCheckHandle( draw.hPipeline, SIR_GRAPHICS_HANDLE_PIPELINE, batch.nDeviceGeneration ) != SIR_GRAPHICS_SUCCESS ) {
			return SIR_GRAPHICS_INVALID_HANDLE;
		}
		for ( uint32_t j = 0; j < draw.nBindingCount; ++j ) {
			const sirGraphicsEraBinding_t& binding = draw.pBindings[j];
			if ( binding.nSlot >= caps.nMaximumBindings ) {
				return SIR_GRAPHICS_UNSUPPORTED;
			}
			if ( sirEraCheckHandle( binding.hSet, SIR_GRAPHICS_HANDLE_BINDING_SET, batch.nDeviceGeneration ) != SIR_GRAPHICS_SUCCESS ) {
				return SIR_GRAPHICS_INVALID_HANDLE;
			}
			for ( uint32_t k = 0; k < j; ++k ) {
				if ( draw.pBindings[k].nSlot == binding.nSlot ) {
					return SIR_GRAPHICS_INVALID_ARGUMENT;
				}
			}
		}
		for ( uint32_t j = 0; j < draw.nStreamCount; ++j ) {
			const sirGraphicsEraStream_t& stream = draw.pStreams[j];
			if ( stream.nSlot >= caps.nMaximumVertexStreams || stream.nStrideBytes == 0 ) {
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			}
			if ( sirEraCheckHandle( stream.hBuffer, SIR_GRAPHICS_HANDLE_BUFFER, batch.nDeviceGeneration ) != SIR_GRAPHICS_SUCCESS ) {
				return SIR_GRAPHICS_INVALID_HANDLE;
			}
			for ( uint32_t k = 0; k < j; ++k ) {
				if ( draw.pStreams[k].nSlot == stream.nSlot ) {
					return SIR_GRAPHICS_INVALID_ARGUMENT;
				}
			}
		}
		if ( draw.nIndexed != 0 ) {
			if ( draw.nVertexOffset != 0 ) {
				return SIR_GRAPHICS_UNSUPPORTED;
			}
			if ( draw.nIndexOffsetBytes % ( draw.nIndex32 ? 4U : 2U ) != 0 ) {
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			}
			if ( sirEraCheckHandle( draw.hIndexBuffer, SIR_GRAPHICS_HANDLE_BUFFER, batch.nDeviceGeneration ) != SIR_GRAPHICS_SUCCESS ) {
				return SIR_GRAPHICS_INVALID_HANDLE;
			}
		} else if ( draw.nVertexOffset != 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRGraphicsEraRHI::record( const sirGraphicsEraBatch_t& batch, SIRGraphicsCommandList& commands,
                                               const sirGraphicsABIPipelineModelCaps_t& caps ) noexcept
{
	const sirGraphicsResult_t eAdmit = admitBatch( batch, caps );
	if ( eAdmit != SIR_GRAPHICS_SUCCESS ) {
		return eAdmit;
	}
	if ( commands.getQueue() != SIR_GRAPHICS_QUEUE_DIRECT ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t eResult = commands.reset();
	if ( eResult != SIR_GRAPHICS_SUCCESS ) {
		return eResult;
	}
	eResult = commands.beginRenderPass( *batch.pPass );
	if ( eResult != SIR_GRAPHICS_SUCCESS ) {
		return eResult;
	}
	eResult = commands.setViewport( *batch.pViewport, *batch.pScissor );
	if ( eResult != SIR_GRAPHICS_SUCCESS ) {
		return eResult;
	}
	for ( uint32_t i = 0; i < batch.nDrawCount; ++i ) {
		const sirGraphicsEraDraw_t& draw = batch.pDraws[i];
		eResult = commands.bindPipeline( draw.hPipeline );
		if ( eResult != SIR_GRAPHICS_SUCCESS ) {
			return eResult;
		}
		for ( uint32_t j = 0; j < draw.nBindingCount; ++j ) {
			eResult = commands.bindBindingSet( draw.pBindings[j].nSlot, draw.pBindings[j].hSet );
			if ( eResult != SIR_GRAPHICS_SUCCESS ) {
				return eResult;
			}
		}
		for ( uint32_t j = 0; j < draw.nStreamCount; ++j ) {
			const sirGraphicsEraStream_t& stream = draw.pStreams[j];
			eResult = commands.bindVertexBuffer( stream.nSlot, stream.hBuffer, stream.nOffsetBytes, stream.nStrideBytes );
			if ( eResult != SIR_GRAPHICS_SUCCESS ) {
				return eResult;
			}
		}
		if ( draw.nIndexed != 0 ) {
			eResult = commands.bindIndexBuffer( draw.hIndexBuffer, draw.nIndexOffsetBytes, draw.nIndex32 != 0 );
			if ( eResult == SIR_GRAPHICS_SUCCESS ) {
				eResult = commands.drawIndexed( draw.nElementCount, 1, draw.nFirstElement, draw.nVertexOffset, 0 );
			}
		} else {
			eResult = commands.draw( draw.nElementCount, 1, draw.nFirstElement, 0 );
		}
		if ( eResult != SIR_GRAPHICS_SUCCESS ) {
			return eResult;
		}
	}
	eResult = commands.endRenderPass();
	return eResult == SIR_GRAPHICS_SUCCESS ? commands.close() : eResult;
}

sirGraphicsResult_t SIRGraphicsEraRHI::execute( const sirGraphicsEraBatch_t& batch, SIRRenderingDevice& device,
                                                sirGraphicsCompletion_t *pCompletion ) noexcept
{
	if ( pCompletion == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pCompletion = {};
	if ( batch.eModel != SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE && batch.eModel != SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsABIPipelineModelCaps_t caps = {};
	caps.nSize = sizeof( caps );
	const sirGraphicsResult_t eQuery = device.queryPipelineModelCaps( batch.eModel, &caps );
	if ( eQuery != SIR_GRAPHICS_SUCCESS ) {
		return eQuery;
	}
	if ( batch.nDeviceGeneration != device.getDeviceGeneration() ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	const sirGraphicsResult_t eAdmit = admitBatch( batch, caps );
	if ( eAdmit != SIR_GRAPHICS_SUCCESS ) {
		return eAdmit;
	}
	SIRGraphicsCommandList *pCommands = nullptr;
	const sirGraphicsResult_t eAcquire = device.acquireCommands( SIR_GRAPHICS_QUEUE_DIRECT, &pCommands );
	if ( eAcquire != SIR_GRAPHICS_SUCCESS ) {
		return eAcquire;
	}
	if ( pCommands == nullptr ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	const sirGraphicsResult_t eRecord = record( batch, *pCommands, caps );
	if ( eRecord != SIR_GRAPHICS_SUCCESS ) {
		return eRecord;
	}
	SIRGraphicsCommandList *szLists[] = { pCommands };
	sirGraphicsSubmission_t submission = {};
	submission.ppCommandLists = szLists;
	submission.nCommandListCount = 1;
	return device.submit( SIR_GRAPHICS_QUEUE_DIRECT, submission, pCompletion );
}

sirGraphicsResult_t SIRGraphicsProgrammableRHI::admitPipeline( const sirGraphicsABIPipeline_t& pipeline,
                                                               const sirGraphicsABIPipelineModelCaps_t& caps ) noexcept
{
	if ( pipeline.nModel != SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE || caps.nModel != SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	return SIRGraphicsEraRHI::admitPipeline( pipeline, caps );
}

sirGraphicsResult_t SIRGraphicsProgrammableRHI::execute( const sirGraphicsEraBatch_t& batch, SIRRenderingDevice& device,
                                                         sirGraphicsCompletion_t *pCompletion ) noexcept
{
	if ( batch.eModel != SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE ) {
		if ( pCompletion != nullptr ) {
			*pCompletion = {};
		}
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	return SIRGraphicsEraRHI::execute( batch, device, pCompletion );
}

sirGraphicsResult_t SIRGraphicsFixedFunctionRHI::admitPipeline( const sirGraphicsABIPipeline_t& pipeline,
                                                                const sirGraphicsABIPipelineModelCaps_t& caps ) noexcept
{
	if ( pipeline.nModel != SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION || caps.nModel != SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	return SIRGraphicsEraRHI::admitPipeline( pipeline, caps );
}

sirGraphicsResult_t SIRGraphicsFixedFunctionRHI::execute( const sirGraphicsEraBatch_t& batch, SIRRenderingDevice& device,
                                                          sirGraphicsCompletion_t *pCompletion ) noexcept
{
	if ( batch.eModel != SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION ) {
		if ( pCompletion != nullptr ) {
			*pCompletion = {};
		}
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	return SIRGraphicsEraRHI::execute( batch, device, pCompletion );
}
