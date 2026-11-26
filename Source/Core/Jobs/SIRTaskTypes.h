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

#ifndef __SIR_TASKTYPES_H__
#define __SIR_TASKTYPES_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

typedef struct sirTaskHandle_s
{
	uint32_t nSlot;
	uint32_t nGeneration;
} sirTaskHandle_t;

enum class SIRTaskExecutor : uint8_t {
	SIR_TASK_EXEC_CPU = 0,
	SIR_TASK_EXEC_MAIN = 1,
	SIR_TASK_EXEC_RENDER = 2,
	SIR_TASK_EXEC_PHYSICS = 3,
	SIR_TASK_EXEC_AUDIO = 4,
	SIR_TASK_EXEC_COUNT = 5
};

enum class SIRTaskPriority : uint8_t {
	SIR_TASK_PRIORITY_CRITICAL = 0,
	SIR_TASK_PRIORITY_NORMAL = 1,
	SIR_TASK_PRIORITY_BACKGROUND = 2
};

enum class SIRTaskState : uint8_t {
	SIR_TASK_INVALID = 0,
	SIR_TASK_QUEUED = 1,
	SIR_TASK_RUNNING = 2,
	SIR_TASK_COMPLETED = 3,
	SIR_TASK_FAILED = 4,
	SIR_TASK_CANCELLED = 5,
	SIR_TASK_BLOCKED = 6,
	SIR_TASK_AWAITING_PROVIDER = 7
};

enum class SIRTaskResult : uint8_t {
	SIR_TASK_RESULT_SUCCESS = 0,
	SIR_TASK_RESULT_FAILURE = 1,
	SIR_TASK_RESULT_PENDING = 2
};

// Provider tasks (render/physics/audio) may return PENDING after scheduling
// asynchronous work, then acknowledge the completion at a provider safe point.
// Never captures heap-managed callable state. The caller owns pContext until
// the task reaches a terminal state and its handle is released.
typedef SIRTaskResult ( *sirTaskFunction_t )( void *pContext, sirTaskHandle_t task ) noexcept;

// Batch indices refer to task positions in the submitted array, not handles.
// Every dependency must point from a lower index to a higher index.
typedef struct sirTaskBatchTask_s
{
	sirTaskFunction_t pFunction;
	void *pContext;
	SIRTaskPriority ePriority;
	SIRTaskExecutor eExecutor;
} sirTaskBatchTask_t;

typedef struct sirTaskBatchEdge_s
{
	uint32_t nBefore;
	uint32_t nAfter;
} sirTaskBatchEdge_t;

typedef struct sirTaskBatchDesc_s
{
	const sirTaskBatchTask_t *pTasks;
	uint32_t nTasks;
	const sirTaskBatchEdge_t *pEdges;
	uint32_t nEdges;
} sirTaskBatchDesc_t;

#endif // __SIR_TASKTYPES_H__
