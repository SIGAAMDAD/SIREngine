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

#include <Renderer/SIRModernFrameGraphExecution.h>

sirGraphicsResult_t SIRModernCompletionObserver::observe(
    const sirModernCompletionQuery_t &query, SIRRenderingDevice &device,
    sirProviderCompletionSet_t *pObserved) noexcept {
  if (pObserved == nullptr) {
    return SIR_GRAPHICS_INVALID_ARGUMENT;
  }
  *pObserved = {};
  if (query.pTokens == nullptr || query.nTokenCount == 0 ||
      query.nProviderId == 0 || query.nProviderGeneration == 0 ||
      query.nLaneCount == 0 || query.nLaneCount > SIR_PROVIDER_MAX_LANES) {
    return SIR_GRAPHICS_INVALID_ARGUMENT;
  }
  sirProviderCompletionSet_t staged = {};
  // Provider completion lanes have independent monotonic value domains.
  // Never coalesce values from different native queues into one lane.
  uint32_t szLaneOwner[SIR_PROVIDER_MAX_LANES] = {};
  for (uint32_t i = 0; i < SIR_PROVIDER_MAX_LANES; ++i) {
    szLaneOwner[i] = SIR_GRAPHICS_QUEUE_COUNT;
  }
  staged.nProviderId = query.nProviderId;
  staged.nProviderGeneration = query.nProviderGeneration;
  staged.nLaneCount = query.nLaneCount;
  for (uint32_t i = 0; i < query.nTokenCount; ++i) {
    const sirGraphicsCompletion_t &token = query.pTokens[i];
    if (token.eQueue >= SIR_GRAPHICS_QUEUE_COUNT || token.nValue == 0 ||
        token.nDeviceGeneration == 0 ||
        token.nDeviceGeneration != device.getDeviceGeneration() ||
        query.szQueueToProviderLane[token.eQueue] >= query.nLaneCount) {
      return SIR_GRAPHICS_INVALID_HANDLE;
    }
    const uint32_t nLane = query.szQueueToProviderLane[token.eQueue];
    if (szLaneOwner[nLane] != SIR_GRAPHICS_QUEUE_COUNT &&
        szLaneOwner[nLane] != token.eQueue) {
      return SIR_GRAPHICS_INVALID_ARGUMENT;
    }
    szLaneOwner[nLane] = token.eQueue;
    bool bCompleted = false;
    const sirGraphicsResult_t ePoll = device.poll(token, &bCompleted);
    if (ePoll != SIR_GRAPHICS_SUCCESS) {
      return ePoll;
    }
    if (!bCompleted) {
      return SIR_GRAPHICS_BUSY;
    }
    staged.nLaneMask |= 1U << nLane;
    if (staged.szCompletedValues[nLane] < token.nValue) {
      staged.szCompletedValues[nLane] = token.nValue;
    }
  }
  if (!sirProviderCompletionValid(staged)) {
    return SIR_GRAPHICS_INVALID_ARGUMENT;
  }
  *pObserved = staged;
  return SIR_GRAPHICS_SUCCESS;
}

bool SIRModernFrameGraphExecutor::matchesTransition(
    const sirGraphicsBarrier_t &barrier,
    const sirFrameGraphTransition_t &transition,
    sirGraphicsHandle_t hResource) noexcept {
  return barrier.hResource.nSlot == hResource.nSlot &&
         barrier.hResource.nGeneration == hResource.nGeneration &&
         barrier.hResource.nDeviceGeneration == hResource.nDeviceGeneration &&
         barrier.hResource.eType == hResource.eType &&
         barrier.eBefore == transition.eBefore &&
         barrier.eAfter == transition.eAfter &&
         barrier.nBaseMip == transition.nBaseMip &&
         barrier.nMipCount == transition.nMipCount &&
         barrier.nBaseLayer == transition.nBaseLayer &&
         barrier.nLayerCount == transition.nLayerCount;
}

// Preflight all live passes with the actual FrameGraph owner's validation
// before allowing any GPU side effect. The callback stages no work.
typedef struct sirModernFrameValidation_s {
  const sirModernFramePlan_t *pPlan;
  const sirGraphicsABIPipelineModelCaps_t *pCaps;
} sirModernFrameValidation_t;

static sirGraphicsResult_t sirModernValidateGraphPass(uint32_t nPass,
                                                      void *pContext) noexcept {
  const sirModernFrameValidation_t &context =
      *static_cast<sirModernFrameValidation_t *>(pContext);
  const sirModernFramePlan_t &plan = *context.pPlan;
  const sirModernBatch_t &batch = plan.pBatches[nPass];
  const sirGraphicsResult_t eAdmit =
      SIRGraphicsModernRHI::admit(batch, *context.pCaps);
  if (eAdmit != SIR_GRAPHICS_SUCCESS) {
    return eAdmit;
  }
  const sirFrameGraphCompiled_t &graph = *plan.pCompiled;
  for (uint32_t i = 0; i < graph.nTransitionCount; ++i) {
    const sirFrameGraphTransition_t &transition = graph.szTransitions[i];
    if (transition.nAfterPass != nPass) {
      continue;
    }
    if (transition.nResource >= plan.nResourceCount) {
      return SIR_GRAPHICS_INVALID_ARGUMENT;
    }
    // The current binary barrier record does not carry source/destination
    // queue family ownership. A wait alone cannot safely implement a Vulkan
    // resource queue-family transfer. Reject instead of submitting unsafe GPU
    // work; an owner-approved ABI contract is required to enable this path.
    if (transition.eFromQueue != transition.eToQueue) {
      return SIR_GRAPHICS_UNSUPPORTED;
    }
    const sirGraphicsHandle_t hResource =
        plan.pResourceHandles[transition.nResource];
    if (SIRModernResourcePlanner::validateHandle(hResource, hResource.eType,
                                                 batch.nDeviceGeneration) !=
        SIR_GRAPHICS_SUCCESS) {
      return SIR_GRAPHICS_INVALID_HANDLE;
    }
    bool bMatched = false;
    for (uint32_t j = 0; j < batch.nBarrierCount; ++j) {
      bMatched = bMatched || SIRModernFrameGraphExecutor::matchesTransition(
                                 batch.pBarriers[j], transition, hResource);
    }
    if (!bMatched) {
      return SIR_GRAPHICS_INVALID_ARGUMENT;
    }
  }
  return SIR_GRAPHICS_SUCCESS;
}

static void sirModernRollbackGraphPass(uint32_t, void *) noexcept {
  // CPU preflight is pure: there is no staged resource or GPU work to undo.
}

sirGraphicsResult_t SIRModernFrameGraphExecutor::validate(
    const sirModernFramePlan_t &plan,
    const sirGraphicsABIPipelineModelCaps_t &caps) noexcept {
  if (plan.pGraph == nullptr || plan.pCompiled == nullptr ||
      plan.pBatches == nullptr ||
      plan.nBatchCount != plan.pCompiled->nOriginalPassCount ||
      plan.nBatchCount > SIR_FRAMEGRAPH_MAX_PASSES ||
      plan.nResourceCount != plan.pGraph->getResourceCount() ||
      (plan.nResourceCount != 0 && plan.pResourceHandles == nullptr) ||
      plan.pCompiled->nTransitionCount > SIR_FRAMEGRAPH_MAX_TRANSITIONS ||
      plan.pCompiled->nDependencyCount > SIR_FRAMEGRAPH_MAX_DEPENDENCIES) {
    return SIR_GRAPHICS_INVALID_ARGUMENT;
  }
  if (caps.nDeviceGeneration == 0 ||
      caps.nModel != SIR_GRAPHICS_PIPELINE_MODEL_MODERN) {
    return SIR_GRAPHICS_UNSUPPORTED;
  }
  // FrameGraph::record checks authoritative owner identity, revision,
  // compiled pass order and dependency DAG without GPU submission.
  sirModernFrameValidation_t context = {&plan, &caps};
  sirFrameGraphCallbacks_t callbacks = {&sirModernValidateGraphPass,
                                        &sirModernRollbackGraphPass, &context};
  return plan.pGraph->record(*plan.pCompiled, callbacks);
}

sirGraphicsResult_t
SIRModernFrameGraphExecutor::execute(const sirModernFramePlan_t &plan,
                                     SIRRenderingDevice &device,
                                     sirModernFrameResult_t *pResult) noexcept {
  if (pResult == nullptr) {
    return SIR_GRAPHICS_INVALID_ARGUMENT;
  }
  pResult->nSubmittedCount = 0;
  pResult->nFailedPass = UINT32_MAX;
  if (plan.pCompiled == nullptr || pResult->pCompletions == nullptr ||
      pResult->nCompletionCapacity < plan.pCompiled->nOriginalPassCount) {
    return SIR_GRAPHICS_INVALID_ARGUMENT;
  }
  for (uint32_t i = 0; i < plan.pCompiled->nOriginalPassCount; ++i) {
    pResult->pCompletions[i] = {};
  }
  sirGraphicsABIPipelineModelCaps_t caps = {};
  caps.nSize = sizeof(caps);
  const sirGraphicsResult_t eQuery =
      device.queryPipelineModelCaps(SIR_GRAPHICS_PIPELINE_MODEL_MODERN, &caps);
  if (eQuery != SIR_GRAPHICS_SUCCESS) {
    return eQuery;
  }
  if (device.getDeviceGeneration() != caps.nDeviceGeneration) {
    return SIR_GRAPHICS_INVALID_HANDLE;
  }
  const sirGraphicsResult_t eValidate = validate(plan, caps);
  if (eValidate != SIR_GRAPHICS_SUCCESS) {
    return eValidate;
  }
  const sirFrameGraphCompiled_t &graph = *plan.pCompiled;
  for (uint32_t iOrder = 0; iOrder < graph.nPassCount; ++iOrder) {
    const uint32_t nPass = graph.szPassOrder[iOrder];
    const sirModernBatch_t &batch = plan.pBatches[nPass];
    pResult->nFailedPass = nPass;
    SIRGraphicsCommandList *pCommands = nullptr;
    const sirGraphicsResult_t eAcquire =
        device.acquireCommands(batch.eQueue, &pCommands);
    if (eAcquire != SIR_GRAPHICS_SUCCESS) {
      return eAcquire;
    }
    if (pCommands == nullptr) {
      return SIR_GRAPHICS_DRIVER_ERROR;
    }
    const sirGraphicsResult_t eRecord =
        SIRGraphicsModernRHI::record(batch, *pCommands, caps);
    if (eRecord != SIR_GRAPHICS_SUCCESS) {
      return eRecord;
    }
    sirGraphicsCompletion_t szWaits[SIR_FRAMEGRAPH_MAX_PASSES] = {};
    uint32_t nWaitCount = 0;
    for (uint32_t i = 0; i < graph.nDependencyCount; ++i) {
      const sirFrameGraphDependency_t &dep = graph.szDependencies[i];
      if (dep.nAfterPass != nPass || !dep.bCrossQueue) {
        continue;
      }
      const sirGraphicsCompletion_t &earlier =
          pResult->pCompletions[dep.nBeforePass];
      if (earlier.nDeviceGeneration != batch.nDeviceGeneration ||
          earlier.nValue == 0) {
        return SIR_GRAPHICS_INVALID_HANDLE;
      }
      if (nWaitCount == SIR_FRAMEGRAPH_MAX_PASSES) {
        return SIR_GRAPHICS_CAPACITY_EXCEEDED;
      }
      szWaits[nWaitCount++] = earlier;
    }
    SIRGraphicsCommandList *pLists[] = {pCommands};
    sirGraphicsSubmission_t submission = {};
    submission.ppCommandLists = pLists;
    submission.nCommandListCount = 1;
    submission.pWaits = szWaits;
    submission.nWaitCount = nWaitCount;
    sirGraphicsCompletion_t completion = {};
    const sirGraphicsResult_t eSubmit =
        device.submit(batch.eQueue, submission, &completion);
    if (eSubmit != SIR_GRAPHICS_SUCCESS) {
      return eSubmit;
    }
    if (completion.nDeviceGeneration != batch.nDeviceGeneration ||
        completion.eQueue != batch.eQueue || completion.nValue == 0) {
      return SIR_GRAPHICS_DRIVER_ERROR;
    }
    pResult->pCompletions[nPass] = completion;
    ++pResult->nSubmittedCount;
  }
  pResult->nFailedPass = UINT32_MAX;
  return SIR_GRAPHICS_SUCCESS;
}
