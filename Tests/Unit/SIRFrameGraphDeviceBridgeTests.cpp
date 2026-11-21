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

class SIRFrameGraphTestCommands final : public SIRGraphicsCommandList
{
public:
	explicit SIRFrameGraphTestCommands( sirGraphicsQueue_t eQueue ) noexcept : m_eQueue( eQueue )
	{
	}

	~SIRFrameGraphTestCommands( void ) noexcept override = default;

	sirGraphicsQueue_t getQueue( void ) const noexcept override
	{
		return m_eQueue;
	}
	sirGraphicsResult_t reset( void ) noexcept override
	{
		++m_nResets;
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t barrier( const sirGraphicsBarrier_t *pBarriers, uint32_t nCount ) noexcept override
	{
		if ( pBarriers == nullptr || nCount == 0 || nCount > 32 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		m_nBarriers += nCount;
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t close( void ) noexcept override
	{
		++m_nCloses;
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t copyBuffer( sirGraphicsHandle_t, uint64_t, sirGraphicsHandle_t, uint64_t, uint64_t ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t copyBufferTexture( const sirGraphicsBufferTextureCopy_t& ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t beginRenderPass( const sirGraphicsRenderPass_t& ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t endRenderPass( void ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t bindPipeline( sirGraphicsHandle_t ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t bindBindingSet( uint32_t, sirGraphicsHandle_t ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t bindVertexBuffer( uint32_t, sirGraphicsHandle_t, uint64_t, uint32_t ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t bindIndexBuffer( sirGraphicsHandle_t, uint64_t, bool ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t setViewport( const sirGraphicsViewport_t&, const sirGraphicsRect_t& ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t draw( uint32_t, uint32_t, uint32_t, uint32_t ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t drawIndexed( uint32_t, uint32_t, uint32_t, int32_t, uint32_t ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t dispatch( uint32_t, uint32_t, uint32_t ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}

	uint32_t m_nResets = 0;
	uint32_t m_nBarriers = 0;
	uint32_t m_nCloses = 0;

protected:
private:
	sirGraphicsQueue_t m_eQueue;
};

class SIRFrameGraphTestDevice final : public SIRRenderingDevice
{
public:
	SIRFrameGraphTestDevice( void ) noexcept : m_Direct( SIR_GRAPHICS_QUEUE_DIRECT ), m_Copy( SIR_GRAPHICS_QUEUE_COPY )
	{
	}
	~SIRFrameGraphTestDevice( void ) noexcept override = default;

	sirGraphicsBackend_t getBackend( void ) const noexcept override
	{
		return SIR_GRAPHICS_BACKEND_VULKAN;
	}
	uint32_t getDeviceGeneration( void ) const noexcept override
	{
		return 7;
	}
	sirGraphicsResult_t queryCapabilities( sirGraphicsCapabilities_t *pCaps ) const noexcept override
	{
		if ( pCaps == nullptr ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		*pCaps = {};
		pCaps->nDeviceGeneration = 7;
		pCaps->nQueueMask = ( 1U << SIR_GRAPHICS_QUEUE_DIRECT ) | ( 1U << SIR_GRAPHICS_QUEUE_COPY );
		pCaps->nFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_COPY_QUEUE;
		pCaps->eBackend = getBackend();
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t acquireCommands( sirGraphicsQueue_t eQueue, SIRGraphicsCommandList **ppCommands ) noexcept override
	{
		if ( ppCommands == nullptr ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( eQueue == SIR_GRAPHICS_QUEUE_DIRECT ) {
			*ppCommands = &m_Direct;
		} else if ( eQueue == SIR_GRAPHICS_QUEUE_COPY ) {
			*ppCommands = &m_Copy;
		} else {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t submit( sirGraphicsQueue_t eQueue, const sirGraphicsSubmission_t& submission,
	                            sirGraphicsCompletion_t *pCompletion ) noexcept override
	{
		if ( pCompletion == nullptr || submission.nCommandListCount != 1 || submission.ppCommandLists == nullptr ||
		     submission.ppCommandLists[0] == nullptr || submission.ppCommandLists[0]->getQueue() != eQueue ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( eQueue == SIR_GRAPHICS_QUEUE_DIRECT &&
		     ( submission.nWaitCount != 1 || submission.pWaits == nullptr || submission.pWaits[0].eQueue != SIR_GRAPHICS_QUEUE_COPY ||
		       submission.pWaits[0].nValue != 1 ) ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( eQueue == SIR_GRAPHICS_QUEUE_COPY && submission.nWaitCount != 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		*pCompletion = { 7, eQueue, 1 };
		++m_nSubmissions;
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t queryPipelineModelCaps( sirGraphicsPipelineModel_t, sirGraphicsABIPipelineModelCaps_t * ) const noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t queryMemoryBudget( sirGraphicsMemoryBudget_t * ) const noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t createBuffer( const sirGraphicsBufferDesc_t&, sirGraphicsHandle_t * ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
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
	sirGraphicsResult_t poll( const sirGraphicsCompletion_t& token, bool *pCompleted ) noexcept override
	{
		if ( pCompleted == nullptr || token.nDeviceGeneration != 7 || token.nValue != 1 ||
		     ( token.eQueue != SIR_GRAPHICS_QUEUE_DIRECT && token.eQueue != SIR_GRAPHICS_QUEUE_COPY ) ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		++m_nPollCount;
		*pCompleted = m_bFenceCompleted;
		return SIR_GRAPHICS_SUCCESS;
	}
	sirGraphicsResult_t wait( const sirGraphicsCompletion_t&, uint64_t ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGraphicsResult_t retireResource( sirGraphicsHandle_t, const sirGraphicsCompletionSet_t& ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
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

	SIRFrameGraphTestCommands m_Direct;
	SIRFrameGraphTestCommands m_Copy;
	uint32_t m_nSubmissions = 0;
	uint32_t m_nPollCount = 0;
	bool m_bFenceCompleted = false;

protected:
private:
};

static sirGraphicsResult_t mockDeviceRecord( uint32_t nPass, SIRGraphicsCommandList *pCommands, void *pContext ) noexcept
{
	SIRFrameGraphTestDevice *pDevice = ( SIRFrameGraphTestDevice * )pContext;
	const sirGraphicsQueue_t eExpected = nPass == 0 ? SIR_GRAPHICS_QUEUE_COPY : SIR_GRAPHICS_QUEUE_DIRECT;
	return pDevice != nullptr && pCommands != nullptr && pCommands->getQueue() == eExpected ? SIR_GRAPHICS_SUCCESS
	                                                                                        : SIR_GRAPHICS_INVALID_ARGUMENT;
}

static bool testDeviceBridge( void ) noexcept
{
	SIRFrameGraph graph;
	uint32_t nIndex = 0;
	sirFrameGraphResource_t resource = {};
	resource.eType = SIR_GRAPHICS_HANDLE_BUFFER;
	resource.nMipLevels = 1;
	resource.nArrayLayers = 1;
	resource.eInitialState = SIR_GRAPHICS_STATE_UNDEFINED;
	resource.eInitialQueue = SIR_GRAPHICS_QUEUE_DIRECT;
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
	access.eState = SIR_GRAPHICS_STATE_SHADER_READ;
	access.bWrite = false;
	if ( graph.addAccess( access ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sirGraphicsCapabilities_t caps = {};
	caps.nDeviceGeneration = 7;
	caps.nQueueMask = ( 1U << SIR_GRAPHICS_QUEUE_DIRECT ) | ( 1U << SIR_GRAPHICS_QUEUE_COPY );
	caps.nFeatureMask = SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_COPY_QUEUE;
	sirFrameGraphCompiled_t compiled = {};
	if ( graph.compile( caps, &compiled ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	SIRFrameGraphTestDevice device;
	const sirGraphicsHandle_t handles[] = { { 8, 1, 7, SIR_GRAPHICS_HANDLE_BUFFER } };
	sirFrameGraphDeviceInput_t input = { &device, handles, 1, mockDeviceRecord, &device };
	sirFrameGraphNativeOutput_t output = {};
	if ( graph.submitDevice( compiled, input, &output ) != SIR_GRAPHICS_SUCCESS || output.nSubmittedPassCount != 2 ||
	     output.completion.nQueueMask != caps.nQueueMask || output.completion.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT] != 1 ||
	     output.completion.szQueueValues[SIR_GRAPHICS_QUEUE_COPY] != 1 || device.m_nSubmissions != 2 || device.m_Copy.m_nResets != 1 ||
	     device.m_Direct.m_nResets != 1 || device.m_Copy.m_nCloses != 1 || device.m_Direct.m_nCloses != 1 ||
	     device.m_Copy.m_nBarriers != 1 || device.m_Direct.m_nBarriers != 1 ) {
		return false;
	}
	sirFrameGraphProviderLaneMap_t mapping = {};
	mapping.nProviderId = 9;
	mapping.nProviderGeneration = 2;
	mapping.nDeviceGeneration = 7;
	mapping.nLaneCount = 3;
	mapping.szQueueToLane[SIR_GRAPHICS_QUEUE_DIRECT] = 0;
	mapping.szQueueToLane[SIR_GRAPHICS_QUEUE_COPY] = 2;
	sirProviderCompletionSet_t required = {};
	if ( sirFrameGraphProviderRequiredCompletion( output, mapping, &required ) != SIR_GRAPHICS_SUCCESS || required.nLaneMask != 5 ) {
		return false;
	}
	sirProviderCompletionSet_t observed = {};
	if ( sirFrameGraphProviderObservedCompletion( &device, output, mapping, &observed ) != SIR_GRAPHICS_BUSY || observed.nLaneMask != 0 ||
	     device.m_nPollCount == 0 ) {
		return false;
	}
	device.m_bFenceCompleted = true;
	if ( sirFrameGraphProviderObservedCompletion( &device, output, mapping, &observed ) != SIR_GRAPHICS_SUCCESS ||
	     !sirProviderCompletionSatisfied( required, observed ) || observed.nLaneMask != 5 ) {
		return false;
	}
	output.bUnknownCompletion = true;
	return sirFrameGraphProviderObservedCompletion( &device, output, mapping, &observed ) == SIR_GRAPHICS_BUSY && observed.nLaneMask == 0;
}

int main( void )
{
	if ( !testDeviceBridge() ) {
		fputs( "FrameGraph C++ device bridge FAILED\n", stderr );
		return 1;
	}
	puts( "FrameGraph C++ device bridge PASS" );
	return 0;
}
