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

#ifndef __SIRENGINE_SIRANIMATIONINTEGRATION_H__
#define __SIRENGINE_SIRANIMATIONINTEGRATION_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include "SIRAnimationTypes.h"
#include <Renderer/SIRGraphicsTypes.h>

// Chooses animation-local, optional visual quality within an already admitted
// global work grant. It may not lower mandatory gameplay/physics update rates.
class SIRAnimationBudgeter
{
public:
	SIRAnimationBudgeter( void ) noexcept = default;
	virtual ~SIRAnimationBudgeter( void ) noexcept = default;

	virtual sirAnimationResult_t selectQuality( const sirAnimationCostHint_t& cost, const sirAnimationBudgetGrant_t& grant,
	                                            sirAnimationQualityDecision_t *pDecision ) noexcept = 0;

protected:
private:
};

// Renderer-owned native skinning/FrameGraph adapter. Pose source is borrowed;
// CPU, vertex and compute deformation modes have equivalent logical output.
// The renderer owns physical buffers, queues, barriers and required completion.
class SIRAnimationSkinningBridge
{
public:
	SIRAnimationSkinningBridge( void ) noexcept = default;
	virtual ~SIRAnimationSkinningBridge( void ) noexcept = default;

	virtual sirAnimationResult_t getSupportedModes( const sirGraphicsCapabilities_t& graphics,
	                                                uint32_t *pDeformationModeMask ) const noexcept = 0;

	virtual sirAnimationResult_t submitSkinning( const sirAnimationSkinRequest_t& request, sirAnimationSkinTicket_t *pTicket ) noexcept = 0;
	virtual sirAnimationResult_t retireSkinning( sirAnimationSkinTicket_t ticket,
	                                             const sirGraphicsCompletionSet_t& requiredCompletion ) noexcept = 0;

protected:
private:
};

// Physics-ABI bridge. A fixed-tick acknowledgment transfers exclusive bone
// authority. No renderer LOD adjustment can demote mandatory physical hitzones.
class SIRAnimationPhysicsBridge
{
public:
	SIRAnimationPhysicsBridge( void ) noexcept = default;
	virtual ~SIRAnimationPhysicsBridge( void ) noexcept = default;

	virtual sirAnimationResult_t getCompletedPhysicsPose( sirAnimationInstance_t instance, const sirAnimationAuthorityAck_t& authority,
	                                                      sirAnimationPhysicsPoseView_t *pView ) noexcept = 0;

	virtual sirAnimationResult_t getAuthorityStatus( sirAnimationAuthorityTicket_t ticket,
	                                                 sirAnimationAuthorityAck_t *pAcknowledgement ) noexcept = 0;

	virtual sirAnimationResult_t requestAuthority( const sirAnimationAuthorityRequest_t& request,
	                                               sirAnimationAuthorityTicket_t *pTicket ) noexcept = 0;
	virtual sirAnimationResult_t releasePhysicsPose( sirAnimationPhysicsPoseView_t *pView ) noexcept = 0;

protected:
private:
};

// Extracts immutable socket/equipment/hit-zone transforms from a completed
// pose; does not mutate collision state or own physical body handles.
class SIRAnimationAttachmentBridge
{
public:
	SIRAnimationAttachmentBridge( void ) noexcept = default;
	virtual ~SIRAnimationAttachmentBridge( void ) noexcept = default;

	virtual sirAnimationResult_t resolveAttachment( const sirAnimationPoseView_t& pose, uint64_t nSocketId,
	                                                sirAnimationTransform_t *pTransform ) const noexcept = 0;

protected:
private:
};

// World owns residency and global streaming demand. This callback only updates
// animation-local visual state and never suspends mandatory gameplay events.
class SIRAnimationResidencyBridge
{
public:
	SIRAnimationResidencyBridge( void ) noexcept = default;
	virtual ~SIRAnimationResidencyBridge( void ) noexcept = default;

	virtual sirAnimationResult_t applyResidency( const sirAnimationResidencyChange_t& change ) noexcept = 0;

protected:
private:
};

// Existing engine telemetry/profiler owns collection; this is a bounded typed
// readout of animation-specific counts, not a second global statistics store.
class SIRAnimationDiagnostics
{
public:
	SIRAnimationDiagnostics( void ) noexcept = default;
	virtual ~SIRAnimationDiagnostics( void ) noexcept = default;

	virtual sirAnimationResult_t getStats( sirAnimationStats_t *pStats ) const noexcept = 0;

protected:
private:
};

#endif // __SIRENGINE_SIRANIMATIONINTEGRATION_H__
