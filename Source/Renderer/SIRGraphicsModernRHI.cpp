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

sirGraphicsResult_t SIRModernPipelineCompiler::validate(
    const sirGraphicsPipelineDesc_t &pipeline,
    const sirGraphicsABIPipelineModelCaps_t &caps) noexcept {
  const sirGraphicsResult_t eValidation = sirGraphicsValidatePipeline(pipeline);
  if (eValidation != SIR_GRAPHICS_SUCCESS) {
    return eValidation;
  }
  if (caps.nModel != SIR_GRAPHICS_PIPELINE_MODEL_MODERN ||
      pipeline.eModel != SIR_GRAPHICS_PIPELINE_MODEL_MODERN ||
      pipeline.pModern == nullptr) {
    return SIR_GRAPHICS_UNSUPPORTED;
  }
  if ((pipeline.pModern->nRequiredFeatures & ~caps.nRequiredFeatureMask) != 0 ||
      (pipeline.pModern->nStageMask & ~caps.nShaderStageMask) != 0 ||
      pipeline.nBindingCount > caps.nMaximumBindings ||
      pipeline.nAttributeCount > caps.nMaximumVertexAttributes ||
      pipeline.nColorFormatCount > caps.nMaximumColorAttachments ||
      pipeline.nSampleCount > caps.nMaximumSamples) {
    return SIR_GRAPHICS_UNSUPPORTED;
  }
  const sirGraphicsShaderCode_t *shaders[3] = {
      &pipeline.vertexShader, &pipeline.pixelShader, &pipeline.computeShader};
  for (uint32_t i = 0; i < 3; ++i) {
    const sirGraphicsShaderCode_t &shader = *shaders[i];
    if (shader.nByteCount != 0 &&
        (shader.eFormat == 0 ||
         shader.eFormat > SIR_GRAPHICS_SHADER_FIXED_FUNCTION ||
         (caps.nShaderFormatMask & (1U << (shader.eFormat - 1))) == 0)) {
      return SIR_GRAPHICS_UNSUPPORTED;
    }
  }
  return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t
SIRModernResourcePlanner::validateHandle(sirGraphicsHandle_t hHandle,
                                         sirGraphicsHandleType_t eType,
                                         uint32_t nGeneration) noexcept {
  if (hHandle.eType != eType || hHandle.nGeneration == 0 ||
      hHandle.nDeviceGeneration != nGeneration) {
    return SIR_GRAPHICS_INVALID_HANDLE;
  }
  return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRModernResourcePlanner::validateBarriers(
    const sirModernBatch_t &batch) noexcept {
  if (batch.nBarrierCount != 0 && batch.pBarriers == nullptr) {
    return SIR_GRAPHICS_INVALID_ARGUMENT;
  }
  for (uint32_t i = 0; i < batch.nBarrierCount; ++i) {
    const sirGraphicsBarrier_t &barrier = batch.pBarriers[i];
    if (barrier.eType > SIR_GRAPHICS_BARRIER_ALIAS ||
        barrier.eBefore > SIR_GRAPHICS_STATE_PRESENT ||
        barrier.eAfter > SIR_GRAPHICS_STATE_PRESENT) {
      return SIR_GRAPHICS_INVALID_ARGUMENT;
    }
    if (barrier.hResource.eType != SIR_GRAPHICS_HANDLE_BUFFER &&
        barrier.hResource.eType != SIR_GRAPHICS_HANDLE_TEXTURE) {
      return SIR_GRAPHICS_INVALID_HANDLE;
    }
    if (validateHandle(barrier.hResource, barrier.hResource.eType,
                       batch.nDeviceGeneration) != SIR_GRAPHICS_SUCCESS) {
      return SIR_GRAPHICS_INVALID_HANDLE;
    }
    if (barrier.eType == SIR_GRAPHICS_BARRIER_ALIAS) {
      if (barrier.hAliasedResource.eType != SIR_GRAPHICS_HANDLE_BUFFER &&
          barrier.hAliasedResource.eType != SIR_GRAPHICS_HANDLE_TEXTURE) {
        return SIR_GRAPHICS_INVALID_HANDLE;
      }
      if (validateHandle(barrier.hAliasedResource,
                         barrier.hAliasedResource.eType,
                         batch.nDeviceGeneration) != SIR_GRAPHICS_SUCCESS) {
        return SIR_GRAPHICS_INVALID_HANDLE;
      }
    }
  }
  return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRModernResourcePlanner::validateCopies(
    const sirModernBatch_t &batch) noexcept {
  if ((batch.nBufferCopyCount != 0 && batch.pBufferCopies == nullptr) ||
      (batch.nTextureCopyCount != 0 && batch.pTextureCopies == nullptr)) {
    return SIR_GRAPHICS_INVALID_ARGUMENT;
  }
  for (uint32_t i = 0; i < batch.nBufferCopyCount; ++i) {
    const sirModernBufferCopy_t &copy = batch.pBufferCopies[i];
    if (copy.nByteCount == 0 ||
        copy.nDestinationOffset > UINT64_MAX - copy.nByteCount ||
        copy.nSourceOffset > UINT64_MAX - copy.nByteCount) {
      return SIR_GRAPHICS_INVALID_ARGUMENT;
    }
    if (validateHandle(copy.hDestination, SIR_GRAPHICS_HANDLE_BUFFER,
                       batch.nDeviceGeneration) != SIR_GRAPHICS_SUCCESS ||
        validateHandle(copy.hSource, SIR_GRAPHICS_HANDLE_BUFFER,
                       batch.nDeviceGeneration) != SIR_GRAPHICS_SUCCESS) {
      return SIR_GRAPHICS_INVALID_HANDLE;
    }
  }
  for (uint32_t i = 0; i < batch.nTextureCopyCount; ++i) {
    const sirGraphicsBufferTextureCopy_t &copy = batch.pTextureCopies[i];
    if (copy.nWidth == 0 || copy.nHeight == 0 || copy.nDepth == 0 ||
        copy.nBytesPerRow == 0 || copy.nRowsPerImage == 0) {
      return SIR_GRAPHICS_INVALID_ARGUMENT;
    }
    if (validateHandle(copy.hBuffer, SIR_GRAPHICS_HANDLE_BUFFER,
                       batch.nDeviceGeneration) != SIR_GRAPHICS_SUCCESS ||
        validateHandle(copy.hTexture, SIR_GRAPHICS_HANDLE_TEXTURE,
                       batch.nDeviceGeneration) != SIR_GRAPHICS_SUCCESS) {
      return SIR_GRAPHICS_INVALID_HANDLE;
    }
  }
  return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t
SIRModernBindingCompiler::validate(const sirModernBinding_t *pBindings,
                                   uint32_t nCount,
                                   uint32_t nGeneration) noexcept {
  if (nCount != 0 && pBindings == nullptr) {
    return SIR_GRAPHICS_INVALID_ARGUMENT;
  }
  for (uint32_t i = 0; i < nCount; ++i) {
    if (SIRModernResourcePlanner::validateHandle(
            pBindings[i].hSet, SIR_GRAPHICS_HANDLE_BINDING_SET, nGeneration) !=
        SIR_GRAPHICS_SUCCESS) {
      return SIR_GRAPHICS_INVALID_HANDLE;
    }
    for (uint32_t j = 0; j < i; ++j) {
      if (pBindings[i].nSet == pBindings[j].nSet) {
        return SIR_GRAPHICS_INVALID_ARGUMENT;
      }
    }
  }
  return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t
SIRModernDrawCompiler::validate(const sirModernDraw_t &draw,
                                uint32_t nGeneration) noexcept {
  if (draw.nElementCount == 0 || draw.nInstanceCount == 0 ||
      draw.nIndexed > 1 || draw.nIndex32 > 1 ||
      (draw.nStreamCount != 0 && draw.pStreams == nullptr)) {
    return SIR_GRAPHICS_INVALID_ARGUMENT;
  }
  if (SIRModernResourcePlanner::validateHandle(
          draw.hPipeline, SIR_GRAPHICS_HANDLE_PIPELINE, nGeneration) !=
      SIR_GRAPHICS_SUCCESS) {
    return SIR_GRAPHICS_INVALID_HANDLE;
  }
  const sirGraphicsResult_t eBindings = SIRModernBindingCompiler::validate(
      draw.pBindings, draw.nBindingCount, nGeneration);
  if (eBindings != SIR_GRAPHICS_SUCCESS) {
    return eBindings;
  }
  for (uint32_t i = 0; i < draw.nStreamCount; ++i) {
    const sirModernVertexStream_t &stream = draw.pStreams[i];
    if (stream.nStrideBytes == 0) {
      return SIR_GRAPHICS_INVALID_ARGUMENT;
    }
    if (SIRModernResourcePlanner::validateHandle(
            stream.hBuffer, SIR_GRAPHICS_HANDLE_BUFFER, nGeneration) !=
        SIR_GRAPHICS_SUCCESS) {
      return SIR_GRAPHICS_INVALID_HANDLE;
    }
    for (uint32_t j = 0; j < i; ++j) {
      if (stream.nSlot == draw.pStreams[j].nSlot) {
        return SIR_GRAPHICS_INVALID_ARGUMENT;
      }
    }
  }
  if (draw.nIndexed != 0) {
    return SIRModernResourcePlanner::validateHandle(
        draw.hIndexBuffer, SIR_GRAPHICS_HANDLE_BUFFER, nGeneration);
  }
  if (draw.nVertexOffset != 0) {
    return SIR_GRAPHICS_INVALID_ARGUMENT;
  }
  return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRModernSynchronizationPlanner::validateQueue(
    const sirModernBatch_t &batch,
    const sirGraphicsABIPipelineModelCaps_t &caps) noexcept {
  if (batch.eQueue >= SIR_GRAPHICS_QUEUE_COUNT ||
      (caps.nQueueMask & (1U << batch.eQueue)) == 0) {
    return SIR_GRAPHICS_UNSUPPORTED;
  }
  if (batch.eQueue == SIR_GRAPHICS_QUEUE_COMPUTE &&
      (caps.nRequiredFeatureMask & SIR_GRAPHICS_FEATURE_ASYNC_COMPUTE) == 0) {
    return SIR_GRAPHICS_UNSUPPORTED;
  }
  if (batch.eQueue == SIR_GRAPHICS_QUEUE_COPY &&
      (caps.nRequiredFeatureMask & SIR_GRAPHICS_FEATURE_COPY_QUEUE) == 0) {
    return SIR_GRAPHICS_UNSUPPORTED;
  }
  if (caps.nExecutionModel == SIR_GRAPHICS_EXECUTION_STATEFUL_SERIAL &&
      batch.eQueue != SIR_GRAPHICS_QUEUE_DIRECT) {
    return SIR_GRAPHICS_UNSUPPORTED;
  }
  return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRGraphicsModernRHI::admit(
    const sirModernBatch_t &batch,
    const sirGraphicsABIPipelineModelCaps_t &caps) noexcept {
  const sirGraphicsResult_t eCaps =
      sirGraphicsValidateABIPipelineModelCaps(&caps);
  if (eCaps != SIR_GRAPHICS_SUCCESS) {
    return eCaps;
  }
  if (caps.nModel != SIR_GRAPHICS_PIPELINE_MODEL_MODERN) {
    return SIR_GRAPHICS_UNSUPPORTED;
  }
  if (batch.nDeviceGeneration == 0 ||
      batch.nDeviceGeneration != caps.nDeviceGeneration ||
      (batch.nDrawCount != 0 && batch.pDraws == nullptr) ||
      (batch.nDispatchCount != 0 && batch.pDispatches == nullptr)) {
    return SIR_GRAPHICS_INVALID_ARGUMENT;
  }
  const sirGraphicsResult_t eQueue =
      SIRModernSynchronizationPlanner::validateQueue(batch, caps);
  if (eQueue != SIR_GRAPHICS_SUCCESS) {
    return eQueue;
  }
  const sirGraphicsResult_t eBarriers =
      SIRModernResourcePlanner::validateBarriers(batch);
  if (eBarriers != SIR_GRAPHICS_SUCCESS) {
    return eBarriers;
  }
  const sirGraphicsResult_t eCopy =
      SIRModernResourcePlanner::validateCopies(batch);
  if (eCopy != SIR_GRAPHICS_SUCCESS) {
    return eCopy;
  }
  if ((batch.nBufferCopyCount != 0 || batch.nTextureCopyCount != 0) &&
      (batch.nDrawCount != 0 || batch.nDispatchCount != 0 ||
       batch.pPass != nullptr)) {
    return SIR_GRAPHICS_INVALID_ARGUMENT;
  }
  if (batch.nDrawCount != 0) {
    if (batch.pPass == nullptr || batch.pViewport == nullptr ||
        batch.pScissor == nullptr ||
        batch.eQueue != SIR_GRAPHICS_QUEUE_DIRECT ||
        (caps.nRequiredFeatureMask & SIR_GRAPHICS_FEATURE_GRAPHICS) == 0) {
      return SIR_GRAPHICS_UNSUPPORTED;
    }
    if (batch.pPass->nColorCount > caps.nMaximumColorAttachments ||
        (batch.pPass->nColorCount != 0 && batch.pPass->pColors == nullptr)) {
      return SIR_GRAPHICS_INVALID_ARGUMENT;
    }
    for (uint32_t i = 0; i < batch.pPass->nColorCount; ++i) {
      if (SIRModernResourcePlanner::validateHandle(
              batch.pPass->pColors[i].hView, SIR_GRAPHICS_HANDLE_VIEW,
              batch.nDeviceGeneration) != SIR_GRAPHICS_SUCCESS) {
        return SIR_GRAPHICS_INVALID_HANDLE;
      }
    }
    if (!(batch.pViewport->fWidth > 0.0f && batch.pViewport->fHeight > 0.0f) ||
        !(batch.pViewport->fMinimumDepth <= batch.pViewport->fMaximumDepth) ||
        batch.pScissor->nRight <= batch.pScissor->nLeft ||
        batch.pScissor->nBottom <= batch.pScissor->nTop) {
      return SIR_GRAPHICS_INVALID_ARGUMENT;
    }
    if (batch.pPass->hDepthView.eType != SIR_GRAPHICS_HANDLE_NONE &&
        SIRModernResourcePlanner::validateHandle(
            batch.pPass->hDepthView, SIR_GRAPHICS_HANDLE_VIEW,
            batch.nDeviceGeneration) != SIR_GRAPHICS_SUCCESS) {
      return SIR_GRAPHICS_INVALID_HANDLE;
    }
    for (uint32_t i = 0; i < batch.nDrawCount; ++i) {
      const sirModernDraw_t &draw = batch.pDraws[i];
      if (draw.nBindingCount > caps.nMaximumBindings ||
          draw.nStreamCount > caps.nMaximumVertexStreams) {
        return SIR_GRAPHICS_UNSUPPORTED;
      }
      for (uint32_t j = 0; j < draw.nBindingCount; ++j) {
        if (draw.pBindings == nullptr ||
            draw.pBindings[j].nSet >= caps.nMaximumBindings) {
          return SIR_GRAPHICS_UNSUPPORTED;
        }
      }
      for (uint32_t j = 0; j < draw.nStreamCount; ++j) {
        if (draw.pStreams == nullptr ||
            draw.pStreams[j].nSlot >= caps.nMaximumVertexStreams) {
          return SIR_GRAPHICS_UNSUPPORTED;
        }
      }
      if (draw.nIndexed != 0 &&
          (draw.nIndexOffsetBytes % (draw.nIndex32 != 0 ? 4U : 2U)) != 0) {
        return SIR_GRAPHICS_INVALID_ARGUMENT;
      }
      const sirGraphicsResult_t eDraw = SIRModernDrawCompiler::validate(
          batch.pDraws[i], batch.nDeviceGeneration);
      if (eDraw != SIR_GRAPHICS_SUCCESS) {
        return eDraw;
      }
    }
  }
  if (batch.nDispatchCount != 0) {
    if (batch.nDrawCount != 0 || batch.pPass != nullptr ||
        (caps.nShaderStageMask & SIR_GRAPHICS_STAGE_COMPUTE) == 0 ||
        (caps.nRequiredFeatureMask & SIR_GRAPHICS_FEATURE_COMPUTE) == 0) {
      return SIR_GRAPHICS_UNSUPPORTED;
    }
    for (uint32_t i = 0; i < batch.nDispatchCount; ++i) {
      if (batch.pDispatches[i].nBindingCount > caps.nMaximumBindings) {
        return SIR_GRAPHICS_UNSUPPORTED;
      }
      const sirModernDispatch_t &dispatch = batch.pDispatches[i];
      if (dispatch.nX == 0 || dispatch.nY == 0 || dispatch.nZ == 0) {
        return SIR_GRAPHICS_INVALID_ARGUMENT;
      }
      if (SIRModernResourcePlanner::validateHandle(
              dispatch.hPipeline, SIR_GRAPHICS_HANDLE_PIPELINE,
              batch.nDeviceGeneration) != SIR_GRAPHICS_SUCCESS) {
        return SIR_GRAPHICS_INVALID_HANDLE;
      }
      const sirGraphicsResult_t eBindings = SIRModernBindingCompiler::validate(
          dispatch.pBindings, dispatch.nBindingCount, batch.nDeviceGeneration);
      if (eBindings != SIR_GRAPHICS_SUCCESS) {
        return eBindings;
      }
    }
  }
  return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRGraphicsModernRHI::record(
    const sirModernBatch_t &batch, SIRGraphicsCommandList &commands,
    const sirGraphicsABIPipelineModelCaps_t &caps) noexcept {
  const sirGraphicsResult_t eAdmission = admit(batch, caps);
  if (eAdmission != SIR_GRAPHICS_SUCCESS) {
    return eAdmission;
  }
  if (commands.getQueue() != batch.eQueue) {
    return SIR_GRAPHICS_INVALID_ARGUMENT;
  }
  sirGraphicsResult_t eResult = commands.reset();
  if (eResult != SIR_GRAPHICS_SUCCESS) {
    return eResult;
  }
  if (batch.nBarrierCount != 0) {
    eResult = commands.barrier(batch.pBarriers, batch.nBarrierCount);
    if (eResult != SIR_GRAPHICS_SUCCESS) {
      return eResult;
    }
  }
  for (uint32_t i = 0; i < batch.nBufferCopyCount; ++i) {
    const sirModernBufferCopy_t &copy = batch.pBufferCopies[i];
    eResult =
        commands.copyBuffer(copy.hDestination, copy.nDestinationOffset,
                            copy.hSource, copy.nSourceOffset, copy.nByteCount);
    if (eResult != SIR_GRAPHICS_SUCCESS) {
      return eResult;
    }
  }
  for (uint32_t i = 0; i < batch.nTextureCopyCount; ++i) {
    eResult = commands.copyBufferTexture(batch.pTextureCopies[i]);
    if (eResult != SIR_GRAPHICS_SUCCESS) {
      return eResult;
    }
  }
  if (batch.nDrawCount != 0) {
    eResult = commands.beginRenderPass(*batch.pPass);
    if (eResult != SIR_GRAPHICS_SUCCESS) {
      return eResult;
    }
    eResult = commands.setViewport(*batch.pViewport, *batch.pScissor);
    if (eResult != SIR_GRAPHICS_SUCCESS) {
      return eResult;
    }
    for (uint32_t i = 0; i < batch.nDrawCount; ++i) {
      const sirModernDraw_t &draw = batch.pDraws[i];
      eResult = commands.bindPipeline(draw.hPipeline);
      if (eResult != SIR_GRAPHICS_SUCCESS) {
        return eResult;
      }
      for (uint32_t j = 0; j < draw.nBindingCount; ++j) {
        eResult = commands.bindBindingSet(draw.pBindings[j].nSet,
                                          draw.pBindings[j].hSet);
        if (eResult != SIR_GRAPHICS_SUCCESS) {
          return eResult;
        }
      }
      for (uint32_t j = 0; j < draw.nStreamCount; ++j) {
        const sirModernVertexStream_t &stream = draw.pStreams[j];
        eResult =
            commands.bindVertexBuffer(stream.nSlot, stream.hBuffer,
                                      stream.nOffsetBytes, stream.nStrideBytes);
        if (eResult != SIR_GRAPHICS_SUCCESS) {
          return eResult;
        }
      }
      if (draw.nIndexed != 0) {
        eResult = commands.bindIndexBuffer(
            draw.hIndexBuffer, draw.nIndexOffsetBytes, draw.nIndex32 != 0);
        if (eResult == SIR_GRAPHICS_SUCCESS) {
          eResult = commands.drawIndexed(
              draw.nElementCount, draw.nInstanceCount, draw.nFirstElement,
              draw.nVertexOffset, draw.nFirstInstance);
        }
      } else {
        eResult = commands.draw(draw.nElementCount, draw.nInstanceCount,
                                draw.nFirstElement, draw.nFirstInstance);
      }
      if (eResult != SIR_GRAPHICS_SUCCESS) {
        return eResult;
      }
    }
    eResult = commands.endRenderPass();
    if (eResult != SIR_GRAPHICS_SUCCESS) {
      return eResult;
    }
  }
  for (uint32_t i = 0; i < batch.nDispatchCount; ++i) {
    const sirModernDispatch_t &dispatch = batch.pDispatches[i];
    eResult = commands.bindPipeline(dispatch.hPipeline);
    if (eResult != SIR_GRAPHICS_SUCCESS) {
      return eResult;
    }
    for (uint32_t j = 0; j < dispatch.nBindingCount; ++j) {
      eResult = commands.bindBindingSet(dispatch.pBindings[j].nSet,
                                        dispatch.pBindings[j].hSet);
      if (eResult != SIR_GRAPHICS_SUCCESS) {
        return eResult;
      }
    }
    eResult = commands.dispatch(dispatch.nX, dispatch.nY, dispatch.nZ);
    if (eResult != SIR_GRAPHICS_SUCCESS) {
      return eResult;
    }
  }
  return commands.close();
}

sirGraphicsResult_t
SIRGraphicsModernRHI::execute(const sirModernBatch_t &batch,
                              SIRRenderingDevice &device,
                              sirGraphicsCompletion_t *pCompletion) noexcept {
  if (pCompletion == nullptr) {
    return SIR_GRAPHICS_INVALID_ARGUMENT;
  }
  *pCompletion = {};
  sirGraphicsABIPipelineModelCaps_t caps = {};
  caps.nSize = sizeof(caps);
  const sirGraphicsResult_t eQuery =
      device.queryPipelineModelCaps(SIR_GRAPHICS_PIPELINE_MODEL_MODERN, &caps);
  if (eQuery != SIR_GRAPHICS_SUCCESS) {
    return eQuery;
  }
  if (device.getDeviceGeneration() != batch.nDeviceGeneration) {
    return SIR_GRAPHICS_INVALID_HANDLE;
  }
  const sirGraphicsResult_t eAdmit = admit(batch, caps);
  if (eAdmit != SIR_GRAPHICS_SUCCESS) {
    return eAdmit;
  }
  SIRGraphicsCommandList *pCommands = nullptr;
  const sirGraphicsResult_t eAcquire =
      device.acquireCommands(batch.eQueue, &pCommands);
  if (eAcquire != SIR_GRAPHICS_SUCCESS) {
    return eAcquire;
  }
  if (pCommands == nullptr) {
    return SIR_GRAPHICS_DRIVER_ERROR;
  }
  const sirGraphicsResult_t eRecord = record(batch, *pCommands, caps);
  if (eRecord != SIR_GRAPHICS_SUCCESS) {
    return eRecord;
  }
  SIRGraphicsCommandList *szLists[] = {pCommands};
  sirGraphicsSubmission_t submission = {};
  submission.ppCommandLists = szLists;
  submission.nCommandListCount = 1;
  return device.submit(batch.eQueue, submission, pCompletion);
}
