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

#include <Renderer/Drivers/OpenGL/SIRGLFormats.h>

static constexpr uint32_t SIR_GL_FORMAT_TEXTURE_2D = 0x0DE1;
static constexpr uint32_t SIR_GL_FORMAT_INTERNALFORMAT_SUPPORTED = 0x826F;
static constexpr uint32_t SIR_GL_FORMAT_RGBA8 = 0x8058;
static constexpr uint32_t SIR_GL_FORMAT_SRGB8_ALPHA8 = 0x8C43;
static constexpr uint32_t SIR_GL_FORMAT_R8 = 0x8229;
static constexpr uint32_t SIR_GL_FORMAT_RG8 = 0x822B;
static constexpr uint32_t SIR_GL_FORMAT_NO_ERROR = 0;

sirGraphicsResult_t sirGLLoadFormatDispatch( sirGLProcedureLookup_t pLookup, void *pContext, sirGLFormatDispatch_t *pAPI ) noexcept
{
	if ( pLookup == nullptr || pContext == nullptr || pAPI == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	void *pQuery = pLookup( pContext, "glGetInternalformativ" );
	void *pError = pLookup( pContext, "glGetError" );
	if ( !sirGLIsUsableProcedure( pQuery ) || !sirGLIsUsableProcedure( pError ) ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGLFormatDispatch_t result = {};
	result.pGetInternalformativ = ( decltype( result.pGetInternalformativ ) )pQuery;
	result.pGetError = ( decltype( result.pGetError ) )pError;
	*pAPI = result;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirGLQueryNativeFormat( const sirGLFormatDispatch_t *pAPI, sirGraphicsABIFormat_t *pFormat ) noexcept
{
	if ( pAPI == nullptr || pFormat == nullptr || pFormat->nSize < sizeof( *pFormat ) || pFormat->nReserved != 0 ||
	     pAPI->pGetInternalformativ == nullptr || pAPI->pGetError == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	uint32_t nInternalFormat = 0;
	switch ( pFormat->nFormat ) {
	case SIR_GRAPHICS_FORMAT_RGBA8_UNORM:
		nInternalFormat = SIR_GL_FORMAT_RGBA8;
		break;
	case SIR_GRAPHICS_FORMAT_RGBA8_SRGB:
		nInternalFormat = SIR_GL_FORMAT_SRGB8_ALPHA8;
		break;
	case SIR_GRAPHICS_FORMAT_R8_UNORM:
		nInternalFormat = SIR_GL_FORMAT_R8;
		break;
	case SIR_GRAPHICS_FORMAT_RG8_UNORM:
		nInternalFormat = SIR_GL_FORMAT_RG8;
		break;
	default:
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pAPI->pGetError() != SIR_GL_FORMAT_NO_ERROR ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	int32_t nSupported = 0;
	pAPI->pGetInternalformativ( SIR_GL_FORMAT_TEXTURE_2D, nInternalFormat, SIR_GL_FORMAT_INTERNALFORMAT_SUPPORTED, 1, &nSupported );
	if ( pAPI->pGetError() != SIR_GL_FORMAT_NO_ERROR ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	if ( nSupported != 1 ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	// Only current 2D single-sample immutable storage and sampled-format support
	// are qualified. Do not expose absent transfer, FBO, storage-image or MSAA paths.
	sirGraphicsABIFormat_t result = {};
	result.nSize = sizeof( result );
	result.nFormat = pFormat->nFormat;
	result.nOptimalFeatures = SIR_GRAPHICS_ABI_FORMAT_SAMPLED;
	result.nSampleCountMask = 1;
	*pFormat = result;
	return SIR_GRAPHICS_SUCCESS;
}
