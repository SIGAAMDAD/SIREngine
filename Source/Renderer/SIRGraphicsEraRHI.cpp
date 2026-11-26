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

#include <Renderer/SIRGraphicsEraRHI.h>

static SIRGraphicsResult sirEraCheckHandle( sirGraphicsHandle_t handle, SIRGraphicsHandleType eType, uint32_t nGeneration ) noexcept
{
	if ( handle.eType != eType || handle.nGeneration == 0 || handle.nDeviceGeneration != nGeneration ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRGraphicsEraRHI::admitPipeline( const sirGraphicsABIPipeline_t& pipeline,
                                                      const sirGraphicsABIPipelineModelCaps_t& caps ) noexcept
{
	if ( pipeline.nModel == static_cast<uint32_t>( SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_MODERN ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	return sirGraphicsValidateABIPipeline( &pipeline, &caps );
}

SIRGraphicsResult SIRGraphicsEraRHI::admitBatch( const sirGraphicsEraBatch_t& batch,
                                                   const sirGraphicsABIPipelineModelCaps_t& caps ) noexcept
{
	const SIRGraphicsResult eCaps = sirGraphicsValidateABIPipelineModelCaps( &caps );
	if ( eCaps != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eCaps;
	}
	if ( batch.eModel != SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE && batch.eModel != SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( caps.nModel != static_cast<uint32_t>( batch.eModel ) || caps.nExecutionModel != static_cast<uint32_t>( SIRGraphicsExecutionModel::SIR_GRAPHICS_EXECUTION_STATEFUL_SERIAL ) ||
	     ( caps.nQueueMask & ( 1U << static_cast<uint32_t>( SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_DIRECT ) ) ) == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( batch.nDeviceGeneration == 0 || batch.nDeviceGeneration != caps.nDeviceGeneration ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( batch.pPass == nullptr || batch.pViewport == nullptr || batch.pScissor == nullptr ||
	     ( batch.nDrawCount != 0 && batch.pDraws == nullptr ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( ( batch.pPass->nColorCount == 0 && batch.pPass->hDepthView.eType == SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_NONE ) ||
	     batch.pPass->nColorCount > caps.nMaximumColorAttachments ||
	     ( batch.pPass->nColorCount != 0 && batch.pPass->pColors == nullptr ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	for ( uint32_t i = 0; i < batch.pPass->nColorCount; ++i ) {
		if ( sirEraCheckHandle( batch.pPass->pColors[i].hView, SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_VIEW, batch.nDeviceGeneration ) !=
		     SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
		}
	}
	if ( batch.pPass->hDepthView.eType != SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_NONE &&
	     sirEraCheckHandle( batch.pPass->hDepthView, SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_VIEW, batch.nDeviceGeneration ) != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	for ( uint32_t i = 0; i < batch.nDrawCount; ++i ) {
		const sirGraphicsEraDraw_t& draw = batch.pDraws[i];
		if ( draw.nElementCount == 0 || draw.nStreamCount == 0 || draw.nIndexed > 1 || draw.nIndex32 > 1 ||
		     draw.nFirstElement > UINT32_MAX - draw.nElementCount || ( draw.nStreamCount != 0 && draw.pStreams == nullptr ) ||
		     ( draw.nBindingCount != 0 && draw.pBindings == nullptr ) ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( draw.nStreamCount > caps.nMaximumVertexStreams || draw.nBindingCount > caps.nMaximumBindings ) {
			return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
		}
		if ( batch.eModel == SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION && draw.nBindingCount != 0 ) {
			// Fixed recipes use their native stage state, not modern descriptor sets.
			return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
		}
		if ( sirEraCheckHandle( draw.hPipeline, SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_PIPELINE, batch.nDeviceGeneration ) != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
		}
		for ( uint32_t j = 0; j < draw.nBindingCount; ++j ) {
			const sirGraphicsEraBinding_t& binding = draw.pBindings[j];
			if ( binding.nSlot >= caps.nMaximumBindings ) {
				return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
			}
			if ( sirEraCheckHandle( binding.hSet, SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BINDING_SET, batch.nDeviceGeneration ) != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
				return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
			}
			for ( uint32_t k = 0; k < j; ++k ) {
				if ( draw.pBindings[k].nSlot == binding.nSlot ) {
					return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
				}
			}
		}
		for ( uint32_t j = 0; j < draw.nStreamCount; ++j ) {
			const sirGraphicsEraStream_t& stream = draw.pStreams[j];
			if ( stream.nSlot >= caps.nMaximumVertexStreams || stream.nStrideBytes == 0 ) {
				return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
			}
			if ( sirEraCheckHandle( stream.hBuffer, SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BUFFER, batch.nDeviceGeneration ) != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
				return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
			}
			for ( uint32_t k = 0; k < j; ++k ) {
				if ( draw.pStreams[k].nSlot == stream.nSlot ) {
					return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
				}
			}
		}
		if ( draw.nIndexed != 0 ) {
			if ( draw.nVertexOffset != 0 ) {
				return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
			}
			if ( draw.nIndexOffsetBytes % ( draw.nIndex32 ? 4U : 2U ) != 0 ) {
				return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
			}
			if ( sirEraCheckHandle( draw.hIndexBuffer, SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BUFFER, batch.nDeviceGeneration ) != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
				return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
			}
		} else if ( draw.nVertexOffset != 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRGraphicsEraRHI::record( const sirGraphicsEraBatch_t& batch, SIRGraphicsCommandList& commands,
                                               const sirGraphicsABIPipelineModelCaps_t& caps ) noexcept
{
	const SIRGraphicsResult eAdmit = admitBatch( batch, caps );
	if ( eAdmit != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eAdmit;
	}
	if ( commands.getQueue() != SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_DIRECT ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	SIRGraphicsResult eResult = commands.reset();
	if ( eResult != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eResult;
	}
	eResult = commands.beginRenderPass( *batch.pPass );
	if ( eResult != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eResult;
	}
	eResult = commands.setViewport( *batch.pViewport, *batch.pScissor );
	if ( eResult != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eResult;
	}
	for ( uint32_t i = 0; i < batch.nDrawCount; ++i ) {
		const sirGraphicsEraDraw_t& draw = batch.pDraws[i];
		eResult = commands.bindPipeline( draw.hPipeline );
		if ( eResult != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
			return eResult;
		}
		for ( uint32_t j = 0; j < draw.nBindingCount; ++j ) {
			eResult = commands.bindBindingSet( draw.pBindings[j].nSlot, draw.pBindings[j].hSet );
			if ( eResult != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
				return eResult;
			}
		}
		for ( uint32_t j = 0; j < draw.nStreamCount; ++j ) {
			const sirGraphicsEraStream_t& stream = draw.pStreams[j];
			eResult = commands.bindVertexBuffer( stream.nSlot, stream.hBuffer, stream.nOffsetBytes, stream.nStrideBytes );
			if ( eResult != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
				return eResult;
			}
		}
		if ( draw.nIndexed != 0 ) {
			eResult = commands.bindIndexBuffer( draw.hIndexBuffer, draw.nIndexOffsetBytes, draw.nIndex32 != 0 );
			if ( eResult == SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
				eResult = commands.drawIndexed( draw.nElementCount, 1, draw.nFirstElement, draw.nVertexOffset, 0 );
			}
		} else {
			eResult = commands.draw( draw.nElementCount, 1, draw.nFirstElement, 0 );
		}
		if ( eResult != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
			return eResult;
		}
	}
	eResult = commands.endRenderPass();
	return eResult == SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ? commands.close() : eResult;
}

SIRGraphicsResult SIRGraphicsEraRHI::execute( const sirGraphicsEraBatch_t& batch, SIRRenderingDevice& device,
                                                sirGraphicsCompletion_t *pCompletion ) noexcept
{
	if ( pCompletion == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pCompletion = {};
	if ( batch.eModel != SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE && batch.eModel != SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsABIPipelineModelCaps_t caps {};
	caps.nSize = sizeof( caps );
	const SIRGraphicsResult eQuery = device.queryPipelineModelCaps( batch.eModel, &caps );
	if ( eQuery != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eQuery;
	}
	if ( batch.nDeviceGeneration != device.getDeviceGeneration() ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	const SIRGraphicsResult eAdmit = admitBatch( batch, caps );
	if ( eAdmit != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eAdmit;
	}
	SIRGraphicsCommandList *pCommands = nullptr;
	const SIRGraphicsResult eAcquire = device.acquireCommands( SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_DIRECT, &pCommands );
	if ( eAcquire != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eAcquire;
	}
	if ( pCommands == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR;
	}
	const SIRGraphicsResult eRecord = record( batch, *pCommands, caps );
	if ( eRecord != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eRecord;
	}
	SIRGraphicsCommandList *szLists[] = { pCommands };
	sirGraphicsSubmission_t submission {};
	submission.ppCommandLists = szLists;
	submission.nCommandListCount = 1;
	return device.submit( SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_DIRECT, submission, pCompletion );
}

SIRGraphicsResult SIRGraphicsProgrammableRHI::admitPipeline( const sirGraphicsABIPipeline_t& pipeline,
                                                               const sirGraphicsABIPipelineModelCaps_t& caps ) noexcept
{
	if ( pipeline.nModel != static_cast<uint32_t>( SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE ) || caps.nModel != static_cast<uint32_t>( SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	return SIRGraphicsEraRHI::admitPipeline( pipeline, caps );
}

SIRGraphicsResult SIRGraphicsProgrammableRHI::execute( const sirGraphicsEraBatch_t& batch, SIRRenderingDevice& device,
                                                         sirGraphicsCompletion_t *pCompletion ) noexcept
{
	if ( batch.eModel != SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE ) {
		if ( pCompletion != nullptr ) {
			*pCompletion = {};
		}
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	return SIRGraphicsEraRHI::execute( batch, device, pCompletion );
}

SIRGraphicsResult SIRGraphicsFixedFunctionRHI::admitPipeline( const sirGraphicsABIPipeline_t& pipeline,
                                                                const sirGraphicsABIPipelineModelCaps_t& caps ) noexcept
{
	if ( pipeline.nModel != static_cast<uint32_t>( SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION ) || caps.nModel != static_cast<uint32_t>( SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	return SIRGraphicsEraRHI::admitPipeline( pipeline, caps );
}

SIRGraphicsResult SIRGraphicsFixedFunctionRHI::execute( const sirGraphicsEraBatch_t& batch, SIRRenderingDevice& device,
                                                          sirGraphicsCompletion_t *pCompletion ) noexcept
{
	if ( batch.eModel != SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION ) {
		if ( pCompletion != nullptr ) {
			*pCompletion = {};
		}
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	return SIRGraphicsEraRHI::execute( batch, device, pCompletion );
}
