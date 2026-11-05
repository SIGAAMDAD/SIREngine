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

#include <Renderer/Drivers/OpenGL/SIRGLTextures.h>
#include <stdint.h>

static constexpr uint32_t SIR_GL_TEX_NO_ERROR = 0;
static constexpr uint32_t SIR_GL_TEX_OUT_OF_MEMORY = 0x0505;
static constexpr uint32_t SIR_GL_TEXTURE_2D = 0x0DE1;
static constexpr uint32_t SIR_GL_RGBA8 = 0x8058;
static constexpr uint32_t SIR_GL_SRGB8_ALPHA8 = 0x8C43;
static constexpr uint32_t SIR_GL_R8 = 0x8229;
static constexpr uint32_t SIR_GL_RG8 = 0x822B;
static constexpr uint32_t SIR_GL_TEX_ALLOWED_USAGE =
    SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_COPY_SOURCE | SIR_GRAPHICS_USAGE_COPY_DESTINATION;

static sirGraphicsResult_t sirGLTextureError( uint32_t nError ) noexcept
{
	return nError == SIR_GL_TEX_OUT_OF_MEMORY ? SIR_GRAPHICS_OUT_OF_MEMORY : SIR_GRAPHICS_DRIVER_ERROR;
}

sirGraphicsResult_t sirGLLoadTextureDispatch( sirGLProcedureLookup_t pLookup, void *pContext, sirGLTextureDispatch_t *pAPI ) noexcept
{
	if ( pLookup == nullptr || pContext == nullptr || pAPI == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	void *pCreate = pLookup( pContext, "glCreateTextures" );
	void *pGen = pLookup( pContext, "glGenTextures" );
	void *pView = pLookup( pContext, "glTextureView" );
	void *pStorage = pLookup( pContext, "glTextureStorage2D" );
	void *pDelete = pLookup( pContext, "glDeleteTextures" );
	void *pError = pLookup( pContext, "glGetError" );
	if ( !sirGLIsUsableProcedure( pCreate ) || !sirGLIsUsableProcedure( pGen ) || !sirGLIsUsableProcedure( pView ) ||
	     !sirGLIsUsableProcedure( pStorage ) || !sirGLIsUsableProcedure( pDelete ) || !sirGLIsUsableProcedure( pError ) ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGLTextureDispatch_t result = {};
	result.pCreateTextures = ( decltype( result.pCreateTextures ) )pCreate;
	result.pGenTextures = ( decltype( result.pGenTextures ) )pGen;
	result.pTextureView = ( decltype( result.pTextureView ) )pView;
	result.pTextureStorage2D = ( decltype( result.pTextureStorage2D ) )pStorage;
	result.pDeleteTextures = ( decltype( result.pDeleteTextures ) )pDelete;
	result.pGetError = ( decltype( result.pGetError ) )pError;
	*pAPI = result;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirGLDescribeTexture2D( const sirGraphicsTextureDesc_t *pDesc, uint32_t nMaximumSize,
                                            sirGLNativeTexture_t *pTexture ) noexcept
{
	if ( pDesc == nullptr || pTexture == nullptr || nMaximumSize == 0 || pDesc->nWidth == 0 || pDesc->nHeight == 0 || pDesc->nDepth == 0 ||
	     pDesc->nArrayLayers == 0 || pDesc->nMipLevels == 0 || pDesc->nSampleCount == 0 || pDesc->nUsage == 0 ||
	     ( pDesc->nUsage & ~( ( SIR_GRAPHICS_USAGE_INDIRECT << 1 ) - 1U ) ) != 0 || pDesc->eFormat <= SIR_GRAPHICS_FORMAT_UNKNOWN ||
	     pDesc->eFormat > SIR_GRAPHICS_FORMAT_BC7_UNORM ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pDesc->eDimension != SIR_GRAPHICS_TEXTURE_2D || pDesc->nDepth != 1 || pDesc->nArrayLayers != 1 || pDesc->nSampleCount != 1 ||
	     pDesc->eMemory != SIR_GRAPHICS_MEMORY_GPU_LOCAL || ( pDesc->nUsage & ~SIR_GL_TEX_ALLOWED_USAGE ) != 0 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pDesc->nWidth > nMaximumSize || pDesc->nHeight > nMaximumSize || pDesc->nWidth > INT32_MAX || pDesc->nHeight > INT32_MAX ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	uint32_t nFormat = 0;
	uint32_t nBytesPerPixel = 0;
	switch ( pDesc->eFormat ) {
	case SIR_GRAPHICS_FORMAT_RGBA8_UNORM:
		nFormat = SIR_GL_RGBA8;
		nBytesPerPixel = 4;
		break;
	case SIR_GRAPHICS_FORMAT_RGBA8_SRGB:
		nFormat = SIR_GL_SRGB8_ALPHA8;
		nBytesPerPixel = 4;
		break;
	case SIR_GRAPHICS_FORMAT_R8_UNORM:
		nFormat = SIR_GL_R8;
		nBytesPerPixel = 1;
		break;
	case SIR_GRAPHICS_FORMAT_RG8_UNORM:
		nFormat = SIR_GL_RG8;
		nBytesPerPixel = 2;
		break;
	default:
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	uint32_t nWidth = pDesc->nWidth;
	uint32_t nHeight = pDesc->nHeight;
	uint64_t nBytes = 0;
	for ( uint32_t i = 0; i < pDesc->nMipLevels; ++i ) {
		if ( i != 0 && nWidth == 1 && nHeight == 1 ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		const uint64_t nLevelBytes = ( uint64_t )nWidth * ( uint64_t )nHeight * nBytesPerPixel;
		if ( nLevelBytes > UINT64_MAX - nBytes ) {
			return SIR_GRAPHICS_CAPACITY_EXCEEDED;
		}
		nBytes += nLevelBytes;
		nWidth = nWidth > 1 ? nWidth / 2 : 1;
		nHeight = nHeight > 1 ? nHeight / 2 : 1;
	}
	pTexture->nName = 0;
	pTexture->nInternalFormat = nFormat;
	pTexture->nRequestedBytes = nBytes;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirGLCreateNativeTexture2D( const sirGLTextureDispatch_t *pAPI, const sirGraphicsTextureDesc_t *pDesc,
                                                sirGLNativeTexture_t *pTexture ) noexcept
{
	if ( pAPI == nullptr || pDesc == nullptr || pTexture == nullptr || pTexture->nRequestedBytes == 0 || pTexture->nInternalFormat == 0 ||
	     pAPI->pCreateTextures == nullptr || pAPI->pTextureStorage2D == nullptr || pAPI->pDeleteTextures == nullptr ||
	     pAPI->pGetError == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pAPI->pGetError() != SIR_GL_TEX_NO_ERROR ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	uint32_t nName = 0;
	pAPI->pCreateTextures( SIR_GL_TEXTURE_2D, 1, &nName );
	const uint32_t nCreateError = pAPI->pGetError();
	if ( nCreateError != SIR_GL_TEX_NO_ERROR || nName == 0 ) {
		if ( nName != 0 ) {
			pAPI->pDeleteTextures( 1, &nName );
		}
		return sirGLTextureError( nCreateError );
	}
	pAPI->pTextureStorage2D( nName, ( int32_t )pDesc->nMipLevels, pTexture->nInternalFormat, ( int32_t )pDesc->nWidth,
	                         ( int32_t )pDesc->nHeight );
	const uint32_t nStorageError = pAPI->pGetError();
	if ( nStorageError != SIR_GL_TEX_NO_ERROR ) {
		pAPI->pDeleteTextures( 1, &nName );
		return sirGLTextureError( nStorageError );
	}
	pTexture->nName = nName;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirGLCreateNativeTextureView( const sirGLTextureDispatch_t *pAPI, const sirGLTextureViewCreateInfo_t *pInfo,
                                                  uint32_t *pName ) noexcept
{
	if ( pAPI == nullptr || pInfo == nullptr || pName == nullptr || pAPI->pGenTextures == nullptr || pAPI->pTextureView == nullptr ||
	     pAPI->pDeleteTextures == nullptr || pAPI->pGetError == nullptr || pInfo->nParentName == 0 || pInfo->nInternalFormat == 0 ||
	     pInfo->nMipLevels == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pAPI->pGetError() != SIR_GL_TEX_NO_ERROR ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	uint32_t nName = 0;
	// GL requires an unbound glGenTextures name, not a DSA-created texture object.
	pAPI->pGenTextures( 1, &nName );
	const uint32_t nGenError = pAPI->pGetError();
	if ( nGenError != SIR_GL_TEX_NO_ERROR || nName == 0 ) {
		if ( nName != 0 ) {
			pAPI->pDeleteTextures( 1, &nName );
		}
		return sirGLTextureError( nGenError );
	}
	pAPI->pTextureView( nName, SIR_GL_TEXTURE_2D, pInfo->nParentName, pInfo->nInternalFormat, pInfo->nBaseMip, pInfo->nMipLevels, 0, 1 );
	const uint32_t nViewError = pAPI->pGetError();
	if ( nViewError != SIR_GL_TEX_NO_ERROR ) {
		pAPI->pDeleteTextures( 1, &nName );
		return sirGLTextureError( nViewError );
	}
	*pName = nName;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirGLDeleteNativeTexture( const sirGLTextureDispatch_t *pAPI, uint32_t nName ) noexcept
{
	if ( pAPI == nullptr || pAPI->pDeleteTextures == nullptr || pAPI->pGetError == nullptr || nName == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pAPI->pGetError() != SIR_GL_TEX_NO_ERROR ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	pAPI->pDeleteTextures( 1, &nName );
	const uint32_t nError = pAPI->pGetError();
	return nError == SIR_GL_TEX_NO_ERROR ? SIR_GRAPHICS_SUCCESS : sirGLTextureError( nError );
}
