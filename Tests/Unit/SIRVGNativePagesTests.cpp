/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#include <Renderer/VirtualGeometry/SIRVGNativePages.h>
#include <Mesh/SIRVGCPUPageViews.h>
#include <Mesh/SIRVGFrameCompiler.h>
#include <Tools/GeometryCooker/SIRGeometryCooker.h>

#include <stdio.h>
#include <string.h>
#include <vector>

#define CHECK( expression )                                                                                                                \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "failed %s:%d %s\n", __FILE__, __LINE__, #expression );                                                       \
			return 1;                                                                                                                      \
		}                                                                                                                                  \
	} while ( 0 )

class SIRVGMockCommands final : public SIRGraphicsCommandList
{
public:
	std::vector<uint8_t> *pBacking = nullptr;
	std::vector<uint8_t> *pUpload = nullptr;
	uint32_t nDraws = 0;
	uint32_t nCopies = 0;
	bool bRecording = false;
	sirGraphicsQueue_t getQueue( void ) const noexcept override
	{
		return SIR_GRAPHICS_QUEUE_DIRECT;
	}
	sirGraphicsResult_t reset( void ) noexcept override
	{
		bRecording = true;
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t barrier( const sirGraphicsBarrier_t *, uint32_t ) noexcept override
	{
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t copyBuffer( sirGraphicsHandle_t, uint64_t dst, sirGraphicsHandle_t, uint64_t src, uint64_t bytes ) noexcept override
	{
		if ( pBacking == nullptr || pUpload == nullptr || bytes > pBacking->size() || dst > pBacking->size() - bytes ||
		     bytes > pUpload->size() || src > pUpload->size() - bytes )
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		memcpy( pBacking->data() + dst, pUpload->data() + src, ( size_t )bytes );
		++nCopies;
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t copyBufferTexture( const sirGraphicsBufferTextureCopy_t& ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t beginRenderPass( const sirGraphicsRenderPass_t& ) noexcept override
	{
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t endRenderPass( void ) noexcept override
	{
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t bindPipeline( sirGraphicsHandle_t ) noexcept override
	{
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t bindBindingSet( uint32_t, sirGraphicsHandle_t ) noexcept override
	{
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t bindVertexBuffer( uint32_t, sirGraphicsHandle_t, uint64_t, uint32_t ) noexcept override
	{
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t bindIndexBuffer( sirGraphicsHandle_t, uint64_t, bool ) noexcept override
	{
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t setViewport( const sirGraphicsViewport_t&, const sirGraphicsRect_t& ) noexcept override
	{
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t draw( uint32_t, uint32_t, uint32_t, uint32_t ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t drawIndexed( uint32_t indices, uint32_t instances, uint32_t, int32_t, uint32_t ) noexcept override
	{
		if ( indices == 0 || instances == 0 )
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		++nDraws;
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t dispatch( uint32_t, uint32_t, uint32_t ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t close( void ) noexcept override
	{
		bRecording = false;
		return SIR_GRAPHICS_SUCCESS;
	}
};

class SIRVGMockDevice final : public SIRRenderingDevice
{
public:
	~SIRVGMockDevice( void ) noexcept override = default;
	std::vector<uint8_t> backing;
	std::vector<uint8_t> upload;
	SIRVGMockCommands commands;
	uint64_t nFence = 0;
	bool bFenceComplete = false;

	sirGraphicsBackend_t getBackend( void ) const noexcept override
	{
		return SIR_GRAPHICS_BACKEND_AUTO;
	}
	uint32_t getDeviceGeneration( void ) const noexcept override
	{
		return 5;
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
		if ( pHandle == nullptr || desc.nSizeBytes == 0 || desc.nSizeBytes > 1024 * 1024 )
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		const bool bUpload = desc.eMemory == SIR_GRAPHICS_MEMORY_CPU_UPLOAD;
		std::vector<uint8_t>& bytes = bUpload ? upload : backing;
		bytes.resize( ( size_t )desc.nSizeBytes );
		*pHandle = { bUpload ? 2U : 1U, 1, 5, SIR_GRAPHICS_HANDLE_BUFFER };
		commands.pBacking = &backing;
		commands.pUpload = &upload;
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
	sirGraphicsResult_t mapBuffer( sirGraphicsHandle_t hBuffer, uint64_t offset, uint64_t bytes,
	                               sirGraphicsMappedBuffer_t *pMapped ) noexcept override
	{
		if ( hBuffer.nSlot != 2 || pMapped == nullptr || bytes > upload.size() || offset > upload.size() - bytes )
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		*pMapped = { upload.data() + offset, offset, bytes };
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t unmapBuffer( sirGraphicsHandle_t, uint64_t, uint64_t ) noexcept override
	{
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t flushBuffer( sirGraphicsHandle_t, uint64_t, uint64_t ) noexcept override
	{
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t invalidateBuffer( sirGraphicsHandle_t, uint64_t, uint64_t ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t acquireCommands( sirGraphicsQueue_t queue, SIRGraphicsCommandList **ppCommands ) noexcept override
	{
		if ( queue != SIR_GRAPHICS_QUEUE_DIRECT || ppCommands == nullptr )
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		*ppCommands = &commands;
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t submit( sirGraphicsQueue_t queue, const sirGraphicsSubmission_t& command,
	                            sirGraphicsCompletion_t *pCompletion ) noexcept override
	{
		if ( queue != SIR_GRAPHICS_QUEUE_DIRECT || pCompletion == nullptr || command.nCommandListCount != 1 )
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		++nFence;
		*pCompletion = { 5, SIR_GRAPHICS_QUEUE_DIRECT, nFence };
		bFenceComplete = false;
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t poll( const sirGraphicsCompletion_t& completion, bool *pCompleted ) noexcept override
	{
		if ( pCompleted == nullptr || completion.nDeviceGeneration != 5 )
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		*pCompleted = bFenceComplete;
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t wait( const sirGraphicsCompletion_t&, uint64_t ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t retireResource( sirGraphicsHandle_t, const sirGraphicsCompletionSet_t& ) noexcept override
	{
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

int main( void )
{
	// A single material provides one root meshlet. Its packed page is copied
	// to native storage exactly once, with no additional canonical mesh copy.
	const float vertices[9] = { 0, 0, 0, 1, 0, 0, 0, 1, 0 };
	const uint32_t indices[3] = { 0, 1, 2 };
	const sirGeometryCookMaterial_t material = { 0, 3, 7 };
	sirGeometryCookInput_t source = {};
	source.key = { 700, 2, 1, 0 };
	source.nResourceTypeId = 103;
	source.nPackageId = 11;
	source.nGeneration = 1;
	source.pPositionsXYZ = vertices;
	source.nVertexCount = 3;
	source.pIndices = indices;
	source.nIndexCount = 3;
	source.pMaterials = &material;
	source.nMaterialCount = 1;
	source.nMaxVerticesPerMeshlet = 32;
	source.nMaxTrianglesPerMeshlet = 16;
	source.nTargetRootTriangles = 1;
	source.fMaxSimplificationError = 0.25f;
	sirGeometryCookOutput_t cooked = {};
	CHECK( SIRGeometryCooker::cook( source, &cooked ) == SIR_PACK_OK );
	SIRMeshPackSchema schema;
	CHECK( schema.open( cooked.manifest.data(), cooked.manifest.size() ) == SIR_MESH_PACK_OK );
	std::vector<sirVGPageSlot_t> pageSlots( schema.getPageCount() );
	std::vector<sirVGClusterDrawLayout_t> clusterLayouts( schema.getClusterCount() );
	uint32_t nVertices = 0, nIndices = 0;
	CHECK( schema.getDecodedPageCounts( 0, &nVertices, &nIndices ) == SIR_MESH_PACK_OK );
	const uint64_t nativeBytes = ( uint64_t )nVertices * sizeof( sirMeshPackVertex_t ) + ( uint64_t )nIndices * 4;
	const uint64_t alignedBytes = ( nativeBytes + 15ULL ) & ~15ULL;
	const sirVGPageStorage_t storage = { pageSlots.data(), clusterLayouts.data(), ( uint32_t )pageSlots.size(),
	                                     ( uint32_t )clusterLayouts.size() };
	const sirVGPageArena_t arena = { 987, alignedBytes, alignedBytes, 5, 1 };
	SIRVGPageDirectory directory;
	CHECK( directory.initialize( schema, storage, arena ) == SIR_VG_RESIDENCY_OK );
	SIRVGMockDevice device;
	sirGraphicsBufferDesc_t desc = {};
	desc.nSizeBytes = alignedBytes + 512;
	desc.nUsage = SIR_GRAPHICS_USAGE_VERTEX | SIR_GRAPHICS_USAGE_INDEX | SIR_GRAPHICS_USAGE_COPY_DESTINATION;
	desc.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	sirGraphicsHandle_t backing = {};
	CHECK( device.createBuffer( desc, &backing ) == SIR_GRAPHICS_SUCCESS );
	desc.nSizeBytes = alignedBytes + 512;
	desc.nUsage = SIR_GRAPHICS_USAGE_COPY_SOURCE;
	desc.eMemory = SIR_GRAPHICS_MEMORY_CPU_UPLOAD;
	sirGraphicsHandle_t upload = {};
	CHECK( device.createBuffer( desc, &upload ) == SIR_GRAPHICS_SUCCESS );
	SIRVGNativePages pages;
	const sirVGNativePagesConfig_t config = { &device,      &directory,          &schema, backing, upload, 987, device.backing.size(),
	                                          alignedBytes, device.upload.size() };
	CHECK( pages.initialize( config ) == SIR_GRAPHICS_SUCCESS );
	const sirVGNativePageUpload_t root = { cooked.pages[0].data(), cooked.pages[0].size(), UINT64_MAX, 0, 0, 1 };
	CHECK( pages.submitUpload( root ) == SIR_GRAPHICS_SUCCESS );
	CHECK( !directory.isResident( 0 ) && device.commands.nCopies == 1 );
	bool bReady = true;
	CHECK( pages.pollUpload( &bReady ) == SIR_GRAPHICS_SUCCESS && !bReady );
	device.bFenceComplete = true;
	CHECK( pages.pollUpload( &bReady ) == SIR_GRAPHICS_SUCCESS && bReady );
	CHECK( directory.isResident( 0 ) );
	bool bNonzero = false;
	for ( uint64_t i = 0; i < nativeBytes; ++i ) {
		bNonzero |= device.backing[i] != 0;
	}
	CHECK( bNonzero );
	sirVGFrameInstance_t instance = { source.key, 1, { 0.25f, 0.25f, 0.0f }, 2.0f, 1.0f, 0 };
	sirVGFrameCompileInput_t input = {};
	input.pSchema = &schema;
	input.pDirectory = &directory;
	input.pInstances = &instance;
	input.nInstanceCount = 1;
	input.view.szCameraXYZ[2] = 3.0f;
	input.view.fProjectionPixelsPerUnit = 1.0f;
	input.view.fAllowedErrorPixels = 1000.0f;
	input.view.fNearDistance = 0.1f;
	input.view.nFrameId = 1;
	input.view.nRequestEpoch = 1;
	sirVGSelectedGroup_t groups[16] = {};
	sirVGDrawRange_t ranges[32] = {};
	sirVGPageDemand_t demands[16] = {};
	uint32_t stack[16] = {}, order[32] = {};
	const sirVGFrameCompileStorage_t scratch = { groups, ranges, demands, stack, order, 16, 32, 16, 16, 32 };
	sirVGFramePlan_t plan = {};
	CHECK( SIRVGFrameCompiler::compile( input, scratch, &plan ) == SIR_VG_PLAN_OK );
	CHECK( plan.nRangeCount != 0 );
	SIRFrameGraph graph;
	const sirFrameGraphPass_t drawPass = { SIR_GRAPHICS_QUEUE_DIRECT, 0, true };
	uint32_t nDrawPass = UINT32_MAX;
	CHECK( graph.addPass( drawPass, &nDrawPass ) == SIR_GRAPHICS_SUCCESS );
	uint32_t nSharedResource = UINT32_MAX;
	CHECK( pages.declareFrameGraphDrawRead( graph, nDrawPass, &nSharedResource ) == SIR_GRAPHICS_SUCCESS );
	CHECK( nSharedResource == 0 );
	CHECK( pages.declareFrameGraphDrawRead( graph, nDrawPass, &nSharedResource ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	const sirVGMaterialPipeline_t materialPipeline = { 7, { 3, 1, 5, SIR_GRAPHICS_HANDLE_PIPELINE }, {}, false };
	sirGraphicsRenderPass_t renderPass = {};
	const sirVGDrawPassSubmission_t drawing = { &plan, order, &materialPipeline, &renderPass, {}, {}, plan.nRangeCount, 1 };
	sirGraphicsCompletion_t drawnFence = {};
	CHECK( pages.submitDrawPass( drawing, &drawnFence ) == SIR_GRAPHICS_SUCCESS );
	CHECK( device.commands.nDraws == plan.nRangeCount && drawnFence.nValue == 2 );
	CHECK( pages.shutdown() == SIR_GRAPHICS_SUCCESS );
	CHECK( directory.shutdown() == SIR_VG_RESIDENCY_OK );
	// Historical fixed-function/CPU render tiers borrow the same cooked
	// page into a caller-owned bounded decoded buffer (no second cooker).
	std::vector<sirVGPageSlot_t> cpuSlots( schema.getPageCount() );
	std::vector<sirVGClusterDrawLayout_t> cpuLayouts( schema.getClusterCount() );
	std::vector<uint8_t> cpuArena( ( size_t )alignedBytes );
	SIRVGPageDirectory cpuDirectory;
	const sirVGPageStorage_t cpuStorage = { cpuSlots.data(), cpuLayouts.data(), ( uint32_t )cpuSlots.size(),
	                                        ( uint32_t )cpuLayouts.size() };
	const sirVGPageArena_t cpuPageArena = { 988, alignedBytes, alignedBytes, 1, 1 };
	CHECK( cpuDirectory.initialize( schema, cpuStorage, cpuPageArena ) == SIR_VG_RESIDENCY_OK );
	SIRVGCPUPageViews cpuViews;
	const sirVGCPUPageConfig_t cpuConfig = { &schema, &cpuDirectory, cpuArena.data(), alignedBytes, 988 };
	CHECK( cpuViews.initialize( cpuConfig ) == SIR_VG_RESIDENCY_OK );
	const SIRResourceConstBytes cookedRoot = { cooked.pages[0].data(), cooked.pages[0].size() };
	CHECK( cpuViews.install( 0, cookedRoot, 1 ) == SIR_VG_RESIDENCY_OK );
	CHECK( cpuViews.install( 0, cookedRoot, 1 ) == SIR_VG_RESIDENCY_BUSY );
	sirVGCPUPageLease_t cpuLease = {};
	CHECK( cpuViews.acquire( 0, &cpuLease ) == SIR_VG_RESIDENCY_OK );
	CHECK( cpuLease.pVertices == ( const sirMeshPackVertex_t * )cpuArena.data() );
	CHECK( cpuLease.pIndices != nullptr && cpuLease.nIndices == nIndices && cpuLease.nVertices == nVertices );
	CHECK( cpuLease.nPhysicalBackingId == 988 && cpuLease.nSourceRevision == source.key.nRevision );
	CHECK( cpuViews.release( cpuLease ) == SIR_VG_RESIDENCY_OK );
	CHECK( cpuViews.release( cpuLease ) == SIR_VG_RESIDENCY_STALE );
	CHECK( cpuViews.shutdown() == SIR_VG_RESIDENCY_OK );
	CHECK( cpuDirectory.shutdown() == SIR_VG_RESIDENCY_OK );
	puts( "SIRVG native page upload, real copy fence, CPU cut and indexed submission passed" );
	puts( "SIRVG native page upload, real copy fence, CPU cut and indexed submission passed" );
	return 0;
}
