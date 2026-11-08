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

#ifndef __SIRENGINE_SIRANIMATIONSYSTEM_H__
#define __SIRENGINE_SIRANIMATIONSYSTEM_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include "SIRAnimationCore.h"
#include "SIRAnimationPose.h"
#include "SIRAnimationIntegration.h"
#include <Core/Jobs/SIRTaskTypes.h>

class SIRMemoryManager;
class SIRResourceGraph;
class SIRTaskScheduler;
class SIRTaskGraph;

// These pointers are borrowed engine dependencies, not subsystem-owned
// schedulers, resource catalogs, native devices or memory allocators.
typedef struct sirAnimationSystemConfig_s
{
	SIRMemoryManager *pMemory;
	SIRResourceGraph *pResources;
	SIRTaskScheduler *pTasks;
	SIRTaskGraph *pTaskGraph;
	SIRAnimationEventSink *pEvents;
	SIRAnimationSkinningBridge *pSkinning;
	SIRAnimationPhysicsBridge *pPhysics;
	SIRAnimationResidencyBridge *pResidency;
	SIRAnimationAttachmentBridge *pAttachments;
	uint32_t nMaximumInstances;
	uint32_t nMaximumBonesPerInstance;
	uint32_t nMaximumPublishedPoseSlots;
	uint64_t nCpuScratchBudgetBytes;
} sirAnimationSystemConfig_t;

typedef struct sirAnimationTickRequest_s
{
	sirAnimationTick_t tick;
	sirAnimationBudgetGrant_t admittedBudget;
	uint32_t nMaximumScheduledInstances;
	uint32_t nMandatoryPhysicsBoneCount;
} sirAnimationTickRequest_t;

// An accepted tick schedules work through existing Core Jobs. The task handle
// proves only CPU/provider task completion, not native graphics fence retirement.
typedef struct sirAnimationTickReceipt_s
{
	sirTaskHandle_t completionTask;
	sirAnimationTick_t tick;
	uint32_t nScheduledInstances;
	uint32_t nRejectedInstances;
} sirAnimationTickReceipt_t;

// Public subsystem entry point. Concrete service implementation owns the
// private SoA registry and pose buffers; interfaces above describe cross-owner
// seams and may be provided by separate animation subsystem implementations.
class SIRAnimationSystem
{
public:
	SIRAnimationSystem( void ) noexcept = default;
	virtual ~SIRAnimationSystem( void ) noexcept = default;

	virtual sirAnimationResult_t getPosePublisher( SIRAnimationPosePublisher **ppPublisher ) noexcept = 0;
	virtual sirAnimationResult_t getInstanceRegistry( SIRAnimationInstanceRegistry **ppRegistry ) noexcept = 0;

	virtual sirAnimationResult_t initialize( const sirAnimationSystemConfig_t& config ) noexcept = 0;
	virtual sirAnimationResult_t scheduleTick( const sirAnimationTickRequest_t& request, sirAnimationTickReceipt_t *pReceipt ) noexcept = 0;
	virtual sirAnimationResult_t cancelWorldEpoch( uint64_t nWorldEpoch, uint32_t nCancellationGeneration ) noexcept = 0;
	virtual sirAnimationResult_t shutdown( void ) noexcept = 0;

protected:
private:
};

#endif // __SIRENGINE_SIRANIMATIONSYSTEM_H__
