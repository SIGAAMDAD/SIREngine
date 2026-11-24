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

#include <Renderer/Visibility/SIRHiZCommands.h>

static bool sirHiZValidHandle( const sirGraphicsHandle_t& handle, SIRGraphicsHandleType eType, uint32_t nGeneration ) noexcept
{
	return handle.eType == eType && handle.nGeneration != 0 && handle.nDeviceGeneration == nGeneration;
}

static bool sirHiZSameHandle( const sirGraphicsHandle_t& first, const sirGraphicsHandle_t& second ) noexcept
{
	return first.nSlot == second.nSlot && first.nGeneration == second.nGeneration && first.nDeviceGeneration == second.nDeviceGeneration &&
	       first.eType == second.eType;
}

static bool sirHiZValidPass( const sirHiZDispatch_t& pass, const sirHiZLayout_t& layout, uint32_t iMip ) noexcept
{
	const sirHiZMip_t& destination = layout.szMips[iMip];
	if ( pass.bCopyDepth != ( iMip == 0 ) || pass.nDestinationMip != iMip ) {
		return false;
	}
	if ( pass.nSourceMip != ( iMip == 0 ? 0 : iMip - 1 ) ) {
		return false;
	}
	if ( pass.parameters.nDestinationWidth != destination.nWidth || pass.parameters.nDestinationHeight != destination.nHeight ) {
		return false;
	}
	if ( pass.nWorkgroupsX != destination.nWidth / 8 + ( destination.nWidth % 8 != 0 ) ) {
		return false;
	}
	if ( pass.nWorkgroupsY != destination.nHeight / 8 + ( destination.nHeight % 8 != 0 ) ) {
		return false;
	}
	if ( pass.parameters.nReserved0 != 0 || pass.parameters.nReserved1 != 0 ) {
		return false;
	}
	if ( iMip == 0 ) {
		const uint64_t nRight = static_cast<uint64_t>(pass.parameters.nSourceOriginX) + destination.nWidth;
		const uint64_t nBottom = static_cast<uint64_t>(pass.parameters.nSourceOriginY) + destination.nHeight;
		return nRight <= pass.parameters.nSourceWidth && nBottom <= pass.parameters.nSourceHeight;
	}
	const sirHiZMip_t& source = layout.szMips[iMip - 1];
	return pass.parameters.nSourceWidth == source.nWidth && pass.parameters.nSourceHeight == source.nHeight &&
	       pass.parameters.nSourceOriginX == 0 && pass.parameters.nSourceOriginY == 0;
}

static SIRGraphicsResult sirHiZValidateRecording( const sirHiZCommandResources_t& resources ) noexcept
{
	if ( resources.pPlan == nullptr || resources.pPassSets == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirHiZDispatchPlan_t& plan = *resources.pPlan;
	const sirGraphicsTextureDesc_t& pyramid = plan.pyramid;
	if ( pyramid.nWidth == 0 || pyramid.nHeight == 0 || pyramid.nDepth != 1 || pyramid.nArrayLayers != 1 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pyramid.nSampleCount != 1 || pyramid.eFormat != SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_R32_FLOAT || pyramid.eDimension != SIRGraphicsTextureDimension::SIR_GRAPHICS_TEXTURE_2D ||
	     pyramid.eMemory != SIRGraphicsMemoryClass::SIR_GRAPHICS_MEMORY_GPU_LOCAL ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	constexpr uint32_t REQUIRED_USAGE = static_cast<uint32_t>(SIRGraphicsUsage::SIR_GRAPHICS_USAGE_SAMPLED) | static_cast<uint32_t>(SIRGraphicsUsage::SIR_GRAPHICS_USAGE_STORAGE);
	if ( ( pyramid.nUsage & REQUIRED_USAGE ) != REQUIRED_USAGE ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirHiZLayout_t layout {};
	if ( sirHiZCalculateLayout( pyramid.nWidth, pyramid.nHeight, &layout ) != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( plan.nPassCount != layout.nMipCount || pyramid.nMipLevels != layout.nMipCount || resources.nPassSetCount < layout.nMipCount ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const uint32_t nGeneration = resources.hPyramidTexture.nDeviceGeneration;
	if ( nGeneration == 0 || !sirHiZValidHandle( resources.hDepthTexture, SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_TEXTURE, nGeneration ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( !sirHiZValidHandle( resources.hPyramidTexture, SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_TEXTURE, nGeneration ) ||
	     sirHiZSameHandle( resources.hDepthTexture, resources.hPyramidTexture ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( !sirHiZValidHandle( resources.hCopyPipeline, SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_PIPELINE, nGeneration ) ||
	     !sirHiZValidHandle( resources.hReducePipeline, SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_PIPELINE, nGeneration ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( resources.eDepthBefore != SIRGraphicsState::SIR_GRAPHICS_STATE_DEPTH_WRITE && resources.eDepthBefore != SIRGraphicsState::SIR_GRAPHICS_STATE_DEPTH_READ &&
	     resources.eDepthBefore != SIRGraphicsState::SIR_GRAPHICS_STATE_SHADER_READ ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( resources.ePyramidBefore != SIRGraphicsState::SIR_GRAPHICS_STATE_UNDEFINED && resources.ePyramidBefore != SIRGraphicsState::SIR_GRAPHICS_STATE_COMMON &&
	     resources.ePyramidBefore != SIRGraphicsState::SIR_GRAPHICS_STATE_SHADER_READ ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t iMip = 0; iMip < layout.nMipCount; ++iMip ) {
		if ( !sirHiZValidHandle( resources.pPassSets[iMip], SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BINDING_SET, nGeneration ) ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
		}
		if ( !sirHiZValidPass( plan.szPasses[iMip], layout, iMip ) ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

static sirGraphicsBarrier_t sirHiZTransition( sirGraphicsHandle_t hResource, SIRGraphicsState eBefore, SIRGraphicsState eAfter,
                                              uint32_t iMip ) noexcept
{
	sirGraphicsBarrier_t barrier {};
	barrier.eType = SIRGraphicsBarrierType::SIR_GRAPHICS_BARRIER_TRANSITION;
	barrier.hResource = hResource;
	barrier.eBefore = eBefore;
	barrier.eAfter = eAfter;
	barrier.nBaseMip = iMip;
	barrier.nMipCount = 1;
	barrier.nBaseLayer = 0;
	barrier.nLayerCount = 1;
	return barrier;
}

SIRGraphicsResult sirHiZRecordPyramid( SIRGraphicsCommandList& commands, const sirHiZCommandResources_t& resources ) noexcept
{
	const SIRGraphicsQueue eQueue = commands.getQueue();
	if ( eQueue != SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_DIRECT && eQueue != SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COMPUTE ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	const SIRGraphicsResult eValid = sirHiZValidateRecording( resources );
	if ( eValid != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eValid;
	}
	const sirHiZDispatchPlan_t& plan = *resources.pPlan;
	for ( uint32_t iMip = 0; iMip < plan.nPassCount; ++iMip ) {
		sirGraphicsBarrier_t szBarriers[2];
		memset( szBarriers, 0, sizeof( szBarriers ) );
		uint32_t nBarrierCount = 0;
		if ( iMip == 0 ) {
			if ( resources.eDepthBefore != SIRGraphicsState::SIR_GRAPHICS_STATE_SHADER_READ ) {
				szBarriers[nBarrierCount++] =
				    sirHiZTransition( resources.hDepthTexture, resources.eDepthBefore, SIRGraphicsState::SIR_GRAPHICS_STATE_SHADER_READ, 0 );
			}
		} else {
			szBarriers[nBarrierCount++] =
			    sirHiZTransition( resources.hPyramidTexture, SIRGraphicsState::SIR_GRAPHICS_STATE_SHADER_WRITE, SIRGraphicsState::SIR_GRAPHICS_STATE_SHADER_READ, iMip - 1 );
		}
		szBarriers[nBarrierCount++] =
		    sirHiZTransition( resources.hPyramidTexture, resources.ePyramidBefore, SIRGraphicsState::SIR_GRAPHICS_STATE_SHADER_WRITE, iMip );
		SIRGraphicsResult eResult = commands.barrier( szBarriers, nBarrierCount );
		if ( eResult != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
			return eResult;
		}
		const sirGraphicsHandle_t hPipeline = iMip == 0 ? resources.hCopyPipeline : resources.hReducePipeline;
		eResult = commands.bindPipeline( hPipeline );
		if ( eResult != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
			return eResult;
		}
		eResult = commands.bindBindingSet( 0, resources.pPassSets[iMip] );
		if ( eResult != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
			return eResult;
		}
		const sirHiZDispatch_t& pass = plan.szPasses[iMip];
		eResult = commands.dispatch( pass.nWorkgroupsX, pass.nWorkgroupsY, 1 );
		if ( eResult != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
			return eResult;
		}
	}
	const sirGraphicsBarrier_t finalBarrier =
	    sirHiZTransition( resources.hPyramidTexture, SIRGraphicsState::SIR_GRAPHICS_STATE_SHADER_WRITE, SIRGraphicsState::SIR_GRAPHICS_STATE_SHADER_READ, plan.nPassCount - 1 );
	return commands.barrier( &finalBarrier, 1 );
}
