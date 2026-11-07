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

#include <Renderer/Graph/SIRFrameGraphSubmission.h>
#include <cstdio>

static bool testQueueSchedule( void ) noexcept
{
	SIRFrameGraph graph;
	uint32_t nIndex = 0;
	sirFrameGraphResource_t resource = {};
	resource.eType = SIR_GRAPHICS_HANDLE_BUFFER;
	resource.eInitialQueue = SIR_GRAPHICS_QUEUE_DIRECT;
	resource.eInitialState = SIR_GRAPHICS_STATE_UNDEFINED;
	resource.nMipLevels = 1;
	resource.nArrayLayers = 1;
	if ( graph.addResource( resource, &nIndex ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirFrameGraphPass_t pass = {};
	pass.eQueue = SIR_GRAPHICS_QUEUE_COPY;
	if ( graph.addPass( pass, &nIndex ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	pass.eQueue = SIR_GRAPHICS_QUEUE_DIRECT;
	if ( graph.addPass( pass, &nIndex ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirFrameGraphAccess_t access = {};
	access.nResource = 0;
	access.nMipCount = 1;
	access.nLayerCount = 1;
	access.eState = SIR_GRAPHICS_STATE_COPY_DESTINATION;
	access.bWrite = true;
	if ( graph.addAccess( access ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	access.nPass = 1;
	access.bWrite = false;
	access.eState = SIR_GRAPHICS_STATE_SHADER_READ;
	if ( graph.addAccess( access ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirGraphicsCapabilities_t caps = {};
	caps.nDeviceGeneration = 1;
	caps.nQueueMask = ( 1U << SIR_GRAPHICS_QUEUE_COPY ) | ( 1U << SIR_GRAPHICS_QUEUE_DIRECT );
	caps.nFeatureMask = SIR_GRAPHICS_FEATURE_COPY_QUEUE | SIR_GRAPHICS_FEATURE_GRAPHICS;
	sirFrameGraphCompiled_t compiled = {};
	if ( graph.compile( caps, &compiled ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirFrameGraphSubmissionPlan_t plan = {};
	if ( graph.buildSubmissionPlan( compiled, &plan ) != SIR_GRAPHICS_SUCCESS || plan.nPassCount != 2 || plan.nWaitCount != 1 ||
	     plan.szWaits[0].nProducerPass != 0 || plan.szWaits[0].nConsumerPass != 1 || plan.szPasses[1].nWaitCount != 1 ||
	     plan.nBarrierCount != compiled.nTransitionCount || plan.szPasses[0].eQueue != SIR_GRAPHICS_QUEUE_COPY ||
	     plan.szPasses[1].eQueue != SIR_GRAPHICS_QUEUE_DIRECT ) {
		return false;
	}
	compiled.szDependencies[0].nBeforePass = 1;
	if ( graph.buildSubmissionPlan( compiled, &plan ) != SIR_GRAPHICS_INVALID_ARGUMENT || plan.nPassCount != 0 ) {
		return false;
	}
	if ( graph.compile( caps, &compiled ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	compiled.szTransitions[0].nAfterPass = 99;
	if ( graph.buildSubmissionPlan( compiled, &plan ) != SIR_GRAPHICS_INVALID_ARGUMENT || plan.nWaitCount != 0 ) {
		return false;
	}
	if ( graph.compile( caps, &compiled ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	compiled.nRevision++;
	return graph.buildSubmissionPlan( compiled, &plan ) == SIR_GRAPHICS_INVALID_ARGUMENT;
}

int main( void )
{
	if ( !testQueueSchedule() ) {
		std::fprintf( stderr, "FrameGraph submission schedule failed\n" );
		return 1;
	}
	std::puts( "FrameGraph submission schedule PASS" );
	return 0;
}
