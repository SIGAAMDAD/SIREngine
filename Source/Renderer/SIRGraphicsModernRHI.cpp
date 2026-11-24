/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#include <Renderer/SIRGraphicsModernRHI.h>
#include <Renderer/SIRGraphicsValidation.h>

SIRGraphicsResult SIRModernPipelineCompiler::validate( const sirGraphicsPipelineDesc_t& pipeline,
                                                         const sirGraphicsABIPipelineModelCaps_t& caps ) noexcept
{
	const SIRGraphicsResult eValidation = sirGraphicsValidatePipeline( pipeline );
	if ( eValidation != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eValidation;
	}
	if ( caps.nModel != SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_MODERN || pipeline.eModel != SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_MODERN ||
	     pipeline.pModern == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( ( pipeline.pModern->nRequiredFeatures & ~caps.nRequiredFeatureMask ) != 0 ||
	     ( pipeline.pModern->nStageMask & ~caps.nShaderStageMask ) != 0 || pipeline.nBindingCount > caps.nMaximumBindings ||
	     pipeline.nAttributeCount > caps.nMaximumVertexAttributes || pipeline.nColorFormatCount > caps.nMaximumColorAttachments ||
	     pipeline.nSampleCount > caps.nMaximumSamples ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	const sirGraphicsShaderCode_t *shaders[3] = { &pipeline.vertexShader, &pipeline.pixelShader, &pipeline.computeShader };
	for ( uint32_t i = 0; i < 3; ++i ) {
		const sirGraphicsShaderCode_t& shader = *shaders[i];
		if ( shader.nByteCount != 0 && ( shader.eFormat == 0 || shader.eFormat > SIRGraphicsShaderFormat::SIR_GRAPHICS_SHADER_FIXED_FUNCTION ||
		                                 ( caps.nShaderFormatMask & ( 1U << ( shader.eFormat - 1 ) ) ) == 0 ) ) {
			return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
		}
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRModernResourcePlanner::validateHandle( sirGraphicsHandle_t hHandle, SIRGraphicsHandleType eType,
                                                              uint32_t nGeneration ) noexcept
{
	if ( hHandle.eType != eType || hHandle.nGeneration == 0 || hHandle.nDeviceGeneration != nGeneration ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRModernResourcePlanner::validateBarriers( const sirModernBatch_t& batch ) noexcept
{
	if ( batch.nBarrierCount != 0 && batch.pBarriers == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t i = 0; i < batch.nBarrierCount; ++i ) {
		const sirGraphicsBarrier_t& barrier = batch.pBarriers[i];
		if ( barrier.eType > SIRGraphicsBarrierType::SIR_GRAPHICS_BARRIER_ALIAS || barrier.eBefore > SIRGraphicsState::SIR_GRAPHICS_STATE_PRESENT ||
		     barrier.eAfter > SIRGraphicsState::SIR_GRAPHICS_STATE_PRESENT ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( barrier.hResource.eType != SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BUFFER && barrier.hResource.eType != SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_TEXTURE ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
		}
		if ( validateHandle( barrier.hResource, barrier.hResource.eType, batch.nDeviceGeneration ) != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
		}
		if ( barrier.eType == SIRGraphicsBarrierType::SIR_GRAPHICS_BARRIER_ALIAS ) {
			if ( barrier.hAliasedResource.eType != SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BUFFER &&
			     barrier.hAliasedResource.eType != SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_TEXTURE ) {
				return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
			}
			if ( validateHandle( barrier.hAliasedResource, barrier.hAliasedResource.eType, batch.nDeviceGeneration ) !=
			     SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
				return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
			}
		}
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRModernResourcePlanner::validateCopies( const sirModernBatch_t& batch ) noexcept
{
	if ( ( batch.nBufferCopyCount != 0 && batch.pBufferCopies == nullptr ) ||
	     ( batch.nTextureCopyCount != 0 && batch.pTextureCopies == nullptr ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t i = 0; i < batch.nBufferCopyCount; ++i ) {
		const sirModernBufferCopy_t& copy = batch.pBufferCopies[i];
		if ( copy.nByteCount == 0 || copy.nDestinationOffset > UINT64_MAX - copy.nByteCount ||
		     copy.nSourceOffset > UINT64_MAX - copy.nByteCount ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( validateHandle( copy.hDestination, SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BUFFER, batch.nDeviceGeneration ) != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ||
		     validateHandle( copy.hSource, SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BUFFER, batch.nDeviceGeneration ) != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
		}
	}
	for ( uint32_t i = 0; i < batch.nTextureCopyCount; ++i ) {
		const sirGraphicsBufferTextureCopy_t& copy = batch.pTextureCopies[i];
		if ( copy.nWidth == 0 || copy.nHeight == 0 || copy.nDepth == 0 || copy.nBytesPerRow == 0 || copy.nRowsPerImage == 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( validateHandle( copy.hBuffer, SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BUFFER, batch.nDeviceGeneration ) != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ||
		     validateHandle( copy.hTexture, SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_TEXTURE, batch.nDeviceGeneration ) != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
		}
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRModernBindingCompiler::validate( const sirModernBinding_t *pBindings, uint32_t nCount,
                                                        uint32_t nGeneration ) noexcept
{
	if ( nCount != 0 && pBindings == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t i = 0; i < nCount; ++i ) {
		if ( SIRModernResourcePlanner::validateHandle( pBindings[i].hSet, SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BINDING_SET, nGeneration ) !=
		     SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
		}
		for ( uint32_t j = 0; j < i; ++j ) {
			if ( pBindings[i].nSet == pBindings[j].nSet ) {
				return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
			}
		}
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRModernDrawCompiler::validate( const sirModernDraw_t& draw, uint32_t nGeneration ) noexcept
{
	if ( draw.nElementCount == 0 || draw.nInstanceCount == 0 || draw.nIndexed > 1 || draw.nIndex32 > 1 ||
	     ( draw.nStreamCount != 0 && draw.pStreams == nullptr ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( SIRModernResourcePlanner::validateHandle( draw.hPipeline, SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_PIPELINE, nGeneration ) != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	const SIRGraphicsResult eBindings = SIRModernBindingCompiler::validate( draw.pBindings, draw.nBindingCount, nGeneration );
	if ( eBindings != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eBindings;
	}
	for ( uint32_t i = 0; i < draw.nStreamCount; ++i ) {
		const sirModernVertexStream_t& stream = draw.pStreams[i];
		if ( stream.nStrideBytes == 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( SIRModernResourcePlanner::validateHandle( stream.hBuffer, SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BUFFER, nGeneration ) != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
		}
		for ( uint32_t j = 0; j < i; ++j ) {
			if ( stream.nSlot == draw.pStreams[j].nSlot ) {
				return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
			}
		}
	}
	if ( draw.nIndexed != 0 ) {
		return SIRModernResourcePlanner::validateHandle( draw.hIndexBuffer, SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BUFFER, nGeneration );
	}
	if ( draw.nVertexOffset != 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRModernSynchronizationPlanner::validateQueue( const sirModernBatch_t& batch,
                                                                    const sirGraphicsABIPipelineModelCaps_t& caps ) noexcept
{
	if ( batch.eQueue >= SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COUNT || ( caps.nQueueMask & ( 1U << batch.eQueue ) ) == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( batch.eQueue == SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COMPUTE && ( caps.nRequiredFeatureMask & SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_ASYNC_COMPUTE ) == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( batch.eQueue == SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COPY && ( caps.nRequiredFeatureMask & SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_COPY_QUEUE ) == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( caps.nExecutionModel == SIRGraphicsExecutionModel::SIR_GRAPHICS_EXECUTION_STATEFUL_SERIAL && batch.eQueue != SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_DIRECT ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRGraphicsModernRHI::admit( const sirModernBatch_t& batch, const sirGraphicsABIPipelineModelCaps_t& caps ) noexcept
{
	const SIRGraphicsResult eCaps = sirGraphicsValidateABIPipelineModelCaps( &caps );
	if ( eCaps != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eCaps;
	}
	if ( caps.nModel != SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_MODERN ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( batch.nDeviceGeneration == 0 || batch.nDeviceGeneration != caps.nDeviceGeneration ||
	     ( batch.nDrawCount != 0 && batch.pDraws == nullptr ) || ( batch.nDispatchCount != 0 && batch.pDispatches == nullptr ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const SIRGraphicsResult eQueue = SIRModernSynchronizationPlanner::validateQueue( batch, caps );
	if ( eQueue != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eQueue;
	}
	const SIRGraphicsResult eBarriers = SIRModernResourcePlanner::validateBarriers( batch );
	if ( eBarriers != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eBarriers;
	}
	const SIRGraphicsResult eCopy = SIRModernResourcePlanner::validateCopies( batch );
	if ( eCopy != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eCopy;
	}
	if ( ( batch.nBufferCopyCount != 0 || batch.nTextureCopyCount != 0 ) &&
	     ( batch.nDrawCount != 0 || batch.nDispatchCount != 0 || batch.pPass != nullptr ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( batch.pPass != nullptr || batch.nDrawCount != 0 ) {
		if ( batch.pPass == nullptr || batch.eQueue != SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_DIRECT ||
		     ( caps.nRequiredFeatureMask & SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_GRAPHICS ) == 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
		}
		if ( batch.nDrawCount != 0 && ( batch.pViewport == nullptr || batch.pScissor == nullptr ) ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( batch.pPass->nColorCount > caps.nMaximumColorAttachments ||
		     ( batch.pPass->nColorCount != 0 && batch.pPass->pColors == nullptr ) ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		for ( uint32_t i = 0; i < batch.pPass->nColorCount; ++i ) {
			if ( SIRModernResourcePlanner::validateHandle( batch.pPass->pColors[i].hView, SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_VIEW,
			                                               batch.nDeviceGeneration ) != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
				return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
			}
		}
		if ( batch.nDrawCount != 0 &&
		     ( !( batch.pViewport->fWidth > 0.0f && batch.pViewport->fHeight > 0.0f ) ||
		       !( batch.pViewport->fMinimumDepth <= batch.pViewport->fMaximumDepth ) || batch.pScissor->nRight <= batch.pScissor->nLeft ||
		       batch.pScissor->nBottom <= batch.pScissor->nTop ) ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( batch.pPass->hDepthView.eType != SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_NONE &&
		     SIRModernResourcePlanner::validateHandle( batch.pPass->hDepthView, SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_VIEW, batch.nDeviceGeneration ) !=
		         SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
		}
		for ( uint32_t i = 0; i < batch.nDrawCount; ++i ) {
			const sirModernDraw_t& draw = batch.pDraws[i];
			if ( draw.nBindingCount > caps.nMaximumBindings || draw.nStreamCount > caps.nMaximumVertexStreams ) {
				return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
			}
			for ( uint32_t j = 0; j < draw.nBindingCount; ++j ) {
				if ( draw.pBindings == nullptr || draw.pBindings[j].nSet >= caps.nMaximumBindings ) {
					return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
				}
			}
			for ( uint32_t j = 0; j < draw.nStreamCount; ++j ) {
				if ( draw.pStreams == nullptr || draw.pStreams[j].nSlot >= caps.nMaximumVertexStreams ) {
					return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
				}
			}
			if ( draw.nIndexed != 0 && ( draw.nIndexOffsetBytes % ( draw.nIndex32 != 0 ? 4U : 2U ) ) != 0 ) {
				return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
			}
			const SIRGraphicsResult eDraw = SIRModernDrawCompiler::validate( batch.pDraws[i], batch.nDeviceGeneration );
			if ( eDraw != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
				return eDraw;
			}
		}
	}
	if ( batch.nDispatchCount != 0 ) {
		if ( batch.nDrawCount != 0 || batch.pPass != nullptr || ( caps.nShaderStageMask & SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_COMPUTE ) == 0 ||
		     ( caps.nRequiredFeatureMask & SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_COMPUTE ) == 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
		}
		for ( uint32_t i = 0; i < batch.nDispatchCount; ++i ) {
			if ( batch.pDispatches[i].nBindingCount > caps.nMaximumBindings ) {
				return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
			}
			const sirModernDispatch_t& dispatch = batch.pDispatches[i];
			if ( dispatch.nX == 0 || dispatch.nY == 0 || dispatch.nZ == 0 ) {
				return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
			}
			if ( SIRModernResourcePlanner::validateHandle( dispatch.hPipeline, SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_PIPELINE, batch.nDeviceGeneration ) !=
			     SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
				return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
			}
			const SIRGraphicsResult eBindings =
			    SIRModernBindingCompiler::validate( dispatch.pBindings, dispatch.nBindingCount, batch.nDeviceGeneration );
			if ( eBindings != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
				return eBindings;
			}
		}
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRGraphicsModernRHI::record( const sirModernBatch_t& batch, SIRGraphicsCommandList& commands,
                                                  const sirGraphicsABIPipelineModelCaps_t& caps ) noexcept
{
	const SIRGraphicsResult eAdmission = admit( batch, caps );
	if ( eAdmission != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eAdmission;
	}
	if ( commands.getQueue() != batch.eQueue ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	SIRGraphicsResult eResult = commands.reset();
	if ( eResult != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eResult;
	}
	if ( batch.nBarrierCount != 0 ) {
		eResult = commands.barrier( batch.pBarriers, batch.nBarrierCount );
		if ( eResult != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
			return eResult;
		}
	}
	for ( uint32_t i = 0; i < batch.nBufferCopyCount; ++i ) {
		const sirModernBufferCopy_t& copy = batch.pBufferCopies[i];
		eResult = commands.copyBuffer( copy.hDestination, copy.nDestinationOffset, copy.hSource, copy.nSourceOffset, copy.nByteCount );
		if ( eResult != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
			return eResult;
		}
	}
	for ( uint32_t i = 0; i < batch.nTextureCopyCount; ++i ) {
		eResult = commands.copyBufferTexture( batch.pTextureCopies[i] );
		if ( eResult != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
			return eResult;
		}
	}
	if ( batch.pPass != nullptr ) {
		eResult = commands.beginRenderPass( *batch.pPass );
		if ( eResult != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
			return eResult;
		}
		if ( batch.nDrawCount != 0 ) {
			eResult = commands.setViewport( *batch.pViewport, *batch.pScissor );
			if ( eResult != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
				return eResult;
			}
		}
		for ( uint32_t i = 0; i < batch.nDrawCount; ++i ) {
			const sirModernDraw_t& draw = batch.pDraws[i];
			eResult = commands.bindPipeline( draw.hPipeline );
			if ( eResult != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
				return eResult;
			}
			for ( uint32_t j = 0; j < draw.nBindingCount; ++j ) {
				eResult = commands.bindBindingSet( draw.pBindings[j].nSet, draw.pBindings[j].hSet );
				if ( eResult != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
					return eResult;
				}
			}
			for ( uint32_t j = 0; j < draw.nStreamCount; ++j ) {
				const sirModernVertexStream_t& stream = draw.pStreams[j];
				eResult = commands.bindVertexBuffer( stream.nSlot, stream.hBuffer, stream.nOffsetBytes, stream.nStrideBytes );
				if ( eResult != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
					return eResult;
				}
			}
			if ( draw.nIndexed != 0 ) {
				eResult = commands.bindIndexBuffer( draw.hIndexBuffer, draw.nIndexOffsetBytes, draw.nIndex32 != 0 );
				if ( eResult == SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
					eResult = commands.drawIndexed( draw.nElementCount, draw.nInstanceCount, draw.nFirstElement, draw.nVertexOffset,
					                                draw.nFirstInstance );
				}
			} else {
				eResult = commands.draw( draw.nElementCount, draw.nInstanceCount, draw.nFirstElement, draw.nFirstInstance );
			}
			if ( eResult != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
				return eResult;
			}
		}
		eResult = commands.endRenderPass();
		if ( eResult != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
			return eResult;
		}
	}
	for ( uint32_t i = 0; i < batch.nDispatchCount; ++i ) {
		const sirModernDispatch_t& dispatch = batch.pDispatches[i];
		eResult = commands.bindPipeline( dispatch.hPipeline );
		if ( eResult != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
			return eResult;
		}
		for ( uint32_t j = 0; j < dispatch.nBindingCount; ++j ) {
			eResult = commands.bindBindingSet( dispatch.pBindings[j].nSet, dispatch.pBindings[j].hSet );
			if ( eResult != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
				return eResult;
			}
		}
		eResult = commands.dispatch( dispatch.nX, dispatch.nY, dispatch.nZ );
		if ( eResult != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
			return eResult;
		}
	}
	return commands.close();
}

SIRGraphicsResult SIRGraphicsModernRHI::execute( const sirModernBatch_t& batch, SIRRenderingDevice& device,
                                                   sirGraphicsCompletion_t *pCompletion ) noexcept
{
	if ( pCompletion == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pCompletion = {};
	sirGraphicsABIPipelineModelCaps_t caps {};
	caps.nSize = sizeof( caps );
	const SIRGraphicsResult eQuery = device.queryPipelineModelCaps( SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_MODERN, &caps );
	if ( eQuery != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eQuery;
	}
	if ( device.getDeviceGeneration() != batch.nDeviceGeneration ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	const SIRGraphicsResult eAdmit = admit( batch, caps );
	if ( eAdmit != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eAdmit;
	}
	SIRGraphicsCommandList *pCommands = nullptr;
	const SIRGraphicsResult eAcquire = device.acquireCommands( batch.eQueue, &pCommands );
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
	return device.submit( batch.eQueue, submission, pCompletion );
}
