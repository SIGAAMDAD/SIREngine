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
#include <stdio.h>

static bool testProviderMapping( void ) noexcept
{
	sirFrameGraphNativeOutput_t output = {};
	output.nSubmittedPassCount = 2;
	output.completion.nDeviceGeneration = 9;
	output.completion.nQueueMask = ( 1U << SIR_GRAPHICS_QUEUE_DIRECT ) | ( 1U << SIR_GRAPHICS_QUEUE_COPY );
	output.completion.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT] = 42;
	output.completion.szQueueValues[SIR_GRAPHICS_QUEUE_COPY] = 11;
	sirFrameGraphProviderLaneMap_t mapping = {};
	mapping.nProviderId = 17;
	mapping.nProviderGeneration = 4;
	mapping.nDeviceGeneration = 9;
	mapping.nLaneCount = 4;
	mapping.szQueueToLane[SIR_GRAPHICS_QUEUE_DIRECT] = 2;
	mapping.szQueueToLane[SIR_GRAPHICS_QUEUE_COPY] = 0;
	sirProviderCompletionSet_t completion = {};
	if ( sirFrameGraphProviderCompletion( output, mapping, &completion ) != SIR_GRAPHICS_SUCCESS || completion.nProviderId != 17 ||
	     completion.nProviderGeneration != 4 || completion.nLaneMask != 5 || completion.szCompletedValues[2] != 42 ||
	     completion.szCompletedValues[0] != 11 || !sirProviderCompletionValid( completion ) ) {
		return false;
	}
	const sirProviderCompletionSet_t reference = completion;
	mapping.szQueueToLane[SIR_GRAPHICS_QUEUE_COPY] = 2;
	if ( sirFrameGraphProviderCompletion( output, mapping, &completion ) != SIR_GRAPHICS_INVALID_ARGUMENT || completion.nLaneMask != 0 ) {
		return false;
	}
	mapping.szQueueToLane[SIR_GRAPHICS_QUEUE_COPY] = 0;
	mapping.nDeviceGeneration++;
	if ( sirFrameGraphProviderCompletion( output, mapping, &completion ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	mapping.nDeviceGeneration--;
	output.bUnknownCompletion = true;
	if ( sirFrameGraphProviderCompletion( output, mapping, &completion ) != SIR_GRAPHICS_BUSY || completion.nProviderId != 0 ) {
		return false;
	}
	output.bUnknownCompletion = false;
	output.completion.szQueueValues[SIR_GRAPHICS_QUEUE_COPY] = 0;
	if ( sirFrameGraphProviderCompletion( output, mapping, &completion ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	output.completion.szQueueValues[SIR_GRAPHICS_QUEUE_COPY] = 11;
	output.completion.nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	if ( sirFrameGraphProviderCompletion( output, mapping, &completion ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	output.completion.szQueueValues[SIR_GRAPHICS_QUEUE_COPY] = 0;
	if ( sirFrameGraphProviderCompletion( output, mapping, &completion ) != SIR_GRAPHICS_SUCCESS || completion.nLaneMask != ( 1U << 2 ) ||
	     !sirProviderCompletionSatisfied( completion, reference ) || sirProviderCompletionSatisfied( reference, completion ) ) {
		return false;
	}
	return true;
}

int main( void )
{
	if ( !testProviderMapping() ) {
		fputs( "FrameGraph provider completion mapping FAILED\n", stderr );
		return 1;
	}
	puts( "FrameGraph provider completion mapping PASS" );
	return 0;
}
