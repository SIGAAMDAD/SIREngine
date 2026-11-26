/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Core/Scheduling/SIRTimelineCoordinator.h>
#include <limits.h>

SIRTimelineCoordinator::SIRTimelineCoordinator( void ) noexcept
    : m_nPublishedFrame( 0 ), m_nPresentedFrame( 0 ), m_Config{}, m_nTick( 0 ), m_nRevision( 0 ), m_nAccumulatorNs( 0 ),
      m_nFrameTimeNs( 0 ), m_nDroppedTimeNs( 0 ), m_nPendingFrameDeltaNs( 0 ), m_nPreviousAccumulatorNs( 0 ), m_nPreviousDroppedNs( 0 ),
      m_nPreviousFrameTimeNs( 0 ), m_nStepsThisFrame( 0 ), m_bInitialized( false ), m_bFrameOpen( false )
{
}

bool SIRTimelineCoordinator::init( const sirTimelineConfig_t& config ) noexcept
{
	if ( m_bInitialized || config.nFixedStepNs == 0 || config.nMaxFixedStepsPerFrame == 0 ) {
		return false;
	}
	if ( config.nFixedStepNs > UINT64_MAX / config.nMaxFixedStepsPerFrame ) {
		return false;
	}

	m_Config = config;
	m_bInitialized = true;
	return true;
}

bool SIRTimelineCoordinator::beginFrame( uint64_t nFrameTimeNs, uint64_t nDeltaNs ) noexcept
{
	if ( !m_bInitialized || m_bFrameOpen || nFrameTimeNs < m_nFrameTimeNs ) {
		return false;
	}
	const uint64_t nPresented = m_nPresentedFrame.load( std::memory_order_acquire );
	const uint64_t nPublished = m_nPublishedFrame.load( std::memory_order_acquire );
	if ( m_Config.nMaxFramesAhead != 0 && nPublished - nPresented >= m_Config.nMaxFramesAhead ) {
		return false;
	}
	if ( nPublished == UINT64_MAX || m_nRevision == UINT64_MAX ) {
		return false;
	}

	const uint64_t nLimitNs = m_Config.nFixedStepNs * m_Config.nMaxFixedStepsPerFrame;
	const uint64_t nAvailableNs = nLimitNs - m_nAccumulatorNs;
	const uint64_t nAcceptedNs = nDeltaNs < nAvailableNs ? nDeltaNs : nAvailableNs;
	const uint64_t nDroppedNs = nDeltaNs - nAcceptedNs;
	if ( m_nDroppedTimeNs > UINT64_MAX - nDroppedNs ) {
		return false;
	}

	m_nPreviousAccumulatorNs = m_nAccumulatorNs;
	m_nPreviousDroppedNs = m_nDroppedTimeNs;
	m_nPreviousFrameTimeNs = m_nFrameTimeNs;
	m_nAccumulatorNs += nAcceptedNs;
	m_nPendingFrameDeltaNs = nAcceptedNs;
	m_nDroppedTimeNs += nDroppedNs;
	m_nFrameTimeNs = nFrameTimeNs;
	m_nStepsThisFrame = 0;
	m_bFrameOpen = true;
	return true;
}

bool SIRTimelineCoordinator::canAdvanceFixed( void ) const noexcept
{
	return m_bInitialized && m_nAccumulatorNs >= m_Config.nFixedStepNs && m_nStepsThisFrame < m_Config.nMaxFixedStepsPerFrame &&
	       m_nTick != UINT64_MAX;
}

bool SIRTimelineCoordinator::completeFixedStep( void ) noexcept
{
	if ( !canAdvanceFixed() ) {
		return false;
	}

	m_nAccumulatorNs -= m_Config.nFixedStepNs;
	++m_nTick;
	++m_nStepsThisFrame;
	return true;
}

bool SIRTimelineCoordinator::publishFrame( void ) noexcept
{
	if ( !m_bFrameOpen ) {
		return false;
	}

	++m_nRevision;
	m_nPublishedFrame.fetch_add( 1, std::memory_order_release );
	m_nPendingFrameDeltaNs = 0;
	m_bFrameOpen = false;
	return true;
}

void SIRTimelineCoordinator::cancelFrame( void ) noexcept
{
	if ( !m_bFrameOpen ) {
		return;
	}
	// Only restore the increment when no completed fixed step consumed it.
	if ( m_nStepsThisFrame == 0 ) {
		m_nAccumulatorNs = m_nPreviousAccumulatorNs;
		m_nDroppedTimeNs = m_nPreviousDroppedNs;
		m_nFrameTimeNs = m_nPreviousFrameTimeNs;
	}
	m_nPendingFrameDeltaNs = 0;
	m_bFrameOpen = false;
}

bool SIRTimelineCoordinator::acknowledgePresentedFrame( uint64_t nFrame ) noexcept
{
	if ( nFrame > m_nPublishedFrame.load( std::memory_order_acquire ) ) {
		return false;
	}
	uint64_t nPrior = m_nPresentedFrame.load( std::memory_order_relaxed );
	while ( nPrior < nFrame ) {
		if ( m_nPresentedFrame.compare_exchange_weak( nPrior, nFrame, std::memory_order_release, std::memory_order_relaxed ) ) {
			return true;
		}
	}
	return nFrame == nPrior;
}

bool SIRTimelineCoordinator::getSnapshot( sirTimelineSnapshot_t *pSnapshot ) const noexcept
{
	if ( pSnapshot == nullptr || !m_bInitialized ) {
		return false;
	}
	pSnapshot->nFrame = m_nPublishedFrame.load( std::memory_order_acquire );
	pSnapshot->nSimulationTick = m_nTick;
	pSnapshot->nPublicationRevision = m_nRevision;
	pSnapshot->nFrameTimeNs = m_nFrameTimeNs;
	pSnapshot->nDroppedTimeNs = m_nDroppedTimeNs;
	pSnapshot->fInterpolationAlpha = static_cast<double>(m_nAccumulatorNs) / static_cast<double>(m_Config.nFixedStepNs);
	return true;
}

uint64_t SIRTimelineCoordinator::getFrame( void ) const noexcept
{
	return m_nPublishedFrame.load( std::memory_order_acquire );
}

uint64_t SIRTimelineCoordinator::getSimulationTick( void ) const noexcept
{
	return m_nTick;
}

bool SIRTimelineCoordinator::isFrameOpen( void ) const noexcept
{
	return m_bFrameOpen;
}
