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
#include <new>

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
	uint32_t nDuplicate = 0;
	if ( graph.addResource( imported, &nDuplicate ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
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
	return graph.compile( caps, &compiled ) == SIR_GRAPHICS_INVALID_ARGUMENT;
}

static bool testDisjointWriteUnion( void ) noexcept
{
	SIRFrameGraph graph;
	uint32_t nResource = 0;
	uint32_t nPass = 0;
	const sirFrameGraphPass_t pass = { SIR_GRAPHICS_QUEUE_DIRECT, 0, false };
	if ( graph.addResource( makeTransient( 2 ), &nResource ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	for ( uint32_t iPass = 0; iPass != 3; ++iPass ) {
		if ( graph.addPass( pass, &nPass ) != SIR_GRAPHICS_SUCCESS ) {
			return false;
		}
	}
	sirFrameGraphAccess_t writeFirst = makeAccess( 0, nResource, SIR_GRAPHICS_STATE_COLOR_ATTACHMENT, true );
	sirFrameGraphAccess_t writeSecond = makeAccess( 1, nResource, SIR_GRAPHICS_STATE_COLOR_ATTACHMENT, true );
	writeSecond.nBaseMip = 1;
	sirFrameGraphAccess_t readBoth = makeAccess( 2, nResource, SIR_GRAPHICS_STATE_SHADER_READ, false );
	readBoth.nMipCount = 2;
	if ( graph.addAccess( writeFirst ) != SIR_GRAPHICS_SUCCESS || graph.addAccess( writeSecond ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addAccess( readBoth ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirFrameGraphCompiled_t compiled = {};
	if ( graph.compile( makeCaps(), &compiled ) != SIR_GRAPHICS_SUCCESS || compiled.nDependencyCount != 2 ||
	     compiled.nTransitionCount != 4 || compiled.szFirstUse[nResource] != 0 || compiled.szLastUse[nResource] != 2 ) {
		return false;
	}
	return compiled.szTransitions[2].nBeforePass == 0 && compiled.szTransitions[3].nBeforePass == 1 &&
	       compiled.szTransitions[2].nBaseMip == 0 && compiled.szTransitions[3].nBaseMip == 1;
}

static bool testImportedMixedState( void ) noexcept
{
	SIRFrameGraph graph;
	uint32_t nResource = 0;
	uint32_t nPass = 0;
	sirFrameGraphResource_t imported = makeTransient( 2 );
	imported.bImported = true;
	imported.eInitialState = SIR_GRAPHICS_STATE_SHADER_READ;
	imported.hImported.eType = SIR_GRAPHICS_HANDLE_TEXTURE;
	imported.hImported.nGeneration = 4;
	imported.hImported.nDeviceGeneration = 2;
	const sirFrameGraphPass_t compute = { SIR_GRAPHICS_QUEUE_COMPUTE, 0, false };
	const sirFrameGraphPass_t direct = { SIR_GRAPHICS_QUEUE_DIRECT, 0, false };
	if ( graph.addResource( imported, &nResource ) != SIR_GRAPHICS_SUCCESS || graph.addPass( compute, &nPass ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addPass( direct, &nPass ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirFrameGraphAccess_t writeMip = makeAccess( 0, nResource, SIR_GRAPHICS_STATE_SHADER_WRITE, true );
	sirFrameGraphAccess_t readBoth = makeAccess( 1, nResource, SIR_GRAPHICS_STATE_SHADER_READ, false );
	readBoth.nMipCount = 2;
	if ( graph.addAccess( writeMip ) != SIR_GRAPHICS_SUCCESS || graph.addAccess( readBoth ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirFrameGraphCompiled_t compiled = {};
	if ( graph.compile( makeCaps(), &compiled ) != SIR_GRAPHICS_SUCCESS || compiled.nTransitionCount != 2 ||
	     compiled.nDependencyCount != 1 ) {
		return false;
	}
	return compiled.szTransitions[1].nBaseMip == 0 && compiled.szTransitions[1].nMipCount == 1 &&
	       compiled.szTransitions[1].eFromQueue == SIR_GRAPHICS_QUEUE_COMPUTE &&
	       compiled.szTransitions[1].eToQueue == SIR_GRAPHICS_QUEUE_DIRECT;
}

static bool testWARAndWAW( void ) noexcept
{
	SIRFrameGraph graph;
	uint32_t nResource = 0;
	uint32_t nPass = 0;
	sirFrameGraphResource_t imported = makeTransient();
	imported.eType = SIR_GRAPHICS_HANDLE_BUFFER;
	imported.bImported = true;
	imported.eInitialState = SIR_GRAPHICS_STATE_SHADER_READ;
	imported.hImported.eType = SIR_GRAPHICS_HANDLE_BUFFER;
	imported.hImported.nGeneration = 2;
	imported.hImported.nDeviceGeneration = 2;
	const sirFrameGraphPass_t direct = { SIR_GRAPHICS_QUEUE_DIRECT, 0, false };
	const sirFrameGraphPass_t compute = { SIR_GRAPHICS_QUEUE_COMPUTE, 0, false };
	if ( graph.addResource( imported, &nResource ) != SIR_GRAPHICS_SUCCESS || graph.addPass( direct, &nPass ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addPass( compute, &nPass ) != SIR_GRAPHICS_SUCCESS || graph.addPass( direct, &nPass ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	if ( graph.addAccess( makeAccess( 0, nResource, SIR_GRAPHICS_STATE_SHADER_READ, false ) ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addAccess( makeAccess( 1, nResource, SIR_GRAPHICS_STATE_SHADER_WRITE, true ) ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addAccess( makeAccess( 2, nResource, SIR_GRAPHICS_STATE_COPY_DESTINATION, true ) ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirFrameGraphCompiled_t compiled = {};
	return graph.compile( makeCaps(), &compiled ) == SIR_GRAPHICS_SUCCESS && compiled.nDependencyCount == 3 &&
	       compiled.nTransitionCount == 2 && compiled.szTransitions[0].bWriteHazard && compiled.szTransitions[1].bWriteHazard;
}

static bool testQueueLegalityAndSparseRanges( void ) noexcept
{
	SIRFrameGraph graph;
	uint32_t nResource = 0;
	uint32_t nPass = 0;
	sirFrameGraphResource_t sparse = makeTransient( UINT32_MAX );
	sparse.nArrayLayers = UINT32_MAX;
	if ( graph.addResource( sparse, &nResource ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	const sirFrameGraphPass_t compute = { SIR_GRAPHICS_QUEUE_COMPUTE, 0, false };
	const sirFrameGraphPass_t copy = { SIR_GRAPHICS_QUEUE_COPY, 0, false };
	if ( graph.addPass( compute, &nPass ) != SIR_GRAPHICS_SUCCESS || graph.addPass( copy, &nPass ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirFrameGraphAccess_t invalidCompute = makeAccess( 0, nResource, SIR_GRAPHICS_STATE_COLOR_ATTACHMENT, true );
	sirFrameGraphAccess_t invalidCopy = makeAccess( 1, nResource, SIR_GRAPHICS_STATE_SHADER_READ, false );
	if ( graph.addAccess( invalidCompute ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     graph.addAccess( invalidCopy ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	sirFrameGraphAccess_t far = makeAccess( 0, nResource, SIR_GRAPHICS_STATE_SHADER_WRITE, true );
	far.nBaseMip = UINT32_MAX - 1;
	far.nBaseLayer = UINT32_MAX - 1;
	if ( graph.addAccess( far ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirFrameGraphCompiled_t compiled = {};
	return graph.compile( makeCaps(), &compiled ) == SIR_GRAPHICS_UNSUPPORTED;
}

static bool testSamePassDisjointAndAtomicFailure( void ) noexcept
{
	SIRFrameGraph graph;
	uint32_t nResource = 0;
	uint32_t nPass = 0;
	const sirFrameGraphPass_t direct = { SIR_GRAPHICS_QUEUE_DIRECT, 0, false };
	if ( graph.addResource( makeTransient( 2 ), &nResource ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addPass( direct, &nPass ) != SIR_GRAPHICS_SUCCESS || graph.addPass( direct, &nPass ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirFrameGraphAccess_t first = makeAccess( 0, nResource, SIR_GRAPHICS_STATE_COLOR_ATTACHMENT, true );
	sirFrameGraphAccess_t second = first;
	second.nBaseMip = 1;
	sirFrameGraphAccess_t readBoth = makeAccess( 1, nResource, SIR_GRAPHICS_STATE_SHADER_READ, false );
	readBoth.nMipCount = 2;
	if ( graph.addAccess( first ) != SIR_GRAPHICS_SUCCESS || graph.addAccess( second ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addAccess( readBoth ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirFrameGraphCompiled_t compiled = {};
	if ( graph.compile( makeCaps(), &compiled ) != SIR_GRAPHICS_SUCCESS || compiled.nDependencyCount != 1 ) {
		return false;
	}
	graph.reset();
	graph.addResource( makeTransient( 2 ), &nResource );
	graph.addPass( direct, &nPass );
	graph.addPass( direct, &nPass );
	graph.addAccess( first );
	graph.addAccess( readBoth );
	return graph.compile( makeCaps(), &compiled ) == SIR_GRAPHICS_INVALID_ARGUMENT && compiled.nPassCount == 0 &&
	       compiled.nDependencyCount == 0 && compiled.nTransitionCount == 0;
}

static bool testLayerWriteUnion( void ) noexcept
{
	SIRFrameGraph graph;
	uint32_t nResource = 0;
	uint32_t nPass = 0;
	sirFrameGraphResource_t layered = makeTransient();
	layered.nArrayLayers = 2;
	const sirFrameGraphPass_t pass = { SIR_GRAPHICS_QUEUE_DIRECT, 0, false };
	if ( graph.addResource( layered, &nResource ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	for ( uint32_t iPass = 0; iPass != 3; ++iPass ) {
		if ( graph.addPass( pass, &nPass ) != SIR_GRAPHICS_SUCCESS ) {
			return false;
		}
	}
	sirFrameGraphAccess_t first = makeAccess( 0, nResource, SIR_GRAPHICS_STATE_COLOR_ATTACHMENT, true );
	sirFrameGraphAccess_t second = makeAccess( 1, nResource, SIR_GRAPHICS_STATE_COLOR_ATTACHMENT, true );
	second.nBaseLayer = 1;
	sirFrameGraphAccess_t read = makeAccess( 2, nResource, SIR_GRAPHICS_STATE_SHADER_READ, false );
	read.nLayerCount = 2;
	if ( graph.addAccess( first ) != SIR_GRAPHICS_SUCCESS || graph.addAccess( second ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addAccess( read ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirFrameGraphCompiled_t compiled = {};
	return graph.compile( makeCaps(), &compiled ) == SIR_GRAPHICS_SUCCESS && compiled.nDependencyCount == 2 &&
	       compiled.nTransitionCount == 4 && compiled.szTransitions[2].nBaseLayer == 0 && compiled.szTransitions[3].nBaseLayer == 1;
}

static bool testBoundedRegionWork( void ) noexcept
{
	SIRFrameGraph graph;
	uint32_t nResource = 0;
	uint32_t nPass = 0;
	sirFrameGraphResource_t imported = makeTransient( 32 );
	imported.nArrayLayers = 32;
	imported.bImported = true;
	imported.eInitialState = SIR_GRAPHICS_STATE_SHADER_READ;
	imported.hImported.eType = SIR_GRAPHICS_HANDLE_TEXTURE;
	imported.hImported.nGeneration = 1;
	imported.hImported.nDeviceGeneration = 2;
	const sirFrameGraphPass_t pass = { SIR_GRAPHICS_QUEUE_DIRECT, 0, false };
	if ( graph.addResource( imported, &nResource ) != SIR_GRAPHICS_SUCCESS || graph.addPass( pass, &nPass ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addPass( pass, &nPass ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	for ( uint32_t iIndex = 0; iIndex < 32; ++iIndex ) {
		sirFrameGraphAccess_t readMip = makeAccess( 0, nResource, SIR_GRAPHICS_STATE_SHADER_READ, false );
		readMip.nBaseMip = iIndex;
		readMip.nLayerCount = 32;
		if ( graph.addAccess( readMip ) != SIR_GRAPHICS_SUCCESS ) {
			return false;
		}
	}
	for ( uint32_t iIndex = 0; iIndex < 32; ++iIndex ) {
		sirFrameGraphAccess_t readLayer = makeAccess( 0, nResource, SIR_GRAPHICS_STATE_SHADER_READ, false );
		readLayer.nBaseLayer = iIndex;
		readLayer.nMipCount = 32;
		if ( graph.addAccess( readLayer ) != SIR_GRAPHICS_SUCCESS ) {
			return false;
		}
	}
	for ( uint32_t iIndex = 0; iIndex < 9; ++iIndex ) {
		sirFrameGraphAccess_t readWide = makeAccess( 1, nResource, SIR_GRAPHICS_STATE_SHADER_READ, false );
		readWide.nMipCount = 32;
		readWide.nLayerCount = 32;
		if ( graph.addAccess( readWide ) != SIR_GRAPHICS_SUCCESS ) {
			return false;
		}
	}
	sirFrameGraphCompiled_t compiled = {};
	return graph.compile( makeCaps(), &compiled ) == SIR_GRAPHICS_CAPACITY_EXCEEDED && compiled.nTransitionCount == 0 &&
	       compiled.nPassCount == 0;
}

static bool testOutputCullingAndOverwrite( void ) noexcept
{
	SIRFrameGraph graph;
	uint32_t nResource = 0;
	uint32_t nPass = 0;
	const sirFrameGraphPass_t pass = { SIR_GRAPHICS_QUEUE_DIRECT, 0, false };
	graph.setCullingEnabled( true );
	if ( graph.addResource( makeTransient(), &nResource ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	for ( uint32_t iPass = 0; iPass != 4; ++iPass ) {
		if ( graph.addPass( pass, &nPass ) != SIR_GRAPHICS_SUCCESS ) {
			return false;
		}
	}
	if ( graph.addAccess( makeAccess( 0, nResource, SIR_GRAPHICS_STATE_COLOR_ATTACHMENT, true ) ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addAccess( makeAccess( 1, nResource, SIR_GRAPHICS_STATE_COLOR_ATTACHMENT, true ) ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addAccess( makeAccess( 2, nResource, SIR_GRAPHICS_STATE_SHADER_READ, false ) ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	const sirFrameGraphOutput_t output = { nResource, 0, 1, 0, 1 };
	if ( graph.addOutput( output ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirFrameGraphCompiled_t compiled = {};
	if ( graph.compile( makeCaps(), &compiled ) != SIR_GRAPHICS_SUCCESS || compiled.nPassCount != 1 || compiled.szPassOrder[0] != 1 ||
	     compiled.szLivePasses[0] != 0 || compiled.szLivePasses[2] != 0 || compiled.szLivePasses[3] != 0 ||
	     compiled.nTransitionCount != 1 ) {
		return false;
	}
	return compiled.szFirstUse[nResource] == 0 && compiled.szLastUse[nResource] == 0;
}

static bool testLiveReadDependencies( void ) noexcept
{
	SIRFrameGraph graph;
	graph.setCullingEnabled( true );
	uint32_t nFirst = 0;
	uint32_t nSecond = 0;
	uint32_t nPass = 0;
	const sirFrameGraphPass_t pass = { SIR_GRAPHICS_QUEUE_DIRECT, 0, false };
	const sirFrameGraphPass_t sideEffect = { SIR_GRAPHICS_QUEUE_DIRECT, 0, true };
	if ( graph.addResource( makeTransient(), &nFirst ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addResource( makeTransient(), &nSecond ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	if ( graph.addPass( pass, &nPass ) != SIR_GRAPHICS_SUCCESS || graph.addPass( pass, &nPass ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addPass( sideEffect, &nPass ) != SIR_GRAPHICS_SUCCESS || graph.addPass( pass, &nPass ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	if ( graph.addAccess( makeAccess( 0, nFirst, SIR_GRAPHICS_STATE_COLOR_ATTACHMENT, true ) ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addAccess( makeAccess( 1, nSecond, SIR_GRAPHICS_STATE_COLOR_ATTACHMENT, true ) ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addAccess( makeAccess( 2, nFirst, SIR_GRAPHICS_STATE_SHADER_READ, false ) ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addAccess( makeAccess( 3, nSecond, SIR_GRAPHICS_STATE_SHADER_READ, false ) ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirFrameGraphCompiled_t compiled = {};
	if ( graph.compile( makeCaps(), &compiled ) != SIR_GRAPHICS_SUCCESS || compiled.nPassCount != 2 || compiled.szPassOrder[0] != 0 ||
	     compiled.szPassOrder[1] != 2 || compiled.nDependencyCount != 1 ) {
		return false;
	}
	return compiled.szFirstUse[nSecond] == SIR_FRAMEGRAPH_INVALID_INDEX && compiled.szLastUse[nSecond] == SIR_FRAMEGRAPH_INVALID_INDEX;
}

static bool testSubresourceOutputAndExplicitDependency( void ) noexcept
{
	SIRFrameGraph graph;
	graph.setCullingEnabled( true );
	uint32_t nResource = 0;
	uint32_t nPass = 0;
	const sirFrameGraphPass_t pass = { SIR_GRAPHICS_QUEUE_DIRECT, 0, false };
	if ( graph.addResource( makeTransient( 2 ), &nResource ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addPass( pass, &nPass ) != SIR_GRAPHICS_SUCCESS || graph.addPass( pass, &nPass ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addPass( pass, &nPass ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirFrameGraphAccess_t a = makeAccess( 0, nResource, SIR_GRAPHICS_STATE_COLOR_ATTACHMENT, true );
	sirFrameGraphAccess_t b = makeAccess( 1, nResource, SIR_GRAPHICS_STATE_COLOR_ATTACHMENT, true );
	b.nBaseMip = 1;
	if ( graph.addAccess( a ) != SIR_GRAPHICS_SUCCESS || graph.addAccess( b ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addDependency( 2, 1 ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	const sirFrameGraphOutput_t output = { nResource, 1, 1, 0, 1 };
	if ( graph.addOutput( output ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirFrameGraphCompiled_t compiled = {};
	if ( graph.compile( makeCaps(), &compiled ) != SIR_GRAPHICS_SUCCESS || compiled.nPassCount != 2 || compiled.szPassOrder[0] != 2 ||
	     compiled.szPassOrder[1] != 1 || compiled.nDependencyCount != 1 ) {
		return false;
	}
	return compiled.szFirstUse[nResource] == 1 && compiled.szLastUse[nResource] == 1;
}

static bool testZeroRootsAndDeadInputs( void ) noexcept
{
	SIRFrameGraph graph;
	graph.setCullingEnabled( true );
	uint32_t nResource = 0;
	uint32_t nPass = 0;
	const sirFrameGraphPass_t pass = { SIR_GRAPHICS_QUEUE_COPY, SIR_GRAPHICS_FEATURE_COPY_QUEUE, false };
	if ( graph.addResource( makeTransient(), &nResource ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addPass( pass, &nPass ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	if ( graph.addAccess( makeAccess( nPass, nResource, SIR_GRAPHICS_STATE_COPY_SOURCE, false ) ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirFrameGraphCompiled_t compiled = {};
	if ( graph.compile( makeCaps(), &compiled ) != SIR_GRAPHICS_SUCCESS || compiled.nPassCount != 0 || compiled.nTransitionCount != 0 ||
	     compiled.nDependencyCount != 0 ) {
		return false;
	}
	const sirFrameGraphOutput_t output = { nResource, 0, 1, 0, 1 };
	if ( graph.addOutput( output ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	return graph.compile( makeCaps(), &compiled ) == SIR_GRAPHICS_INVALID_ARGUMENT && compiled.nPassCount == 0;
}

static bool testOutputValidation( void ) noexcept
{
	SIRFrameGraph graph;
	graph.setCullingEnabled( true );
	uint32_t nResource = 0;
	const sirFrameGraphOutput_t invalid = { 5, 0, 1, 0, 1 };
	if ( graph.addOutput( invalid ) != SIR_GRAPHICS_INVALID_ARGUMENT ||
	     graph.addResource( makeTransient(), &nResource ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirFrameGraphOutput_t bad = { nResource, 0, 0, 0, 1 };
	if ( graph.addOutput( bad ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	bad.nMipCount = 1;
	bad.nBaseLayer = 1;
	if ( graph.addOutput( bad ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	sirFrameGraphResource_t imported = makeTransient();
	imported.bImported = true;
	imported.eInitialState = SIR_GRAPHICS_STATE_SHADER_READ;
	imported.hImported.eType = SIR_GRAPHICS_HANDLE_TEXTURE;
	imported.hImported.nGeneration = 1;
	imported.hImported.nDeviceGeneration = 2;
	if ( graph.addResource( imported, &nResource ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	const sirFrameGraphOutput_t importedOutput = { nResource, 0, 1, 0, 1 };
	if ( graph.addOutput( importedOutput ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirFrameGraphCompiled_t compiled = {};
	if ( graph.compile( makeCaps(), &compiled ) != SIR_GRAPHICS_SUCCESS || compiled.nPassCount != 0 ) {
		return false;
	}
	sirGraphicsCapabilities_t staleCaps = makeCaps();
	staleCaps.nDeviceGeneration = 8;
	return graph.compile( staleCaps, &compiled ) == SIR_GRAPHICS_INVALID_HANDLE && compiled.nPassCount == 0;
}

typedef struct sirFrameGraphRecordTrace_s
{
	uint32_t szRecorded[SIR_FRAMEGRAPH_MAX_PASSES];
	uint32_t szRolledBack[SIR_FRAMEGRAPH_MAX_PASSES];
	uint32_t nRecordCount;
	uint32_t nRollbackCount;
	uint32_t nFailPass;
} sirFrameGraphRecordTrace_t;

static sirGraphicsResult_t traceRecord( uint32_t nPass, void *pContext ) noexcept
{
	sirFrameGraphRecordTrace_t *pTrace = ( sirFrameGraphRecordTrace_t * )pContext;
	pTrace->szRecorded[pTrace->nRecordCount++] = nPass;
	return nPass == pTrace->nFailPass ? SIR_GRAPHICS_UNSUPPORTED : SIR_GRAPHICS_SUCCESS;
}

static void traceRollback( uint32_t nPass, void *pContext ) noexcept
{
	sirFrameGraphRecordTrace_t *pTrace = ( sirFrameGraphRecordTrace_t * )pContext;
	pTrace->szRolledBack[pTrace->nRollbackCount++] = nPass;
}

static bool testRecordRollbackAndStalePlan( void ) noexcept
{
	SIRFrameGraph graph;
	graph.setCullingEnabled( true );
	uint32_t nPass = 0;
	const sirFrameGraphPass_t sideEffect = { SIR_GRAPHICS_QUEUE_DIRECT, 0, true };
	for ( uint32_t iPass = 0; iPass != 3; ++iPass ) {
		if ( graph.addPass( sideEffect, &nPass ) != SIR_GRAPHICS_SUCCESS ) {
			return false;
		}
	}
	sirFrameGraphCompiled_t compiled = {};
	if ( graph.compile( makeCaps(), &compiled ) != SIR_GRAPHICS_SUCCESS || compiled.nPassCount != 3 ) {
		return false;
	}
	sirFrameGraphRecordTrace_t trace = {};
	trace.nFailPass = 1;
	sirFrameGraphCallbacks_t callbacks = { traceRecord, traceRollback, &trace };
	if ( graph.record( compiled, callbacks ) != SIR_GRAPHICS_UNSUPPORTED || trace.nRecordCount != 2 || trace.nRollbackCount != 2 ||
	     trace.szRolledBack[0] != 1 || trace.szRolledBack[1] != 0 ) {
		return false;
	}
	trace = {};
	trace.nFailPass = SIR_FRAMEGRAPH_INVALID_INDEX;
	if ( graph.record( compiled, callbacks ) != SIR_GRAPHICS_SUCCESS || trace.nRecordCount != 3 || trace.nRollbackCount != 0 ) {
		return false;
	}
	callbacks.pRollback = nullptr;
	if ( graph.record( compiled, callbacks ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	callbacks.pRollback = traceRollback;
	graph.addPass( sideEffect, &nPass );
	return graph.record( compiled, callbacks ) == SIR_GRAPHICS_INVALID_ARGUMENT && trace.nRecordCount == 3;
}

static bool testRecordRejectsForgedPlan( void ) noexcept
{
	SIRFrameGraph graph;
	graph.setCullingEnabled( true );
	uint32_t nPass = 0;
	const sirFrameGraphPass_t pass = { SIR_GRAPHICS_QUEUE_DIRECT, 0, true };
	graph.addPass( pass, &nPass );
	graph.addPass( pass, &nPass );
	sirFrameGraphCompiled_t compiled = {};
	if ( graph.compile( makeCaps(), &compiled ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirFrameGraphRecordTrace_t trace = {};
	sirFrameGraphCallbacks_t callbacks = { traceRecord, traceRollback, &trace };
	compiled.szPassOrder[1] = compiled.szPassOrder[0];
	if ( graph.record( compiled, callbacks ) != SIR_GRAPHICS_INVALID_ARGUMENT || trace.nRecordCount != 0 ) {
		return false;
	}
	compiled.szPassOrder[1] = 1;
	SIRFrameGraph other;
	return other.record( compiled, callbacks ) == SIR_GRAPHICS_INVALID_ARGUMENT && trace.nRecordCount == 0;
}

static bool testRecordEnforcesDependencies( void ) noexcept
{
	SIRFrameGraph graph;
	graph.setCullingEnabled( true );
	uint32_t nPass = 0;
	const sirFrameGraphPass_t pass = { SIR_GRAPHICS_QUEUE_DIRECT, 0, true };
	if ( graph.addPass( pass, &nPass ) != SIR_GRAPHICS_SUCCESS || graph.addPass( pass, &nPass ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addDependency( 0, 1 ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirFrameGraphCompiled_t compiled = {};
	if ( graph.compile( makeCaps(), &compiled ) != SIR_GRAPHICS_SUCCESS || compiled.nDependencyCount != 1 ) {
		return false;
	}
	sirFrameGraphRecordTrace_t trace = {};
	trace.nFailPass = SIR_FRAMEGRAPH_INVALID_INDEX;
	const sirFrameGraphCallbacks_t callbacks = { traceRecord, traceRollback, &trace };
	compiled.szPassOrder[0] = 1;
	compiled.szPassOrder[1] = 0;
	return graph.record( compiled, callbacks ) == SIR_GRAPHICS_INVALID_ARGUMENT && trace.nRecordCount == 0;
}

static bool testPlacementReuseRejectsStalePlan( void ) noexcept
{
	alignas( SIRFrameGraph ) uint8_t szStorage[sizeof( SIRFrameGraph )] = {};
	SIRFrameGraph *pFirst = new ( szStorage ) SIRFrameGraph();
	pFirst->setCullingEnabled( true );
	uint32_t nPass = 0;
	const sirFrameGraphPass_t pass = { SIR_GRAPHICS_QUEUE_DIRECT, 0, true };
	if ( pFirst->addPass( pass, &nPass ) != SIR_GRAPHICS_SUCCESS ) {
		pFirst->~SIRFrameGraph();
		return false;
	}
	sirFrameGraphCompiled_t compiled = {};
	const bool bCompiled = pFirst->compile( makeCaps(), &compiled ) == SIR_GRAPHICS_SUCCESS;
	pFirst->~SIRFrameGraph();
	SIRFrameGraph *pSecond = new ( szStorage ) SIRFrameGraph();
	pSecond->setCullingEnabled( true );
	pSecond->addPass( pass, &nPass );
	sirFrameGraphRecordTrace_t trace = {};
	const sirFrameGraphCallbacks_t callbacks = { traceRecord, traceRollback, &trace };
	const bool bRejected = pSecond->record( compiled, callbacks ) == SIR_GRAPHICS_INVALID_ARGUMENT;
	pSecond->~SIRFrameGraph();
	return bCompiled && bRejected && trace.nRecordCount == 0;
}

static bool testLiveSplitWriteUnion( void ) noexcept
{
	SIRFrameGraph graph;
	graph.setCullingEnabled( true );
	uint32_t nResource = 0;
	uint32_t nPass = 0;
	const sirFrameGraphPass_t pass = { SIR_GRAPHICS_QUEUE_DIRECT, 0, false };
	const sirFrameGraphPass_t consumer = { SIR_GRAPHICS_QUEUE_DIRECT, 0, true };
	if ( graph.addResource( makeTransient( 2 ), &nResource ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addPass( pass, &nPass ) != SIR_GRAPHICS_SUCCESS || graph.addPass( pass, &nPass ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addPass( consumer, &nPass ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirFrameGraphAccess_t first = makeAccess( 0, nResource, SIR_GRAPHICS_STATE_COLOR_ATTACHMENT, true );
	sirFrameGraphAccess_t second = makeAccess( 1, nResource, SIR_GRAPHICS_STATE_COLOR_ATTACHMENT, true );
	second.nBaseMip = 1;
	sirFrameGraphAccess_t read = makeAccess( 2, nResource, SIR_GRAPHICS_STATE_SHADER_READ, false );
	read.nMipCount = 2;
	if ( graph.addAccess( first ) != SIR_GRAPHICS_SUCCESS || graph.addAccess( second ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addAccess( read ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirFrameGraphCompiled_t compiled = {};
	return graph.compile( makeCaps(), &compiled ) == SIR_GRAPHICS_SUCCESS && compiled.nPassCount == 3 && compiled.nDependencyCount == 2 &&
	       compiled.nTransitionCount == 4;
}

typedef struct sirFrameGraphTestCase_s
{
	const char *pszName;
	bool ( *pTest )( void ) noexcept;
} sirFrameGraphTestCase_t;

int main( void )
{
	const sirFrameGraphTestCase_t szCases[] = { { "hazards/queue ownership", testHazardsAndQueueOwnership },
	                                            { "transient/cycle", testTransientAndCycleRejection },
	                                            { "imports/ranges", testImportsAndInvalidRanges },
	                                            { "unsupported/partial", testUnsupportedQueuesAndPartialRanges },
	                                            { "split-write union", testDisjointWriteUnion },
	                                            { "mixed imported state", testImportedMixedState },
	                                            { "WAR/WAW", testWARAndWAW },
	                                            { "queue legality/sparse", testQueueLegalityAndSparseRanges },
	                                            { "same-pass/rollback", testSamePassDisjointAndAtomicFailure },
	                                            { "layer-write union", testLayerWriteUnion },
	                                            { "bounded region work", testBoundedRegionWork },
	                                            { "output overwrite culling", testOutputCullingAndOverwrite },
	                                            { "live read roots", testLiveReadDependencies },
	                                            { "subresource export", testSubresourceOutputAndExplicitDependency },
	                                            { "zero roots", testZeroRootsAndDeadInputs },
	                                            { "output validation", testOutputValidation },
	                                            { "record rollback", testRecordRollbackAndStalePlan },
	                                            { "record forged plan", testRecordRejectsForgedPlan },
	                                            { "record dependency order", testRecordEnforcesDependencies },
	                                            { "placement reuse", testPlacementReuseRejectsStalePlan },
	                                            { "live split write union", testLiveSplitWriteUnion } };
	for ( uint32_t iCase = 0; iCase < sizeof( szCases ) / sizeof( szCases[0] ); ++iCase ) {
		if ( !szCases[iCase].pTest() ) {
			fprintf( stderr, "FrameGraph regression failed: %s\n", szCases[iCase].pszName );
			return 1;
		}
	}
	fprintf( stdout, "FrameGraph graph compiler regression passed\n" );
	return 0;
}
