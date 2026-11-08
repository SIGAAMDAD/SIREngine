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

#ifndef __SIRENGINE_SIRANIMATIONCORE_H__
#define __SIRENGINE_SIRANIMATIONCORE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include "SIRAnimationTypes.h"
#include <Resources/SIRResourceViews.h>

class SIRResourceGraph;
class SIRAnimationClipView;

// Resolves revision-checked borrowed GRDG views. It owns neither cooked bytes
// nor the SIRResourceReadScope that keeps those bytes alive.
class SIRAnimationAssetResolver
{
public:
	SIRAnimationAssetResolver( void ) noexcept = default;
	virtual ~SIRAnimationAssetResolver( void ) noexcept = default;

	virtual sirAnimationResult_t openAsset( const SIRResourceGraph& graph, const SIRResourceReadScope& scope,
	                                        const sirAnimationAssetRef_t& asset, SIRResourceView *pView ) const noexcept = 0;

protected:
private:
};

// Only this subsystem mutates per-entity graph/clip state. A command is
// accepted against an exact state revision at a fixed-tick writer safe point.
class SIRAnimationInstanceRegistry
{
public:
	SIRAnimationInstanceRegistry( void ) noexcept = default;
	virtual ~SIRAnimationInstanceRegistry( void ) noexcept = default;

	virtual sirAnimationResult_t getState( sirAnimationInstance_t instance, sirAnimationStateSnapshot_t *pState ) const noexcept = 0;

	virtual sirAnimationResult_t createInstance( const sirAnimationInstanceDesc_t& desc, sirAnimationInstance_t *pInstance ) noexcept = 0;
	virtual sirAnimationResult_t updateState( const sirAnimationStateSnapshot_t& nextState, uint64_t nExpectedRevision ) noexcept = 0;
	virtual sirAnimationResult_t retireInstance( sirAnimationInstance_t instance, const sirAnimationTick_t& tick ) noexcept = 0;

protected:
private:
};

// Cooked graph/state-machine/blend evaluation. Neither this interface nor a
// graph instance may dispatch gameplay events twice for the same event cursor.
class SIRAnimationGraphEvaluator
{
public:
	SIRAnimationGraphEvaluator( void ) noexcept = default;
	virtual ~SIRAnimationGraphEvaluator( void ) noexcept = default;

	virtual sirAnimationResult_t evaluate( const sirAnimationGraphRequest_t& request, sirAnimationGraphScratch_t& scratch,
	                                       sirAnimationGraphOutput_t *pOutput ) noexcept = 0;

protected:
private:
};

// Scalar/SIMD clip implementation consumes existing SIRAnimationClipView and
// caller-supplied bounded transform storage. No per-sample heap allocations.
class SIRAnimationClipSampler
{
public:
	SIRAnimationClipSampler( void ) noexcept = default;
	virtual ~SIRAnimationClipSampler( void ) noexcept = default;

	virtual sirAnimationResult_t sample( const SIRAnimationClipView& clip, const sirAnimationClipSampleRequest_t& request,
	                                     sirAnimationMutablePose_t *pPose ) noexcept = 0;

protected:
private:
};

// Optional rig mapping, IK and procedural constraints operate on per-instance
// scratch only; shared skeleton hierarchy remains immutable.
class SIRAnimationRetargetSolver
{
public:
	SIRAnimationRetargetSolver( void ) noexcept = default;
	virtual ~SIRAnimationRetargetSolver( void ) noexcept = default;

	virtual sirAnimationResult_t retarget( const sirAnimationRetargetRequest_t& request, const sirAnimationMutablePose_t& sourcePose,
	                                       sirAnimationMutablePose_t *pTargetPose ) noexcept = 0;

protected:
private:
};

#endif // __SIRENGINE_SIRANIMATIONCORE_H__
