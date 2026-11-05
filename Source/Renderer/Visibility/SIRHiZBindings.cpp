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

static bool sirHiZBindingsValidHandle( sirGraphicsHandle_t hResource, sirGraphicsHandleType_t eType, uint32_t nGeneration ) noexcept
{
	return hResource.nGeneration != 0 && hResource.nDeviceGeneration == nGeneration && hResource.eType == eType;
}

static bool sirHiZBindingsSameHandle( sirGraphicsHandle_t a, sirGraphicsHandle_t b ) noexcept
{
	return a.nSlot == b.nSlot && a.nGeneration == b.nGeneration && a.nDeviceGeneration == b.nDeviceGeneration && a.eType == b.eType;
}

static bool sirHiZBindingsSamePlan( const sirHiZDispatchPlan_t& a, const sirHiZDispatchPlan_t& b ) noexcept
{
	if ( a.nPassCount != b.nPassCount ) {
		return false;
	}
	const sirGraphicsTextureDesc_t& ta = a.pyramid;
	const sirGraphicsTextureDesc_t& tb = b.pyramid;
	if ( ta.nWidth != tb.nWidth || ta.nHeight != tb.nHeight || ta.nDepth != tb.nDepth ) {
		return false;
	}
	if ( ta.nArrayLayers != tb.nArrayLayers || ta.nMipLevels != tb.nMipLevels || ta.nSampleCount != tb.nSampleCount ) {
		return false;
	}
	if ( ta.nUsage != tb.nUsage || ta.eDimension != tb.eDimension || ta.eFormat != tb.eFormat || ta.eMemory != tb.eMemory ) {
		return false;
	}
	for ( uint32_t iMip = 0; iMip < a.nPassCount; ++iMip ) {
		const sirHiZDispatch_t& pa = a.szPasses[iMip];
		const sirHiZDispatch_t& pb = b.szPasses[iMip];
		if ( pa.nSourceMip != pb.nSourceMip || pa.nDestinationMip != pb.nDestinationMip || pa.bCopyDepth != pb.bCopyDepth ) {
			return false;
		}
		if ( pa.nWorkgroupsX != pb.nWorkgroupsX || pa.nWorkgroupsY != pb.nWorkgroupsY ) {
			return false;
		}
		if ( memcmp( &pa.parameters, &pb.parameters, sizeof( pa.parameters ) ) != 0 ) {
			return false;
		}
	}
	return true;
}

static sirGraphicsResult_t sirHiZBindingsValidate( const sirHiZBindingsDesc_t& desc, uint32_t nGeneration ) noexcept
{
	if ( desc.pPlan == nullptr || nGeneration == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( desc.nParameterAlignment < 256 || ( desc.nParameterAlignment & ( desc.nParameterAlignment - 1 ) ) != 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( desc.nParameterAlignment > 4096 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( !sirHiZBindingsValidHandle( desc.hDepthTexture, SIR_GRAPHICS_HANDLE_TEXTURE, nGeneration ) ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( !sirHiZBindingsValidHandle( desc.hPyramidTexture, SIR_GRAPHICS_HANDLE_TEXTURE, nGeneration ) ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( sirHiZBindingsSameHandle( desc.hDepthTexture, desc.hPyramidTexture ) ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( !sirHiZBindingsValidHandle( desc.hCopyPipeline, SIR_GRAPHICS_HANDLE_PIPELINE, nGeneration ) ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( !sirHiZBindingsValidHandle( desc.hReducePipeline, SIR_GRAPHICS_HANDLE_PIPELINE, nGeneration ) ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( desc.depth.texture.eFormat != SIR_GRAPHICS_FORMAT_D32_FLOAT ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirHiZLayout_t layout = {};
	if ( sirHiZCalculateLayout( desc.pPlan->pyramid.nWidth, desc.pPlan->pyramid.nHeight, &layout ) != SIR_GRAPHICS_SUCCESS ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirHiZDispatchPlan_t expected = {};
	if ( sirHiZPlanDispatches( layout, desc.depth, &expected ) != SIR_GRAPHICS_SUCCESS ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	return sirHiZBindingsSamePlan( *desc.pPlan, expected ) ? SIR_GRAPHICS_SUCCESS : SIR_GRAPHICS_INVALID_ARGUMENT;
}

static sirGraphicsResult_t sirHiZBindingsCreateView( SIRRenderingDevice& device, sirGraphicsHandle_t hResource, uint32_t iMip,
                                                     uint32_t nUsage, sirGraphicsHandle_t *pView ) noexcept
{
	sirGraphicsViewDesc_t view = {};
	view.hResource = hResource;
	view.eFormat = SIR_GRAPHICS_FORMAT_R32_FLOAT;
	view.nBaseMip = iMip;
	view.nMipLevels = 1;
	view.nBaseLayer = 0;
	view.nLayers = 1;
	view.nUsage = nUsage;
	return device.createView( view, pView );
}

static sirGraphicsResult_t sirHiZBindingsUpload( SIRRenderingDevice& device, const sirHiZDispatchPlan_t& plan,
                                                 sirHiZBindings_t *pBindings ) noexcept
{
	const uint64_t nTotalBytes = ( uint64_t )pBindings->nParameterStride * plan.nPassCount;
	sirGraphicsBufferDesc_t buffer = {};
	buffer.nSizeBytes = nTotalBytes;
	buffer.nAlignmentBytes = pBindings->nParameterStride;
	buffer.nUsage = SIR_GRAPHICS_USAGE_CONSTANT;
	buffer.eMemory = SIR_GRAPHICS_MEMORY_CPU_UPLOAD;
	sirGraphicsResult_t eResult = device.createBuffer( buffer, &pBindings->hParameterBuffer );
	if ( eResult != SIR_GRAPHICS_SUCCESS ) {
		return eResult;
	}
	sirGraphicsMappedBuffer_t mapped = {};
	eResult = device.mapBuffer( pBindings->hParameterBuffer, 0, nTotalBytes, &mapped );
	if ( eResult != SIR_GRAPHICS_SUCCESS ) {
		return eResult;
	}
	pBindings->bMapped = true;
	if ( mapped.pData == nullptr || mapped.nBytes < nTotalBytes ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	memset( mapped.pData, 0, ( size_t )nTotalBytes );
	for ( uint32_t iMip = 0; iMip < plan.nPassCount; ++iMip ) {
		uint8_t *pDestination = ( uint8_t * )mapped.pData + ( uint64_t )iMip * pBindings->nParameterStride;
		memcpy( pDestination, &plan.szPasses[iMip].parameters, sizeof( sirHiZShaderParameters_t ) );
	}
	eResult = device.flushBuffer( pBindings->hParameterBuffer, 0, nTotalBytes );
	if ( eResult != SIR_GRAPHICS_SUCCESS ) {
		return eResult;
	}
	eResult = device.unmapBuffer( pBindings->hParameterBuffer, 0, nTotalBytes );
	if ( eResult == SIR_GRAPHICS_SUCCESS ) {
		pBindings->bMapped = false;
	}
	return eResult;
}

sirGraphicsResult_t sirHiZBindingsCreate( SIRRenderingDevice& device, const sirHiZBindingsDesc_t& desc,
                                          sirHiZBindings_t *pBindings ) noexcept
{
	if ( pBindings == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pBindings->nDeviceGeneration != 0 || pBindings->hParameterBuffer.nGeneration != 0 ) {
		return SIR_GRAPHICS_BUSY;
	}
	const uint32_t nGeneration = device.getDeviceGeneration();
	const sirGraphicsResult_t eValid = sirHiZBindingsValidate( desc, nGeneration );
	if ( eValid != SIR_GRAPHICS_SUCCESS ) {
		return eValid;
	}
	pBindings->plan = *desc.pPlan;
	pBindings->hDepthTexture = desc.hDepthTexture;
	pBindings->hPyramidTexture = desc.hPyramidTexture;
	pBindings->hCopyPipeline = desc.hCopyPipeline;
	pBindings->hReducePipeline = desc.hReducePipeline;
	pBindings->nDeviceGeneration = nGeneration;
	pBindings->nPassCount = desc.pPlan->nPassCount;
	pBindings->nParameterStride = desc.nParameterAlignment;

	sirGraphicsResult_t eResult = sirHiZBindingsUpload( device, *desc.pPlan, pBindings );
	if ( eResult != SIR_GRAPHICS_SUCCESS ) {
		return eResult;
	}
	// The shader reads depth through the native D32F depth-plane sampled view.
	sirGraphicsViewDesc_t depthView = {};
	depthView.hResource = desc.hDepthTexture;
	depthView.eFormat = SIR_GRAPHICS_FORMAT_D32_FLOAT;
	depthView.nBaseMip = 0;
	depthView.nMipLevels = 1;
	depthView.nLayers = 1;
	depthView.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	eResult = device.createView( depthView, &pBindings->hDepthView );
	if ( eResult != SIR_GRAPHICS_SUCCESS ) {
		return eResult;
	}
	for ( uint32_t iMip = 0; iMip < pBindings->nPassCount; ++iMip ) {
		eResult =
		    sirHiZBindingsCreateView( device, desc.hPyramidTexture, iMip, SIR_GRAPHICS_USAGE_SAMPLED, &pBindings->szSampleViews[iMip] );
		if ( eResult != SIR_GRAPHICS_SUCCESS ) {
			return eResult;
		}
		eResult =
		    sirHiZBindingsCreateView( device, desc.hPyramidTexture, iMip, SIR_GRAPHICS_USAGE_STORAGE, &pBindings->szStorageViews[iMip] );
		if ( eResult != SIR_GRAPHICS_SUCCESS ) {
			return eResult;
		}
	}
	for ( uint32_t iMip = 0; iMip < pBindings->nPassCount; ++iMip ) {
		const sirGraphicsHandle_t hPipeline = iMip == 0 ? desc.hCopyPipeline : desc.hReducePipeline;
		eResult = device.createBindingSet( hPipeline, 0, &pBindings->szPassSets[iMip] );
		if ( eResult != SIR_GRAPHICS_SUCCESS ) {
			return eResult;
		}
		sirGraphicsDescriptorWrite_t szWrites[3] = {};
		szWrites[0] = { pBindings->hParameterBuffer, 0, 0, ( uint64_t )iMip * pBindings->nParameterStride,
		                sizeof( sirHiZShaderParameters_t ) };
		const sirGraphicsHandle_t hSource = iMip == 0 ? pBindings->hDepthView : pBindings->szSampleViews[iMip - 1];
		szWrites[1] = { hSource, 1, 0, 0, 0 };
		szWrites[2] = { pBindings->szStorageViews[iMip], 2, 0, 0, 0 };
		eResult = device.updateBindings( pBindings->szPassSets[iMip], szWrites, 3 );
		if ( eResult != SIR_GRAPHICS_SUCCESS ) {
			return eResult;
		}
	}
	pBindings->bReady = true;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirHiZBindingsGetCommands( const sirHiZBindings_t& bindings, sirHiZCommandResources_t *pCommands ) noexcept
{
	if ( pCommands == nullptr || !bindings.bReady || bindings.nDeviceGeneration == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirHiZCommandResources_t output = *pCommands;
	output.pPlan = &bindings.plan;
	output.pPassSets = bindings.szPassSets;
	output.nPassSetCount = bindings.nPassCount;
	output.hDepthTexture = bindings.hDepthTexture;
	output.hPyramidTexture = bindings.hPyramidTexture;
	output.hCopyPipeline = bindings.hCopyPipeline;
	output.hReducePipeline = bindings.hReducePipeline;
	*pCommands = output;
	return SIR_GRAPHICS_SUCCESS;
}

static sirGraphicsResult_t sirHiZBindingsRetireHandle( SIRRenderingDevice& device, const sirGraphicsCompletionSet_t& completion,
                                                       sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pHandle->nGeneration == 0 ) {
		return SIR_GRAPHICS_SUCCESS;
	}
	const sirGraphicsResult_t eResult = device.retireResource( *pHandle, completion );
	if ( eResult == SIR_GRAPHICS_SUCCESS ) {
		*pHandle = {};
	}
	return eResult;
}

sirGraphicsResult_t sirHiZBindingsRetire( SIRRenderingDevice& device, const sirGraphicsCompletionSet_t& completion,
                                          sirHiZBindings_t *pBindings ) noexcept
{
	if ( pBindings == nullptr || pBindings->nDeviceGeneration == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const uint32_t nGeneration = pBindings->nDeviceGeneration;
	if ( device.getDeviceGeneration() != nGeneration ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	if ( completion.nDeviceGeneration != nGeneration ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	pBindings->bReady = false;
	const uint64_t nBytes = ( uint64_t )pBindings->nPassCount * pBindings->nParameterStride;
	if ( pBindings->bMapped ) {
		const sirGraphicsResult_t eResult = device.unmapBuffer( pBindings->hParameterBuffer, 0, nBytes );
		if ( eResult != SIR_GRAPHICS_SUCCESS ) {
			return eResult;
		}
		pBindings->bMapped = false;
	}
	for ( uint32_t iMip = 0; iMip < pBindings->nPassCount; ++iMip ) {
		const sirGraphicsResult_t eResult = sirHiZBindingsRetireHandle( device, completion, &pBindings->szPassSets[iMip] );
		if ( eResult != SIR_GRAPHICS_SUCCESS ) {
			return eResult;
		}
	}
	for ( uint32_t iMip = 0; iMip < pBindings->nPassCount; ++iMip ) {
		const sirGraphicsResult_t eResult = sirHiZBindingsRetireHandle( device, completion, &pBindings->szStorageViews[iMip] );
		if ( eResult != SIR_GRAPHICS_SUCCESS ) {
			return eResult;
		}
	}
	for ( uint32_t iMip = 0; iMip < pBindings->nPassCount; ++iMip ) {
		const sirGraphicsResult_t eResult = sirHiZBindingsRetireHandle( device, completion, &pBindings->szSampleViews[iMip] );
		if ( eResult != SIR_GRAPHICS_SUCCESS ) {
			return eResult;
		}
	}
	sirGraphicsResult_t eResult = sirHiZBindingsRetireHandle( device, completion, &pBindings->hDepthView );
	if ( eResult != SIR_GRAPHICS_SUCCESS ) {
		return eResult;
	}
	eResult = sirHiZBindingsRetireHandle( device, completion, &pBindings->hParameterBuffer );
	if ( eResult != SIR_GRAPHICS_SUCCESS ) {
		return eResult;
	}
	*pBindings = {};
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirHiZBindingsDiscardLost( uint32_t nReplacementGeneration, sirHiZBindings_t *pBindings ) noexcept
{
	if ( pBindings == nullptr || pBindings->nDeviceGeneration == 0 || nReplacementGeneration == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( nReplacementGeneration == pBindings->nDeviceGeneration ) {
		return SIR_GRAPHICS_BUSY;
	}
	*pBindings = {};
	return SIR_GRAPHICS_SUCCESS;
}
