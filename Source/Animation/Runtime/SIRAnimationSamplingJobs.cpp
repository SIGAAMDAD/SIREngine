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
#include "SIRAnimationSamplingJobs.h"
#include <Core/Jobs/SIRTaskScheduler.h>

sirTaskResult_t SIRAnimationSamplingJobs::execute( void *pContext, sirTaskHandle_t ) noexcept
{
	sirAnimationSamplingJob_t *pJob = ( sirAnimationSamplingJob_t * )pContext;
	if ( !pJob || !pJob->pCancellationGeneration ||
	     pJob->pCancellationGeneration->load( SIR_MEMORY_ORDER_ACQUIRE ) != pJob->nExpectedGeneration ) {
		if ( pJob ) {
			pJob->eResult = SIR_ANIM_COOK_MISSING;
		}
		return SIR_TASK_RESULT_FAILURE;
	}
	pJob->eResult = SIRAnimationSampler::sample( pJob->clip, pJob->options, &pJob->scratch, &pJob->pose );
	// Callers publish no pose while tasks are pending. A cancellation that
	// races execution must fail this task rather than publish stale output.
	if ( pJob->pCancellationGeneration->load( SIR_MEMORY_ORDER_ACQUIRE ) != pJob->nExpectedGeneration ) {
		pJob->eResult = SIR_ANIM_COOK_MISSING;
		return SIR_TASK_RESULT_FAILURE;
	}
	return pJob->eResult == SIR_ANIM_COOK_OK ? SIR_TASK_RESULT_SUCCESS : SIR_TASK_RESULT_FAILURE;
}

bool SIRAnimationSamplingJobs::submitBatch( SIRTaskScheduler& scheduler, const sirAnimationSamplingBatch_t& batch ) noexcept
{
	if ( !batch.pJobs || !batch.pTasks || !batch.pHandles || !batch.nJobCount || batch.nJobCount > 4096 ) {
		return false;
	}
	for ( uint32_t i = 0; i < batch.nJobCount; ++i ) {
		const sirAnimationSamplingJob_t& job = batch.pJobs[i];
		if ( !job.pCancellationGeneration || !job.scratch.pKeys || !job.pose.pLocalTransforms ) {
			return false;
		}
	}
	for ( uint32_t i = 0; i < batch.nJobCount; ++i ) {
		batch.pTasks[i] = { &SIRAnimationSamplingJobs::execute, &batch.pJobs[i], SIR_TASK_PRIORITY_NORMAL, SIR_TASK_EXEC_CPU };
	}
	return scheduler.submitBatch( { batch.pTasks, batch.nJobCount, nullptr, 0 }, batch.pHandles );
}
