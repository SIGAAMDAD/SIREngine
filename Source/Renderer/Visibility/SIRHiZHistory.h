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

#ifndef __SIRENGINE_SIRHIZHISTORY_H__
#define __SIRENGINE_SIRHIZHISTORY_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRRenderingDevice.h>
#include <Renderer/Visibility/SIRHiZDispatch.h>

// One caller-owned image slot for one view. A caller maintains as many slots
// as the maximum number of simultaneously in-flight depth pyramids requires.
// A slot must remain immutable after publication. Every queue that uses its
// texture must register its completion before the slot can be retired.
// The native device owns GPU allocations; no GPU memory is owned by this POD.
typedef struct sirHiZHistoryBacking_s
{
	sirGraphicsHandle_t hPyramid;
	sirVisibilityView_t producerView;
	sirGraphicsCompletion_t producerCompletion;
	sirGraphicsCompletionSet_t lastUse;
	bool bPublished;
} sirHiZHistoryBacking_t;

// Requires a zero-initialized or fully retired backing; creates one R32F mip
// pyramid. Does not modify the backing when allocation/validation fails.
sirGraphicsResult_t sirHiZHistoryCreate( SIRRenderingDevice& device, const sirHiZDispatchPlan_t& plan, const sirVisibilityView_t& view,
                                         sirHiZHistoryBacking_t *pBacking ) noexcept;

// Called after the producer command list has been submitted and assigned an
// actual completion token. Marks the texture eligible for future frames; the
// returned completion must be waited upon by readers in their own submissions.
sirGraphicsResult_t sirHiZHistoryPublish( sirHiZHistoryBacking_t *pBacking, const sirGraphicsCompletion_t& completion ) noexcept;

// Returns false without changing outputs if the camera/device/history is
// invalid. The caller must include pWait in the GPU submission's wait set.
bool sirHiZHistoryTryRead( const sirHiZHistoryBacking_t& backing, const sirVisibilityView_t& current, sirGraphicsHandle_t *pTexture,
                           sirGraphicsCompletion_t *pWait ) noexcept;

// Record each submitted reader completion, including readers on other queues.
// A repeated token on the same queue tracks the highest completion value.
sirGraphicsResult_t sirHiZHistoryTrackRead( sirHiZHistoryBacking_t *pBacking, const sirGraphicsCompletion_t& completion ) noexcept;

// Invalidates the public texture handle immediately on SUCCESS. Native memory
// is reclaimed by the device only after all registered queue fences complete.
// BUSY and other errors preserve the backing for a subsequent retry.
sirGraphicsResult_t sirHiZHistoryRetire( SIRRenderingDevice& device, sirHiZHistoryBacking_t *pBacking ) noexcept;

// Only permitted after the previous device generation was irrecoverably lost.
// Do not attempt native retirement on a replacement device's handle table.
sirGraphicsResult_t sirHiZHistoryDiscardLost( sirHiZHistoryBacking_t *pBacking, uint32_t nReplacementGeneration ) noexcept;

#endif // __SIRENGINE_SIRHIZHISTORY_H__
