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

sirGraphicsResult_t sirD3D9ValidateTexture( const sirGraphicsTextureDesc_t *pTexture, uint32_t nMaximumDimension,
                                            uint32_t nMaximumSamples ) noexcept
{
	if ( pTexture == nullptr || nMaximumDimension == 0 || nMaximumSamples == 0 || pTexture->nWidth == 0 || pTexture->nHeight == 0 ||
	     pTexture->nDepth != 1 || pTexture->nArrayLayers != 1 || pTexture->nWidth > nMaximumDimension ||
	     pTexture->nHeight > nMaximumDimension || pTexture->nMipLevels == 0 || pTexture->nSampleCount == 0 ||
	     ( pTexture->nSampleCount & ( pTexture->nSampleCount - 1 ) ) != 0 || pTexture->nSampleCount > nMaximumSamples ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pTexture->eDimension != SIR_GRAPHICS_TEXTURE_2D || pTexture->nSampleCount != 1 ||
	     pTexture->eMemory != SIR_GRAPHICS_MEMORY_GPU_LOCAL || ( pTexture->nUsage & ~SIR_GRAPHICS_USAGE_SAMPLED ) != 0 ||
	     ( pTexture->nUsage & SIR_GRAPHICS_USAGE_SAMPLED ) == 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	switch ( pTexture->eFormat ) {
	case SIR_GRAPHICS_FORMAT_RGBA8_UNORM:
	case SIR_GRAPHICS_FORMAT_BGRA8_UNORM:
	case SIR_GRAPHICS_FORMAT_BC1_UNORM:
	case SIR_GRAPHICS_FORMAT_BC3_UNORM:
		return SIR_GRAPHICS_SUCCESS;
	default:
		return SIR_GRAPHICS_UNSUPPORTED;
	}
}
