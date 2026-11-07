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
#include <stdio.h>

static bool expectResult(sirGraphicsResult_t observed,
                         sirGraphicsResult_t expected,
                         const char *pszName) noexcept {
  if (observed == expected) {
    return true;
  }
  fprintf(stderr, "%s expected %u, got %u\n", pszName, (uint32_t)expected,
          (uint32_t)observed);
  return false;
}

int main(void) {
  SIRFrameGraph graph;
  sirFrameGraphResource_t resource = {};
  resource.eType = SIR_GRAPHICS_HANDLE_TEXTURE;
  resource.nMipLevels = 1;
  resource.nArrayLayers = 1;
  resource.eInitialState = SIR_GRAPHICS_STATE_UNDEFINED;
  resource.eInitialQueue = SIR_GRAPHICS_QUEUE_DIRECT;
  uint32_t nResource = UINT32_MAX;
  uint32_t nPass = UINT32_MAX;
  sirFrameGraphPass_t pass = {SIR_GRAPHICS_QUEUE_DIRECT,
                              SIR_GRAPHICS_FEATURE_GRAPHICS, true};
  sirFrameGraphAccess_t access = {};
  access.nResource = 0;
  access.nPass = 0;
  access.nMipCount = 1;
  access.nLayerCount = 1;
  access.eState = SIR_GRAPHICS_STATE_COLOR_ATTACHMENT;
  access.bWrite = true;
  if (graph.addResource(resource, &nResource) != SIR_GRAPHICS_SUCCESS ||
      graph.addPass(pass, &nPass) != SIR_GRAPHICS_SUCCESS ||
      graph.addAccess(access) != SIR_GRAPHICS_SUCCESS) {
    return 1;
  }
  sirGraphicsCapabilities_t graphCaps = {};
  graphCaps.nDeviceGeneration = 7;
  graphCaps.nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
  graphCaps.nFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS;
  sirFrameGraphCompiled_t compiled = {};
  if (graph.compile(graphCaps, &compiled) != SIR_GRAPHICS_SUCCESS ||
      compiled.nPassCount != 1) {
    fprintf(stderr, "graph compile failed\n");
    return 1;
  }
  sirGraphicsABIPipelineModelCaps_t caps = {};
  caps.nSize = sizeof(caps);
  caps.nDeviceGeneration = 7;
  caps.nModel = SIR_GRAPHICS_PIPELINE_MODEL_MODERN;
  caps.nExecutionModel = SIR_GRAPHICS_EXECUTION_EXPLICIT;
  caps.nQueueMask = graphCaps.nQueueMask;
  caps.nShaderStageMask =
      SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT;
  caps.nShaderFormatMask = 1U << (SIR_GRAPHICS_SHADER_SPIRV - 1);
  caps.nRequiredFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS;
  caps.nMaximumSamples = 1;
  caps.nMaximumColorAttachments = 1;
  caps.nMaximumTextureDimension = 1024;

  sirGraphicsHandle_t handle = {3, 1, 7, SIR_GRAPHICS_HANDLE_TEXTURE};
  sirGraphicsBarrier_t barrier = {};
  const sirFrameGraphTransition_t &transition = compiled.szTransitions[0];
  barrier.hResource = handle;
  barrier.eBefore = transition.eBefore;
  barrier.eAfter = transition.eAfter;
  barrier.nBaseMip = transition.nBaseMip;
  barrier.nMipCount = transition.nMipCount;
  barrier.nBaseLayer = transition.nBaseLayer;
  barrier.nLayerCount = transition.nLayerCount;
  sirModernBatch_t batch = {};
  batch.nDeviceGeneration = 7;
  batch.eQueue = SIR_GRAPHICS_QUEUE_DIRECT;
  batch.pBarriers = &barrier;
  batch.nBarrierCount = 1;
  sirModernFramePlan_t frame = {&graph, &compiled, &batch, 1, &handle, 1};
  bool ok = expectResult(SIRModernFrameGraphExecutor::validate(frame, caps),
                         SIR_GRAPHICS_SUCCESS, "graph pass");
  batch.nBarrierCount = 0;
  ok = expectResult(SIRModernFrameGraphExecutor::validate(frame, caps),
                    SIR_GRAPHICS_INVALID_ARGUMENT, "missing transition") &&
       ok;
  batch.nBarrierCount = 1;
  barrier.eAfter = SIR_GRAPHICS_STATE_SHADER_READ;
  ok = expectResult(SIRModernFrameGraphExecutor::validate(frame, caps),
                    SIR_GRAPHICS_INVALID_ARGUMENT, "wrong transition") &&
       ok;
  barrier.eAfter = transition.eAfter;
  handle.nDeviceGeneration = 8;
  ok = expectResult(SIRModernFrameGraphExecutor::validate(frame, caps),
                    SIR_GRAPHICS_INVALID_HANDLE, "stale resource") &&
       ok;
  handle.nDeviceGeneration = 7;
  graph.reset();
  ok = expectResult(SIRModernFrameGraphExecutor::validate(frame, caps),
                    SIR_GRAPHICS_INVALID_ARGUMENT, "stale framegraph") &&
       ok;
  return ok ? 0 : 1;
}
