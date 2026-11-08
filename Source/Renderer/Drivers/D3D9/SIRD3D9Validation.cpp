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
#include <Renderer/Drivers/D3D9/SIRD3D9Validation.h>
#include <string.h>

sirGraphicsResult_t sirD3D9ValidateShaderConstants( const sirD3D9ShaderConstants_t *pConstants, uint32_t nShaderProfile,
                                                    uint32_t nMaximumVertexFloat4 ) noexcept
{
	if ( pConstants == nullptr ||
	     ( pConstants->nStage != SIR_GRAPHICS_STAGE_VERTEX && pConstants->nStage != SIR_GRAPHICS_STAGE_FRAGMENT ) ||
	     pConstants->nRegisterCount == 0 ||
	     ( ( pConstants->pFloat4 != nullptr ) + ( pConstants->pInt4 != nullptr ) + ( pConstants->pBool != nullptr ) ) != 1 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const uint32_t nMajor = ( nShaderProfile >> 8 ) & 0xffU;
	if ( nMajor < 2 || nMajor > 3 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	uint32_t nLimit = 16;
	if ( pConstants->pFloat4 != nullptr ) {
		nLimit = pConstants->nStage == SIR_GRAPHICS_STAGE_VERTEX ? nMaximumVertexFloat4 : ( nMajor == 3 ? 224U : 32U );
	}
	return pConstants->nFirstRegister >= nLimit || pConstants->nRegisterCount > nLimit - pConstants->nFirstRegister
	           ? SIR_GRAPHICS_UNSUPPORTED
			   : SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirD3D9ValidateShaderTokens( const void *pBytes, uint64_t nBytes, uint32_t nStage, uint32_t nMaximumProfile ) noexcept
{
	if ( pBytes == nullptr || nBytes < 8 || ( nBytes & 3 ) != 0 || nBytes > 1024 * 1024 ||
	     ( nStage != SIR_GRAPHICS_STAGE_VERTEX && nStage != SIR_GRAPHICS_STAGE_FRAGMENT ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	uint32_t nVersion = 0;
	uint32_t nTerminator = 0;
	memcpy( &nVersion, pBytes, sizeof( nVersion ) );
	memcpy( &nTerminator, ( const uint8_t * )pBytes + nBytes - 4, sizeof( nTerminator ) );
	const uint32_t nType = nStage == SIR_GRAPHICS_STAGE_VERTEX ? 0xfffe0000U : 0xffff0000U;
	const uint32_t nMajor = ( nVersion >> 8 ) & 0xff;
	const uint32_t nMinor = nVersion & 0xff;
	const uint32_t nMaximumMajor = ( nMaximumProfile >> 8 ) & 0xff;
	const uint32_t nMaximumMinor = nMaximumProfile & 0xff;
	if ( ( nVersion & 0xffff0000U ) != nType || nTerminator != 0x0000ffffU || nMajor < 2 || nMajor > 3 || nMajor > nMaximumMajor ||
	     ( nMajor == nMaximumMajor && nMinor > nMaximumMinor ) ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirD3D9ValidateBuffer( const sirGraphicsBufferDesc_t *pBuffer ) noexcept
{
	if ( pBuffer == nullptr || pBuffer->nSizeBytes == 0 || pBuffer->nSizeBytes > UINT32_MAX || pBuffer->nAlignmentBytes == 0 ||
	     ( pBuffer->nAlignmentBytes & ( pBuffer->nAlignmentBytes - 1 ) ) != 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const uint32_t nUsages = pBuffer->nUsage;
	if ( nUsages != SIR_GRAPHICS_USAGE_VERTEX && nUsages != SIR_GRAPHICS_USAGE_INDEX ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pBuffer->eMemory != SIR_GRAPHICS_MEMORY_GPU_LOCAL && pBuffer->eMemory != SIR_GRAPHICS_MEMORY_CPU_UPLOAD ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirD3D9ValidateBufferWrite( uint32_t nCapacity, uint32_t nOffset, uint32_t nBytes, uint32_t nAlignment ) noexcept
{
	if ( nCapacity == 0 || nBytes == 0 || nAlignment == 0 || ( nAlignment & ( nAlignment - 1 ) ) != 0 || nOffset % nAlignment != 0 ||
	     nBytes % nAlignment != 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( nOffset > nCapacity || nBytes > nCapacity - nOffset ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirD3D9ValidateSampler( const sirGraphicsSamplerDesc_t *pSampler, uint32_t nMaximumAnisotropy ) noexcept
{
	if ( pSampler == nullptr || nMaximumAnisotropy == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pSampler->eFilter > SIR_GRAPHICS_FILTER_ANISOTROPIC || pSampler->eAddressU > SIR_GRAPHICS_ADDRESS_BORDER ||
	     pSampler->eAddressV > SIR_GRAPHICS_ADDRESS_BORDER || pSampler->eAddressW > SIR_GRAPHICS_ADDRESS_BORDER ||
	     !( pSampler->fMaximumAnisotropy >= 1.0f && pSampler->fMaximumAnisotropy <= ( float )nMaximumAnisotropy ) ||
	     !( pSampler->fMinimumLod == 0.0f && pSampler->fMaximumLod == 0.0f ) ||
	     ( pSampler->eFilter != SIR_GRAPHICS_FILTER_ANISOTROPIC && pSampler->fMaximumAnisotropy != 1.0f ) ) {
		// The shared descriptor has no independent mip-LOD clamp control which can be
		// represented losslessly by the D3D9 sampler state machine.
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirD3D9ValidateTexture( const sirGraphicsTextureDesc_t *pTexture, uint32_t nMaximumDimension,
                                            uint32_t nMaximumSamples ) noexcept
{
	if ( pTexture == nullptr || nMaximumDimension == 0 || nMaximumSamples == 0 || pTexture->nWidth == 0 || pTexture->nHeight == 0 ||
	     pTexture->nDepth == 0 || pTexture->nArrayLayers == 0 || pTexture->nWidth > nMaximumDimension ||
	     pTexture->nHeight > nMaximumDimension || pTexture->nDepth > nMaximumDimension || pTexture->nMipLevels == 0 ||
	     pTexture->nSampleCount == 0 || ( pTexture->nSampleCount & ( pTexture->nSampleCount - 1 ) ) != 0 ||
	     pTexture->nSampleCount > nMaximumSamples ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	switch ( pTexture->eDimension ) {
	case SIR_GRAPHICS_TEXTURE_2D:
		if ( pTexture->nDepth != 1 || pTexture->nArrayLayers != 1 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		break;
	case SIR_GRAPHICS_TEXTURE_3D:
		if ( pTexture->nArrayLayers != 1 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		break;
	case SIR_GRAPHICS_TEXTURE_CUBE:
		if ( pTexture->nDepth != 1 || pTexture->nArrayLayers != 6 || pTexture->nWidth != pTexture->nHeight ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		break;
	default:
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pTexture->nSampleCount != 1 || pTexture->eMemory != SIR_GRAPHICS_MEMORY_GPU_LOCAL ||
	     ( pTexture->nUsage & ~( SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT | SIR_GRAPHICS_USAGE_COPY_SOURCE |
	                             SIR_GRAPHICS_USAGE_COPY_DESTINATION ) ) != 0 ||
	     ( pTexture->nUsage & ( SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT | SIR_GRAPHICS_USAGE_COPY_SOURCE |
	                            SIR_GRAPHICS_USAGE_COPY_DESTINATION ) ) == 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	uint32_t nLargest = pTexture->nWidth > pTexture->nHeight ? pTexture->nWidth : pTexture->nHeight;
	if ( pTexture->nDepth > nLargest ) {
		nLargest = pTexture->nDepth;
	}
	uint32_t nFullMipLevels = 1;
	while ( nLargest > 1 ) {
		nLargest >>= 1;
		++nFullMipLevels;
	}
	if ( pTexture->nMipLevels > nFullMipLevels ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	switch ( pTexture->eFormat ) {
	case SIR_GRAPHICS_FORMAT_RGBA8_UNORM:
	case SIR_GRAPHICS_FORMAT_BGRA8_UNORM:
		return SIR_GRAPHICS_SUCCESS;
	case SIR_GRAPHICS_FORMAT_BC1_UNORM:
	case SIR_GRAPHICS_FORMAT_BC3_UNORM:
		return pTexture->eDimension == SIR_GRAPHICS_TEXTURE_3D || ( pTexture->nUsage & SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT ) != 0
		           ? SIR_GRAPHICS_UNSUPPORTED
				   : SIR_GRAPHICS_SUCCESS;
	default:
		return SIR_GRAPHICS_UNSUPPORTED;
	}
}

sirGraphicsResult_t sirD3D9EstimateTextureBytes( const sirGraphicsTextureDesc_t *pTexture, uint64_t *pBytes ) noexcept
{
	if ( pBytes == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pBytes = 0;
	const sirGraphicsResult_t eResult = sirD3D9ValidateTexture( pTexture, UINT32_MAX, 1 );
	if ( eResult != SIR_GRAPHICS_SUCCESS ) {
		return eResult;
	}
	const uint32_t nBlockBytes = pTexture->eFormat == SIR_GRAPHICS_FORMAT_BC1_UNORM ? 8 : 16;
	const bool bCompressed = pTexture->eFormat == SIR_GRAPHICS_FORMAT_BC1_UNORM || pTexture->eFormat == SIR_GRAPHICS_FORMAT_BC3_UNORM;
	uint32_t nWidth = pTexture->nWidth;
	uint32_t nHeight = pTexture->nHeight;
	uint32_t nDepth = pTexture->nDepth;
	uint64_t nTotal = 0;
	for ( uint32_t i = 0; i < pTexture->nMipLevels; ++i ) {
		uint64_t nLevel = 0;
		if ( bCompressed ) {
			const uint64_t nBlocksX = ( ( uint64_t )nWidth + 3 ) / 4;
			const uint64_t nBlocksY = ( ( uint64_t )nHeight + 3 ) / 4;
			if ( nBlocksX > UINT64_MAX / nBlocksY / nBlockBytes / pTexture->nArrayLayers ) {
				return SIR_GRAPHICS_CAPACITY_EXCEEDED;
			}
			nLevel = nBlocksX * nBlocksY * nBlockBytes * pTexture->nArrayLayers;
		} else {
			const uint64_t nArea = ( uint64_t )nWidth * nHeight;
			if ( nArea > UINT64_MAX / nDepth / pTexture->nArrayLayers / 4 ) {
				return SIR_GRAPHICS_CAPACITY_EXCEEDED;
			}
			nLevel = nArea * nDepth * pTexture->nArrayLayers * 4;
		}
		if ( nLevel > UINT64_MAX - nTotal ) {
			return SIR_GRAPHICS_CAPACITY_EXCEEDED;
		}
		nTotal += nLevel;
		nWidth = nWidth > 1 ? nWidth >> 1 : 1;
		nHeight = nHeight > 1 ? nHeight >> 1 : 1;
		nDepth = nDepth > 1 ? nDepth >> 1 : 1;
	}
	*pBytes = nTotal;
	return SIR_GRAPHICS_SUCCESS;
}

static bool sirD3D9IsFixedOperation( uint32_t nOperation ) noexcept
{
	// LERP needs a third texture-stage argument that the current v1 fixed recipe cannot encode.
	return nOperation <= SIR_GRAPHICS_FIXED_OP_DOT3 && nOperation != SIR_GRAPHICS_FIXED_OP_INTERPOLATE;
}

static bool sirD3D9IsFixedArgument( uint32_t nArgument ) noexcept
{
	return nArgument <= SIR_GRAPHICS_FIXED_ARG_CONSTANT;
}

sirGraphicsResult_t sirD3D9ValidateFixedRecipe( const sirGraphicsABIFixedRecipe_t *pRecipe, uint32_t nMaximumStages,
                                                uint32_t nMaximumTextures, uint32_t nMaximumLights ) noexcept
{
	if ( pRecipe == nullptr || pRecipe->nSize < sizeof( sirGraphicsABIFixedRecipe_t ) || pRecipe->nReserved != 0 ||
	     ( pRecipe->nTextureStageCount != 0 && pRecipe->pStages == nullptr ) ||
	     ( pRecipe->nLightCount != 0 && pRecipe->pLights == nullptr ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pRecipe->nTextureStageCount > nMaximumStages || pRecipe->nTextureStageCount > nMaximumTextures ||
	     pRecipe->nLightCount > nMaximumLights ||
	     ( pRecipe->nRequiredFeatures &
	       ~( SIR_GRAPHICS_FIXED_TRANSFORM | SIR_GRAPHICS_FIXED_LIGHTING | SIR_GRAPHICS_FIXED_FOG | SIR_GRAPHICS_FIXED_ALPHA_TEST |
	          SIR_GRAPHICS_FIXED_CPU_TRANSFORM | SIR_GRAPHICS_FIXED_TEXCOORD_GEN ) ) != 0 ||
	     pRecipe->nFogMode > SIR_GRAPHICS_FIXED_FOG_EXP2 || pRecipe->nAlphaCompare > SIR_GRAPHICS_COMPARE_ALWAYS ||
	     !( pRecipe->fAlphaReference >= 0.0f && pRecipe->fAlphaReference <= 1.0f ) ||
	     ( pRecipe->nFogMode != SIR_GRAPHICS_FIXED_FOG_NONE &&
	       ( !( pRecipe->fFogStart <= pRecipe->fFogEnd ) || !( pRecipe->fFogDensity >= 0.0f ) ) ) ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	for ( uint32_t i = 0; i < pRecipe->nTextureStageCount; ++i ) {
		const sirGraphicsABIFixedStage_t *pStage = &pRecipe->pStages[i];
		if ( ( i > 0 && pRecipe->pStages[i - 1].nColorOperation == SIR_GRAPHICS_FIXED_OP_DISABLE &&
		       pStage->nColorOperation != SIR_GRAPHICS_FIXED_OP_DISABLE ) ||
		     pStage->nTextureUnit != i || !sirD3D9IsFixedOperation( pStage->nColorOperation ) ||
		     !sirD3D9IsFixedOperation( pStage->nAlphaOperation ) || !sirD3D9IsFixedArgument( pStage->nColorArgument0 ) ||
		     !sirD3D9IsFixedArgument( pStage->nColorArgument1 ) || !sirD3D9IsFixedArgument( pStage->nAlphaArgument0 ) ||
		     !sirD3D9IsFixedArgument( pStage->nAlphaArgument1 ) ) {
			return SIR_GRAPHICS_UNSUPPORTED;
		}
	}
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirD3D9PrimitiveCount( uint32_t nTopology, uint32_t nElementCount, uint32_t *pPrimitiveCount ) noexcept
{
	if ( pPrimitiveCount == nullptr || nElementCount == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	uint32_t nCount = 0;
	switch ( nTopology ) {
	case SIR_GRAPHICS_TOPOLOGY_POINTS:
		nCount = nElementCount;
		break;
	case SIR_GRAPHICS_TOPOLOGY_LINES:
		if ( nElementCount % 2 != 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		nCount = nElementCount / 2;
		break;
	case SIR_GRAPHICS_TOPOLOGY_LINE_STRIP:
		if ( nElementCount < 2 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		nCount = nElementCount - 1;
		break;
	case SIR_GRAPHICS_TOPOLOGY_TRIANGLES:
		if ( nElementCount % 3 != 0 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		nCount = nElementCount / 3;
		break;
	case SIR_GRAPHICS_TOPOLOGY_TRIANGLE_STRIP:
		if ( nElementCount < 3 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		nCount = nElementCount - 2;
		break;
	default:
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	*pPrimitiveCount = nCount;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirD3D9ValidateDrawRange( uint32_t nTopology, uint32_t nPrimitiveCount, uint32_t nFirstElement,
                                              uint32_t nAvailableElements ) noexcept
{
	if ( nPrimitiveCount == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	uint64_t nNeeded = 0;
	switch ( nTopology ) {
	case SIR_GRAPHICS_TOPOLOGY_POINTS:
		nNeeded = nPrimitiveCount;
		break;
	case SIR_GRAPHICS_TOPOLOGY_LINES:
		nNeeded = ( uint64_t )nPrimitiveCount * 2;
		break;
	case SIR_GRAPHICS_TOPOLOGY_LINE_STRIP:
		nNeeded = ( uint64_t )nPrimitiveCount + 1;
		break;
	case SIR_GRAPHICS_TOPOLOGY_TRIANGLES:
		nNeeded = ( uint64_t )nPrimitiveCount * 3;
		break;
	case SIR_GRAPHICS_TOPOLOGY_TRIANGLE_STRIP:
		nNeeded = ( uint64_t )nPrimitiveCount + 2;
		break;
	default:
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	return nFirstElement > nAvailableElements || nNeeded > ( uint64_t )nAvailableElements - nFirstElement ? SIR_GRAPHICS_INVALID_ARGUMENT
	                                                                                                      : SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirD3D9ValidateTextureUploadLayout( const sirD3D9TextureUploadLayout_t *pLayout, uint32_t *pRowBytes,
                                                        uint32_t *pRows ) noexcept
{
	if ( pLayout == nullptr || pRowBytes == nullptr || pRows == nullptr || pLayout->nWidth == 0 || pLayout->nHeight == 0 ||
	     pLayout->nRowPitch == 0 || pLayout->nBytes == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	uint64_t nMinimumPitch = 0;
	uint64_t nRows = pLayout->nHeight;
	switch ( pLayout->eFormat ) {
	case SIR_GRAPHICS_FORMAT_BGRA8_UNORM:
	case SIR_GRAPHICS_FORMAT_RGBA8_UNORM:
		nMinimumPitch = ( uint64_t )pLayout->nWidth * 4;
		break;
	case SIR_GRAPHICS_FORMAT_BC1_UNORM:
	case SIR_GRAPHICS_FORMAT_BC3_UNORM:
		nMinimumPitch = ( ( ( uint64_t )pLayout->nWidth + 3 ) / 4 ) * ( pLayout->eFormat == SIR_GRAPHICS_FORMAT_BC1_UNORM ? 8U : 16U );
		nRows = ( ( uint64_t )pLayout->nHeight + 3 ) / 4;
		break;
	default:
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( nMinimumPitch == 0 || nMinimumPitch > UINT32_MAX || nRows > UINT32_MAX || pLayout->nRowPitch < nMinimumPitch ||
	     ( uint64_t )pLayout->nRowPitch * ( nRows - 1 ) + nMinimumPitch > pLayout->nBytes ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pRowBytes = ( uint32_t )nMinimumPitch;
	*pRows = ( uint32_t )nRows;
	return SIR_GRAPHICS_SUCCESS;
}
