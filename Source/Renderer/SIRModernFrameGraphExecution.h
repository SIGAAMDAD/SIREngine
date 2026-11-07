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

#ifndef __SIRENGINE_SIRMODERNFRAMEGRAPHEXECUTION_H__
#define __SIRENGINE_SIRMODERNFRAMEGRAPHEXECUTION_H__

#include <SIRPCH.hpp>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#include <Renderer/Graph/SIRFrameGraph.h>
#include <Renderer/SIRGraphicsModernRHI.h>
#include <Resources/SIRProviderCompletion.h>

// Borrowed, immutable graph-bound execution plan. Batches are addressed by
// ORIGINAL FrameGraph pass index, including slots for culled passes. The
// resource map translates FrameGraph resource indices into provider handles;
// it does not own GRDG identities, residency leases or native allocations.
typedef struct sirModernFramePlan_s {
  const SIRFrameGraph *pGraph;
  const sirFrameGraphCompiled_t *pCompiled;
  const sirModernBatch_t *pBatches;
  uint32_t nBatchCount;
  const sirGraphicsHandle_t *pResourceHandles;
  uint32_t nResourceCount;
} sirModernFramePlan_t;

// The caller retains all resource leases until every returned, genuinely
// completed queue token is observed. Earlier submitted passes remain in
// flight if a later pass fails. No rollback of GPU submissions is implied.
typedef struct sirModernFrameResult_s {
  sirGraphicsCompletion_t *pCompletions; // indexed by original pass ID
  uint32_t nCompletionCapacity;
  uint32_t nSubmittedCount;
  uint32_t nFailedPass; // UINT32_MAX on success
} sirModernFrameResult_t;

// Device-completion observation only: the GRDG provider retains authority to
// publish the returned completion snapshot. Lane mapping belongs to that
// provider and is supplied explicitly by its owner.
typedef struct sirModernCompletionQuery_s {
  const sirGraphicsCompletion_t *pTokens;
  uint32_t nTokenCount;
  uint32_t nProviderId;
  uint32_t nProviderGeneration;
  uint32_t nLaneCount;
  uint32_t szQueueToProviderLane[SIR_GRAPHICS_QUEUE_COUNT];
} sirModernCompletionQuery_t;

class SIRModernCompletionObserver {
public:
  static sirGraphicsResult_t
  observe(const sirModernCompletionQuery_t &query, SIRRenderingDevice &device,
          sirProviderCompletionSet_t *pObserved) noexcept;

protected:
private:
};

class SIRModernFrameGraphExecutor {
public:
  static sirGraphicsResult_t
  validate(const sirModernFramePlan_t &plan,
           const sirGraphicsABIPipelineModelCaps_t &caps) noexcept;
  static sirGraphicsResult_t execute(const sirModernFramePlan_t &plan,
                                     SIRRenderingDevice &device,
                                     sirModernFrameResult_t *pResult) noexcept;
  static bool matchesTransition(const sirGraphicsBarrier_t &barrier,
                                const sirFrameGraphTransition_t &transition,
                                sirGraphicsHandle_t hResource) noexcept;

protected:
private:
};

#endif // __SIRENGINE_SIRMODERNFRAMEGRAPHEXECUTION_H__
