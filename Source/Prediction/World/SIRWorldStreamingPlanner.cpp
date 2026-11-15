/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#include <Prediction/World/SIRWorldStreamingPlanner.h>

#include <limits.h>

static bool sirPlannerOverlaps( const void *pFirst, uint64_t nFirstBytes, const void *pSecond, uint64_t nSecondBytes ) noexcept
{
	const uintptr_t nFirst = ( uintptr_t )pFirst;
	const uintptr_t nSecond = ( uintptr_t )pSecond;
	if ( nFirstBytes > UINTPTR_MAX - nFirst || nSecondBytes > UINTPTR_MAX - nSecond ) {
		return true;
	}
	return nFirst < nSecond + nSecondBytes && nSecond < nFirst + nFirstBytes;
}

SIRWorldStreamingPlanner::SIRWorldStreamingPlanner( void ) noexcept : m_pPartition( nullptr ), m_pReconciler( nullptr ), m_Workspace{}
{
}

bool SIRWorldStreamingPlanner::initialize( SIRWorldPartition *pPartition, SIRStreamingDemandReconciler *pReconciler,
                                           const sirWorldStreamingPlanWorkspace_t& workspace ) noexcept
{
	if ( m_pPartition != nullptr || pPartition == nullptr || pReconciler == nullptr ) {
		return false;
	}
	if ( workspace.pSources == nullptr || workspace.pDemands == nullptr || workspace.nSourceCapacity == 0 ||
	     workspace.nDemandCapacity == 0 ) {
		return false;
	}
	const uint64_t nSourcesBytes = ( uint64_t )workspace.nSourceCapacity * sizeof( sirWorldStreamingSource_t );
	const uint64_t nDemandsBytes = ( uint64_t )workspace.nDemandCapacity * sizeof( sirStreamingDemand_t );
	if ( sirPlannerOverlaps( workspace.pSources, nSourcesBytes, workspace.pDemands, nDemandsBytes ) ) {
		return false;
	}

	m_pPartition = pPartition;
	m_pReconciler = pReconciler;
	m_Workspace = workspace;
	return true;
}

void SIRWorldStreamingPlanner::shutdown( void ) noexcept
{
	m_pPartition = nullptr;
	m_pReconciler = nullptr;
	m_Workspace = {};
}

sirWorldStreamingPlanResult_t SIRWorldStreamingPlanner::update( const sirWorldStreamingPlanInput_t& input,
                                                                sirWorldStreamingPlanStats_t *pStats ) noexcept
{
	if ( m_pPartition == nullptr || m_pReconciler == nullptr || pStats == nullptr || input.nRequestEpoch == 0 ) {
		return SIR_WORLD_STREAM_PLAN_INVALID_ARGUMENT;
	}
	*pStats = {};

	uint32_t nPredicted = 0;
	pStats->ePrediction = SIRWorldTrajectoryPredictor::plan( input.pActors, input.nActors, input.pHints, input.nHints, input.prediction,
	                                                         m_Workspace.pSources, m_Workspace.nSourceCapacity, &nPredicted );
	pStats->nPredictedSources = nPredicted;
	if ( pStats->ePrediction == SIR_WORLD_PREDICTION_STALE_WORLD ) {
		return SIR_WORLD_STREAM_PLAN_STALE_WORLD;
	}
	if ( pStats->ePrediction != SIR_WORLD_PREDICTION_OK ) {
		return SIR_WORLD_STREAM_PLAN_PREDICTION_FAILED;
	}

	uint32_t nDemands = 0;
	pStats->ePartition = m_pPartition->gather( m_Workspace.pSources, nPredicted, input.nEnabledLayers, input.nRequestEpoch,
	                                           m_Workspace.pDemands, m_Workspace.nDemandCapacity, &nDemands );
	pStats->nPartitionDemands = nDemands;
	if ( pStats->ePartition != SIR_WORLD_PARTITION_OK ) {
		return SIR_WORLD_STREAM_PLAN_PARTITION_FAILED;
	}

	pStats->eReconcile = m_pReconciler->reconcile( m_Workspace.pDemands, nDemands, input.nRequestEpoch, &pStats->reconcile );
	if ( pStats->eReconcile != SIR_STREAMING_OK ) {
		return SIR_WORLD_STREAM_PLAN_COORDINATOR_FAILED;
	}
	return SIR_WORLD_STREAM_PLAN_OK;
}
