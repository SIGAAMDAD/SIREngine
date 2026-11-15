/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#ifndef __SIRENGINE_SIRWORLDSTREAMINGPLANNER_H__
#define __SIRENGINE_SIRWORLDSTREAMINGPLANNER_H__

#include <Prediction/World/SIRWorldTrajectoryPredictor.h>
#include <Streaming/SIRStreamingDemandReconciler.h>

typedef enum sirWorldStreamingPlanResult_e : uint8_t
{
	SIR_WORLD_STREAM_PLAN_OK = 0,
	SIR_WORLD_STREAM_PLAN_INVALID_ARGUMENT,
	SIR_WORLD_STREAM_PLAN_STALE_WORLD,
	SIR_WORLD_STREAM_PLAN_PREDICTION_FAILED,
	SIR_WORLD_STREAM_PLAN_PARTITION_FAILED,
	SIR_WORLD_STREAM_PLAN_COORDINATOR_FAILED
} sirWorldStreamingPlanResult_t;

typedef struct sirWorldStreamingPlanWorkspace_s
{
	sirWorldStreamingSource_t *pSources;
	sirStreamingDemand_t *pDemands;
	uint32_t nSourceCapacity;
	uint32_t nDemandCapacity;
} sirWorldStreamingPlanWorkspace_t;

typedef struct sirWorldStreamingPlanInput_s
{
	const sirWorldTrajectorySource_t *pActors;
	const sirWorldCoAccessHint_t *pHints;
	sirWorldPredictionConfig_t prediction;
	uint64_t nEnabledLayers;
	uint32_t nActors;
	uint32_t nHints;
	uint32_t nRequestEpoch;
} sirWorldStreamingPlanInput_t;

typedef struct sirWorldStreamingPlanStats_s
{
	uint32_t nPredictedSources;
	uint32_t nPartitionDemands;
	sirWorldPredictionResult_t ePrediction;
	sirWorldPartitionResult_t ePartition;
	sirStreamingResult_t eReconcile;
	sirStreamingReconcileStats_t reconcile;
} sirWorldStreamingPlanStats_t;

// Owner-thread composition of predictor -> world partition -> the existing
// canonical reconciler. It owns no asset cache or second scheduling queue.
// No partial demand set reaches the reconciler on prediction/capacity failure.
class SIRWorldStreamingPlanner final
{
private:
	SIRWorldPartition *m_pPartition;
	SIRStreamingDemandReconciler *m_pReconciler;
	sirWorldStreamingPlanWorkspace_t m_Workspace;

public:
	SIRWorldStreamingPlanner( void ) noexcept;
	bool initialize( SIRWorldPartition *pPartition, SIRStreamingDemandReconciler *pReconciler,
	                 const sirWorldStreamingPlanWorkspace_t& workspace ) noexcept;
	void shutdown( void ) noexcept;
	sirWorldStreamingPlanResult_t update( const sirWorldStreamingPlanInput_t& input, sirWorldStreamingPlanStats_t *pStats ) noexcept;
};

#endif // __SIRENGINE_SIRWORLDSTREAMINGPLANNER_H__
