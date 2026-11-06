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

#include <Renderer/Visibility/SIRHiZHistory.h>
#include <stdio.h>

#define REQUIRE( condition )                                                                                                               \
	if ( !( condition ) ) {                                                                                                                \
		printf( "FAIL: %s at line %d\n", #condition, __LINE__ );                                                                           \
		return false;                                                                                                                      \
	}

class SIRHiZMockDevice : public SIRRenderingDevice
{
public:
	SIRHiZMockDevice( void ) noexcept = default;
	~SIRHiZMockDevice( void ) noexcept override = default;

	sirGraphicsBackend_t getBackend( void ) const noexcept override
	{
		return SIR_GRAPHICS_BACKEND_AUTO;
	}

	uint32_t getDeviceGeneration( void ) const noexcept override
	{
		return m_nGeneration;
	}

	sirGraphicsResult_t createTexture( const sirGraphicsTextureDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept override
	{
		++m_nCreateCalls;
		if ( m_eCreateResult != SIR_GRAPHICS_SUCCESS ) {
			return m_eCreateResult;
		}
		m_lastTexture = desc;
		*pHandle = { 11, m_nCreateCalls, m_nGeneration, SIR_GRAPHICS_HANDLE_TEXTURE };
		return SIR_GRAPHICS_SUCCESS;
	}

	sirGraphicsResult_t retireResource( sirGraphicsHandle_t hResource, const sirGraphicsCompletionSet_t& completion ) noexcept override
	{
		++m_nRetireCalls;
		m_lastRetired = hResource;
		m_lastCompletion = completion;
		return m_eRetireResult;
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

	sirGraphicsResult_t createBuffer( const sirGraphicsBufferDesc_t&, sirGraphicsHandle_t * ) noexcept override
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

	sirGraphicsResult_t poll( const sirGraphicsCompletion_t&, bool * ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}

	sirGraphicsResult_t wait( const sirGraphicsCompletion_t&, uint64_t ) noexcept override
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
		return SIR_GRAPHICS_UNSUPPORTED;
	}

	uint32_t m_nGeneration = 4;
	uint32_t m_nCreateCalls = 0;
	uint32_t m_nRetireCalls = 0;
	sirGraphicsResult_t m_eCreateResult = SIR_GRAPHICS_SUCCESS;
	sirGraphicsResult_t m_eRetireResult = SIR_GRAPHICS_SUCCESS;
	sirGraphicsTextureDesc_t m_lastTexture = {};
	sirGraphicsHandle_t m_lastRetired = {};
	sirGraphicsCompletionSet_t m_lastCompletion = {};

protected:
private:
};

static sirVisibilityView_t sirHiZTestView( void ) noexcept
{
	sirVisibilityView_t view = {};
	view.nViewId = 7;
	view.nFrameIndex = 100;
	view.nProjectionRevision = 2;
	view.nWorldOriginRevision = 3;
	view.nDepthProducerRevision = 4;
	view.nSceneResetRevision = 5;
	view.nDeviceGeneration = 4;
	view.nWidth = 13;
	view.nHeight = 9;
	view.nSampleCount = 1;
	view.eDepthFormat = SIR_GRAPHICS_FORMAT_D32_FLOAT;
	view.eDepthConvention = SIR_VISIBILITY_REVERSE_Z;
	return view;
}

static sirHiZDispatchPlan_t sirHiZTestPlan( const sirVisibilityView_t& view ) noexcept
{
	sirHiZLayout_t layout = {};
	( void )sirHiZCalculateLayout( view.nWidth, view.nHeight, &layout );
	sirHiZDispatchPlan_t plan = {};
	plan.pyramid = { view.nWidth,
	                 view.nHeight,
	                 1,
	                 1,
	                 layout.nMipCount,
	                 1,
	                 SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_STORAGE,
	                 SIR_GRAPHICS_TEXTURE_2D,
	                 SIR_GRAPHICS_FORMAT_R32_FLOAT,
	                 SIR_GRAPHICS_MEMORY_GPU_LOCAL };
	plan.nPassCount = layout.nMipCount;
	return plan;
}

static bool sirHiZTestPublicationAndRetirement( void ) noexcept
{
	SIRHiZMockDevice device;
	const sirVisibilityView_t producer = sirHiZTestView();
	const sirHiZDispatchPlan_t plan = sirHiZTestPlan( producer );
	sirHiZHistoryBacking_t backing = {};
	REQUIRE( sirHiZHistoryCreate( device, plan, producer, &backing ) == SIR_GRAPHICS_SUCCESS );
	REQUIRE( backing.hPyramid.eType == SIR_GRAPHICS_HANDLE_TEXTURE );
	REQUIRE( !backing.bPublished && backing.lastUse.nQueueMask == 0 );
	REQUIRE( device.m_lastTexture.nMipLevels == 5 && device.m_nCreateCalls == 1 );
	REQUIRE( sirHiZHistoryCreate( device, plan, producer, &backing ) == SIR_GRAPHICS_BUSY );

	sirVisibilityView_t next = producer;
	++next.nFrameIndex;
	sirGraphicsHandle_t hRead = { 49, 1, 1, SIR_GRAPHICS_HANDLE_BUFFER };
	sirGraphicsCompletion_t wait = { 1, SIR_GRAPHICS_QUEUE_COPY, 999 };
	REQUIRE( !sirHiZHistoryTryRead( backing, next, &hRead, &wait ) );
	REQUIRE( hRead.nSlot == 49 && wait.nValue == 999 );
	REQUIRE( sirHiZHistoryTrackRead( &backing, { 4, SIR_GRAPHICS_QUEUE_DIRECT, 2 } ) == SIR_GRAPHICS_BUSY );
	REQUIRE( sirHiZHistoryPublish( &backing, { 4, SIR_GRAPHICS_QUEUE_DIRECT, 0 } ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	REQUIRE( sirHiZHistoryPublish( &backing, { 4, SIR_GRAPHICS_QUEUE_COPY, 1 } ) == SIR_GRAPHICS_UNSUPPORTED );
	REQUIRE( sirHiZHistoryPublish( &backing, { 5, SIR_GRAPHICS_QUEUE_COMPUTE, 1 } ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	REQUIRE( sirHiZHistoryPublish( &backing, { 4, SIR_GRAPHICS_QUEUE_COMPUTE, 11 } ) == SIR_GRAPHICS_SUCCESS );
	REQUIRE( sirHiZHistoryPublish( &backing, { 4, SIR_GRAPHICS_QUEUE_COMPUTE, 12 } ) == SIR_GRAPHICS_BUSY );
	REQUIRE( sirHiZHistoryTryRead( backing, next, &hRead, &wait ) );
	REQUIRE( hRead.nSlot == 11 && wait.nValue == 11 && wait.eQueue == SIR_GRAPHICS_QUEUE_COMPUTE );

	REQUIRE( sirHiZHistoryTrackRead( &backing, { 4, SIR_GRAPHICS_QUEUE_DIRECT, 29 } ) == SIR_GRAPHICS_SUCCESS );
	REQUIRE( sirHiZHistoryTrackRead( &backing, { 4, SIR_GRAPHICS_QUEUE_DIRECT, 20 } ) == SIR_GRAPHICS_SUCCESS );
	REQUIRE( sirHiZHistoryTrackRead( &backing, { 4, SIR_GRAPHICS_QUEUE_COMPUTE, 15 } ) == SIR_GRAPHICS_SUCCESS );
	REQUIRE( sirHiZHistoryTrackRead( &backing, { 4, SIR_GRAPHICS_QUEUE_COUNT, 8 } ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	REQUIRE( sirHiZHistoryTrackRead( &backing, { 5, SIR_GRAPHICS_QUEUE_COPY, 9 } ) == SIR_GRAPHICS_INVALID_ARGUMENT );

	device.m_eRetireResult = SIR_GRAPHICS_BUSY;
	REQUIRE( sirHiZHistoryRetire( device, &backing ) == SIR_GRAPHICS_BUSY );
	REQUIRE( backing.bPublished && backing.hPyramid.nGeneration != 0 );
	REQUIRE( device.m_lastCompletion.nQueueMask == 3 );
	REQUIRE( device.m_lastCompletion.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT] == 29 );
	REQUIRE( device.m_lastCompletion.szQueueValues[SIR_GRAPHICS_QUEUE_COMPUTE] == 15 );
	device.m_eRetireResult = SIR_GRAPHICS_SUCCESS;
	REQUIRE( sirHiZHistoryRetire( device, &backing ) == SIR_GRAPHICS_SUCCESS );
	REQUIRE( backing.hPyramid.nGeneration == 0 && !backing.bPublished );
	REQUIRE( sirHiZHistoryTrackRead( &backing, { 4, SIR_GRAPHICS_QUEUE_DIRECT, 30 } ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	REQUIRE( sirHiZHistoryCreate( device, plan, producer, &backing ) == SIR_GRAPHICS_SUCCESS );
	REQUIRE( device.m_nCreateCalls == 2 && backing.hPyramid.nGeneration == 2 );
	return true;
}

static bool sirHiZTestHistoryRejection( void ) noexcept
{
	SIRHiZMockDevice device;
	const sirVisibilityView_t producer = sirHiZTestView();
	const sirHiZDispatchPlan_t plan = sirHiZTestPlan( producer );
	sirHiZHistoryBacking_t backing = {};
	REQUIRE( sirHiZHistoryCreate( device, plan, producer, &backing ) == SIR_GRAPHICS_SUCCESS );
	REQUIRE( sirHiZHistoryPublish( &backing, { 4, SIR_GRAPHICS_QUEUE_DIRECT, 17 } ) == SIR_GRAPHICS_SUCCESS );
	sirVisibilityView_t next = producer;
	++next.nFrameIndex;
	sirGraphicsHandle_t hRead = {};
	sirGraphicsCompletion_t wait = {};
	REQUIRE( sirHiZHistoryTryRead( backing, next, &hRead, &wait ) );
	next.bCameraCut = true;
	REQUIRE( !sirHiZHistoryTryRead( backing, next, &hRead, &wait ) );
	next.bCameraCut = false;
	++next.nProjectionRevision;
	REQUIRE( !sirHiZHistoryTryRead( backing, next, &hRead, &wait ) );
	--next.nProjectionRevision;
	++next.nWorldOriginRevision;
	REQUIRE( !sirHiZHistoryTryRead( backing, next, &hRead, &wait ) );
	--next.nWorldOriginRevision;
	++next.nDepthProducerRevision;
	REQUIRE( !sirHiZHistoryTryRead( backing, next, &hRead, &wait ) );
	--next.nDepthProducerRevision;
	++next.nSceneResetRevision;
	REQUIRE( !sirHiZHistoryTryRead( backing, next, &hRead, &wait ) );
	--next.nSceneResetRevision;
	++next.nWidth;
	REQUIRE( !sirHiZHistoryTryRead( backing, next, &hRead, &wait ) );
	--next.nWidth;
	next.nDeviceGeneration = 5;
	REQUIRE( !sirHiZHistoryTryRead( backing, next, &hRead, &wait ) );
	REQUIRE( sirHiZHistoryDiscardLost( &backing, 4 ) == SIR_GRAPHICS_BUSY );
	REQUIRE( sirHiZHistoryRetire( device, &backing ) == SIR_GRAPHICS_SUCCESS );
	REQUIRE( !backing.bPublished );
	return true;
}

static bool sirHiZTestFailureAndLoss( void ) noexcept
{
	SIRHiZMockDevice device;
	sirVisibilityView_t producer = sirHiZTestView();
	sirHiZDispatchPlan_t plan = sirHiZTestPlan( producer );
	sirHiZHistoryBacking_t backing = {};
	REQUIRE( sirHiZHistoryCreate( device, plan, producer, nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	plan.pyramid.nMipLevels = 99;
	REQUIRE( sirHiZHistoryCreate( device, plan, producer, &backing ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	plan.pyramid.nMipLevels = 5;
	producer.nSampleCount = 4;
	REQUIRE( sirHiZHistoryCreate( device, plan, producer, &backing ) == SIR_GRAPHICS_UNSUPPORTED );
	producer.nSampleCount = 1;
	producer.eDepthFormat = SIR_GRAPHICS_FORMAT_R32_FLOAT;
	REQUIRE( sirHiZHistoryCreate( device, plan, producer, &backing ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	producer.eDepthFormat = SIR_GRAPHICS_FORMAT_D32_FLOAT;
	producer.nDeviceGeneration = 5;
	REQUIRE( sirHiZHistoryCreate( device, plan, producer, &backing ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	producer.nDeviceGeneration = 4;
	device.m_eCreateResult = SIR_GRAPHICS_OUT_OF_MEMORY;
	REQUIRE( sirHiZHistoryCreate( device, plan, producer, &backing ) == SIR_GRAPHICS_OUT_OF_MEMORY );
	REQUIRE( backing.hPyramid.nGeneration == 0 );
	device.m_eCreateResult = SIR_GRAPHICS_SUCCESS;
	REQUIRE( sirHiZHistoryCreate( device, plan, producer, &backing ) == SIR_GRAPHICS_SUCCESS );
	device.m_nGeneration = 5;
	REQUIRE( sirHiZHistoryRetire( device, &backing ) == SIR_GRAPHICS_DEVICE_LOST );
	REQUIRE( device.m_nRetireCalls == 0 );
	REQUIRE( sirHiZHistoryDiscardLost( &backing, 0 ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	REQUIRE( sirHiZHistoryDiscardLost( &backing, 5 ) == SIR_GRAPHICS_SUCCESS );
	REQUIRE( backing.hPyramid.nGeneration == 0 && !backing.bPublished );
	return true;
}

static bool sirHiZTestMultipleViews( void ) noexcept
{
	SIRHiZMockDevice device;
	const uint32_t szWidths[] = { 1, 1, 7, 1920, 2560, 3840 };
	const uint32_t szHeights[] = { 1, 31, 3, 1080, 1440, 2160 };
	for ( uint32_t iCase = 0; iCase < sizeof( szWidths ) / sizeof( szWidths[0] ); ++iCase ) {
		sirVisibilityView_t view = sirHiZTestView();
		view.nWidth = szWidths[iCase];
		view.nHeight = szHeights[iCase];
		view.nViewId = iCase + 1;
		sirHiZHistoryBacking_t backing = {};
		const sirHiZDispatchPlan_t plan = sirHiZTestPlan( view );
		REQUIRE( sirHiZHistoryCreate( device, plan, view, &backing ) == SIR_GRAPHICS_SUCCESS );
		REQUIRE( sirHiZHistoryPublish( &backing, { 4, SIR_GRAPHICS_QUEUE_DIRECT, 100 + iCase } ) == SIR_GRAPHICS_SUCCESS );
		sirVisibilityView_t next = view;
		++next.nFrameIndex;
		sirGraphicsHandle_t hTexture = {};
		sirGraphicsCompletion_t wait = {};
		REQUIRE( sirHiZHistoryTryRead( backing, next, &hTexture, &wait ) );
		++next.nViewId;
		REQUIRE( !sirHiZHistoryTryRead( backing, next, &hTexture, &wait ) );
		REQUIRE( sirHiZHistoryRetire( device, &backing ) == SIR_GRAPHICS_SUCCESS );
	}
	REQUIRE( device.m_nCreateCalls == 6 && device.m_nRetireCalls == 6 );
	return true;
}

int main( void )
{
	if ( !sirHiZTestPublicationAndRetirement() || !sirHiZTestHistoryRejection() || !sirHiZTestFailureAndLoss() ||
	     !sirHiZTestMultipleViews() ) {
		return 1;
	}
	puts( "PASS: Hi-Z per-view history, multi-queue retirement, rejection and device-loss paths" );
	return 0;
}
