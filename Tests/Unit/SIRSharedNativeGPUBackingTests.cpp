/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#include <Renderer/SIRSharedGPUBackingOwner.h>
#include <Renderer/Graph/SIRFrameGraph.h>
#include <Renderer/SIRGraphicsValidation.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#define CHECK( predicate )                                                                                                                 \
	do {                                                                                                                                   \
		if ( !( predicate ) ) {                                                                                                            \
			fprintf( stderr, "Shared GPU backing failure %s:%u: %s\n", __FILE__, __LINE__, #predicate );                                   \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( false )

struct sirTestAllocCount_t
{
	uint32_t nLive = 0;
};

static void *allocBytes( void *pContext, uint64_t nBytes, uint32_t nAlign, uint32_t ) noexcept
{
	void *p = nullptr;
	if ( posix_memalign( &p, nAlign < sizeof( void * ) ? sizeof( void * ) : nAlign, ( size_t )nBytes ) != 0 ) {
		return nullptr;
	}
	++( ( sirTestAllocCount_t * )pContext )->nLive;
	return p;
}

static void freeBytes( void *pContext, void *pData, uint64_t, uint32_t, uint32_t ) noexcept
{
	--( ( sirTestAllocCount_t * )pContext )->nLive;
	free( pData );
}

class SIRNativeBackingTestDevice final : public SIRRenderingDevice
{
public:
	uint32_t nBuffersCreated = 0;
	uint32_t nBuffersRetired = 0;
	uint64_t nCompleted = 0;

	sirGraphicsBackend_t getBackend( void ) const noexcept override
	{
		return SIR_GRAPHICS_BACKEND_VULKAN;
	}
	uint32_t getDeviceGeneration( void ) const noexcept override
	{
		return 4;
	}
	sirGraphicsResult_t queryCapabilities( sirGraphicsCapabilities_t * ) const noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t queryPipelineModelCaps( sirGraphicsPipelineModel_t, sirGraphicsABIPipelineModelCaps_t * ) const noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t queryMemoryBudget( sirGraphicsMemoryBudget_t * ) const noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t createBuffer( const sirGraphicsBufferDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept override
	{
		if ( !pHandle || sirGraphicsValidateBuffer( desc ) != SIR_GRAPHICS_SUCCESS ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		*pHandle = { nBuffersCreated++, 1, 4, SIR_GRAPHICS_HANDLE_BUFFER };
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t createTexture( const sirGraphicsTextureDesc_t&, sirGraphicsHandle_t * ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t createView( const sirGraphicsViewDesc_t&, sirGraphicsHandle_t * ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t createSampler( const sirGraphicsSamplerDesc_t&, sirGraphicsHandle_t * ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t createPipeline( const sirGraphicsPipelineDesc_t&, sirGraphicsHandle_t * ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t createBindingSet( sirGraphicsHandle_t, uint32_t, sirGraphicsHandle_t * ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t updateBindings( sirGraphicsHandle_t, const sirGraphicsDescriptorWrite_t *, uint32_t ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t mapBuffer( sirGraphicsHandle_t, uint64_t, uint64_t, sirGraphicsMappedBuffer_t * ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t unmapBuffer( sirGraphicsHandle_t, uint64_t, uint64_t ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t flushBuffer( sirGraphicsHandle_t, uint64_t, uint64_t ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t invalidateBuffer( sirGraphicsHandle_t, uint64_t, uint64_t ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t acquireCommands( sirGraphicsQueue_t, SIRGraphicsCommandList ** ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t submit( sirGraphicsQueue_t, const sirGraphicsSubmission_t&, sirGraphicsCompletion_t * ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t poll( const sirGraphicsCompletion_t& value, bool *pDone ) noexcept override
	{
		if ( !pDone || value.nDeviceGeneration != 4 || value.eQueue != SIR_GRAPHICS_QUEUE_DIRECT ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		*pDone = value.nValue <= nCompleted;
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t wait( const sirGraphicsCompletion_t&, uint64_t ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t retireResource( sirGraphicsHandle_t h, const sirGraphicsCompletionSet_t& completed ) noexcept override
	{
		if ( !sirGraphicsIsHandleValid( h, SIR_GRAPHICS_HANDLE_BUFFER, 4 ) || completed.nDeviceGeneration != 4 ||
		     completed.nQueueMask != 1 || completed.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT] < 5 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		++nBuffersRetired;
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t createSurface( const sirGraphicsNativeSurface_t&, const sirGraphicsPresentDesc_t&,
	                                   sirGraphicsHandle_t * ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t resizeSurface( sirGraphicsHandle_t, const sirGraphicsNativeSurface_t&,
	                                   const sirGraphicsPresentDesc_t& ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t acquireSurfaceImage( sirGraphicsHandle_t, sirGraphicsSurfaceImage_t * ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t present( const sirGraphicsSurfaceImage_t&, const sirGraphicsCompletion_t& ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t shutdown( void ) noexcept override
	{
		return SIR_GRAPHICS_SUCCESS;
	}
};

static bool noNativeRepresentationRelease( void *, uint64_t ) noexcept
{
	return true;
}

int main( void )
{
	sirTestAllocCount_t allocations = {};
	{
		SIRResourceGraphConfig cfg = {};
		cfg.m_Allocator = { &allocations, allocBytes, freeBytes };
		cfg.m_nMetadataDomain = 1;
		cfg.m_nMaxResources = 4;
		cfg.m_nMaxStaging = 4;
		cfg.m_nMaxProviderPins = 12;
		cfg.m_nMaxCommittedBytes = 16384;
		SIRResourceGraph graph( cfg );
		CHECK( graph.isReady() );
		const SIRResourceDerivedConfig drvCfg = { cfg.m_Allocator, 1, 8, 4, 8192, 12 };
		SIRResourceDerivedRegistry registry( graph, drvCfg );
		CHECK( registry.isReady() );
		SIRNativeBackingTestDevice device;
		SIRSharedGPUBackingOwner owner;
		CHECK( owner.initialize( registry, device, 90, 11 ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( registry.registerProvider( 12, 4, SIR_GRAPHICS_QUEUE_COUNT, { nullptr, noNativeRepresentationRelease } ) ==
		       SIR_RESOURCE_RESULT_SUCCESS );
		auto *pWriter = graph.getWriter();
		CHECK( pWriter );
		SIRResourceStagingHandle stage = {};
		SIRResourceMutableBytes bytes = {};
		CHECK( pWriter->allocateStaging( 256, 16, 1, &stage, &bytes ) == SIR_RESOURCE_RESULT_SUCCESS );
		memset( bytes.m_pData, 0xA5, 256 );
		const SIRResourceChunkPublication chunk = { { 1 }, { 0x1234, 0x5678, 1, 0 }, stage, false };
		const SIRResourcePublication pub = { { 777, 9, 0 }, SIR_RESOURCE_KIND_BLOB, 1, 1, &chunk, 1, nullptr, 0 };
		SIRResourceHandle hSource = {};
		CHECK( pWriter->publish( pub, &hSource ) == SIR_RESOURCE_RESULT_SUCCESS );
		sirNativeGPUBufferBacking_t backing = {}, same = {};
		sirNativeGPUBufferLease_t gpu = {}, physics = {};
		{
			SIRResourceReadScope scope( graph );
			SIRResourceView source;
			CHECK( graph.resolve( scope, hSource, &source ) == SIR_RESOURCE_RESULT_SUCCESS );
			const sirGraphicsBufferDesc_t desc = { 256, 16, SIR_GRAPHICS_USAGE_STORAGE | SIR_GRAPHICS_USAGE_VERTEX,
			                                       SIR_GRAPHICS_MEMORY_GPU_LOCAL };
			CHECK( owner.createOrReuseBuffer( scope, source, desc, 1, 1, &backing ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( owner.createOrReuseBuffer( scope, source, desc, 1, 1, &same ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( backing.nNativeToken == same.nNativeToken && device.nBuffersCreated == 1 );
			const sirSharedDeviceViewRequest_t req = {
			    90, source.getRevision(), graph.getStats().nPublicationEpoch, 0, 128, 4, 1, 1, SIR_GRAPHICS_USAGE_STORAGE, 16, 101, 11 };
			CHECK( owner.acquireBuffer( scope, source, req, &gpu ) == SIR_RESOURCE_RESULT_SUCCESS );
			sirSharedDeviceViewRequest_t physicsReq = req;
			physicsReq.nConsumerId = 202;
			physicsReq.nCompletionProviderId = 12;
			CHECK( owner.acquireBuffer( scope, source, physicsReq, &physics ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( gpu.hNative.nSlot == physics.hNative.nSlot && gpu.view.nPhysicalBackingId == physics.view.nPhysicalBackingId );
			SIRFrameGraph frameGraph;
			uint32_t firstImport = UINT32_MAX, secondImport = UINT32_MAX, passId = UINT32_MAX;
			CHECK( frameGraph.importSharedBuffer( gpu, SIR_GRAPHICS_STATE_SHADER_READ, SIR_GRAPHICS_QUEUE_DIRECT, &firstImport ) ==
			       SIR_GRAPHICS_SUCCESS );
			CHECK( frameGraph.importSharedBuffer( physics, SIR_GRAPHICS_STATE_SHADER_READ, SIR_GRAPHICS_QUEUE_DIRECT, &secondImport ) ==
			       SIR_GRAPHICS_SUCCESS );
			CHECK( firstImport == secondImport && frameGraph.getResourceCount() == 1 );
			// Raw graph access would evade the GRDG borrow's source pin and byte
			// bounds. Only importSharedBuffer/addSharedBufferRead may admit it.
			CHECK( frameGraph.addAccess( { 0, firstImport, 0, 1, 0, 1, SIR_GRAPHICS_STATE_SHADER_READ, false } ) ==
			       SIR_GRAPHICS_INVALID_ARGUMENT );
			const sirFrameGraphResource_t forgedImport = {
			    gpu.hNative, SIR_GRAPHICS_HANDLE_BUFFER,      SIR_GRAPHICS_STATE_SHADER_READ, SIR_GRAPHICS_QUEUE_DIRECT, 1, 1,
			    true,        gpu.view.nPhysicalBackingId + 1, gpu.view.nNativeResourceToken };
			uint32_t forgedId = UINT32_MAX;
			CHECK( frameGraph.addResource( forgedImport, &forgedId ) == SIR_GRAPHICS_INVALID_ARGUMENT );
			CHECK( frameGraph.getResourceCount() == 1 );
			sirNativeGPUBufferLease_t mismatched = physics;
			++mismatched.view.nNativeResourceToken;
			CHECK( frameGraph.importSharedBuffer( mismatched, SIR_GRAPHICS_STATE_SHADER_READ, SIR_GRAPHICS_QUEUE_DIRECT, &secondImport ) ==
			       SIR_GRAPHICS_INVALID_HANDLE );
			mismatched = physics;
			mismatched.view.nOffset = UINT64_MAX;
			CHECK( frameGraph.importSharedBuffer( mismatched, SIR_GRAPHICS_STATE_SHADER_READ, SIR_GRAPHICS_QUEUE_DIRECT, &secondImport ) ==
			       SIR_GRAPHICS_INVALID_ARGUMENT );
			CHECK( frameGraph.addPass( { SIR_GRAPHICS_QUEUE_DIRECT, 0, true }, &passId ) == SIR_GRAPHICS_SUCCESS );
			CHECK( frameGraph.addSharedBufferRead( passId, firstImport, gpu, gpu.view.nOffset, 64, SIR_GRAPHICS_STATE_SHADER_READ ) ==
			       SIR_GRAPHICS_SUCCESS );
			CHECK( frameGraph.addSharedBufferRead( passId, firstImport, physics, physics.view.nOffset + 64, 64,
			                                       SIR_GRAPHICS_STATE_SHADER_READ ) == SIR_GRAPHICS_SUCCESS );
			CHECK( frameGraph.addSharedBufferRead( passId, firstImport, gpu, 0, 0, SIR_GRAPHICS_STATE_SHADER_READ ) ==
			       SIR_GRAPHICS_INVALID_ARGUMENT );
			CHECK( frameGraph.addSharedBufferRead( passId, firstImport, gpu, 200, 64, SIR_GRAPHICS_STATE_SHADER_READ ) ==
			       SIR_GRAPHICS_INVALID_ARGUMENT );
			CHECK( frameGraph.addAccess( { passId, firstImport, 0, 1, 0, 1, SIR_GRAPHICS_STATE_SHADER_WRITE, true } ) ==
			       SIR_GRAPHICS_INVALID_ARGUMENT );
			sirGraphicsCapabilities_t caps = {};
			caps.nDeviceGeneration = 4;
			caps.nQueueMask = 1u << SIR_GRAPHICS_QUEUE_DIRECT;
			sirFrameGraphCompiled_t compiled = {};
			CHECK( frameGraph.compile( caps, &compiled ) == SIR_GRAPHICS_SUCCESS );
			CHECK( compiled.nPassCount == 1 );
			CHECK( compiled.nSharedReadCount == 2 && gpu.view.nCompletionProviderId == 11 && physics.view.nCompletionProviderId == 12 );
			CHECK( compiled.szSharedReads[0].nPhysicalBackingId == compiled.szSharedReads[1].nPhysicalBackingId &&
			       compiled.szSharedReads[0].nCompletionProviderId == 11 && compiled.szSharedReads[1].nCompletionProviderId == 12 &&
			       compiled.szSharedReads[0].nOffsetBytes == 0 && compiled.szSharedReads[0].nBytes == 64 &&
			       compiled.szSharedReads[1].nOffsetBytes == 64 && compiled.szSharedReads[1].nBytes == 64 );
			CHECK( gpu.view.nNativeResourceToken == physics.view.nNativeResourceToken && device.nBuffersCreated == 1 );
			// A different but generation-valid native resource handle cannot release
			// a canonical buffer lease or retire its physical allocation.
			sirNativeGPUBufferLease_t forgedLease = gpu;
			forgedLease.hNative.nSlot = gpu.hNative.nSlot + 1;
			CHECK( owner.releaseBuffer( forgedLease, { 11, 4, 1, 3, { 5, 0, 0, 0 } } ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );
			forgedLease = gpu;
			forgedLease.hNative.nGeneration = gpu.hNative.nGeneration + 1;
			CHECK( owner.releaseBuffer( forgedLease, { 11, 4, 1, 3, { 5, 0, 0, 0 } } ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );
			sirNativeGPUBufferBacking_t forgedBacking = backing;
			forgedBacking.hNative.nSlot = backing.hNative.nSlot + 1;
			CHECK( owner.retireBuffer( forgedBacking, { 11, 4, 1, 3, { 5, 0, 0, 0 } } ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );
			CHECK( device.nBuffersCreated == 1 && device.nBuffersRetired == 0 );
			CHECK( owner.retireBuffer( backing, { 11, 4, 1, 3, { 5, 0, 0, 0 } } ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( owner.releaseBuffer( gpu, { 11, 4, 1, 3, { 5, 0, 0, 0 } } ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( owner.releaseBuffer( physics, { 12, 4, 1, 3, { 3, 0, 0, 0 } } ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( owner.releaseBuffer( physics, { 12, 4, 1, 3, { 3, 0, 0, 0 } } ) == SIR_RESOURCE_RESULT_INVALID_HANDLE );
		}
		CHECK( owner.pollAndObserve( { 4, 1, { 5, 0, 0 } } ) == SIR_RESOURCE_RESULT_BUSY );
		CHECK( device.nBuffersRetired == 0 );
		device.nCompleted = 5;
		CHECK( owner.pollAndObserve( { 4, 1, { 5, 0, 0 } } ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( device.nBuffersRetired == 0 );
		CHECK( registry.updateProviderCompletion( { 12, 4, 1, 3, { 3, 0, 0, 0 } } ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( device.nBuffersRetired == 1 );
		SIRResourceDerivedStats stats = {};
		CHECK( registry.getStats( &stats ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( stats.nCommittedBytes[SIR_MEMORY_SPACE_DEVICE] == 0 && stats.nLiveRepresentations == 0 );
		CHECK( pWriter->retire( hSource ) == SIR_RESOURCE_RESULT_SUCCESS );
		( void )pWriter->collectRetired( 8 );
	}
	CHECK( allocations.nLive == 0 );
	return 0;
}
