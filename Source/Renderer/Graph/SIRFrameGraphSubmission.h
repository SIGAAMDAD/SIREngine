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

#ifndef __SIRENGINE_SIRFRAMEGRAPHSUBMISSION_H__
#define __SIRENGINE_SIRFRAMEGRAPHSUBMISSION_H__

#include <SIRPCH.hpp>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif

#include <Renderer/Graph/SIRFrameGraph.h>

// Driver-neutral command recording plan. Native queue ownership, queue
// timeline waits, GPU barriers and completion tracking belong to the driver.
// Indices refer to the immutable compiled graph; no GPU work is submitted.
typedef struct sirFrameGraphSubmissionPass_s
{
	uint32_t nPass;
	sirGraphicsQueue_t eQueue;
	uint32_t nBarrierBegin;
	uint32_t nBarrierCount;
	uint32_t nWaitBegin;
	uint32_t nWaitCount;
} sirFrameGraphSubmissionPass_t;

typedef struct sirFrameGraphSubmissionWait_s
{
	uint32_t nProducerPass;
	uint32_t nConsumerPass;
	sirGraphicsQueue_t eProducerQueue;
	sirGraphicsQueue_t eConsumerQueue;
} sirFrameGraphSubmissionWait_t;

typedef struct sirFrameGraphSubmissionPlan_s
{
	uint32_t nPassCount;
	uint32_t nBarrierCount;
	uint32_t nWaitCount;
	sirFrameGraphSubmissionPass_t szPasses[SIR_FRAMEGRAPH_MAX_PASSES];
	uint32_t szBarrierIndices[SIR_FRAMEGRAPH_MAX_TRANSITIONS];
	sirFrameGraphSubmissionWait_t szWaits[SIR_FRAMEGRAPH_MAX_DEPENDENCIES];
} sirFrameGraphSubmissionPlan_t;

#endif // __SIRENGINE_SIRFRAMEGRAPHSUBMISSION_H__
