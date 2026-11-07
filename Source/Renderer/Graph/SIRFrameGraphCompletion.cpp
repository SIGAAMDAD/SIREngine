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

#include <Renderer/Graph/SIRFrameGraphCompletion.h>

sirGraphicsResult_t sirFrameGraphProviderCompletion( const sirFrameGraphNativeOutput_t& output,
													 const sirFrameGraphProviderLaneMap_t& mapping,
													 sirProviderCompletionSet_t *pCompletion ) noexcept
{
	if ( pCompletion == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pCompletion = {};
	if ( output.bUnknownCompletion ) {
		return SIR_GRAPHICS_BUSY;
	}
	const sirGraphicsCompletionSet_t& graphics = output.completion;
	if ( mapping.nProviderId == 0 || mapping.nProviderGeneration == 0 || mapping.nDeviceGeneration == 0 || mapping.nLaneCount == 0 ||
		 mapping.nLaneCount > SIR_PROVIDER_MAX_LANES || graphics.nDeviceGeneration != mapping.nDeviceGeneration ||
		 graphics.nQueueMask == 0 || ( graphics.nQueueMask & ~( ( 1U << SIR_GRAPHICS_QUEUE_COUNT ) - 1U ) ) != 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirProviderCompletionSet_t staged = {};
	staged.nProviderId = mapping.nProviderId;
	staged.nProviderGeneration = mapping.nProviderGeneration;
	staged.nLaneCount = mapping.nLaneCount;
	for ( uint32_t iQueue = 0; iQueue < SIR_GRAPHICS_QUEUE_COUNT; ++iQueue ) {
		const uint32_t nMask = 1U << iQueue;
		if ( ( graphics.nQueueMask & nMask ) == 0 ) {
			if ( graphics.szQueueValues[iQueue] != 0 ) {
				return SIR_GRAPHICS_INVALID_ARGUMENT;
			}
			continue;
		}
		const uint32_t iLane = mapping.szQueueToLane[iQueue];
		if ( iLane >= mapping.nLaneCount || graphics.szQueueValues[iQueue] == 0 || ( staged.nLaneMask & ( 1U << iLane ) ) != 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		staged.nLaneMask |= 1U << iLane;
		staged.szCompletedValues[iLane] = graphics.szQueueValues[iQueue];
	}
	if ( !sirProviderCompletionValid( staged ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pCompletion = staged;
	return SIR_GRAPHICS_SUCCESS;
}
