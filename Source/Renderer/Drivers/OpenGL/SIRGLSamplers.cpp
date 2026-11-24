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

#include <Renderer/Drivers/OpenGL/SIRGLSamplers.h>
#include <cmath>

static constexpr uint32_t SIR_GL_SAMPLER_NO_ERROR = 0;
static constexpr uint32_t SIR_GL_SAMPLER_OUT_OF_MEMORY = 0x0505;
static constexpr uint32_t SIR_GL_TEXTURE_MIN_FILTER = 0x2801;
static constexpr uint32_t SIR_GL_TEXTURE_MAG_FILTER = 0x2800;
static constexpr uint32_t SIR_GL_TEXTURE_WRAP_S = 0x2802;
static constexpr uint32_t SIR_GL_TEXTURE_WRAP_T = 0x2803;
static constexpr uint32_t SIR_GL_TEXTURE_WRAP_R = 0x8072;
static constexpr uint32_t SIR_GL_TEXTURE_MIN_LOD = 0x813A;
static constexpr uint32_t SIR_GL_TEXTURE_MAX_LOD = 0x813B;
static constexpr int32_t SIR_GL_NEAREST = 0x2600;
static constexpr int32_t SIR_GL_LINEAR = 0x2601;
static constexpr int32_t SIR_GL_LINEAR_MIPMAP_LINEAR = 0x2703;
static constexpr int32_t SIR_GL_REPEAT = 0x2901;
static constexpr int32_t SIR_GL_MIRRORED_REPEAT = 0x8370;
static constexpr int32_t SIR_GL_CLAMP_TO_EDGE = 0x812F;
static constexpr int32_t SIR_GL_CLAMP_TO_BORDER = 0x812D;

static SIRGraphicsResult sirGLSamplerError( uint32_t nError ) noexcept
{
	return nError == SIR_GL_SAMPLER_OUT_OF_MEMORY ? SIRGraphicsResult::SIR_GRAPHICS_OUT_OF_MEMORY : SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR;
}

static int32_t sirGLSamplerAddress( SIRGraphicsAddressMode eAddress ) noexcept
{
	static constexpr int32_t SIR_GL_ADDRESS[] = { SIR_GL_REPEAT, SIR_GL_MIRRORED_REPEAT, SIR_GL_CLAMP_TO_EDGE, SIR_GL_CLAMP_TO_BORDER };
	return SIR_GL_ADDRESS[static_cast<uint32_t>(eAddress)];
}

SIRGraphicsResult sirGLLoadSamplerDispatch( sirGLProcedureLookup_t pLookup, void *pContext, sirGLSamplerDispatch_t *pAPI ) noexcept
{
	if ( pLookup == nullptr || pContext == nullptr || pAPI == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	void *pCreate = pLookup( pContext, "glCreateSamplers" );
	void *pDelete = pLookup( pContext, "glDeleteSamplers" );
	void *pSetInt = pLookup( pContext, "glSamplerParameteri" );
	void *pSetFloat = pLookup( pContext, "glSamplerParameterf" );
	void *pError = pLookup( pContext, "glGetError" );
	if ( !sirGLIsUsableProcedure( pCreate ) || !sirGLIsUsableProcedure( pDelete ) || !sirGLIsUsableProcedure( pSetInt ) ||
	     !sirGLIsUsableProcedure( pSetFloat ) || !sirGLIsUsableProcedure( pError ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGLSamplerDispatch_t result {};
	result.pCreateSamplers = reinterpret_cast<void ( * )( int32_t, uint32_t * )>( pCreate );
	result.pDeleteSamplers = reinterpret_cast<void ( * )( int32_t, const uint32_t * )>( pDelete );
	result.pSamplerParameteri = reinterpret_cast<void ( * )( uint32_t, uint32_t, int32_t )>( pSetInt );
	result.pSamplerParameterf = reinterpret_cast<void ( * )( uint32_t, uint32_t, float )>( pSetFloat );
	result.pGetError = reinterpret_cast<uint32_t ( * )( void )>( pError );
	*pAPI = result;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult sirGLCreateNativeSampler( const sirGLSamplerDispatch_t *pAPI, const sirGraphicsSamplerDesc_t *pDesc,
                                              uint32_t *pName ) noexcept
{
	if ( pAPI == nullptr || pDesc == nullptr || pName == nullptr || pAPI->pCreateSamplers == nullptr || pAPI->pDeleteSamplers == nullptr ||
	     pAPI->pSamplerParameteri == nullptr || pAPI->pSamplerParameterf == nullptr || pAPI->pGetError == nullptr ||
	     pDesc->eFilter > SIRGraphicsSamplerFilter::SIR_GRAPHICS_FILTER_ANISOTROPIC || pDesc->eAddressU > SIRGraphicsAddressMode::SIR_GRAPHICS_ADDRESS_BORDER ||
	     pDesc->eAddressV > SIRGraphicsAddressMode::SIR_GRAPHICS_ADDRESS_BORDER || pDesc->eAddressW > SIRGraphicsAddressMode::SIR_GRAPHICS_ADDRESS_BORDER ||
	     !( pDesc->fMaximumAnisotropy >= 1.0f ) || !( pDesc->fMaximumAnisotropy <= 16.0f ) || !std::isfinite( pDesc->fMinimumLod ) ||
	     !std::isfinite( pDesc->fMaximumLod ) || pDesc->fMinimumLod > pDesc->fMaximumLod ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	// GL_EXT_texture_filter_anisotropic is not part of mandatory OpenGL 4.6 core.
	if ( pDesc->eFilter == SIRGraphicsSamplerFilter::SIR_GRAPHICS_FILTER_ANISOTROPIC || pDesc->fMaximumAnisotropy != 1.0f ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pAPI->pGetError() != SIR_GL_SAMPLER_NO_ERROR ) {
		return SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR;
	}
	uint32_t nName = 0;
	pAPI->pCreateSamplers( 1, &nName );
	const uint32_t nCreateError = pAPI->pGetError();
	if ( nCreateError != SIR_GL_SAMPLER_NO_ERROR || nName == 0 ) {
		if ( nName != 0 ) {
			pAPI->pDeleteSamplers( 1, &nName );
		}
		return sirGLSamplerError( nCreateError );
	}
	const int32_t nMinFilter = pDesc->eFilter == SIRGraphicsSamplerFilter::SIR_GRAPHICS_FILTER_TRILINEAR ? SIR_GL_LINEAR_MIPMAP_LINEAR
	                           : pDesc->eFilter == SIRGraphicsSamplerFilter::SIR_GRAPHICS_FILTER_NEAREST ? SIR_GL_NEAREST
	                                                                           : SIR_GL_LINEAR;
	const int32_t nMagFilter = pDesc->eFilter == SIRGraphicsSamplerFilter::SIR_GRAPHICS_FILTER_NEAREST ? SIR_GL_NEAREST : SIR_GL_LINEAR;
	pAPI->pSamplerParameteri( nName, SIR_GL_TEXTURE_MIN_FILTER, nMinFilter );
	pAPI->pSamplerParameteri( nName, SIR_GL_TEXTURE_MAG_FILTER, nMagFilter );
	pAPI->pSamplerParameteri( nName, SIR_GL_TEXTURE_WRAP_S, sirGLSamplerAddress( pDesc->eAddressU ) );
	pAPI->pSamplerParameteri( nName, SIR_GL_TEXTURE_WRAP_T, sirGLSamplerAddress( pDesc->eAddressV ) );
	pAPI->pSamplerParameteri( nName, SIR_GL_TEXTURE_WRAP_R, sirGLSamplerAddress( pDesc->eAddressW ) );
	pAPI->pSamplerParameterf( nName, SIR_GL_TEXTURE_MIN_LOD, pDesc->fMinimumLod );
	pAPI->pSamplerParameterf( nName, SIR_GL_TEXTURE_MAX_LOD, pDesc->fMaximumLod );
	const uint32_t nConfigError = pAPI->pGetError();
	if ( nConfigError != SIR_GL_SAMPLER_NO_ERROR ) {
		pAPI->pDeleteSamplers( 1, &nName );
		return sirGLSamplerError( nConfigError );
	}
	*pName = nName;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult sirGLDeleteNativeSampler( const sirGLSamplerDispatch_t *pAPI, uint32_t nName ) noexcept
{
	if ( pAPI == nullptr || pAPI->pDeleteSamplers == nullptr || pAPI->pGetError == nullptr || nName == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pAPI->pGetError() != SIR_GL_SAMPLER_NO_ERROR ) {
		return SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR;
	}
	pAPI->pDeleteSamplers( 1, &nName );
	const uint32_t nError = pAPI->pGetError();
	return nError == SIR_GL_SAMPLER_NO_ERROR ? SIRGraphicsResult::SIR_GRAPHICS_SUCCESS : sirGLSamplerError( nError );
}
