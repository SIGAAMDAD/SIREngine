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

#include <Renderer/Visibility/SIRHiZBindings.h>
#include <stdio.h>

#define REQUIRE( condition )                                                                                                               \
	if ( !( condition ) ) {                                                                                                                \
		printf( "FAIL: %s at line %d\n", #condition, __LINE__ );                                                                           \
		return false;                                                                                                                      \
	}

class SIRHiZBindingMockDevice : public SIRRenderingDevice
{
public:
	SIRHiZBindingMockDevice( void ) noexcept = default;
	~SIRHiZBindingMockDevice( void ) noexcept override = default;

	sirGraphicsBackend_t getBackend( void ) const noexcept override
	{
		return SIR_GRAPHICS_BACKEND_AUTO;
	}

	uint32_t getDeviceGeneration( void ) const noexcept override
	{
		return m_nGeneration;
	}

	sirGraphicsResult_t createTexture( const sirGraphicsTextureDesc_t&, sirGraphicsHandle_t * ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}

	sirGraphicsResult_t retireResource( sirGraphicsHandle_t hResource, const sirGraphicsCompletionSet_t& completion ) noexcept override
	{
		++m_nRetireCalls;
		if ( m_nFailRetirement == m_nRetireCalls ) {
			return SIR_GRAPHICS_BUSY;
		}
		if ( hResource.nGeneration == 0 || hResource.nDeviceGeneration != m_nGeneration ) {
			return SIR_GRAPHICS_INVALID_HANDLE;
		}
		m_lastCompletion = completion;
		--m_nLiveHandles;
		return SIR_GRAPHICS_SUCCESS;
	}

	sirGraphicsResult_t queryCapabilities( sirGraphicsCapabilities_t * ) const noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}

	sirGraphicsResult_t queryMemoryBudget( sirGraphicsMemoryBudget_t * ) const noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}

	sirGraphicsResult_t createBuffer( const sirGraphicsBufferDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept override
	{
		if ( shouldFail() ) {
			return SIR_GRAPHICS_OUT_OF_MEMORY;
		}
		m_nBufferBytes = desc.nSizeBytes;
		m_nBufferAlignment = desc.nAlignmentBytes;
		*pHandle = { ++m_nNextSlot, 1, m_nGeneration, SIR_GRAPHICS_HANDLE_BUFFER };
		++m_nLiveHandles;
		return SIR_GRAPHICS_SUCCESS;
	}

	sirGraphicsResult_t createView( const sirGraphicsViewDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept override
	{
		if ( shouldFail() ) {
			return SIR_GRAPHICS_OUT_OF_MEMORY;
		}
		m_szViews[m_nViewCount++] = desc;
		*pHandle = { ++m_nNextSlot, 1, m_nGeneration, SIR_GRAPHICS_HANDLE_VIEW };
		++m_nLiveHandles;
		return SIR_GRAPHICS_SUCCESS;
	}

	sirGraphicsResult_t createSampler( const sirGraphicsSamplerDesc_t&, sirGraphicsHandle_t * ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}

	sirGraphicsResult_t createPipeline( const sirGraphicsPipelineDesc_t&, sirGraphicsHandle_t * ) noexcept override
	{
		return SIR_GRAPHICS_UNSUPPORTED;
	}

	sirGraphicsResult_t createBindingSet( sirGraphicsHandle_t hPipeline, uint32_t nIndex, sirGraphicsHandle_t *pHandle ) noexcept override
	{
		if ( shouldFail() ) {
			return SIR_GRAPHICS_OUT_OF_MEMORY;
		}
		if ( nIndex != 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		m_szPipelines[m_nSetCount++] = hPipeline;
		*pHandle = { ++m_nNextSlot, 1, m_nGeneration, SIR_GRAPHICS_HANDLE_BINDING_SET };
		++m_nLiveHandles;
		return SIR_GRAPHICS_SUCCESS;
	}

	sirGraphicsResult_t updateBindings( sirGraphicsHandle_t, const sirGraphicsDescriptorWrite_t *pWrites,
	                                    uint32_t nCount ) noexcept override
	{
		if ( shouldFail() ) {
			return SIR_GRAPHICS_DRIVER_ERROR;
		}
		if ( nCount != 3 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		for ( uint32_t i = 0; i < 3; ++i ) {
			m_szWrites[m_nWriteCount][i] = pWrites[i];
		}
		++m_nWriteCount;
		return SIR_GRAPHICS_SUCCESS;
	}

	sirGraphicsResult_t mapBuffer( sirGraphicsHandle_t, uint64_t nOffset, uint64_t nBytes,
	                               sirGraphicsMappedBuffer_t *pMapped ) noexcept override
	{
		if ( shouldFail() ) {
			return SIR_GRAPHICS_BUSY;
		}
		*pMapped = { m_szUpload, nOffset, nBytes };
		m_bMapped = true;
		return SIR_GRAPHICS_SUCCESS;
	}

	sirGraphicsResult_t unmapBuffer( sirGraphicsHandle_t, uint64_t, uint64_t ) noexcept override
	{
		if ( shouldFail() ) {
			return SIR_GRAPHICS_BUSY;
		}
		m_bMapped = false;
		return SIR_GRAPHICS_SUCCESS;
	}

	sirGraphicsResult_t flushBuffer( sirGraphicsHandle_t, uint64_t, uint64_t ) noexcept override
	{
		if ( shouldFail() ) {
			return SIR_GRAPHICS_DRIVER_ERROR;
		}
		return SIR_GRAPHICS_SUCCESS;
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

	bool shouldFail( void ) noexcept
	{
		return m_nOp++ == m_nFailOperation;
	}

	uint32_t m_nGeneration = 4;
	uint32_t m_nNextSlot = 100;
	uint32_t m_nOp = 0;
	uint32_t m_nFailOperation = UINT32_MAX;
	uint32_t m_nFailRetirement = 0;
	uint32_t m_nLiveHandles = 0;
	uint32_t m_nRetireCalls = 0;
	uint32_t m_nViewCount = 0;
	uint32_t m_nSetCount = 0;
	uint32_t m_nWriteCount = 0;
	uint64_t m_nBufferBytes = 0;
	uint64_t m_nBufferAlignment = 0;
	bool m_bMapped = false;
	sirGraphicsResult_t m_eCreateResult = SIR_GRAPHICS_SUCCESS;
	sirGraphicsTextureDesc_t m_lastTexture = {};
	sirGraphicsCompletionSet_t m_lastCompletion = {};
	sirGraphicsViewDesc_t m_szViews[65] = {};
	sirGraphicsHandle_t m_szPipelines[32] = {};
	sirGraphicsDescriptorWrite_t m_szWrites[32][3] = {};
	uint8_t m_szUpload[32 * 4096] = {};

protected:
private:
};

static sirHiZBindingsDesc_t sirHiZMakeDesc( uint32_t nWidth, uint32_t nHeight, sirHiZDispatchPlan_t *pPlan ) noexcept
{
	sirHiZLayout_t layout = {};
	( void )sirHiZCalculateLayout( nWidth, nHeight, &layout );
	sirHiZDepthInput_t input = {};
	input.texture = { nWidth,
	                  nHeight,
	                  1,
	                  1,
	                  1,
	                  1,
	                  SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_DEPTH_STENCIL,
	                  SIR_GRAPHICS_TEXTURE_2D,
	                  SIR_GRAPHICS_FORMAT_D32_FLOAT,
	                  SIR_GRAPHICS_MEMORY_GPU_LOCAL };
	input.viewport = { 0, 0, ( int32_t )nWidth, ( int32_t )nHeight };
	input.eConvention = SIR_VISIBILITY_REVERSE_Z;
	( void )sirHiZPlanDispatches( layout, input, pPlan );
	sirHiZBindingsDesc_t desc = {};
	desc.pPlan = pPlan;
	desc.depth = input;
	desc.hDepthTexture = { 10, 1, 4, SIR_GRAPHICS_HANDLE_TEXTURE };
	desc.hPyramidTexture = { 11, 1, 4, SIR_GRAPHICS_HANDLE_TEXTURE };
	desc.hCopyPipeline = { 12, 1, 4, SIR_GRAPHICS_HANDLE_PIPELINE };
	desc.hReducePipeline = { 13, 1, 4, SIR_GRAPHICS_HANDLE_PIPELINE };
	desc.nParameterAlignment = 256;
	return desc;
}

static bool sirHiZTestBindings( void ) noexcept
{
	const uint32_t szWidths[] = { 1, 7, 13, 1920, 2560, 3840 };
	const uint32_t szHeights[] = { 1, 3, 9, 1080, 1440, 2160 };
	for ( uint32_t iCase = 0; iCase < 6; ++iCase ) {
		SIRHiZBindingMockDevice device;
		sirHiZDispatchPlan_t plan = {};
		sirHiZBindingsDesc_t desc = sirHiZMakeDesc( szWidths[iCase], szHeights[iCase], &plan );
		if ( iCase == 2 ) {
			desc.nParameterAlignment = 512;
		}
		sirHiZBindings_t bindings = {};
		REQUIRE( sirHiZBindingsCreate( device, desc, &bindings ) == SIR_GRAPHICS_SUCCESS );
		REQUIRE( bindings.bReady && !bindings.bMapped );
		REQUIRE( device.m_nLiveHandles == plan.nPassCount * 3 + 2 );
		REQUIRE( device.m_nViewCount == 1 + plan.nPassCount * 2 );
		REQUIRE( device.m_nWriteCount == plan.nPassCount );
		REQUIRE( device.m_nBufferBytes == ( uint64_t )desc.nParameterAlignment * plan.nPassCount );
		REQUIRE( device.m_nBufferAlignment == desc.nParameterAlignment );
		REQUIRE( device.m_szViews[0].nBaseMip == 0 && device.m_szViews[0].eFormat == SIR_GRAPHICS_FORMAT_D32_FLOAT );
		for ( uint32_t iMip = 0; iMip < plan.nPassCount; ++iMip ) {
			REQUIRE( device.m_szPipelines[iMip].nSlot == ( iMip == 0 ? 12U : 13U ) );
			REQUIRE( device.m_szViews[iMip * 2 + 1].nBaseMip == iMip );
			REQUIRE( device.m_szViews[iMip * 2 + 2].nUsage == SIR_GRAPHICS_USAGE_STORAGE );
			const sirGraphicsDescriptorWrite_t *pWrites = device.m_szWrites[iMip];
			REQUIRE( pWrites[0].nBinding == 0 && pWrites[0].nOffsetBytes == ( uint64_t )iMip * desc.nParameterAlignment );
			REQUIRE( pWrites[0].nRangeBytes == sizeof( sirHiZShaderParameters_t ) );
			REQUIRE( pWrites[1].nBinding == 1 && pWrites[2].nBinding == 2 );
			REQUIRE( pWrites[2].hResource.nSlot == bindings.szStorageViews[iMip].nSlot );
			const uint32_t nSource = iMip == 0 ? bindings.hDepthView.nSlot : bindings.szSampleViews[iMip - 1].nSlot;
			REQUIRE( pWrites[1].hResource.nSlot == nSource );
			const uint8_t *pBytes = device.m_szUpload + ( uint64_t )iMip * desc.nParameterAlignment;
			REQUIRE( memcmp( pBytes, &plan.szPasses[iMip].parameters, sizeof( sirHiZShaderParameters_t ) ) == 0 );
			REQUIRE( pBytes[desc.nParameterAlignment - 1] == 0 );
		}
		sirHiZCommandResources_t commands = {};
		commands.eDepthBefore = SIR_GRAPHICS_STATE_DEPTH_WRITE;
		commands.ePyramidBefore = SIR_GRAPHICS_STATE_UNDEFINED;
		REQUIRE( sirHiZBindingsGetCommands( bindings, &commands ) == SIR_GRAPHICS_SUCCESS );
		REQUIRE( commands.pPassSets == bindings.szPassSets );
		REQUIRE( commands.pPlan == &bindings.plan );
		REQUIRE( commands.pPlan->szPasses[0].parameters.nSourceWidth == szWidths[iCase] );
		REQUIRE( commands.eDepthBefore == SIR_GRAPHICS_STATE_DEPTH_WRITE );
		REQUIRE( commands.ePyramidBefore == SIR_GRAPHICS_STATE_UNDEFINED );
		const sirGraphicsCompletionSet_t completed = { 4, 3, { 37, 42, 0 } };
		REQUIRE( sirHiZBindingsRetire( device, completed, &bindings ) == SIR_GRAPHICS_SUCCESS );
		REQUIRE( device.m_nLiveHandles == 0 && bindings.nDeviceGeneration == 0 );
		REQUIRE( device.m_lastCompletion.szQueueValues[SIR_GRAPHICS_QUEUE_COMPUTE] == 42 );
	}
	return true;
}

static bool sirHiZTestFailures( void ) noexcept
{
	// Fail every individual GPU allocation, map, flush and descriptor write.
	for ( uint32_t iFail = 0; iFail < 25; ++iFail ) {
		SIRHiZBindingMockDevice device;
		device.m_nFailOperation = iFail;
		sirHiZDispatchPlan_t plan = {};
		sirHiZBindingsDesc_t desc = sirHiZMakeDesc( 13, 9, &plan );
		sirHiZBindings_t bindings = {};
		const sirGraphicsResult_t eResult = sirHiZBindingsCreate( device, desc, &bindings );
		REQUIRE( eResult != SIR_GRAPHICS_SUCCESS );
		REQUIRE( !bindings.bReady );
		REQUIRE( sirHiZBindingsRetire( device, { 4, 0, {} }, &bindings ) == SIR_GRAPHICS_SUCCESS );
		REQUIRE( device.m_nLiveHandles == 0 );
	}
	return true;
}

static bool sirHiZTestInvalid( void ) noexcept
{
	SIRHiZBindingMockDevice device;
	sirHiZDispatchPlan_t plan = {};
	sirHiZBindingsDesc_t desc = sirHiZMakeDesc( 13, 9, &plan );
	sirHiZBindings_t bindings = {};
	REQUIRE( sirHiZBindingsCreate( device, desc, nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	desc.nParameterAlignment = 64;
	REQUIRE( sirHiZBindingsCreate( device, desc, &bindings ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	desc.nParameterAlignment = 256;
	desc.hPyramidTexture.nDeviceGeneration++;
	REQUIRE( sirHiZBindingsCreate( device, desc, &bindings ) == SIR_GRAPHICS_INVALID_HANDLE );
	desc.hPyramidTexture.nDeviceGeneration--;
	plan.szPasses[0].parameters.nSourceWidth++;
	REQUIRE( sirHiZBindingsCreate( device, desc, &bindings ) == SIR_GRAPHICS_INVALID_ARGUMENT );
	plan.szPasses[0].parameters.nSourceWidth--;
	desc.depth.texture.eFormat = SIR_GRAPHICS_FORMAT_D24_UNORM_S8_UINT;
	REQUIRE( sirHiZBindingsCreate( device, desc, &bindings ) == SIR_GRAPHICS_UNSUPPORTED );
	desc.depth.texture.eFormat = SIR_GRAPHICS_FORMAT_D32_FLOAT;
	REQUIRE( sirHiZBindingsCreate( device, desc, &bindings ) == SIR_GRAPHICS_SUCCESS );
	REQUIRE( sirHiZBindingsCreate( device, desc, &bindings ) == SIR_GRAPHICS_BUSY );
	device.m_nFailRetirement = 2;
	REQUIRE( sirHiZBindingsRetire( device, { 4, 0, {} }, &bindings ) == SIR_GRAPHICS_BUSY );
	REQUIRE( !bindings.bReady && bindings.nDeviceGeneration == 4 );
	device.m_nFailRetirement = 0;
	REQUIRE( sirHiZBindingsRetire( device, { 4, 0, {} }, &bindings ) == SIR_GRAPHICS_SUCCESS );
	REQUIRE( device.m_nLiveHandles == 0 );
	return true;
}

static bool sirHiZTestDeviceLoss( void ) noexcept
{
	SIRHiZBindingMockDevice device;
	sirHiZDispatchPlan_t plan = {};
	sirHiZBindingsDesc_t desc = sirHiZMakeDesc( 1, 1, &plan );
	sirHiZBindings_t bindings = {};
	REQUIRE( sirHiZBindingsCreate( device, desc, &bindings ) == SIR_GRAPHICS_SUCCESS );
	device.m_nGeneration = 5;
	REQUIRE( sirHiZBindingsRetire( device, { 4, 0, {} }, &bindings ) == SIR_GRAPHICS_DEVICE_LOST );
	REQUIRE( sirHiZBindingsDiscardLost( 4, &bindings ) == SIR_GRAPHICS_BUSY );
	REQUIRE( sirHiZBindingsDiscardLost( 5, &bindings ) == SIR_GRAPHICS_SUCCESS );
	REQUIRE( bindings.nDeviceGeneration == 0 );
	return true;
}

int main( void )
{
	if ( !sirHiZTestBindings() || !sirHiZTestFailures() || !sirHiZTestInvalid() || !sirHiZTestDeviceLoss() ) {
		return 1;
	}
	puts( "PASS: Hi-Z bindings, mip views, descriptor writes, failures and retirement" );
	return 0;
}
