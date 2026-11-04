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

#include <Renderer/SIRGraphicsValidation.h>

bool sirGraphicsIsBackendValid( sirGraphicsBackend_t eBackend ) noexcept
{
	return eBackend >= SIR_GRAPHICS_BACKEND_AUTO && eBackend <= SIR_GRAPHICS_BACKEND_OPENGL;
}

bool sirGraphicsHasFeatures( uint64_t nAvailable, uint64_t nRequired ) noexcept
{
	return ( nAvailable & nRequired ) == nRequired;
}

bool sirGraphicsIsHandleValid( const sirGraphicsHandle_t &handle, sirGraphicsHandleType_t eExpected, uint32_t nDeviceGeneration ) noexcept
{
	if ( handle.nGeneration == 0 || nDeviceGeneration == 0 || handle.nDeviceGeneration != nDeviceGeneration ) {
		return false;
	}
	if ( handle.eType < SIR_GRAPHICS_HANDLE_BUFFER || handle.eType > SIR_GRAPHICS_HANDLE_SURFACE ) {
		return false;
	}
	return eExpected == SIR_GRAPHICS_HANDLE_NONE || handle.eType == eExpected;
}

bool sirGraphicsIsCompletionSetValid( const sirGraphicsCompletionSet_t &completion, uint32_t nDeviceGeneration ) noexcept
{
	constexpr uint32_t VALID_QUEUE_MASK = ( 1U << SIR_GRAPHICS_QUEUE_COUNT ) - 1U;
	if ( nDeviceGeneration == 0 || completion.nDeviceGeneration != nDeviceGeneration ) {
		return false;
	}
	if ( ( completion.nQueueMask & ~VALID_QUEUE_MASK ) != 0 ) {
		return false;
	}

	for ( uint32_t iQueue = 0; iQueue < SIR_GRAPHICS_QUEUE_COUNT; ++iQueue ) {
		const bool bNamed = ( completion.nQueueMask & ( 1U << iQueue ) ) != 0;
		const bool bHasFence = completion.szQueueValues[iQueue] != 0;
		if ( bNamed != bHasFence ) {
			return false;
		}
	}
	return true;
}

static bool sirGraphicsValidUsage( uint32_t nUsage ) noexcept
{
	constexpr uint32_t VALID_USAGE_MASK = ( SIR_GRAPHICS_USAGE_INDIRECT << 1 ) - 1U;
	return nUsage != 0 && ( nUsage & ~VALID_USAGE_MASK ) == 0;
}

sirGraphicsResult_t sirGraphicsValidateBuffer( const sirGraphicsBufferDesc_t &desc ) noexcept
{
	if ( desc.nSizeBytes == 0 || !sirGraphicsValidUsage( desc.nUsage ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( desc.eMemory > SIR_GRAPHICS_MEMORY_TRANSIENT ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( ( desc.nUsage & ( SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT | SIR_GRAPHICS_USAGE_DEPTH_STENCIL ) ) != 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( desc.nAlignmentBytes != 0 ) {
		const uint64_t nAlignment = desc.nAlignmentBytes;
		if ( ( nAlignment & ( nAlignment - 1 ) ) != 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirGraphicsValidateTexture( const sirGraphicsTextureDesc_t &desc ) noexcept
{
	if ( desc.nWidth == 0 || desc.nHeight == 0 || desc.nDepth == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( desc.nMipLevels == 0 || desc.nMipLevels > 32 || desc.nArrayLayers == 0 || desc.nSampleCount == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( ( desc.nSampleCount & ( desc.nSampleCount - 1 ) ) != 0 || desc.nSampleCount > 64 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( !sirGraphicsValidUsage( desc.nUsage ) || desc.eFormat <= SIR_GRAPHICS_FORMAT_UNKNOWN ||
	     desc.eFormat > SIR_GRAPHICS_FORMAT_BC7_UNORM ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( desc.eMemory > SIR_GRAPHICS_MEMORY_TRANSIENT ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( desc.eDimension == SIR_GRAPHICS_TEXTURE_1D && ( desc.nHeight != 1 || desc.nDepth != 1 ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( desc.eDimension == SIR_GRAPHICS_TEXTURE_2D && desc.nDepth != 1 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( desc.eDimension == SIR_GRAPHICS_TEXTURE_3D && desc.nArrayLayers != 1 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( desc.eDimension == SIR_GRAPHICS_TEXTURE_CUBE &&
	     ( desc.nWidth != desc.nHeight || desc.nDepth != 1 || desc.nArrayLayers % 6 != 0 ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( desc.eDimension < SIR_GRAPHICS_TEXTURE_1D || desc.eDimension > SIR_GRAPHICS_TEXTURE_CUBE ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirGraphicsValidateSampler( const sirGraphicsSamplerDesc_t &desc ) noexcept
{
	if ( desc.eFilter > SIR_GRAPHICS_FILTER_ANISOTROPIC || desc.eAddressU > SIR_GRAPHICS_ADDRESS_BORDER ||
	     desc.eAddressV > SIR_GRAPHICS_ADDRESS_BORDER || desc.eAddressW > SIR_GRAPHICS_ADDRESS_BORDER ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( desc.fMaximumAnisotropy < 1.0f || desc.fMaximumAnisotropy > 16.0f ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( desc.fMinimumLod > desc.fMaximumLod ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	return SIR_GRAPHICS_SUCCESS;
}

static bool sirGraphicsValidShader( const sirGraphicsShaderCode_t &shader ) noexcept
{
	return shader.eFormat >= SIR_GRAPHICS_SHADER_DXIL && shader.eFormat <= SIR_GRAPHICS_SHADER_DXBC && shader.pBytes != nullptr &&
	       shader.nByteCount >= 4;
}

static bool sirGraphicsEmptyShader( const sirGraphicsShaderCode_t &shader ) noexcept
{
	return shader.pBytes == nullptr && shader.nByteCount == 0;
}

sirGraphicsResult_t sirGraphicsValidatePipeline( const sirGraphicsPipelineDesc_t &desc ) noexcept
{
	if ( desc.nBindingCount > 0 && desc.pBindings == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( desc.nAttributeCount > 0 && desc.pAttributes == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( desc.nColorFormatCount > 8 || desc.nBindingCount > 4096 || desc.nAttributeCount > 32 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	constexpr uint32_t VALID_STAGE_MASK = ( SIR_GRAPHICS_STAGE_TASK << 1 ) - 1U;
	for ( uint32_t iBinding = 0; iBinding < desc.nBindingCount; ++iBinding ) {
		const sirGraphicsBindingDesc_t &binding = desc.pBindings[iBinding];
		if ( binding.nArrayCount == 0 || binding.nShaderStageMask == 0 || ( binding.nShaderStageMask & ~VALID_STAGE_MASK ) != 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( binding.eType > SIR_GRAPHICS_BINDING_SAMPLER ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	if ( desc.eType == SIR_GRAPHICS_PIPELINE_COMPUTE ) {
		if ( !sirGraphicsValidShader( desc.computeShader ) || !sirGraphicsEmptyShader( desc.vertexShader ) ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( !sirGraphicsEmptyShader( desc.pixelShader ) || desc.nColorFormatCount != 0 || desc.nAttributeCount != 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		return SIR_GRAPHICS_SUCCESS;
	}
	if ( desc.eType == SIR_GRAPHICS_PIPELINE_FIXED_FUNCTION ) {
		if ( !sirGraphicsEmptyShader( desc.vertexShader ) || !sirGraphicsEmptyShader( desc.pixelShader ) ||
		     !sirGraphicsEmptyShader( desc.computeShader ) ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	} else if ( desc.eType == SIR_GRAPHICS_PIPELINE_GRAPHICS ) {
		if ( !sirGraphicsValidShader( desc.vertexShader ) || !sirGraphicsValidShader( desc.pixelShader ) ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( desc.vertexShader.eFormat != desc.pixelShader.eFormat || !sirGraphicsEmptyShader( desc.computeShader ) ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	} else {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}

	if ( desc.eTopology > SIR_GRAPHICS_TOPOLOGY_PATCHES || desc.eCullMode > SIR_GRAPHICS_CULL_BACK ||
	     desc.eDepthCompare > SIR_GRAPHICS_COMPARE_ALWAYS ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t iColor = 0; iColor < desc.nColorFormatCount; ++iColor ) {
		const sirGraphicsBlendState_t &blend = desc.szBlendStates[iColor];
		if ( !blend.bEnabled ) {
			continue;
		}
		if ( blend.eSourceColor > SIR_GRAPHICS_BLEND_ONE_MINUS_DST_ALPHA ||
		     blend.eDestinationColor > SIR_GRAPHICS_BLEND_ONE_MINUS_DST_ALPHA ||
		     blend.eSourceAlpha > SIR_GRAPHICS_BLEND_ONE_MINUS_DST_ALPHA ||
		     blend.eDestinationAlpha > SIR_GRAPHICS_BLEND_ONE_MINUS_DST_ALPHA ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( blend.eColorOperation > SIR_GRAPHICS_BLEND_MAX || blend.eAlphaOperation > SIR_GRAPHICS_BLEND_MAX ||
		     ( blend.nColorWriteMask & ~0xFU ) != 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	if ( desc.nSampleCount == 0 || ( desc.nSampleCount & ( desc.nSampleCount - 1 ) ) != 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t iColor = 0; iColor < desc.nColorFormatCount; ++iColor ) {
		if ( desc.szColorFormats[iColor] == SIR_GRAPHICS_FORMAT_UNKNOWN ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirGraphicsValidateSubmission( sirGraphicsQueue_t eQueue, const sirGraphicsSubmission_t &submission,
                                                   uint32_t nDeviceGeneration ) noexcept
{
	if ( eQueue >= SIR_GRAPHICS_QUEUE_COUNT || nDeviceGeneration == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( submission.nCommandListCount == 0 || submission.nCommandListCount > 64 || submission.ppCommandLists == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( submission.nWaitCount > 64 || ( submission.nWaitCount != 0 && submission.pWaits == nullptr ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( submission.nAcquisitionCount > 8 || ( submission.nAcquisitionCount != 0 && submission.pAcquisitions == nullptr ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t iList = 0; iList < submission.nCommandListCount; ++iList ) {
		SIRGraphicsCommandList *pCommands = submission.ppCommandLists[iList];
		if ( pCommands == nullptr || pCommands->getQueue() != eQueue ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	for ( uint32_t iWait = 0; iWait < submission.nWaitCount; ++iWait ) {
		const sirGraphicsCompletion_t &completion = submission.pWaits[iWait];
		if ( completion.nDeviceGeneration != nDeviceGeneration || completion.eQueue >= SIR_GRAPHICS_QUEUE_COUNT ||
		     completion.nValue == 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	for ( uint32_t iImage = 0; iImage < submission.nAcquisitionCount; ++iImage ) {
		const sirGraphicsSurfaceImage_t &image = submission.pAcquisitions[iImage];
		if ( !sirGraphicsIsHandleValid( image.hSurface, SIR_GRAPHICS_HANDLE_SURFACE, nDeviceGeneration ) ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( !sirGraphicsIsHandleValid( image.hImage, SIR_GRAPHICS_HANDLE_TEXTURE, nDeviceGeneration ) || image.nAcquisitionId == 0 ||
		     image.nWindowGeneration == 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	return SIR_GRAPHICS_SUCCESS;
}
