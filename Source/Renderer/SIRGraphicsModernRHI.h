/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#ifndef __SIRENGINE_SIRGRAPHICSMODERNRHI_H__
#define __SIRENGINE_SIRGRAPHICSMODERNRHI_H__

#include <Renderer/SIRGraphicsDriverABI.h>
#include <Renderer/SIRRenderingDevice.h>
#include <SIRPCH.hpp>

// A borrowed, pre-cooked execution plan. Its arrays and native handles must
// remain alive until command recording finishes; submitted resources remain
// leased by the caller until their actual GPU completion is observed.
// FrameGraph supplies barriers and ordering; GRDG retains resource identity.
// There is no shader compilation, heap allocation or implicit queue emulation.
typedef struct sirModernBinding_s {
  uint32_t nSet;
  sirGraphicsHandle_t hSet;
} sirModernBinding_t;

typedef struct sirModernVertexStream_s {
  uint32_t nSlot;
  sirGraphicsHandle_t hBuffer;
  uint64_t nOffsetBytes;
  uint32_t nStrideBytes;
} sirModernVertexStream_t;

typedef struct sirModernDraw_s {
  sirGraphicsHandle_t hPipeline;
  const sirModernBinding_t *pBindings;
  const sirModernVertexStream_t *pStreams;
  uint32_t nBindingCount;
  uint32_t nStreamCount;
  sirGraphicsHandle_t hIndexBuffer;
  uint64_t nIndexOffsetBytes;
  uint32_t nIndex32;
  uint32_t nIndexed;
  uint32_t nElementCount;
  uint32_t nInstanceCount;
  uint32_t nFirstElement;
  int32_t nVertexOffset;
  uint32_t nFirstInstance;
} sirModernDraw_t;

typedef struct sirModernDispatch_s {
  sirGraphicsHandle_t hPipeline;
  const sirModernBinding_t *pBindings;
  uint32_t nBindingCount;
  uint32_t nX;
  uint32_t nY;
  uint32_t nZ;
} sirModernDispatch_t;

typedef struct sirModernBufferCopy_s {
  sirGraphicsHandle_t hDestination;
  sirGraphicsHandle_t hSource;
  uint64_t nDestinationOffset;
  uint64_t nSourceOffset;
  uint64_t nByteCount;
} sirModernBufferCopy_t;

typedef struct sirModernBatch_s {
  uint32_t nDeviceGeneration;
  sirGraphicsQueue_t eQueue;
  const sirGraphicsBarrier_t *pBarriers;
  uint32_t nBarrierCount;
  const sirGraphicsRenderPass_t *pPass;
  const sirGraphicsViewport_t *pViewport;
  const sirGraphicsRect_t *pScissor;
  const sirModernDraw_t *pDraws;
  uint32_t nDrawCount;
  const sirModernDispatch_t *pDispatches;
  uint32_t nDispatchCount;
  const sirModernBufferCopy_t *pBufferCopies;
  uint32_t nBufferCopyCount;
  const sirGraphicsBufferTextureCopy_t *pTextureCopies;
  uint32_t nTextureCopyCount;
} sirModernBatch_t;

// Stateless validation components. Execute only a fully admitted batch.
class SIRModernPipelineCompiler {
public:
  static sirGraphicsResult_t
  validate(const sirGraphicsPipelineDesc_t &pipeline,
           const sirGraphicsABIPipelineModelCaps_t &caps) noexcept;

protected:
private:
};

class SIRModernResourcePlanner {
public:
  static sirGraphicsResult_t validateHandle(sirGraphicsHandle_t hHandle,
                                            sirGraphicsHandleType_t eType,
                                            uint32_t nGeneration) noexcept;
  static sirGraphicsResult_t
  validateBarriers(const sirModernBatch_t &batch) noexcept;
  static sirGraphicsResult_t
  validateCopies(const sirModernBatch_t &batch) noexcept;

protected:
private:
};

class SIRModernBindingCompiler {
public:
  static sirGraphicsResult_t validate(const sirModernBinding_t *pBindings,
                                      uint32_t nCount,
                                      uint32_t nGeneration) noexcept;

protected:
private:
};

class SIRModernDrawCompiler {
public:
  static sirGraphicsResult_t validate(const sirModernDraw_t &draw,
                                      uint32_t nGeneration) noexcept;

protected:
private:
};

class SIRModernSynchronizationPlanner {
public:
  static sirGraphicsResult_t
  validateQueue(const sirModernBatch_t &batch,
                const sirGraphicsABIPipelineModelCaps_t &caps) noexcept;

protected:
private:
};

class SIRGraphicsModernRHI {
public:
  static sirGraphicsResult_t
  admit(const sirModernBatch_t &batch,
        const sirGraphicsABIPipelineModelCaps_t &caps) noexcept;
  static sirGraphicsResult_t
  record(const sirModernBatch_t &batch, SIRGraphicsCommandList &commands,
         const sirGraphicsABIPipelineModelCaps_t &caps) noexcept;
  static sirGraphicsResult_t
  execute(const sirModernBatch_t &batch, SIRRenderingDevice &device,
          sirGraphicsCompletion_t *pCompletion) noexcept;

protected:
private:
};

#endif // __SIRENGINE_SIRGRAPHICSMODERNRHI_H__
