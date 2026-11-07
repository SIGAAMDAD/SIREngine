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

#ifndef __SIRENGINE_SIRFRAMEGRAPHCOMPLETION_H__
#define __SIRENGINE_SIRFRAMEGRAPHCOMPLETION_H__

#include <SIRPCH.hpp>

#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif

#include <Renderer/Graph/SIRFrameGraphNative.h>
#include <Resources/SIRProviderCompletion.h>

// Provider lanes are independent identities: the caller supplies the
// exact queue-to-lane mapping, never an inferred shared scalar fence.
typedef struct sirFrameGraphProviderLaneMap_s
{
	uint32_t nProviderId;
	uint32_t nProviderGeneration;
	uint32_t nDeviceGeneration;
	uint32_t nLaneCount;
	uint32_t szQueueToLane[SIR_GRAPHICS_QUEUE_COUNT];
} sirFrameGraphProviderLaneMap_t;

// Pure translation of an already-submitted graphics completion set. Unknown
// submission outcomes must never produce an apparently complete retirement.
sirGraphicsResult_t sirFrameGraphProviderCompletion( const sirFrameGraphNativeOutput_t& output,
													 const sirFrameGraphProviderLaneMap_t& mapping,
													 sirProviderCompletionSet_t *pCompletion ) noexcept;

#endif // __SIRENGINE_SIRFRAMEGRAPHCOMPLETION_H__
