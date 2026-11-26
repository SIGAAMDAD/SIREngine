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

#ifndef __SIR_TIMELINECOORDINATOR_H__
#define __SIR_TIMELINECOORDINATOR_H__
#include <Core/SIRPCH.h>
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif
#include <atomic>

typedef struct sirTimelineConfig_s
{
	uint64_t nFixedStepNs;
	uint32_t nMaxFixedStepsPerFrame;
	// Zero disables presentation backpressure for headless/server operation.
	uint32_t nMaxFramesAhead;
} sirTimelineConfig_t;

typedef struct sirTimelineSnapshot_s
{
	uint64_t nFrame;
	uint64_t nSimulationTick;
	uint64_t nPublicationRevision;
	uint64_t nFrameTimeNs;
	uint64_t nDroppedTimeNs;
	double fInterpolationAlpha;
} sirTimelineSnapshot_t;

// One simulation owner; the presentation thread may only acknowledge frames.
// Clock domains are explicit. No platform waits or global frame barriers.
class SIRTimelineCoordinator
{
public:
	SIRTimelineCoordinator( void ) noexcept;
	SIRTimelineCoordinator( const SIRTimelineCoordinator& ) = delete;
	SIRTimelineCoordinator& operator=( const SIRTimelineCoordinator& ) = delete;

	bool init( const sirTimelineConfig_t& config ) noexcept;
	bool beginFrame( uint64_t nFrameTimeNs, uint64_t nDeltaNs ) noexcept;
	bool canAdvanceFixed( void ) const noexcept;
	bool completeFixedStep( void ) noexcept;
	bool publishFrame( void ) noexcept;
	void cancelFrame( void ) noexcept;
	bool acknowledgePresentedFrame( uint64_t nFrame ) noexcept;
	bool getSnapshot( sirTimelineSnapshot_t *pSnapshot ) const noexcept;
	uint64_t getFrame( void ) const noexcept;
	uint64_t getSimulationTick( void ) const noexcept;
	bool isFrameOpen( void ) const noexcept;

private:
	std::atomic<uint64_t> m_nPublishedFrame;
	std::atomic<uint64_t> m_nPresentedFrame;
	sirTimelineConfig_t m_Config;
	uint64_t m_nTick;
	uint64_t m_nRevision;
	uint64_t m_nAccumulatorNs;
	uint64_t m_nFrameTimeNs;
	uint64_t m_nDroppedTimeNs;
	uint64_t m_nPendingFrameDeltaNs;
	uint64_t m_nPreviousAccumulatorNs;
	uint64_t m_nPreviousDroppedNs;
	uint64_t m_nPreviousFrameTimeNs;
	uint32_t m_nStepsThisFrame;
	bool m_bInitialized;
	bool m_bFrameOpen;
};
#endif // __SIR_TIMELINECOORDINATOR_H__
