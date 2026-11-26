/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Renderer/SIRGraphicsValidation.h>
#include <Renderer/SIRGraphicsABIOperations.h>

bool sirGraphicsIsBackendValid( SIRGraphicsBackend eBackend ) noexcept
{
	return eBackend >= SIRGraphicsBackend::SIR_GRAPHICS_BACKEND_AUTO && eBackend <= SIRGraphicsBackend::SIR_GRAPHICS_BACKEND_OPENGL;
}

bool sirGraphicsHasFeatures( uint64_t nAvailable, uint64_t nRequired ) noexcept
{
	return ( nAvailable & nRequired ) == nRequired;
}

bool sirGraphicsIsHandleValid( const sirGraphicsHandle_t& handle, SIRGraphicsHandleType eExpected, uint32_t nDeviceGeneration ) noexcept
{
	if ( handle.nGeneration == 0 || nDeviceGeneration == 0 || handle.nDeviceGeneration != nDeviceGeneration ) {
		return false;
	}
	if ( handle.eType < SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BUFFER || handle.eType > SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_SURFACE ) {
		return false;
	}
	return eExpected == SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_NONE || handle.eType == eExpected;
}

bool sirGraphicsIsCompletionSetValid( const sirGraphicsCompletionSet_t& completion, uint32_t nDeviceGeneration ) noexcept
{
	constexpr uint32_t VALID_QUEUE_MASK = ( 1U << SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COUNT ) - 1U;
	if ( nDeviceGeneration == 0 || completion.nDeviceGeneration != nDeviceGeneration ) {
		return false;
	}
	if ( ( completion.nQueueMask & ~VALID_QUEUE_MASK ) != 0 ) {
		return false;
	}

	for ( uint32_t iQueue = 0; iQueue < SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COUNT; ++iQueue ) {
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
	constexpr uint32_t VALID_USAGE_MASK = ( SIRGraphicsUsage::SIR_GRAPHICS_USAGE_INDIRECT << 1 ) - 1U;
	return nUsage != 0 && ( nUsage & ~VALID_USAGE_MASK ) == 0;
}

SIRGraphicsResult sirGraphicsValidateBuffer( const sirGraphicsBufferDesc_t& desc ) noexcept
{
	if ( desc.nSizeBytes == 0 || !sirGraphicsValidUsage( desc.nUsage ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( desc.eMemory > SIRGraphicsMemoryClass::SIR_GRAPHICS_MEMORY_TRANSIENT ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( ( desc.nUsage & ( SIRGraphicsUsage::SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT | SIRGraphicsUsage::SIR_GRAPHICS_USAGE_DEPTH_STENCIL ) ) != 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( desc.nAlignmentBytes != 0 ) {
		const uint64_t nAlignment = desc.nAlignmentBytes;
		if ( ( nAlignment & ( nAlignment - 1 ) ) != 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult sirGraphicsValidateTexture( const sirGraphicsTextureDesc_t& desc ) noexcept
{
	if ( desc.nWidth == 0 || desc.nHeight == 0 || desc.nDepth == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( desc.nMipLevels == 0 || desc.nMipLevels > 32 || desc.nArrayLayers == 0 || desc.nSampleCount == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( ( desc.nSampleCount & ( desc.nSampleCount - 1 ) ) != 0 || desc.nSampleCount > 64 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( !sirGraphicsValidUsage( desc.nUsage ) || desc.eFormat <= SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_UNKNOWN ||
	     desc.eFormat > SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_BC7_UNORM ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( desc.eMemory > SIRGraphicsMemoryClass::SIR_GRAPHICS_MEMORY_TRANSIENT ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( desc.eDimension == SIRGraphicsTextureDimension::SIR_GRAPHICS_TEXTURE_1D && ( desc.nHeight != 1 || desc.nDepth != 1 ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( desc.eDimension == SIRGraphicsTextureDimension::SIR_GRAPHICS_TEXTURE_2D && desc.nDepth != 1 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( desc.eDimension == SIRGraphicsTextureDimension::SIR_GRAPHICS_TEXTURE_3D && desc.nArrayLayers != 1 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( desc.eDimension == SIRGraphicsTextureDimension::SIR_GRAPHICS_TEXTURE_CUBE &&
	     ( desc.nWidth != desc.nHeight || desc.nDepth != 1 || desc.nArrayLayers % 6 != 0 ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( desc.eDimension < SIRGraphicsTextureDimension::SIR_GRAPHICS_TEXTURE_1D || desc.eDimension > SIRGraphicsTextureDimension::SIR_GRAPHICS_TEXTURE_CUBE ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult sirGraphicsValidateSampler( const sirGraphicsSamplerDesc_t& desc ) noexcept
{
	if ( desc.eFilter > SIRGraphicsSamplerFilter::SIR_GRAPHICS_FILTER_ANISOTROPIC || desc.eAddressU > SIRGraphicsAddressMode::SIR_GRAPHICS_ADDRESS_BORDER ||
	     desc.eAddressV > SIRGraphicsAddressMode::SIR_GRAPHICS_ADDRESS_BORDER || desc.eAddressW > SIRGraphicsAddressMode::SIR_GRAPHICS_ADDRESS_BORDER ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( !( desc.fMaximumAnisotropy >= 1.0f && desc.fMaximumAnisotropy <= 16.0f ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( !( desc.fMinimumLod <= desc.fMaximumLod ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

static bool sirGraphicsValidShader( const sirGraphicsShaderCode_t& shader ) noexcept
{
	return shader.eFormat >= SIRGraphicsShaderFormat::SIR_GRAPHICS_SHADER_DXIL && shader.eFormat <= SIRGraphicsShaderFormat::SIR_GRAPHICS_SHADER_D3D_LEGACY_TOKENS &&
	       shader.eFormat != SIRGraphicsShaderFormat::SIR_GRAPHICS_SHADER_FIXED_FUNCTION && shader.pBytes != nullptr && shader.nByteCount >= 4;
}

static bool sirGraphicsEmptyShader( const sirGraphicsShaderCode_t& shader ) noexcept
{
	return shader.pBytes == nullptr && shader.nByteCount == 0 && shader.nContentHash == 0;
}

static SIRGraphicsResult sirGraphicsValidateSourcePipelineModel( const sirGraphicsPipelineDesc_t& desc ) noexcept
{
	if ( desc.eModel == SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_MODERN ) {
		if ( desc.pModern == nullptr || desc.pProgrammable != nullptr || desc.pFixed != nullptr ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( desc.pModern->nSize < sizeof( sirGraphicsABIModernRecipe_t ) ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		const uint32_t nExpectedStages = desc.eType == SIRGraphicsPipelineType::SIR_GRAPHICS_PIPELINE_COMPUTE
		                                     ? SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_COMPUTE
		                                     : SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_VERTEX | SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_FRAGMENT;
		if ( desc.eType != SIRGraphicsPipelineType::SIR_GRAPHICS_PIPELINE_COMPUTE && desc.eType != SIRGraphicsPipelineType::SIR_GRAPHICS_PIPELINE_GRAPHICS ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		return desc.pModern->nStageMask == nExpectedStages ? SIRGraphicsResult::SIR_GRAPHICS_SUCCESS : SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( desc.eModel == SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE ) {
		if ( desc.pProgrammable == nullptr || desc.pModern != nullptr || desc.pFixed != nullptr ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( desc.pProgrammable->nSize < sizeof( sirGraphicsABIProgrammableRecipe_t ) || desc.pProgrammable->nReserved != 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( desc.eType != SIRGraphicsPipelineType::SIR_GRAPHICS_PIPELINE_GRAPHICS ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
	}
	if ( desc.eModel == SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION ) {
		if ( desc.pFixed == nullptr || desc.pModern != nullptr || desc.pProgrammable != nullptr ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( desc.pFixed->nSize < sizeof( sirGraphicsABIFixedRecipe_t ) || desc.pFixed->nReserved != 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( desc.eType != SIRGraphicsPipelineType::SIR_GRAPHICS_PIPELINE_FIXED_FUNCTION || desc.nBindingCount != 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( desc.pFixed->nTextureStageCount != 0 && desc.pFixed->pStages == nullptr ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( desc.pFixed->nLightCount != 0 && desc.pFixed->pLights == nullptr ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
	}
	return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
}

SIRGraphicsResult sirGraphicsValidatePipeline( const sirGraphicsPipelineDesc_t& desc ) noexcept
{
	const SIRGraphicsResult eModel = sirGraphicsValidateSourcePipelineModel( desc );
	if ( eModel != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eModel;
	}
	if ( desc.nBindingCount > 0 && desc.pBindings == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( desc.nAttributeCount > 0 && desc.pAttributes == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( desc.nColorFormatCount > 8 || desc.nBindingCount > 4096 || desc.nAttributeCount > 32 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	constexpr uint32_t VALID_STAGE_MASK = ( SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_TASK << 1 ) - 1U;
	for ( uint32_t iBinding = 0; iBinding < desc.nBindingCount; ++iBinding ) {
		const sirGraphicsBindingDesc_t& binding = desc.pBindings[iBinding];
		if ( binding.nArrayCount == 0 || binding.nShaderStageMask == 0 || ( binding.nShaderStageMask & ~VALID_STAGE_MASK ) != 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( binding.eType > SIRGraphicsBindingType::SIR_GRAPHICS_BINDING_SAMPLER ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	if ( desc.eType == SIRGraphicsPipelineType::SIR_GRAPHICS_PIPELINE_COMPUTE ) {
		if ( !sirGraphicsValidShader( desc.computeShader ) || !sirGraphicsEmptyShader( desc.vertexShader ) ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( !sirGraphicsEmptyShader( desc.pixelShader ) || desc.nColorFormatCount != 0 || desc.nAttributeCount != 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( desc.nSampleCount != 1 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
	}
	if ( desc.eType == SIRGraphicsPipelineType::SIR_GRAPHICS_PIPELINE_FIXED_FUNCTION ) {
		if ( !sirGraphicsEmptyShader( desc.vertexShader ) || !sirGraphicsEmptyShader( desc.pixelShader ) ||
		     !sirGraphicsEmptyShader( desc.computeShader ) ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	} else {
		// Model validation has already restricted non-compute types to
		// graphics or fixed-function pipelines.
		if ( !sirGraphicsValidShader( desc.vertexShader ) || !sirGraphicsValidShader( desc.pixelShader ) ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( desc.vertexShader.eFormat != desc.pixelShader.eFormat || !sirGraphicsEmptyShader( desc.computeShader ) ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}

	if ( desc.eTopology > SIRGraphicsTopology::SIR_GRAPHICS_TOPOLOGY_PATCHES || desc.eCullMode > SIRGraphicsCullMode::SIR_GRAPHICS_CULL_BACK ||
	     desc.eDepthCompare > SIRGraphicsCompareOp::SIR_GRAPHICS_COMPARE_ALWAYS ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t iColor = 0; iColor < desc.nColorFormatCount; ++iColor ) {
		const sirGraphicsBlendState_t& blend = desc.szBlendStates[iColor];
		if ( !blend.bEnabled ) {
			continue;
		}
		if ( blend.eSourceColor > SIRGraphicsBlendFactor::SIR_GRAPHICS_BLEND_ONE_MINUS_DST_ALPHA ||
		     blend.eDestinationColor > SIRGraphicsBlendFactor::SIR_GRAPHICS_BLEND_ONE_MINUS_DST_ALPHA ||
		     blend.eSourceAlpha > SIRGraphicsBlendFactor::SIR_GRAPHICS_BLEND_ONE_MINUS_DST_ALPHA ||
		     blend.eDestinationAlpha > SIRGraphicsBlendFactor::SIR_GRAPHICS_BLEND_ONE_MINUS_DST_ALPHA ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( blend.eColorOperation > SIRGraphicsBlendOp::SIR_GRAPHICS_BLEND_MAX || blend.eAlphaOperation > SIRGraphicsBlendOp::SIR_GRAPHICS_BLEND_MAX ||
		     ( blend.nColorWriteMask & ~0xFU ) != 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	if ( desc.nSampleCount == 0 || ( desc.nSampleCount & ( desc.nSampleCount - 1 ) ) != 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t iColor = 0; iColor < desc.nColorFormatCount; ++iColor ) {
		if ( desc.szColorFormats[iColor] == SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_UNKNOWN ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult sirGraphicsValidateSubmission( SIRGraphicsQueue eQueue, const sirGraphicsSubmission_t& submission,
                                                   uint32_t nDeviceGeneration ) noexcept
{
	if ( eQueue >= SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COUNT || nDeviceGeneration == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( submission.nCommandListCount == 0 || submission.nCommandListCount > 64 || submission.ppCommandLists == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( submission.nWaitCount > 64 || ( submission.nWaitCount != 0 && submission.pWaits == nullptr ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( submission.nAcquisitionCount > 8 || ( submission.nAcquisitionCount != 0 && submission.pAcquisitions == nullptr ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t iList = 0; iList < submission.nCommandListCount; ++iList ) {
		SIRGraphicsCommandList *pCommands = submission.ppCommandLists[iList];
		if ( pCommands == nullptr || pCommands->getQueue() != eQueue ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	for ( uint32_t iWait = 0; iWait < submission.nWaitCount; ++iWait ) {
		const sirGraphicsCompletion_t& completion = submission.pWaits[iWait];
		if ( completion.nDeviceGeneration != nDeviceGeneration || completion.eQueue >= SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COUNT ||
		     completion.nValue == 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	for ( uint32_t iImage = 0; iImage < submission.nAcquisitionCount; ++iImage ) {
		const sirGraphicsSurfaceImage_t& image = submission.pAcquisitions[iImage];
		if ( !sirGraphicsIsHandleValid( image.hSurface, SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_SURFACE, nDeviceGeneration ) ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( !sirGraphicsIsHandleValid( image.hImage, SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_TEXTURE, nDeviceGeneration ) || image.nAcquisitionId == 0 ||
		     image.nWindowGeneration == 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}
