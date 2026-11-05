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

#include <Renderer/Graph/SIRFrameGraph.h>

#include <stdio.h>

static sirGraphicsCapabilities_t makeCaps( void ) noexcept
{
	sirGraphicsCapabilities_t caps = {};
	caps.nDeviceGeneration = 2;
	caps.nQueueMask = ( 1U << SIR_GRAPHICS_QUEUE_DIRECT ) | ( 1U << SIR_GRAPHICS_QUEUE_COMPUTE );
	caps.nFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_COMPUTE | SIR_GRAPHICS_FEATURE_ASYNC_COMPUTE;
	return caps;
}

static sirFrameGraphResource_t makeTransient( uint32_t nMips = 1 ) noexcept
{
	sirFrameGraphResource_t resource = {};
	resource.eType = SIR_GRAPHICS_HANDLE_TEXTURE;
	resource.nMipLevels = nMips;
	resource.nArrayLayers = 1;
	resource.eInitialState = SIR_GRAPHICS_STATE_UNDEFINED;
	resource.eInitialQueue = SIR_GRAPHICS_QUEUE_DIRECT;
	return resource;
}

static sirFrameGraphAccess_t makeAccess( uint32_t nPass, uint32_t nResource, sirGraphicsState_t eState, bool bWrite ) noexcept
{
	sirFrameGraphAccess_t access = {};
	access.nPass = nPass;
	access.nResource = nResource;
	access.nMipCount = 1;
	access.nLayerCount = 1;
	access.eState = eState;
	access.bWrite = bWrite;
	return access;
}

static bool testHazardsAndQueueOwnership( void ) noexcept
{
	SIRFrameGraph graph;
	uint32_t nResource = 0;
	uint32_t nPass = 0;
	const sirFrameGraphPass_t direct = { SIR_GRAPHICS_QUEUE_DIRECT, SIR_GRAPHICS_FEATURE_GRAPHICS, false };
	const sirFrameGraphPass_t compute = { SIR_GRAPHICS_QUEUE_COMPUTE, SIR_GRAPHICS_FEATURE_COMPUTE, false };
	if ( graph.addResource( makeTransient(), &nResource ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addPass( direct, &nPass ) != SIR_GRAPHICS_SUCCESS || graph.addPass( compute, &nPass ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addPass( direct, &nPass ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	if ( graph.addAccess( makeAccess( 0, nResource, SIR_GRAPHICS_STATE_COLOR_ATTACHMENT, true ) ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addAccess( makeAccess( 1, nResource, SIR_GRAPHICS_STATE_SHADER_READ, false ) ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addAccess( makeAccess( 2, nResource, SIR_GRAPHICS_STATE_SHADER_READ, false ) ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}

	sirFrameGraphCompiled_t compiled = {};
	const sirGraphicsCapabilities_t caps = makeCaps();
	if ( graph.compile( caps, &compiled ) != SIR_GRAPHICS_SUCCESS || compiled.nPassCount != 3 || compiled.nDependencyCount != 3 ||
	     compiled.nTransitionCount != 3 ) {
		return false;
	}
	if ( compiled.szFirstUse[nResource] != 0 || compiled.szLastUse[nResource] != 2 || compiled.szPassOrder[0] != 0 ||
	     compiled.szPassOrder[1] != 1 || compiled.szPassOrder[2] != 2 ) {
		return false;
	}
	if ( !compiled.szDependencies[0].bCrossQueue || compiled.szTransitions[1].eFromQueue != SIR_GRAPHICS_QUEUE_DIRECT ||
	     compiled.szTransitions[1].eToQueue != SIR_GRAPHICS_QUEUE_COMPUTE ) {
		return false;
	}
	const bool bStable = graph.compile( caps, &compiled ) == SIR_GRAPHICS_SUCCESS && compiled.nDependencyCount == 3;
	graph.reset();
	return bStable && graph.getPassCount() == 0 && graph.getResourceCount() == 0;
}

static bool testTransientAndCycleRejection( void ) noexcept
{
	SIRFrameGraph graph;
	uint32_t nResource = 0;
	uint32_t nPass = 0;
	const sirFrameGraphPass_t pass = { SIR_GRAPHICS_QUEUE_DIRECT, 0, false };
	if ( graph.addResource( makeTransient(), &nResource ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addPass( pass, &nPass ) != SIR_GRAPHICS_SUCCESS || graph.addPass( pass, &nPass ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	if ( graph.addAccess( makeAccess( 0, nResource, SIR_GRAPHICS_STATE_SHADER_READ, false ) ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirFrameGraphCompiled_t compiled = {};
	if ( graph.compile( makeCaps(), &compiled ) != SIR_GRAPHICS_INVALID_ARGUMENT || compiled.nPassCount != 0 ) {
		return false;
	}

	graph.reset();
	graph.addResource( makeTransient(), &nResource );
	graph.addPass( pass, &nPass );
	graph.addPass( pass, &nPass );
	if ( graph.addAccess( makeAccess( 0, nResource, SIR_GRAPHICS_STATE_COLOR_ATTACHMENT, true ) ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addAccess( makeAccess( 1, nResource, SIR_GRAPHICS_STATE_SHADER_READ, false ) ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addDependency( 1, 0 ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	return graph.compile( makeCaps(), &compiled ) == SIR_GRAPHICS_INVALID_ARGUMENT;
}

static bool testImportsAndInvalidRanges( void ) noexcept
{
	SIRFrameGraph graph;
	uint32_t nResource = 0;
	uint32_t nPass = 0;
	sirFrameGraphResource_t imported = makeTransient( 2 );
	imported.bImported = true;
	imported.hImported.eType = SIR_GRAPHICS_HANDLE_TEXTURE;
	imported.hImported.nGeneration = 3;
	imported.hImported.nDeviceGeneration = 2;
	imported.eInitialState = SIR_GRAPHICS_STATE_SHADER_READ;
	const sirFrameGraphPass_t pass = { SIR_GRAPHICS_QUEUE_DIRECT, 0, false };
	if ( graph.addResource( imported, &nResource ) != SIR_GRAPHICS_SUCCESS || graph.addPass( pass, &nPass ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	const sirFrameGraphAccess_t read = makeAccess( nPass, nResource, SIR_GRAPHICS_STATE_SHADER_READ, false );
	if ( graph.addAccess( read ) != SIR_GRAPHICS_SUCCESS || graph.addAccess( read ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	if ( graph.addAccess( makeAccess( nPass, nResource, SIR_GRAPHICS_STATE_SHADER_WRITE, true ) ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	sirFrameGraphAccess_t invalid = read;
	invalid.nBaseMip = 2;
	if ( graph.addAccess( invalid ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	sirFrameGraphCompiled_t compiled = {};
	if ( graph.compile( makeCaps(), &compiled ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirGraphicsCapabilities_t caps = makeCaps();
	caps.nDeviceGeneration = 9;
	return graph.compile( caps, &compiled ) == SIR_GRAPHICS_INVALID_HANDLE;
}

static bool testUnsupportedQueuesAndPartialRanges( void ) noexcept
{
	SIRFrameGraph graph;
	uint32_t nResource = 0;
	uint32_t nPass = 0;
	const sirFrameGraphPass_t direct = { SIR_GRAPHICS_QUEUE_DIRECT, 0, false };
	const sirFrameGraphPass_t compute = { SIR_GRAPHICS_QUEUE_COMPUTE, 0, false };
	graph.addResource( makeTransient( 2 ), &nResource );
	graph.addPass( direct, &nPass );
	graph.addPass( compute, &nPass );
	sirFrameGraphAccess_t wide = makeAccess( 0, nResource, SIR_GRAPHICS_STATE_COLOR_ATTACHMENT, true );
	wide.nMipCount = 2;
	if ( graph.addAccess( wide ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirFrameGraphAccess_t narrow = makeAccess( 1, nResource, SIR_GRAPHICS_STATE_SHADER_READ, false );
	if ( graph.addAccess( narrow ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirFrameGraphCompiled_t compiled = {};
	sirGraphicsCapabilities_t caps = makeCaps();
	caps.nFeatureMask &= ~SIR_GRAPHICS_FEATURE_ASYNC_COMPUTE;
	if ( graph.compile( caps, &compiled ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return false;
	}
	caps = makeCaps();
	if ( graph.compile( caps, &compiled ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	graph.reset();
	graph.addResource( makeTransient( 2 ), &nResource );
	graph.addPass( direct, &nPass );
	graph.addPass( compute, &nPass );
	graph.addAccess( makeAccess( 0, nResource, SIR_GRAPHICS_STATE_COLOR_ATTACHMENT, true ) );
	narrow = makeAccess( 1, nResource, SIR_GRAPHICS_STATE_SHADER_READ, false );
	narrow.nMipCount = 2;
	graph.addAccess( narrow );
	return graph.compile( caps, &compiled ) == SIR_GRAPHICS_UNSUPPORTED;
}

int main( void )
{
	const bool bPass = testHazardsAndQueueOwnership() && testTransientAndCycleRejection() && testImportsAndInvalidRanges() &&
	                   testUnsupportedQueuesAndPartialRanges();
	if ( !bPass ) {
		fprintf( stderr, "FrameGraph graph compiler regression failed\n" );
		return 1;
	}
	fprintf( stdout, "FrameGraph graph compiler regression passed\n" );
	return 0;
}
