/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#include <Renderer/Drivers/Vulkan/SIRVulkanGraphics.h>
#include <Renderer/SIRSharedGPUBackingOwner.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#define CHECK( x )                                                                                                                         \
	do {                                                                                                                                   \
		if ( !( x ) ) {                                                                                                                    \
			fprintf( stderr, "%s:%d %s\n", __FILE__, __LINE__, #x );                                                                       \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( false )
static void *allocBytes( void *, uint64_t n, uint32_t a, uint32_t ) noexcept
{
	void *p = nullptr;
	return posix_memalign( &p, a < sizeof( void * ) ? sizeof( void * ) : a, ( size_t )n ) == 0 ? p : nullptr;
}
static void freeBytes( void *, void *p, uint64_t, uint32_t, uint32_t ) noexcept
{
	free( p );
}
static bool freeNothing( void *, uint64_t ) noexcept
{
	return true;
}

int main( void )
{
	SIRVulkanDriver driver;
	sirGraphicsDeviceCreateDesc_t create = {};
	create.bHeadless = true;
	create.bAllowSoftware = true;
	create.nFramesInFlight = 2;
	create.nResidentBudgetBytes = 8ULL << 20;
	create.nUploadBudgetBytes = 2ULL << 20;
	create.nReadbackBudgetBytes = 2ULL << 20;
	SIRRenderingDevice *pDevice = nullptr;
	const sirGraphicsResult_t result = driver.createDevice( create, &pDevice );
	if ( result == SIR_GRAPHICS_UNSUPPORTED ) {
		puts( "SKIP: native Vulkan unavailable" );
		return 77;
	}
	CHECK( result == SIR_GRAPHICS_SUCCESS && pDevice );
	{
		SIRResourceGraphConfig config = {};
		config.m_Allocator = { nullptr, allocBytes, freeBytes };
		config.m_nMetadataDomain = 1;
		config.m_nMaxResources = 4;
		config.m_nMaxStaging = 4;
		config.m_nMaxProviderPins = 16;
		config.m_nMaxCommittedBytes = 16384;
		SIRResourceGraph graph( config );
		CHECK( graph.isReady() );
		SIRResourceDerivedConfig rc = { config.m_Allocator, 1, 8, 4, 8192, 16 };
		SIRResourceDerivedRegistry registry( graph, rc );
		CHECK( registry.isReady() );
		SIRSharedGPUBackingOwner owner;
		CHECK( owner.initialize( registry, *pDevice, 0x10203040, 17 ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( registry.registerProvider( 18, pDevice->getDeviceGeneration(), SIR_GRAPHICS_QUEUE_COUNT, { nullptr, freeNothing } ) ==
		       SIR_RESOURCE_RESULT_SUCCESS );
		SIRResourceGraphWriter *pWriter = graph.getWriter();
		CHECK( pWriter );
		SIRResourceStagingHandle stage = {};
		SIRResourceMutableBytes bytes = {};
		CHECK( pWriter->allocateStaging( 256, 16, 1, &stage, &bytes ) == SIR_RESOURCE_RESULT_SUCCESS );
		memset( bytes.m_pData, 0x5A, 256 );
		const SIRResourceChunkPublication chunk = { { 1 }, { 0xD012, 0xA89C, 1, 0 }, stage, false };
		const SIRResourcePublication pub = { { 991, 17, 0 }, SIR_RESOURCE_KIND_BLOB, 1, 1, &chunk, 1, nullptr, 0 };
		SIRResourceHandle hSource = {};
		CHECK( pWriter->publish( pub, &hSource ) == SIR_RESOURCE_RESULT_SUCCESS );
		sirGraphicsMemoryBudget_t before = {}, after = {}, again = {};
		CHECK( pDevice->queryMemoryBudget( &before ) == SIR_GRAPHICS_SUCCESS );
		sirNativeGPUBufferBacking_t first = {}, second = {};
		sirNativeGPUBufferLease_t gpu = {}, physics = {};
		uint64_t nRealCompletion = 0;
		{
			SIRResourceReadScope scope( graph );
			SIRResourceView source;
			CHECK( graph.resolve( scope, hSource, &source ) == SIR_RESOURCE_RESULT_SUCCESS );
			const sirGraphicsBufferDesc_t buffer = {
			    256, 0, SIR_GRAPHICS_USAGE_STORAGE | SIR_GRAPHICS_USAGE_COPY_DESTINATION | SIR_GRAPHICS_USAGE_COPY_SOURCE,
			    SIR_GRAPHICS_MEMORY_GPU_LOCAL };
			CHECK( owner.createOrReuseBuffer( scope, source, buffer, 1, 1, &first ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( pDevice->queryMemoryBudget( &after ) == SIR_GRAPHICS_SUCCESS );
			CHECK( owner.createOrReuseBuffer( scope, source, buffer, 1, 1, &second ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( pDevice->queryMemoryBudget( &again ) == SIR_GRAPHICS_SUCCESS );
			CHECK( first.nNativeToken == second.nNativeToken && first.hNative.nSlot == second.hNative.nSlot );
			CHECK( again.nCommittedBytes == after.nCommittedBytes );
			const sirSharedDeviceViewRequest_t view = { 0x10203040,
			                                            source.getRevision(),
			                                            graph.getStats().nPublicationEpoch,
			                                            0,
			                                            256,
			                                            pDevice->getDeviceGeneration(),
			                                            1,
			                                            1,
			                                            SIR_GRAPHICS_USAGE_STORAGE,
			                                            1,
			                                            1,
			                                            17 };
			CHECK( owner.acquireBuffer( scope, source, view, &gpu ) == SIR_RESOURCE_RESULT_SUCCESS );
			sirSharedDeviceViewRequest_t secondView = view;
			secondView.nConsumerId = 2;
			secondView.nCompletionProviderId = 18;
			CHECK( owner.acquireBuffer( scope, source, secondView, &physics ) == SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( gpu.hNative.nGeneration == physics.hNative.nGeneration && gpu.hNative.nSlot == physics.hNative.nSlot );
			CHECK( gpu.view.nPhysicalBackingId == physics.view.nPhysicalBackingId );
			const uint32_t gen = pDevice->getDeviceGeneration();
			sirGraphicsBufferDesc_t uploadDesc = { 256, 0, SIR_GRAPHICS_USAGE_COPY_SOURCE, SIR_GRAPHICS_MEMORY_CPU_UPLOAD };
			sirGraphicsBufferDesc_t readbackDesc = { 256, 0, SIR_GRAPHICS_USAGE_COPY_DESTINATION, SIR_GRAPHICS_MEMORY_CPU_READBACK };
			sirGraphicsHandle_t hUpload = {}, hReadback = {};
			CHECK( pDevice->createBuffer( uploadDesc, &hUpload ) == SIR_GRAPHICS_SUCCESS );
			CHECK( pDevice->createBuffer( readbackDesc, &hReadback ) == SIR_GRAPHICS_SUCCESS );
			sirGraphicsMappedBuffer_t mapped = {};
			CHECK( pDevice->mapBuffer( hUpload, 0, 256, &mapped ) == SIR_GRAPHICS_SUCCESS && mapped.pData );
			memset( mapped.pData, 0x5A, 256 );
			CHECK( pDevice->flushBuffer( hUpload, 0, 256 ) == SIR_GRAPHICS_SUCCESS );
			CHECK( pDevice->unmapBuffer( hUpload, 0, 256 ) == SIR_GRAPHICS_SUCCESS );
			SIRGraphicsCommandList *pUploadCommands = nullptr;
			CHECK( pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_DIRECT, &pUploadCommands ) == SIR_GRAPHICS_SUCCESS && pUploadCommands );
			CHECK( pUploadCommands->copyBuffer( first.hNative, 0, hUpload, 0, 256 ) == SIR_GRAPHICS_SUCCESS );
			CHECK( pUploadCommands->close() == SIR_GRAPHICS_SUCCESS );
			sirGraphicsSubmission_t submission = {};
			submission.ppCommandLists = &pUploadCommands;
			submission.nCommandListCount = 1;
			sirGraphicsCompletion_t uploaded = {};
			CHECK( pDevice->submit( SIR_GRAPHICS_QUEUE_DIRECT, submission, &uploaded ) == SIR_GRAPHICS_SUCCESS );
			SIRGraphicsCommandList *pReadbackCommands = nullptr;
			CHECK( pDevice->acquireCommands( SIR_GRAPHICS_QUEUE_DIRECT, &pReadbackCommands ) == SIR_GRAPHICS_SUCCESS && pReadbackCommands );
			CHECK( pReadbackCommands->copyBuffer( hReadback, 0, first.hNative, 0, 256 ) == SIR_GRAPHICS_SUCCESS );
			CHECK( pReadbackCommands->close() == SIR_GRAPHICS_SUCCESS );
			sirGraphicsSubmission_t readbackSubmission = {};
			readbackSubmission.ppCommandLists = &pReadbackCommands;
			readbackSubmission.nCommandListCount = 1;
			readbackSubmission.pWaits = &uploaded;
			readbackSubmission.nWaitCount = 1;
			sirGraphicsCompletion_t completed = {};
			CHECK( pDevice->submit( SIR_GRAPHICS_QUEUE_DIRECT, readbackSubmission, &completed ) == SIR_GRAPHICS_SUCCESS );
			CHECK( completed.nValue > uploaded.nValue && pDevice->wait( completed, 5000000000ULL ) == SIR_GRAPHICS_SUCCESS );
			nRealCompletion = completed.nValue;
			CHECK( pDevice->mapBuffer( hReadback, 0, 256, &mapped ) == SIR_GRAPHICS_SUCCESS && mapped.pData );
			CHECK( pDevice->invalidateBuffer( hReadback, 0, 256 ) == SIR_GRAPHICS_SUCCESS );
			for ( uint32_t i = 0; i < 256; ++i )
				CHECK( ( ( uint8_t * )mapped.pData )[i] == 0x5A );
			CHECK( pDevice->unmapBuffer( hReadback, 0, 256 ) == SIR_GRAPHICS_SUCCESS );
			const sirGraphicsCompletionSet_t nativeCompleted = { gen, 1, { completed.nValue, 0, 0 } };
			CHECK( pDevice->retireResource( hUpload, nativeCompleted ) == SIR_GRAPHICS_SUCCESS );
			CHECK( pDevice->retireResource( hReadback, nativeCompleted ) == SIR_GRAPHICS_SUCCESS );
			CHECK( owner.retireBuffer( first, { 17, gen, 1, SIR_GRAPHICS_QUEUE_COUNT, { completed.nValue, 0, 0, 0 } } ) ==
			       SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( owner.releaseBuffer( gpu, { 17, gen, 1, SIR_GRAPHICS_QUEUE_COUNT, { completed.nValue, 0, 0, 0 } } ) ==
			       SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( owner.releaseBuffer( physics, { 18, gen, 1, SIR_GRAPHICS_QUEUE_COUNT, { 0, 0, 0, 0 } } ) ==
			       SIR_RESOURCE_RESULT_SUCCESS );
			CHECK( registry.updateProviderCompletion( { 18, gen, 1, SIR_GRAPHICS_QUEUE_COUNT, { 0, 0, 0, 0 } } ) ==
			       SIR_RESOURCE_RESULT_SUCCESS );
		}
		// Poll only the real submitted native timeline value.
		CHECK( owner.pollAndObserve( { pDevice->getDeviceGeneration(), 1, { nRealCompletion, 0, 0 } } ) == SIR_RESOURCE_RESULT_SUCCESS );
		SIRResourceDerivedStats stats = {};
		CHECK( registry.getStats( &stats ) == SIR_RESOURCE_RESULT_SUCCESS );
		CHECK( stats.nCommittedBytes[SIR_MEMORY_SPACE_DEVICE] == 0 && stats.nLiveRepresentations == 0 );
		CHECK( pWriter->retire( hSource ) == SIR_RESOURCE_RESULT_SUCCESS );
		( void )pWriter->collectRetired( 4 );
	}
	CHECK( driver.destroyDevice( pDevice ) == SIR_GRAPHICS_SUCCESS );
	return 0;
}
