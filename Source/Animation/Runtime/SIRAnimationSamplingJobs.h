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
#ifndef __SIRENGINE_SIRANIMATIONSAMPLINGJOBS_H__
#define __SIRENGINE_SIRANIMATIONSAMPLINGJOBS_H__

#include <SIRPCH.hpp>
// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include "SIRAnimationSampler.h"
#include <Core/Jobs/SIRTaskTypes.h>
#include <Core/Platform/SIRAtomic.h>

class SIRTaskScheduler;

typedef struct sirAnimationSamplingJob_s
{
	sirAnimationCookSpan_t clip;
	sirAnimationSampleOptions_t options;
	sirAnimationSampleScratch_t scratch;
	sirAnimationMutablePose_t pose;
	const SIRAtomic<uint32_t> *pCancellationGeneration;
	uint32_t nExpectedGeneration;
	sirAnimationCookResult_t eResult;
} sirAnimationSamplingJob_t;

typedef struct sirAnimationSamplingBatch_s
{
	sirAnimationSamplingJob_t *pJobs;
	sirTaskBatchTask_t *pTasks;
	sirTaskHandle_t *pHandles;
	uint32_t nJobCount;
} sirAnimationSamplingBatch_t;

class SIRAnimationSamplingJobs
{
public:
	SIRAnimationSamplingJobs( void ) noexcept = delete;

	static sirTaskResult_t execute( void *pContext, sirTaskHandle_t task ) noexcept;
	static bool submitBatch( SIRTaskScheduler& scheduler, const sirAnimationSamplingBatch_t& batch ) noexcept;

protected:
private:
};

#endif // __SIRENGINE_SIRANIMATIONSAMPLINGJOBS_H__
