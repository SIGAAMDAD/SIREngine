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

#ifndef __SIRENGINE_SIRANIMATIONPOSE_H__
#define __SIRENGINE_SIRANIMATIONPOSE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include "SIRAnimationTypes.h"

// Evaluates blended local transforms and hierarchy into a bounded model-space
// working palette, honoring required gameplay bones regardless of visual LOD.
class SIRAnimationPoseEvaluator
{
public:
	SIRAnimationPoseEvaluator( void ) noexcept = default;
	virtual ~SIRAnimationPoseEvaluator( void ) noexcept = default;

	virtual sirAnimationResult_t evaluate( const sirAnimationPoseEvaluation_t& request, const sirAnimationGraphOutput_t& graphOutput,
	                                       sirAnimationMutablePose_t *pPose ) noexcept = 0;

protected:
private:
};

// Sole animation-pose publisher. Readers acquire immutable generation-checked
// leases and release them only after every CPU/GPU use has completed. A new
// source/instance generation invalidates stale in-flight publications.
class SIRAnimationPosePublisher
{
public:
	SIRAnimationPosePublisher( void ) noexcept = default;
	virtual ~SIRAnimationPosePublisher( void ) noexcept = default;

	virtual sirAnimationResult_t getLatestPose( sirAnimationInstance_t instance, sirAnimationPoseHandle_t *pHandle ) const noexcept = 0;

	virtual sirAnimationResult_t publishPose( const sirAnimationPosePublication_t& pose, sirAnimationPoseHandle_t *pHandle ) noexcept = 0;
	virtual sirAnimationResult_t acquirePose( sirAnimationPoseHandle_t handle, sirAnimationPoseView_t *pView ) noexcept = 0;
	virtual sirAnimationResult_t releasePose( sirAnimationPoseLease_t lease ) noexcept = 0;

protected:
private:
};

// Exactly-once gameplay notify/root-motion consumer. Event and root-motion
// cursors advance at fixed gameplay ticks, independently of render cadence.
class SIRAnimationEventSink
{
public:
	SIRAnimationEventSink( void ) noexcept = default;
	virtual ~SIRAnimationEventSink( void ) noexcept = default;

	virtual sirAnimationResult_t commitEvents( const sirAnimationEventBatch_t& batch, uint64_t *pCommittedCursor ) noexcept = 0;

protected:
private:
};

#endif // __SIRENGINE_SIRANIMATIONPOSE_H__
