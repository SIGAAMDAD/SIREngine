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

static bool sirHiZHistoryValidCompletion( uint32_t nGeneration, const sirGraphicsCompletion_t& completion ) noexcept
{
	return completion.nDeviceGeneration == nGeneration && completion.eQueue < SIR_GRAPHICS_QUEUE_COUNT && completion.nValue != 0;
}

static void sirHiZHistoryRecordCompletion( sirGraphicsCompletionSet_t *pSet, const sirGraphicsCompletion_t& completion ) noexcept
{
	const uint32_t iQueue = ( uint32_t )completion.eQueue;
	pSet->nQueueMask |= 1U << iQueue;
	if ( completion.nValue > pSet->szQueueValues[iQueue] ) {
		pSet->szQueueValues[iQueue] = completion.nValue;
	}
}

sirGraphicsResult_t sirHiZHistoryCreate( SIRRenderingDevice& device, const sirHiZDispatchPlan_t& plan, const sirVisibilityView_t& view,
                                         sirHiZHistoryBacking_t *pBacking ) noexcept
{
	if ( pBacking == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pBacking->hPyramid.nGeneration != 0 ) {
		return SIR_GRAPHICS_BUSY;
	}
	const uint32_t nGeneration = device.getDeviceGeneration();
	if ( nGeneration == 0 || nGeneration != view.nDeviceGeneration || view.nViewId == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( view.eDepthConvention != SIR_VISIBILITY_REVERSE_Z || view.nSampleCount != 1 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( view.eDepthFormat != SIR_GRAPHICS_FORMAT_D16_UNORM && view.eDepthFormat != SIR_GRAPHICS_FORMAT_D24_UNORM_S8_UINT &&
	     view.eDepthFormat != SIR_GRAPHICS_FORMAT_D32_FLOAT ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirGraphicsTextureDesc_t& texture = plan.pyramid;
	if ( texture.nWidth == 0 || texture.nHeight == 0 || texture.nWidth != view.nWidth || texture.nHeight != view.nHeight ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( texture.eFormat != SIR_GRAPHICS_FORMAT_R32_FLOAT || texture.eDimension != SIR_GRAPHICS_TEXTURE_2D || texture.nDepth != 1 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( texture.nArrayLayers != 1 || texture.nSampleCount != 1 || texture.eMemory != SIR_GRAPHICS_MEMORY_GPU_LOCAL ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	constexpr uint32_t REQUIRED_USAGE = SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_STORAGE;
	if ( ( texture.nUsage & REQUIRED_USAGE ) != REQUIRED_USAGE ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirHiZLayout_t layout = {};
	if ( sirHiZCalculateLayout( texture.nWidth, texture.nHeight, &layout ) != SIR_GRAPHICS_SUCCESS ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( texture.nMipLevels != layout.nMipCount || plan.nPassCount != layout.nMipCount ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}

	sirGraphicsHandle_t hTexture = {};
	const sirGraphicsResult_t eResult = device.createTexture( texture, &hTexture );
	if ( eResult != SIR_GRAPHICS_SUCCESS ) {
		return eResult;
	}
	if ( hTexture.eType != SIR_GRAPHICS_HANDLE_TEXTURE || hTexture.nGeneration == 0 || hTexture.nDeviceGeneration != nGeneration ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}

	sirHiZHistoryBacking_t backing = {};
	backing.hPyramid = hTexture;
	backing.producerView = view;
	backing.lastUse.nDeviceGeneration = nGeneration;
	*pBacking = backing;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirHiZHistoryPublish( sirHiZHistoryBacking_t *pBacking, const sirGraphicsCompletion_t& completion ) noexcept
{
	if ( pBacking == nullptr || pBacking->hPyramid.nGeneration == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pBacking->bPublished ) {
		return SIR_GRAPHICS_BUSY;
	}
	const uint32_t nGeneration = pBacking->hPyramid.nDeviceGeneration;
	if ( !sirHiZHistoryValidCompletion( nGeneration, completion ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( completion.eQueue == SIR_GRAPHICS_QUEUE_COPY ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	pBacking->producerCompletion = completion;
	sirHiZHistoryRecordCompletion( &pBacking->lastUse, completion );
	pBacking->bPublished = true;
	return SIR_GRAPHICS_SUCCESS;
}

bool sirHiZHistoryTryRead( const sirHiZHistoryBacking_t& backing, const sirVisibilityView_t& current, sirGraphicsHandle_t *pTexture,
                           sirGraphicsCompletion_t *pWait ) noexcept
{
	if ( pTexture == nullptr || pWait == nullptr || !backing.bPublished || backing.hPyramid.nGeneration == 0 ) {
		return false;
	}
	const uint32_t nGeneration = backing.hPyramid.nDeviceGeneration;
	if ( nGeneration != current.nDeviceGeneration || !sirHiZHistoryValidCompletion( nGeneration, backing.producerCompletion ) ) {
		return false;
	}
	if ( !sirVisibilityCanReuseHistory( backing.producerView, current ) ) {
		return false;
	}
	*pTexture = backing.hPyramid;
	*pWait = backing.producerCompletion;
	return true;
}

sirGraphicsResult_t sirHiZHistoryTrackRead( sirHiZHistoryBacking_t *pBacking, const sirGraphicsCompletion_t& completion ) noexcept
{
	if ( pBacking == nullptr || pBacking->hPyramid.nGeneration == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( !pBacking->bPublished ) {
		return SIR_GRAPHICS_BUSY;
	}
	if ( !sirHiZHistoryValidCompletion( pBacking->hPyramid.nDeviceGeneration, completion ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirHiZHistoryRecordCompletion( &pBacking->lastUse, completion );
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirHiZHistoryRetire( SIRRenderingDevice& device, sirHiZHistoryBacking_t *pBacking ) noexcept
{
	if ( pBacking == nullptr || pBacking->hPyramid.nGeneration == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( device.getDeviceGeneration() != pBacking->hPyramid.nDeviceGeneration ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	const sirGraphicsResult_t eResult = device.retireResource( pBacking->hPyramid, pBacking->lastUse );
	if ( eResult != SIR_GRAPHICS_SUCCESS ) {
		return eResult;
	}
	*pBacking = {};
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirHiZHistoryDiscardLost( sirHiZHistoryBacking_t *pBacking, uint32_t nReplacementGeneration ) noexcept
{
	if ( pBacking == nullptr || pBacking->hPyramid.nGeneration == 0 || nReplacementGeneration == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( nReplacementGeneration == pBacking->hPyramid.nDeviceGeneration ) {
		return SIR_GRAPHICS_BUSY;
	}
	*pBacking = {};
	return SIR_GRAPHICS_SUCCESS;
}
