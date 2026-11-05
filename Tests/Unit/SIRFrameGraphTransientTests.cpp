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

#include <Renderer/Graph/SIRFrameGraphTransient.h>

#include <cstdio>

static sirFrameGraphResource_t makeResource( sirGraphicsHandleType_t eType = SIR_GRAPHICS_HANDLE_BUFFER ) noexcept
{
	sirFrameGraphResource_t resource = {};
	resource.eType = eType;
	resource.eInitialQueue = SIR_GRAPHICS_QUEUE_DIRECT;
	resource.eInitialState = SIR_GRAPHICS_STATE_UNDEFINED;
	resource.nMipLevels = 1;
	resource.nArrayLayers = 1;
	return resource;
}

static sirFrameGraphTransientDesc_t makeDesc( uint32_t nResource, bool bTexture = false ) noexcept
{
	sirFrameGraphTransientDesc_t desc = {};
	desc.nResource = nResource;
	desc.nRequiredBytes = 128;
	desc.nAlignmentBytes = 64;
	desc.nProviderCompatibilityKey = 11;
	desc.eMemory = SIR_GRAPHICS_MEMORY_TRANSIENT;
	desc.eFormat = bTexture ? SIR_GRAPHICS_FORMAT_RGBA8_UNORM : SIR_GRAPHICS_FORMAT_UNKNOWN;
	desc.nSampleCount = 1;
	desc.bAllowAlias = true;
	return desc;
}

static sirGraphicsCapabilities_t makeCaps( void ) noexcept
{
	sirGraphicsCapabilities_t caps = {};
	caps.nDeviceGeneration = 2;
	caps.nQueueMask = ( 1U << SIR_GRAPHICS_QUEUE_DIRECT ) | ( 1U << SIR_GRAPHICS_QUEUE_COPY );
	caps.nFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_COPY_QUEUE;
	return caps;
}

static bool addWrite( SIRFrameGraph *pGraph, uint32_t nPass, uint32_t nResource, sirGraphicsQueue_t eQueue ) noexcept
{
	sirFrameGraphAccess_t access = {};
	access.nPass = nPass;
	access.nResource = nResource;
	access.nMipCount = 1;
	access.nLayerCount = 1;
	access.bWrite = true;
	access.eState = eQueue == SIR_GRAPHICS_QUEUE_COPY ? SIR_GRAPHICS_STATE_COPY_DESTINATION : SIR_GRAPHICS_STATE_SHADER_WRITE;
	return pGraph->addAccess( access ) == SIR_GRAPHICS_SUCCESS;
}

static bool setupGraph( SIRFrameGraph *pGraph, sirFrameGraphCompiled_t *pCompiled, bool bSamePass = false,
                        sirGraphicsQueue_t eSecondQueue = SIR_GRAPHICS_QUEUE_DIRECT ) noexcept
{
	uint32_t nResource = 0;
	uint32_t nPass = 0;
	const sirFrameGraphPass_t first = { SIR_GRAPHICS_QUEUE_DIRECT, 0, false };
	const sirFrameGraphPass_t second = { eSecondQueue, 0, false };
	if ( pGraph->addResource( makeResource(), &nResource ) != SIR_GRAPHICS_SUCCESS ||
	     pGraph->addResource( makeResource(), &nResource ) != SIR_GRAPHICS_SUCCESS ||
	     pGraph->addPass( first, &nPass ) != SIR_GRAPHICS_SUCCESS ||
	     ( !bSamePass && pGraph->addPass( second, &nPass ) != SIR_GRAPHICS_SUCCESS ) ) {
		return false;
	}
	return addWrite( pGraph, 0, 0, SIR_GRAPHICS_QUEUE_DIRECT ) && addWrite( pGraph, bSamePass ? 0 : 1, 1, eSecondQueue ) &&
	       pGraph->compile( makeCaps(), pCompiled ) == SIR_GRAPHICS_SUCCESS;
}

static bool testSerialReuse( void ) noexcept
{
	SIRFrameGraph graph;
	sirFrameGraphCompiled_t compiled = {};
	if ( !setupGraph( &graph, &compiled ) ) {
		return false;
	}
	const sirFrameGraphTransientDesc_t descs[2] = { makeDesc( 0 ), makeDesc( 1 ) };
	const sirFrameGraphTransientInput_t input = { descs, 2, 128 };
	sirFrameGraphTransientPlan_t plan = {};
	return graph.planTransients( compiled, input, &plan ) == SIR_GRAPHICS_SUCCESS && plan.nSlotCount == 1 && plan.szResourceSlot[0] == 0 &&
	       plan.szResourceSlot[1] == 0 && plan.nRequestedBackingBytes == 128 && plan.nLogicalBytes == 256;
}

static bool testSamePassOverlap( void ) noexcept
{
	SIRFrameGraph graph;
	sirFrameGraphCompiled_t compiled = {};
	if ( !setupGraph( &graph, &compiled, true ) ) {
		return false;
	}
	const sirFrameGraphTransientDesc_t descs[2] = { makeDesc( 0 ), makeDesc( 1 ) };
	const sirFrameGraphTransientInput_t input = { descs, 2, 256 };
	sirFrameGraphTransientPlan_t plan = {};
	return graph.planTransients( compiled, input, &plan ) == SIR_GRAPHICS_SUCCESS && plan.nSlotCount == 2 &&
	       plan.nRequestedBackingBytes == 256;
}

static bool testCrossQueue( void ) noexcept
{
	SIRFrameGraph graph;
	sirFrameGraphCompiled_t compiled = {};
	if ( !setupGraph( &graph, &compiled, false, SIR_GRAPHICS_QUEUE_COPY ) ) {
		return false;
	}
	const sirFrameGraphTransientDesc_t descs[2] = { makeDesc( 0 ), makeDesc( 1 ) };
	const sirFrameGraphTransientInput_t input = { descs, 2, 256 };
	sirFrameGraphTransientPlan_t plan = {};
	return graph.planTransients( compiled, input, &plan ) == SIR_GRAPHICS_SUCCESS && plan.nSlotCount == 2;
}

static bool testExportPin( void ) noexcept
{
	SIRFrameGraph graph;
	sirFrameGraphCompiled_t compiled = {};
	if ( !setupGraph( &graph, &compiled ) ) {
		return false;
	}
	const sirFrameGraphOutput_t output = { 0, 0, 1, 0, 1 };
	if ( graph.addOutput( output ) != SIR_GRAPHICS_SUCCESS || graph.compile( makeCaps(), &compiled ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	const sirFrameGraphTransientDesc_t descs[2] = { makeDesc( 0 ), makeDesc( 1 ) };
	const sirFrameGraphTransientInput_t input = { descs, 2, 256 };
	sirFrameGraphTransientPlan_t plan = {};
	return graph.planTransients( compiled, input, &plan ) == SIR_GRAPHICS_SUCCESS && plan.nSlotCount == 2;
}

static bool testCompatibilityAndAliasOptOut( void ) noexcept
{
	SIRFrameGraph graph;
	sirFrameGraphCompiled_t compiled = {};
	if ( !setupGraph( &graph, &compiled ) ) {
		return false;
	}
	sirFrameGraphTransientDesc_t descs[2] = { makeDesc( 0 ), makeDesc( 1 ) };
	const sirFrameGraphTransientInput_t input = { descs, 2, 384 };
	sirFrameGraphTransientPlan_t plan = {};
	descs[1].nProviderCompatibilityKey = 12;
	if ( graph.planTransients( compiled, input, &plan ) != SIR_GRAPHICS_SUCCESS || plan.nSlotCount != 2 ) {
		return false;
	}
	descs[1].nProviderCompatibilityKey = 11;
	descs[0].bAllowAlias = false;
	if ( graph.planTransients( compiled, input, &plan ) != SIR_GRAPHICS_SUCCESS || plan.nSlotCount != 2 ) {
		return false;
	}
	descs[0].bAllowAlias = true;
	descs[1].nRequiredBytes = 256;
	return graph.planTransients( compiled, input, &plan ) == SIR_GRAPHICS_SUCCESS && plan.nSlotCount == 2;
}

static bool testBadInputs( void ) noexcept
{
	SIRFrameGraph graph;
	sirFrameGraphCompiled_t compiled = {};
	if ( !setupGraph( &graph, &compiled ) ) {
		return false;
	}
	sirFrameGraphTransientDesc_t descs[2] = { makeDesc( 0 ), makeDesc( 1 ) };
	sirFrameGraphTransientInput_t input = { descs, 2, 128 };
	sirFrameGraphTransientPlan_t plan = {};
	if ( graph.planTransients( compiled, input, nullptr ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	input.nMaxBackingBytes = 127;
	if ( graph.planTransients( compiled, input, &plan ) != SIR_GRAPHICS_OUT_OF_MEMORY || plan.nSlotCount != 0 ) {
		return false;
	}
	input.nMaxBackingBytes = 128;
	descs[1].nAlignmentBytes = 3;
	if ( graph.planTransients( compiled, input, &plan ) != SIR_GRAPHICS_INVALID_ARGUMENT || plan.nSlotCount != 0 ) {
		return false;
	}
	descs[1] = makeDesc( 1 );
	descs[1].nProviderCompatibilityKey = 0;
	if ( graph.planTransients( compiled, input, &plan ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	descs[1] = makeDesc( 0 );
	if ( graph.planTransients( compiled, input, &plan ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	descs[1] = makeDesc( 1 );
	input.nDescCount = 1;
	if ( graph.planTransients( compiled, input, &plan ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	input.nDescCount = 2;
	compiled.szLastUse[1] = 0;
	if ( graph.planTransients( compiled, input, &plan ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	compiled.szLastUse[1] = 1;
	compiled.nRevision = 9;
	return graph.planTransients( compiled, input, &plan ) == SIR_GRAPHICS_INVALID_ARGUMENT;
}

static bool testThreeResourceReuse( void ) noexcept
{
	SIRFrameGraph graph;
	uint32_t nResource = 0;
	uint32_t nPass = 0;
	const sirFrameGraphPass_t pass = { SIR_GRAPHICS_QUEUE_DIRECT, 0, false };
	for ( uint32_t iResource = 0; iResource != 3; ++iResource ) {
		if ( graph.addResource( makeResource(), &nResource ) != SIR_GRAPHICS_SUCCESS ||
		     graph.addPass( pass, &nPass ) != SIR_GRAPHICS_SUCCESS ||
		     !addWrite( &graph, iResource, iResource, SIR_GRAPHICS_QUEUE_DIRECT ) ) {
			return false;
		}
	}
	sirFrameGraphCompiled_t compiled = {};
	if ( graph.compile( makeCaps(), &compiled ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	const sirFrameGraphTransientDesc_t descs[3] = { makeDesc( 2 ), makeDesc( 0 ), makeDesc( 1 ) };
	const sirFrameGraphTransientInput_t input = { descs, 3, 128 };
	sirFrameGraphTransientPlan_t plan = {};
	return graph.planTransients( compiled, input, &plan ) == SIR_GRAPHICS_SUCCESS && plan.nSlotCount == 1 &&
	       plan.szResourceSlot[2] == plan.szResourceSlot[0] && plan.szResourceSlot[0] == plan.szResourceSlot[1] &&
	       plan.nLogicalBytes == 384 && plan.nRequestedBackingBytes == 128;
}

static bool testBufferDescriptorAlignmentAndOverflow( void ) noexcept
{
	SIRFrameGraph graph;
	sirFrameGraphCompiled_t compiled = {};
	if ( !setupGraph( &graph, &compiled ) ) {
		return false;
	}
	sirFrameGraphTransientDesc_t descs[2] = { makeDesc( 0 ), makeDesc( 1 ) };
	sirFrameGraphTransientInput_t input = { descs, 2, 191 };
	sirFrameGraphTransientPlan_t plan = {};
	descs[0].nRequiredBytes = 129;
	descs[1].nRequiredBytes = 129;
	if ( graph.planTransients( compiled, input, &plan ) != SIR_GRAPHICS_OUT_OF_MEMORY || plan.nSlotCount != 0 ) {
		return false;
	}
	input.nMaxBackingBytes = 192;
	if ( graph.planTransients( compiled, input, &plan ) != SIR_GRAPHICS_SUCCESS || plan.nSlotCount != 1 ||
	     plan.nRequestedBackingBytes != 192 || plan.nLogicalBytes != 258 ) {
		return false;
	}
	descs[0].nRequiredBytes = UINT64_MAX;
	return graph.planTransients( compiled, input, &plan ) == SIR_GRAPHICS_CAPACITY_EXCEEDED && plan.nSlotCount == 0;
}

static bool testTextureCompatibility( void ) noexcept
{
	SIRFrameGraph graph;
	uint32_t nResource = 0;
	uint32_t nPass = 0;
	const sirFrameGraphPass_t pass = { SIR_GRAPHICS_QUEUE_DIRECT, 0, false };
	for ( uint32_t iPass = 0; iPass != 2; ++iPass ) {
		if ( graph.addResource( makeResource( SIR_GRAPHICS_HANDLE_TEXTURE ), &nResource ) != SIR_GRAPHICS_SUCCESS ||
		     graph.addPass( pass, &nPass ) != SIR_GRAPHICS_SUCCESS || !addWrite( &graph, iPass, iPass, SIR_GRAPHICS_QUEUE_DIRECT ) ) {
			return false;
		}
	}
	sirFrameGraphCompiled_t compiled = {};
	if ( graph.compile( makeCaps(), &compiled ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirFrameGraphTransientDesc_t descs[2] = { makeDesc( 0, true ), makeDesc( 1, true ) };
	sirFrameGraphTransientInput_t input = { descs, 2, 256 };
	sirFrameGraphTransientPlan_t plan = {};
	if ( graph.planTransients( compiled, input, &plan ) != SIR_GRAPHICS_SUCCESS || plan.nSlotCount != 1 ) {
		return false;
	}
	descs[1].nSampleCount = 4;
	if ( graph.planTransients( compiled, input, &plan ) != SIR_GRAPHICS_SUCCESS || plan.nSlotCount != 2 ) {
		return false;
	}
	descs[1].eFormat = SIR_GRAPHICS_FORMAT_UNKNOWN;
	if ( graph.planTransients( compiled, input, &plan ) != SIR_GRAPHICS_INVALID_ARGUMENT || plan.nSlotCount != 0 ) {
		return false;
	}
	descs[1].eFormat = static_cast<sirGraphicsFormat_t>( 99 );
	if ( graph.planTransients( compiled, input, &plan ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	descs[1].eFormat = static_cast<sirGraphicsFormat_t>( 2 );
	descs[1].nSampleCount = 1;
	return graph.planTransients( compiled, input, &plan ) == SIR_GRAPHICS_SUCCESS && plan.nSlotCount == 2;
}

static bool testImportedAndStalePlan( void ) noexcept
{
	SIRFrameGraph graph;
	uint32_t nResource = 0;
	uint32_t nPass = 0;
	sirFrameGraphResource_t imported = makeResource();
	imported.bImported = true;
	imported.eInitialState = SIR_GRAPHICS_STATE_SHADER_READ;
	imported.hImported.eType = SIR_GRAPHICS_HANDLE_BUFFER;
	imported.hImported.nGeneration = 1;
	imported.hImported.nDeviceGeneration = 2;
	const sirFrameGraphPass_t pass = { SIR_GRAPHICS_QUEUE_DIRECT, 0, false };
	if ( graph.addResource( imported, &nResource ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addResource( makeResource(), &nResource ) != SIR_GRAPHICS_SUCCESS || graph.addPass( pass, &nPass ) != SIR_GRAPHICS_SUCCESS ||
	     !addWrite( &graph, 0, 1, SIR_GRAPHICS_QUEUE_DIRECT ) ) {
		return false;
	}
	sirFrameGraphAccess_t read = {};
	read.nPass = 0;
	read.nResource = 0;
	read.nMipCount = 1;
	read.nLayerCount = 1;
	read.eState = SIR_GRAPHICS_STATE_SHADER_READ;
	if ( graph.addAccess( read ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirFrameGraphCompiled_t compiled = {};
	if ( graph.compile( makeCaps(), &compiled ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirFrameGraphTransientDesc_t descs[2] = { makeDesc( 0 ), makeDesc( 1 ) };
	sirFrameGraphTransientInput_t input = { descs, 2, 256 };
	sirFrameGraphTransientPlan_t plan = {};
	if ( graph.planTransients( compiled, input, &plan ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	input.pDescs = &descs[1];
	input.nDescCount = 1;
	if ( graph.planTransients( compiled, input, &plan ) != SIR_GRAPHICS_SUCCESS || plan.nSlotCount != 1 ||
	     plan.szResourceSlot[0] != SIR_FRAMEGRAPH_INVALID_INDEX ) {
		return false;
	}
	descs[1].eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	if ( graph.planTransients( compiled, input, &plan ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	descs[1].eFormat = SIR_GRAPHICS_FORMAT_UNKNOWN;
	graph.addPass( pass, &nPass );
	return graph.planTransients( compiled, input, &plan ) == SIR_GRAPHICS_INVALID_ARGUMENT;
}

static bool testDescriptorRejectionAndNoAccess( void ) noexcept
{
	SIRFrameGraph graph;
	uint32_t nResource = 0;
	uint32_t nPass = 0;
	const sirFrameGraphPass_t pass = { SIR_GRAPHICS_QUEUE_DIRECT, 0, false };
	if ( graph.addResource( makeResource(), &nResource ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addPass( pass, &nPass ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirFrameGraphCompiled_t compiled = {};
	if ( graph.compile( makeCaps(), &compiled ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirFrameGraphTransientDesc_t desc = makeDesc( 0 );
	sirFrameGraphTransientInput_t input = { &desc, 1, 128 };
	sirFrameGraphTransientPlan_t plan = {};
	if ( graph.planTransients( compiled, input, &plan ) != SIR_GRAPHICS_SUCCESS || plan.nSlotCount != 0 ) {
		return false;
	}
	desc.nRequiredBytes = 0;
	if ( graph.planTransients( compiled, input, &plan ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	desc = makeDesc( 0 );
	desc.eMemory = static_cast<sirGraphicsMemoryClass_t>( 99 );
	if ( graph.planTransients( compiled, input, &plan ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	desc = makeDesc( 0 );
	desc.nAlignmentBytes = 0;
	if ( graph.planTransients( compiled, input, &plan ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	input.pDescs = nullptr;
	return graph.planTransients( compiled, input, &plan ) == SIR_GRAPHICS_INVALID_ARGUMENT;
}
static bool testCompiledPlanValidation( void ) noexcept
{
	SIRFrameGraph graph;
	sirFrameGraphCompiled_t compiled = {};
	if ( !setupGraph( &graph, &compiled ) ) {
		return false;
	}
	const sirFrameGraphTransientDesc_t descs[2] = { makeDesc( 0 ), makeDesc( 1 ) };
	sirFrameGraphTransientInput_t input = { descs, 2, 256 };
	sirFrameGraphTransientPlan_t plan = {};
	SIRFrameGraph other;
	if ( other.planTransients( compiled, input, &plan ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	sirFrameGraphCompiled_t altered = compiled;
	altered.szLivePasses[0] = 0;
	if ( graph.planTransients( altered, input, &plan ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	altered = compiled;
	altered.szPassOrder[1] = 0;
	if ( graph.planTransients( altered, input, &plan ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	altered = compiled;
	altered.szPassOrder[1] = SIR_FRAMEGRAPH_MAX_PASSES;
	if ( graph.planTransients( altered, input, &plan ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	altered = compiled;
	altered.nDependencyCount = 1;
	altered.szDependencies[0] = { 1, 0, false };
	if ( graph.planTransients( altered, input, &plan ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	altered.szDependencies[0] = { 0, 1, false };
	if ( graph.planTransients( altered, input, &plan ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	altered = compiled;
	altered.nPassCount = 1;
	if ( graph.planTransients( altered, input, &plan ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	altered = compiled;
	altered.szFirstUse[0] = SIR_FRAMEGRAPH_INVALID_INDEX;
	if ( graph.planTransients( altered, input, &plan ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	altered = compiled;
	altered.nPassCount = 1;
	altered.szLivePasses[1] = 0;
	altered.szFirstUse[1] = SIR_FRAMEGRAPH_INVALID_INDEX;
	altered.szLastUse[1] = SIR_FRAMEGRAPH_INVALID_INDEX;
	if ( graph.planTransients( altered, input, &plan ) != SIR_GRAPHICS_SUCCESS || plan.nSlotCount != 1 ||
	     plan.szResourceSlot[1] != SIR_FRAMEGRAPH_INVALID_INDEX ) {
		return false;
	}
	input.nDescCount = SIR_FRAMEGRAPH_MAX_RESOURCES + 1;
	if ( graph.planTransients( compiled, input, &plan ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	input.nDescCount = 0;
	input.pDescs = nullptr;
	return graph.planTransients( compiled, input, &plan ) == SIR_GRAPHICS_INVALID_ARGUMENT;
}

static bool testQueueOwnershipAndResourceSorting( void ) noexcept
{
	SIRFrameGraph graph;
	uint32_t nResource = 0;
	uint32_t nPass = 0;
	const sirFrameGraphPass_t direct = { SIR_GRAPHICS_QUEUE_DIRECT, 0, false };
	const sirFrameGraphPass_t copy = { SIR_GRAPHICS_QUEUE_COPY, 0, false };
	if ( graph.addResource( makeResource(), &nResource ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addResource( makeResource(), &nResource ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addPass( direct, &nPass ) != SIR_GRAPHICS_SUCCESS || graph.addPass( copy, &nPass ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addPass( direct, &nPass ) != SIR_GRAPHICS_SUCCESS || !addWrite( &graph, 0, 1, SIR_GRAPHICS_QUEUE_DIRECT ) ||
	     !addWrite( &graph, 1, 0, SIR_GRAPHICS_QUEUE_COPY ) || !addWrite( &graph, 2, 0, SIR_GRAPHICS_QUEUE_DIRECT ) ) {
		return false;
	}
	sirFrameGraphCompiled_t compiled = {};
	if ( graph.compile( makeCaps(), &compiled ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	const sirFrameGraphTransientDesc_t descs[2] = { makeDesc( 0 ), makeDesc( 1 ) };
	const sirFrameGraphTransientInput_t input = { descs, 2, 256 };
	sirFrameGraphTransientPlan_t plan = {};
	return graph.planTransients( compiled, input, &plan ) == SIR_GRAPHICS_SUCCESS && plan.nSlotCount == 2 &&
	       plan.szResourceSlot[0] != plan.szResourceSlot[1] && plan.szSlots[plan.szResourceSlot[0]].eQueue == SIR_GRAPHICS_QUEUE_COUNT;
}

static bool testLogicalCapacityAndSerialReplay( void ) noexcept
{
	SIRFrameGraph graph;
	sirFrameGraphCompiled_t compiled = {};
	if ( !setupGraph( &graph, &compiled ) ) {
		return false;
	}
	sirFrameGraphTransientDesc_t descs[2] = { makeDesc( 0 ), makeDesc( 1 ) };
	sirFrameGraphTransientInput_t input = { descs, 2, UINT64_MAX };
	sirFrameGraphTransientPlan_t plan = {};
	descs[0].nRequiredBytes = UINT64_MAX - 1;
	descs[0].nAlignmentBytes = 1;
	descs[1].nRequiredBytes = 128;
	if ( graph.planTransients( compiled, input, &plan ) != SIR_GRAPHICS_CAPACITY_EXCEEDED || plan.nSlotCount != 0 ) {
		return false;
	}
	descs[0] = makeDesc( 0 );
	input.nMaxBackingBytes = 128;
	for ( uint32_t iTrial = 0; iTrial != 1000; ++iTrial ) {
		if ( graph.planTransients( compiled, input, &plan ) != SIR_GRAPHICS_SUCCESS || plan.nSlotCount != 1 ||
		     plan.nRequestedBackingBytes != 128 || plan.nLogicalBytes != 256 ) {
			return false;
		}
	}
	return true;
}
static bool testAdversarialPlanAndProviderInputs( void ) noexcept
{
	SIRFrameGraph graph;
	sirFrameGraphCompiled_t compiled = {};
	if ( !setupGraph( &graph, &compiled ) ) {
		return false;
	}
	sirFrameGraphTransientDesc_t descs[2] = { makeDesc( 0 ), makeDesc( 1 ) };
	sirFrameGraphTransientInput_t input = { descs, 2, 256 };
	sirFrameGraphTransientPlan_t plan = {};
	sirFrameGraphCompiled_t altered = compiled;
	altered.nOwnerIdentity += 1;
	if ( graph.planTransients( altered, input, &plan ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	altered = compiled;
	altered.nOriginalPassCount += 1;
	if ( graph.planTransients( altered, input, &plan ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	altered = compiled;
	altered.nPassCount += 1;
	if ( graph.planTransients( altered, input, &plan ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	altered = compiled;
	altered.nDependencyCount = SIR_FRAMEGRAPH_MAX_DEPENDENCIES + 1;
	if ( graph.planTransients( altered, input, &plan ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	altered = compiled;
	altered.nDependencyCount = 1;
	altered.szDependencies[0] = { SIR_FRAMEGRAPH_MAX_PASSES, 0, false };
	if ( graph.planTransients( altered, input, &plan ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	altered.szDependencies[0] = { 0, SIR_FRAMEGRAPH_MAX_PASSES, false };
	if ( graph.planTransients( altered, input, &plan ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	altered.szDependencies[0] = { 0, 0, false };
	if ( graph.planTransients( altered, input, &plan ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	altered = compiled;
	altered.szLastUse[0] = SIR_FRAMEGRAPH_INVALID_INDEX;
	if ( graph.planTransients( altered, input, &plan ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}

	descs[1].nResource = SIR_FRAMEGRAPH_MAX_RESOURCES;
	if ( graph.planTransients( compiled, input, &plan ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	descs[1] = makeDesc( 1 );
	descs[1].nSampleCount = 0;
	if ( graph.planTransients( compiled, input, &plan ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	descs[1] = makeDesc( 1 );
	descs[1].nSampleCount = 2;
	if ( graph.planTransients( compiled, input, &plan ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	descs[1] = makeDesc( 1 );
	descs[1].nAlignmentBytes = 128;
	if ( graph.planTransients( compiled, input, &plan ) != SIR_GRAPHICS_SUCCESS || plan.nSlotCount != 2 ) {
		return false;
	}
	descs[1] = makeDesc( 1 );
	descs[1].eMemory = SIR_GRAPHICS_MEMORY_CPU_UPLOAD;
	if ( graph.planTransients( compiled, input, &plan ) != SIR_GRAPHICS_SUCCESS || plan.nSlotCount != 2 ) {
		return false;
	}
	descs[1] = makeDesc( 1 );
	descs[1].nRequiredBytes = 192;
	if ( graph.planTransients( compiled, input, &plan ) != SIR_GRAPHICS_OUT_OF_MEMORY ) {
		return false;
	}
	descs[1] = makeDesc( 1 );
	descs[1].bAllowAlias = false;
	if ( graph.planTransients( compiled, input, &plan ) != SIR_GRAPHICS_SUCCESS || plan.nSlotCount != 2 ) {
		return false;
	}
	return true;
}
static bool testSameResourceWithinQueue( void ) noexcept
{
	SIRFrameGraph graph;
	uint32_t nResource = 0;
	uint32_t nPass = 0;
	const sirFrameGraphPass_t pass = { SIR_GRAPHICS_QUEUE_DIRECT, 0, false };
	if ( graph.addResource( makeResource(), &nResource ) != SIR_GRAPHICS_SUCCESS || graph.addPass( pass, &nPass ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addPass( pass, &nPass ) != SIR_GRAPHICS_SUCCESS || !addWrite( &graph, 0, 0, SIR_GRAPHICS_QUEUE_DIRECT ) ||
	     !addWrite( &graph, 1, 0, SIR_GRAPHICS_QUEUE_DIRECT ) ) {
		return false;
	}
	sirFrameGraphCompiled_t compiled = {};
	if ( graph.compile( makeCaps(), &compiled ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	const sirFrameGraphTransientDesc_t desc = makeDesc( 0 );
	const sirFrameGraphTransientInput_t input = { &desc, 1, 128 };
	sirFrameGraphTransientPlan_t plan = {};
	return graph.planTransients( compiled, input, &plan ) == SIR_GRAPHICS_SUCCESS && plan.nSlotCount == 1 &&
	       plan.szSlots[0].nLastUse == 1 && plan.szSlots[0].eQueue == SIR_GRAPHICS_QUEUE_DIRECT;
}

static bool testDeadDependencyAndMixedResourceQueue( void ) noexcept
{
	SIRFrameGraph graph;
	uint32_t nResource = 0;
	uint32_t nPass = 0;
	const sirFrameGraphPass_t direct = { SIR_GRAPHICS_QUEUE_DIRECT, 0, false };
	const sirFrameGraphPass_t copy = { SIR_GRAPHICS_QUEUE_COPY, 0, false };
	if ( graph.addResource( makeResource(), &nResource ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addResource( makeResource(), &nResource ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addPass( direct, &nPass ) != SIR_GRAPHICS_SUCCESS || graph.addPass( copy, &nPass ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addPass( direct, &nPass ) != SIR_GRAPHICS_SUCCESS || !addWrite( &graph, 0, 0, SIR_GRAPHICS_QUEUE_DIRECT ) ||
	     !addWrite( &graph, 1, 0, SIR_GRAPHICS_QUEUE_COPY ) || !addWrite( &graph, 2, 1, SIR_GRAPHICS_QUEUE_DIRECT ) ) {
		return false;
	}
	sirFrameGraphCompiled_t compiled = {};
	if ( graph.compile( makeCaps(), &compiled ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	const sirFrameGraphTransientDesc_t descs[2] = { makeDesc( 0 ), makeDesc( 1 ) };
	const sirFrameGraphTransientInput_t input = { descs, 2, 256 };
	sirFrameGraphTransientPlan_t plan = {};
	if ( graph.planTransients( compiled, input, &plan ) != SIR_GRAPHICS_SUCCESS || plan.nSlotCount != 2 ) {
		return false;
	}
	sirFrameGraphCompiled_t altered = compiled;
	altered.nPassCount = 2;
	altered.szLivePasses[2] = 0;
	altered.szFirstUse[1] = SIR_FRAMEGRAPH_INVALID_INDEX;
	altered.szLastUse[1] = SIR_FRAMEGRAPH_INVALID_INDEX;
	altered.nDependencyCount = 1;
	altered.szDependencies[0] = { 2, 0, false };
	if ( graph.planTransients( altered, input, &plan ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	altered.szDependencies[0] = { 0, 2, false };
	return graph.planTransients( altered, input, &plan ) == SIR_GRAPHICS_INVALID_ARGUMENT;
}

static bool testDistinctResourceTypes( void ) noexcept
{
	SIRFrameGraph graph;
	uint32_t nResource = 0;
	uint32_t nPass = 0;
	const sirFrameGraphPass_t pass = { SIR_GRAPHICS_QUEUE_DIRECT, 0, false };
	if ( graph.addResource( makeResource(), &nResource ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addResource( makeResource( SIR_GRAPHICS_HANDLE_TEXTURE ), &nResource ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addPass( pass, &nPass ) != SIR_GRAPHICS_SUCCESS || graph.addPass( pass, &nPass ) != SIR_GRAPHICS_SUCCESS ||
	     !addWrite( &graph, 0, 0, SIR_GRAPHICS_QUEUE_DIRECT ) || !addWrite( &graph, 1, 1, SIR_GRAPHICS_QUEUE_DIRECT ) ) {
		return false;
	}
	sirFrameGraphCompiled_t compiled = {};
	if ( graph.compile( makeCaps(), &compiled ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	const sirFrameGraphTransientDesc_t descs[2] = { makeDesc( 0 ), makeDesc( 1, true ) };
	const sirFrameGraphTransientInput_t input = { descs, 2, 256 };
	sirFrameGraphTransientPlan_t plan = {};
	return graph.planTransients( compiled, input, &plan ) == SIR_GRAPHICS_SUCCESS && plan.nSlotCount == 2;
}
static bool testReorderedNonHazardousImports( void ) noexcept
{
	SIRFrameGraph graph;
	uint32_t nResource = 0;
	uint32_t nPass = 0;
	sirFrameGraphResource_t imported = makeResource();
	imported.bImported = true;
	imported.eInitialState = SIR_GRAPHICS_STATE_SHADER_READ;
	imported.hImported.eType = SIR_GRAPHICS_HANDLE_BUFFER;
	imported.hImported.nGeneration = 1;
	imported.hImported.nDeviceGeneration = 2;
	const sirFrameGraphPass_t pass = { SIR_GRAPHICS_QUEUE_DIRECT, 0, false };
	if ( graph.addResource( imported, &nResource ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addResource( makeResource(), &nResource ) != SIR_GRAPHICS_SUCCESS || graph.addPass( pass, &nPass ) != SIR_GRAPHICS_SUCCESS ||
	     graph.addPass( pass, &nPass ) != SIR_GRAPHICS_SUCCESS || graph.addDependency( 1, 0 ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirFrameGraphAccess_t read = {};
	read.nResource = 0;
	read.nMipCount = 1;
	read.nLayerCount = 1;
	read.eState = SIR_GRAPHICS_STATE_SHADER_READ;
	if ( graph.addAccess( read ) != SIR_GRAPHICS_SUCCESS || !addWrite( &graph, 0, 1, SIR_GRAPHICS_QUEUE_DIRECT ) ) {
		return false;
	}
	read.nPass = 1;
	if ( graph.addAccess( read ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirFrameGraphCompiled_t compiled = {};
	if ( graph.compile( makeCaps(), &compiled ) != SIR_GRAPHICS_SUCCESS || compiled.szPassOrder[0] != 1 || compiled.szPassOrder[1] != 0 ) {
		return false;
	}
	const sirFrameGraphTransientDesc_t desc = makeDesc( 1 );
	const sirFrameGraphTransientInput_t input = { &desc, 1, 128 };
	sirFrameGraphTransientPlan_t plan = {};
	return graph.planTransients( compiled, input, &plan ) == SIR_GRAPHICS_SUCCESS && plan.nSlotCount == 1 &&
	       plan.szResourceSlot[0] == SIR_FRAMEGRAPH_INVALID_INDEX && plan.szResourceSlot[1] == 0;
}
typedef struct sirFrameGraphTestCase_s
{
	const char *pszName;
	bool ( *pFunction )( void ) noexcept;
} sirFrameGraphTestCase_t;

int main( void )
{
	const sirFrameGraphTestCase_t cases[] = {
	    { "serial reuse", testSerialReuse },
	    { "same pass overlap", testSamePassOverlap },
	    { "cross queue", testCrossQueue },
	    { "export pin", testExportPin },
	    { "provider compatibility", testCompatibilityAndAliasOptOut },
	    { "invalid inputs", testBadInputs },
	    { "three resources", testThreeResourceReuse },
	    { "alignment overflow", testBufferDescriptorAlignmentAndOverflow },
	    { "textures", testTextureCompatibility },
	    { "import and stale", testImportedAndStalePlan },
	    { "descriptor reject", testDescriptorRejectionAndNoAccess },
	    { "compiled plan reject", testCompiledPlanValidation },
	    { "queue ownership", testQueueOwnershipAndResourceSorting },
	    { "capacity and replay", testLogicalCapacityAndSerialReplay },
	    { "adversarial inputs", testAdversarialPlanAndProviderInputs },
	    { "repeated queue use", testSameResourceWithinQueue },
	    { "dead dependency", testDeadDependencyAndMixedResourceQueue },
	    { "distinct types", testDistinctResourceTypes },
	    { "reordered imports", testReorderedNonHazardousImports },
	};
	uint32_t nFailed = 0;
	for ( const sirFrameGraphTestCase_t& test : cases ) {
		if ( !test.pFunction() ) {
			printf( "FrameGraph R3 FAILED: %s\n", test.pszName );
			++nFailed;
		}
	}
	printf( "FrameGraph R3: %zu scenarios, %u failed\n", sizeof( cases ) / sizeof( cases[0] ), nFailed );
	return nFailed == 0 ? 0 : 1;
}