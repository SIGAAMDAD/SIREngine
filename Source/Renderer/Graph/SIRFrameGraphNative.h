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

#ifndef __SIR_FRAMEGRAPHNATIVE_H__
#define __SIR_FRAMEGRAPHNATIVE_H__

#include <Core/SIRPCH.h>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif

#include <Renderer/Graph/SIRFrameGraphSubmission.h>
#include <Renderer/SIRGraphicsDriverABI.h>
#include <Renderer/SIRRenderingDevice.h>

// Called once after native barriers have been recorded. The command context
// is borrowed from the driver for this pass only; this callback must not submit.
typedef SIRGraphicsResult ( *sirFrameGraphNativeRecordFn_t )( uint32_t nPass, sirGraphicsABICommandList_t *pCommands,
																void *pContext ) noexcept;

typedef struct sirFrameGraphNativeInput_s
{
	const sirGraphicsABIDevice_t *pDevice;
	const sirGraphicsHandle_t *pResources;
	uint32_t nResourceCount;
	sirFrameGraphNativeRecordFn_t pRecord;
	void *pContext;
} sirFrameGraphNativeInput_t;

// On any failure following a successful submit, retains every completed
// queue token. The caller must wait for these before retiring resources.
typedef struct sirFrameGraphNativeOutput_s
{
	sirGraphicsCompletionSet_t completion;
	uint32_t nSubmittedPassCount;
	bool bUnknownCompletion;
} sirFrameGraphNativeOutput_t;

// The C++ rendering-device adapter uses the same FrameGraph native lowering
// path as binary drivers; no second queue scheduler or barrier planner.
typedef SIRGraphicsResult ( *sirFrameGraphDeviceRecordFn_t )( uint32_t nPass, SIRGraphicsCommandList *pCommands,
																void *pContext ) noexcept;

typedef struct sirFrameGraphDeviceInput_s
{
	SIRRenderingDevice *pDevice;
	const sirGraphicsHandle_t *pResources;
	uint32_t nResourceCount;
	sirFrameGraphDeviceRecordFn_t pRecord;
	void *pContext;
} sirFrameGraphDeviceInput_t;

#endif // __SIR_FRAMEGRAPHNATIVE_H__
