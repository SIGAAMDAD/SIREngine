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

#include <Renderer/SIRGraphicsDriverABI.h>
#include <Renderer/SIRGraphicsTypes.h>

static bool sirGraphicsIsABIPowerOfTwo( uint32_t nValue ) noexcept
{
	return nValue != 0 && ( nValue & ( nValue - 1 ) ) == 0;
}

static bool sirGraphicsIsABIPipelineModelValid( uint32_t nModel ) noexcept
{
	return nModel >= SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_MODERN && nModel <= SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION;
}

static bool sirGraphicsABIShaderIsEmpty( const sirGraphicsABIShaderCode_t *pShader ) noexcept
{
	return pShader->pBytes == nullptr && pShader->nByteCount == 0 && pShader->nContentHash == 0;
}

static bool sirGraphicsABILegacyTokensValid( const sirGraphicsABIShaderCode_t *pShader ) noexcept
{
	if ( pShader->pBytes == nullptr || pShader->nByteCount < 8 || pShader->nByteCount % 4 != 0 || pShader->nByteCount > ( 64ULL << 20 ) ) {
		return false;
	}
	const uint8_t *pBytes = static_cast<const uint8_t *>(pShader->pBytes);
	const uint32_t nKind = static_cast<uint32_t>(pBytes[2]) | ( static_cast<uint32_t>(pBytes[3]) << 8 );
	if ( ( nKind != 0xfffe && nKind != 0xffff ) || pBytes[1] == 0 || pBytes[1] > 3 ) {
		return false;
	}
	const uint64_t iEnd = pShader->nByteCount - 4;
	return pBytes[iEnd] == 0xff && pBytes[iEnd + 1] == 0xff && pBytes[iEnd + 2] == 0 && pBytes[iEnd + 3] == 0;
}

static bool sirGraphicsABIShaderIsSupported( const sirGraphicsABIShaderCode_t *pShader, uint32_t nFormatMask ) noexcept
{
	if ( pShader->nSize != sizeof( sirGraphicsABIShaderCode_t ) || pShader->pBytes == nullptr ) {
		return false;
	}
	return ( nFormatMask & ( 1U << ( pShader->nFormat - 1 ) ) ) != 0;
}

static bool sirGraphicsABIShaderIsWellFormed( const sirGraphicsABIShaderCode_t *pShader ) noexcept
{
	if ( sirGraphicsABIShaderIsEmpty( pShader ) ) {
		return pShader->nSize == 0 || pShader->nSize >= sizeof( sirGraphicsABIShaderCode_t );
	}
	if ( pShader->nSize < sizeof( sirGraphicsABIShaderCode_t ) || pShader->pBytes == nullptr ) {
		return false;
	}
	if ( pShader->nFormat < SIRGraphicsShaderFormat::SIR_GRAPHICS_SHADER_DXIL || pShader->nFormat > SIRGraphicsShaderFormat::SIR_GRAPHICS_SHADER_D3D_LEGACY_TOKENS ) {
		return false;
	}
	return pShader->nFormat == SIRGraphicsShaderFormat::SIR_GRAPHICS_SHADER_D3D_LEGACY_TOKENS ? sirGraphicsABILegacyTokensValid( pShader )
	                                                                 : pShader->nByteCount >= 4;
}

// Pure descriptor validity: runs before capability discovery so malformed
// authored alternatives cannot disappear behind an unsupported model.
sirGraphicsABIResult_t sirGraphicsValidateABIPipelineStructure( const sirGraphicsABIPipeline_t *pPipeline ) noexcept
{
	if ( pPipeline == nullptr || pPipeline->nSize < sizeof( sirGraphicsABIPipeline_t ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pPipeline->nReserved != 0 || !sirGraphicsIsABIPipelineModelValid( pPipeline->nModel ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pPipeline->nType > SIRGraphicsPipelineType::SIR_GRAPHICS_PIPELINE_FIXED_FUNCTION || pPipeline->nColorFormatCount > 8 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( !sirGraphicsIsABIPowerOfTwo( pPipeline->nSampleCount ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( ( pPipeline->nBindingCount != 0 && pPipeline->pBindings == nullptr ) ||
	     ( pPipeline->nAttributeCount != 0 && pPipeline->pAttributes == nullptr ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}

	const bool bVertexValid = sirGraphicsABIShaderIsWellFormed( &pPipeline->vertexShader );
	const bool bFragmentValid = sirGraphicsABIShaderIsWellFormed( &pPipeline->pixelShader );
	const bool bComputeValid = sirGraphicsABIShaderIsWellFormed( &pPipeline->computeShader );
	if ( !bVertexValid || !bFragmentValid || !bComputeValid ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}

	if ( pPipeline->nModel == SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION ) {
		if ( pPipeline->pFixed == nullptr || pPipeline->pModern != nullptr || pPipeline->pProgrammable != nullptr ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( pPipeline->nType != SIRGraphicsPipelineType::SIR_GRAPHICS_PIPELINE_FIXED_FUNCTION || pPipeline->nBindingCount != 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( !sirGraphicsABIShaderIsEmpty( &pPipeline->vertexShader ) || !sirGraphicsABIShaderIsEmpty( &pPipeline->pixelShader ) ||
		     !sirGraphicsABIShaderIsEmpty( &pPipeline->computeShader ) ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		const sirGraphicsABIFixedRecipe_t *pRecipe = pPipeline->pFixed;
		if ( pRecipe->nSize < sizeof( sirGraphicsABIFixedRecipe_t ) || pRecipe->nReserved != 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( ( pRecipe->nTextureStageCount != 0 && pRecipe->pStages == nullptr ) ||
		     ( pRecipe->nLightCount != 0 && pRecipe->pLights == nullptr ) ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
	}

	if ( pPipeline->nModel == SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE ) {
		if ( pPipeline->pProgrammable == nullptr || pPipeline->pModern != nullptr || pPipeline->pFixed != nullptr ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( pPipeline->nType != SIRGraphicsPipelineType::SIR_GRAPHICS_PIPELINE_GRAPHICS ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		const sirGraphicsABIProgrammableRecipe_t *pRecipe = pPipeline->pProgrammable;
		if ( pRecipe->nSize < sizeof( sirGraphicsABIProgrammableRecipe_t ) || pRecipe->nReserved != 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( pRecipe->nVertexShaderProfile == 0 || pRecipe->nFragmentShaderProfile == 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( sirGraphicsABIShaderIsEmpty( &pPipeline->vertexShader ) || sirGraphicsABIShaderIsEmpty( &pPipeline->pixelShader ) ||
		     !sirGraphicsABIShaderIsEmpty( &pPipeline->computeShader ) ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
	}

	if ( pPipeline->pModern == nullptr || pPipeline->pProgrammable != nullptr || pPipeline->pFixed != nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pPipeline->pModern->nSize < sizeof( sirGraphicsABIModernRecipe_t ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pPipeline->nType == SIRGraphicsPipelineType::SIR_GRAPHICS_PIPELINE_COMPUTE ) {
		if ( !sirGraphicsABIShaderIsEmpty( &pPipeline->vertexShader ) || !sirGraphicsABIShaderIsEmpty( &pPipeline->pixelShader ) ||
		     sirGraphicsABIShaderIsEmpty( &pPipeline->computeShader ) ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( pPipeline->nColorFormatCount != 0 || pPipeline->nAttributeCount != 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		return pPipeline->pModern->nStageMask == SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_COMPUTE ? SIRGraphicsResult::SIR_GRAPHICS_SUCCESS : SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pPipeline->nType != SIRGraphicsPipelineType::SIR_GRAPHICS_PIPELINE_GRAPHICS ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( sirGraphicsABIShaderIsEmpty( &pPipeline->vertexShader ) || sirGraphicsABIShaderIsEmpty( &pPipeline->pixelShader ) ||
	     !sirGraphicsABIShaderIsEmpty( &pPipeline->computeShader ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const uint32_t nGraphicsStages = SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_VERTEX | SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_FRAGMENT;
	return pPipeline->pModern->nStageMask == nGraphicsStages ? SIRGraphicsResult::SIR_GRAPHICS_SUCCESS : SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
}

sirGraphicsABIResult_t sirGraphicsValidateABIPipelineModelCaps( const sirGraphicsABIPipelineModelCaps_t *pCaps ) noexcept
{
	if ( pCaps == nullptr || pCaps->nSize < sizeof( sirGraphicsABIPipelineModelCaps_t ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( !sirGraphicsIsABIPipelineModelValid( pCaps->nModel ) || pCaps->nDeviceGeneration == 0 || pCaps->nReserved != 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pCaps->nExecutionModel < SIRGraphicsExecutionModel::SIR_GRAPHICS_EXECUTION_EXPLICIT || pCaps->nExecutionModel > SIRGraphicsExecutionModel::SIR_GRAPHICS_EXECUTION_STATEFUL_SERIAL ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const uint32_t nQueueMask = ( 1U << SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COUNT ) - 1U;
	const uint32_t nShaderStageMask = ( SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_TASK << 1 ) - 1U;
	const uint32_t nShaderFormatMask = ( 1U << SIRGraphicsShaderFormat::SIR_GRAPHICS_SHADER_D3D_LEGACY_TOKENS ) - 1U;
	const uint32_t nFixedFeatures = ( SIRGraphicsFixedFeature::SIR_GRAPHICS_FIXED_TEXCOORD_GEN << 1 ) - 1U;
	const uint32_t nFixedOperations = ( 1U << ( SIRGraphicsFixedTextureOp::SIR_GRAPHICS_FIXED_OP_DOT3 + 1 ) ) - 1U;
	const uint32_t nFogModes = ( 1U << ( SIRGraphicsFixedFogMode::SIR_GRAPHICS_FIXED_FOG_EXP2 + 1 ) ) - 1U;

	if ( pCaps->nQueueMask == 0 || ( pCaps->nQueueMask & ~nQueueMask ) != 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( ( pCaps->nShaderStageMask & ~nShaderStageMask ) != 0 || ( pCaps->nShaderFormatMask & ~nShaderFormatMask ) != 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( !sirGraphicsIsABIPowerOfTwo( pCaps->nMaximumSamples ) || pCaps->nMaximumColorAttachments > 8 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pCaps->nMaximumTextureDimension == 0 || pCaps->nSupportsNPOT > 1 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( ( pCaps->nFixedFeatureMask & ~nFixedFeatures ) != 0 || ( pCaps->nFixedTextureOperationMask & ~nFixedOperations ) != 0 ||
	     ( pCaps->nFixedFogModeMask & ~nFogModes ) != 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}

	if ( pCaps->nModel == SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION ) {
		if ( pCaps->nExecutionModel != SIRGraphicsExecutionModel::SIR_GRAPHICS_EXECUTION_STATEFUL_SERIAL ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( pCaps->nShaderStageMask != 0 || pCaps->nShaderFormatMask != 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		const uint64_t nRequired = SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_GRAPHICS | SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_FIXED_FUNCTION;
		if ( ( pCaps->nRequiredFeatureMask & nRequired ) != nRequired ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( ( pCaps->nQueueMask & ( 1U << SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_DIRECT ) ) == 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( pCaps->nMaximumTextureStages == 0 || pCaps->nMaximumTextureStages > pCaps->nMaximumTextureUnits ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( pCaps->nFixedTextureOperationMask == 0 || ( pCaps->nFixedFogModeMask & ( 1U << SIRGraphicsFixedFogMode::SIR_GRAPHICS_FIXED_FOG_NONE ) ) == 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( ( pCaps->nFixedFeatureMask & ( SIRGraphicsFixedFeature::SIR_GRAPHICS_FIXED_TRANSFORM | SIRGraphicsFixedFeature::SIR_GRAPHICS_FIXED_CPU_TRANSFORM ) ) == 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
	}

	// Shader-based models cannot silently borrow fixed-function state.
	if ( pCaps->nFixedFeatureMask != 0 || pCaps->nFixedTextureOperationMask != 0 || pCaps->nFixedFogModeMask != 0 ||
	     pCaps->nMaximumFixedLights != 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pCaps->nShaderStageMask == 0 || pCaps->nShaderFormatMask == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pCaps->nModel == SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE ) {
		const uint32_t nStages = SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_VERTEX | SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_FRAGMENT;
		if ( ( pCaps->nShaderStageMask & nStages ) != nStages ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( ( pCaps->nRequiredFeatureMask & SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_GRAPHICS ) == 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( pCaps->nMaximumVertexShaderProfile == 0 || pCaps->nMaximumFragmentShaderProfile == 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		const uint32_t nModernStages = SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_COMPUTE | SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_MESH | SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_TASK;
		if ( ( pCaps->nShaderStageMask & nModernStages ) != 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( ( pCaps->nRequiredFeatureMask & SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_COMPUTE ) != 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( pCaps->nMaximumTextureUnits == 0 || pCaps->nMaximumVertexAttributes == 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
	}

	const uint32_t nGraphicsStages = SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_VERTEX | SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_FRAGMENT;
	const bool bSupportsGraphics = ( pCaps->nShaderStageMask & nGraphicsStages ) == nGraphicsStages;
	const bool bSupportsCompute = ( pCaps->nShaderStageMask & SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_COMPUTE ) != 0;
	if ( !bSupportsGraphics && !bSupportsCompute ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( bSupportsGraphics && ( pCaps->nRequiredFeatureMask & SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_GRAPHICS ) == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( bSupportsCompute && ( pCaps->nRequiredFeatureMask & SIRGraphicsFeature::SIR_GRAPHICS_FEATURE_COMPUTE ) == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

// The public entrypoint validates authored structure before these capability
// checks. Rechecking the same invariants would duplicate work on every PSO.
static sirGraphicsABIResult_t sirGraphicsValidateABIPipelineCommon( const sirGraphicsABIPipeline_t *pPipeline,
                                                                    const sirGraphicsABIPipelineModelCaps_t *pCaps ) noexcept
{
	if ( pPipeline->nBindingCount > pCaps->nMaximumBindings || pPipeline->nAttributeCount > pCaps->nMaximumVertexAttributes ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	const uint32_t nBoundStages = pPipeline->nType == SIRGraphicsPipelineType::SIR_GRAPHICS_PIPELINE_COMPUTE
	                                  ? SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_COMPUTE
	                                  : SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_VERTEX | SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_FRAGMENT;
	for ( uint32_t iBinding = 0; iBinding < pPipeline->nBindingCount; ++iBinding ) {
		const sirGraphicsBindingDesc_t *pBinding = &pPipeline->pBindings[iBinding];
		if ( pBinding->nArrayCount == 0 || pBinding->nShaderStageMask == 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( pBinding->eType > SIRGraphicsBindingType::SIR_GRAPHICS_BINDING_SAMPLER ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( pBinding->nBinding >= pCaps->nMaximumBindings || pBinding->nArrayCount > pCaps->nMaximumBindings - pBinding->nBinding ) {
			return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
		}
		if ( ( pBinding->nShaderStageMask & ~nBoundStages ) != 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( pPipeline->nModel == SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE && pBinding->nSpace != 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
		}
		if ( pPipeline->nModel == SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE && pBinding->eType == SIRGraphicsBindingType::SIR_GRAPHICS_BINDING_STORAGE_TEXTURE ) {
			return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
		}
	}
	for ( uint32_t iAttribute = 0; iAttribute < pPipeline->nAttributeCount; ++iAttribute ) {
		const sirGraphicsVertexAttribute_t *pAttribute = &pPipeline->pAttributes[iAttribute];
		if ( pAttribute->nLocation >= pCaps->nMaximumVertexAttributes || pAttribute->nBufferSlot >= pCaps->nMaximumVertexStreams ) {
			return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
		}
		if ( pAttribute->eFormat <= SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_UNKNOWN || pAttribute->eFormat > SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_BC7_UNORM ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	if ( pPipeline->nColorFormatCount > pCaps->nMaximumColorAttachments ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pPipeline->nSampleCount > pCaps->nMaximumSamples ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pPipeline->nTopology > SIRGraphicsTopology::SIR_GRAPHICS_TOPOLOGY_PATCHES || pPipeline->nCullMode > SIRGraphicsCullMode::SIR_GRAPHICS_CULL_BACK ||
	     pPipeline->nDepthCompare > SIRGraphicsCompareOp::SIR_GRAPHICS_COMPARE_ALWAYS ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pPipeline->nDepthTest > 1 || pPipeline->nDepthWrite > 1 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t i = 0; i < pPipeline->nColorFormatCount; ++i ) {
		if ( pPipeline->szColorFormats[i] <= SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_UNKNOWN ||
		     pPipeline->szColorFormats[i] >= SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_D16_UNORM ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( pPipeline->szBlendStates[i].nEnabled > 1 || ( pPipeline->szBlendStates[i].nColorWriteMask & ~15U ) != 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		const sirGraphicsABIBlendState_t *pBlend = &pPipeline->szBlendStates[i];
		if ( pBlend->nEnabled != 0 ) {
			if ( pBlend->nSourceColor > SIRGraphicsBlendFactor::SIR_GRAPHICS_BLEND_ONE_MINUS_DST_ALPHA ||
			     pBlend->nDestinationColor > SIRGraphicsBlendFactor::SIR_GRAPHICS_BLEND_ONE_MINUS_DST_ALPHA ) {
				return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
			}
			if ( pBlend->nSourceAlpha > SIRGraphicsBlendFactor::SIR_GRAPHICS_BLEND_ONE_MINUS_DST_ALPHA ||
			     pBlend->nDestinationAlpha > SIRGraphicsBlendFactor::SIR_GRAPHICS_BLEND_ONE_MINUS_DST_ALPHA ) {
				return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
			}
			if ( pBlend->nColorOperation > SIRGraphicsBlendOp::SIR_GRAPHICS_BLEND_MAX || pBlend->nAlphaOperation > SIRGraphicsBlendOp::SIR_GRAPHICS_BLEND_MAX ) {
				return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
			}
		}
	}
	if ( pPipeline->nDepthFormat != SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_UNKNOWN &&
	     ( pPipeline->nDepthFormat < SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_D16_UNORM || pPipeline->nDepthFormat > SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_D32_FLOAT ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

static sirGraphicsABIResult_t sirGraphicsValidateABIPipelineShaders( const sirGraphicsABIPipeline_t *pPipeline,
                                                                     const sirGraphicsABIPipelineModelCaps_t *pCaps ) noexcept
{
	const bool bCompute = pPipeline->nType == SIRGraphicsPipelineType::SIR_GRAPHICS_PIPELINE_COMPUTE;
	if ( bCompute ) {
		if ( ( pCaps->nShaderStageMask & SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_COMPUTE ) == 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
		}
		return sirGraphicsABIShaderIsSupported( &pPipeline->computeShader, pCaps->nShaderFormatMask ) ? SIRGraphicsResult::SIR_GRAPHICS_SUCCESS
		                                                                                              : SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	const uint32_t nStages = SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_VERTEX | SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_FRAGMENT;
	if ( ( pCaps->nShaderStageMask & nStages ) != nStages ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( !sirGraphicsABIShaderIsSupported( &pPipeline->vertexShader, pCaps->nShaderFormatMask ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pPipeline->vertexShader.nFormat == SIRGraphicsShaderFormat::SIR_GRAPHICS_SHADER_D3D_LEGACY_TOKENS &&
	     ( static_cast<const uint8_t *>(pPipeline->vertexShader.pBytes)[3] != 0xff ||
	       static_cast<const uint8_t *>(pPipeline->vertexShader.pBytes)[2] != 0xfe ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( !sirGraphicsABIShaderIsSupported( &pPipeline->pixelShader, pCaps->nShaderFormatMask ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pPipeline->pixelShader.nFormat == SIRGraphicsShaderFormat::SIR_GRAPHICS_SHADER_D3D_LEGACY_TOKENS &&
	     ( static_cast<const uint8_t *>(pPipeline->pixelShader.pBytes)[3] != 0xff ||
	       static_cast<const uint8_t *>(pPipeline->pixelShader.pBytes)[2] != 0xff ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pPipeline->vertexShader.nFormat != pPipeline->pixelShader.nFormat ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

static sirGraphicsABIResult_t sirGraphicsValidateABIFixedPipeline( const sirGraphicsABIPipeline_t *pPipeline,
                                                                   const sirGraphicsABIPipelineModelCaps_t *pCaps ) noexcept
{
	const sirGraphicsABIFixedRecipe_t *pFixed = pPipeline->pFixed;
	if ( ( pFixed->nRequiredFeatures & ~pCaps->nFixedFeatureMask ) != 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pFixed->nTextureStageCount > pCaps->nMaximumTextureStages || pFixed->nLightCount > pCaps->nMaximumFixedLights ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pFixed->nFogMode > SIRGraphicsFixedFogMode::SIR_GRAPHICS_FIXED_FOG_EXP2 || pFixed->nAlphaCompare > SIRGraphicsCompareOp::SIR_GRAPHICS_COMPARE_ALWAYS ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( ( pCaps->nFixedFogModeMask & ( 1U << pFixed->nFogMode ) ) == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( !( pFixed->fAlphaReference >= 0.0f && pFixed->fAlphaReference <= 1.0f ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pFixed->fFogEnd < pFixed->fFogStart || pFixed->fFogDensity < 0.0f ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pFixed->nLightCount != 0 && ( pFixed->nRequiredFeatures & SIRGraphicsFixedFeature::SIR_GRAPHICS_FIXED_LIGHTING ) == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pFixed->nFogMode != SIRGraphicsFixedFogMode::SIR_GRAPHICS_FIXED_FOG_NONE && ( pFixed->nRequiredFeatures & SIRGraphicsFixedFeature::SIR_GRAPHICS_FIXED_FOG ) == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	for ( uint32_t i = 0; i < pFixed->nTextureStageCount; ++i ) {
		const sirGraphicsABIFixedStage_t *pStage = &pFixed->pStages[i];
		if ( pStage->nTextureUnit >= pCaps->nMaximumTextureUnits || pStage->nCoordinateSet >= pCaps->nMaximumVertexStreams ) {
			return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
		}
		if ( pStage->nColorOperation > SIRGraphicsFixedTextureOp::SIR_GRAPHICS_FIXED_OP_DOT3 || pStage->nAlphaOperation > SIRGraphicsFixedTextureOp::SIR_GRAPHICS_FIXED_OP_DOT3 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( ( pCaps->nFixedTextureOperationMask & ( 1U << pStage->nColorOperation ) ) == 0 ||
		     ( pCaps->nFixedTextureOperationMask & ( 1U << pStage->nAlphaOperation ) ) == 0 ) {
			return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
		}
		if ( pStage->nColorArgument0 > SIRGraphicsFixedTextureArg::SIR_GRAPHICS_FIXED_ARG_CONSTANT || pStage->nColorArgument1 > SIRGraphicsFixedTextureArg::SIR_GRAPHICS_FIXED_ARG_CONSTANT ||
		     pStage->nAlphaArgument0 > SIRGraphicsFixedTextureArg::SIR_GRAPHICS_FIXED_ARG_CONSTANT || pStage->nAlphaArgument1 > SIRGraphicsFixedTextureArg::SIR_GRAPHICS_FIXED_ARG_CONSTANT ) {
			return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
		}
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

sirGraphicsABIResult_t sirGraphicsValidateABIPipeline( const sirGraphicsABIPipeline_t *pPipeline,
                                                       const sirGraphicsABIPipelineModelCaps_t *pCaps ) noexcept
{
	if ( pPipeline == nullptr || pCaps == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirGraphicsABIResult_t eCaps = sirGraphicsValidateABIPipelineModelCaps( pCaps );
	if ( eCaps != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eCaps;
	}
	if ( pPipeline->nModel != pCaps->nModel ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	const sirGraphicsABIResult_t eStructure = sirGraphicsValidateABIPipelineStructure( pPipeline );
	if ( eStructure != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eStructure;
	}
	const sirGraphicsABIResult_t eCommon = sirGraphicsValidateABIPipelineCommon( pPipeline, pCaps );
	if ( eCommon != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eCommon;
	}
	if ( pPipeline->nModel == SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION ) {
		return sirGraphicsValidateABIFixedPipeline( pPipeline, pCaps );
	}
	if ( pPipeline->nModel == SIRGraphicsPipelineModel::SIR_GRAPHICS_PIPELINE_MODEL_PROGRAMMABLE ) {
		const sirGraphicsABIProgrammableRecipe_t *pRecipe = pPipeline->pProgrammable;
		if ( pRecipe->nVertexShaderProfile > pCaps->nMaximumVertexShaderProfile ||
		     pRecipe->nFragmentShaderProfile > pCaps->nMaximumFragmentShaderProfile ) {
			return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
		}
		if ( pRecipe->nTextureUnits > pCaps->nMaximumTextureUnits || pRecipe->nVaryings > pCaps->nMaximumVaryings ||
		     pRecipe->nConstantSlots > pCaps->nMaximumConstantSlots ) {
			return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
		}
		return sirGraphicsValidateABIPipelineShaders( pPipeline, pCaps );
	}
	const sirGraphicsABIModernRecipe_t *pRecipe = pPipeline->pModern;
	if ( ( pRecipe->nRequiredFeatures & ~pCaps->nRequiredFeatureMask ) != 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	return sirGraphicsValidateABIPipelineShaders( pPipeline, pCaps );
}
