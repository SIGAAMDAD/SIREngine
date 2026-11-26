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

#include <Renderer/Drivers/OpenGL/SIRGLFramebuffers.h>

static constexpr uint32_t SIR_GL_FBO_NONE = 0;
static constexpr uint32_t SIR_GL_FBO_NO_ERROR = 0;
static constexpr uint32_t SIR_GL_FBO_OUT_OF_MEMORY = 0x0505;
static constexpr uint32_t SIR_GL_FRAMEBUFFER = 0x8D40;
static constexpr uint32_t SIR_GL_FRAMEBUFFER_COMPLETE = 0x8CD5;
static constexpr uint32_t SIR_GL_COLOR_ATTACHMENT0 = 0x8CE0;
static constexpr uint32_t SIR_GL_DEPTH_ATTACHMENT = 0x8D00;
static constexpr uint32_t SIR_GL_DEPTH_STENCIL_ATTACHMENT = 0x821A;

static SIRGraphicsResult sirGLFramebufferError( uint32_t nError ) noexcept
{
	return nError == SIR_GL_FBO_OUT_OF_MEMORY ? SIRGraphicsResult::SIR_GRAPHICS_OUT_OF_MEMORY : SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR;
}

SIRGraphicsResult sirGLLoadFramebufferDispatch( sirGLProcedureLookup_t pLookup, void *pContext,
                                                  sirGLFramebufferDispatch_t *pAPI ) noexcept
{
	if ( pLookup == nullptr || pContext == nullptr || pAPI == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	void *pCreate = pLookup( pContext, "glCreateFramebuffers" );
	void *pDelete = pLookup( pContext, "glDeleteFramebuffers" );
	void *pAttach = pLookup( pContext, "glNamedFramebufferTexture" );
	void *pCheck = pLookup( pContext, "glCheckNamedFramebufferStatus" );
	void *pDraw = pLookup( pContext, "glNamedFramebufferDrawBuffer" );
	void *pRead = pLookup( pContext, "glNamedFramebufferReadBuffer" );
	void *pError = pLookup( pContext, "glGetError" );
	if ( !sirGLIsUsableProcedure( pCreate ) || !sirGLIsUsableProcedure( pDelete ) || !sirGLIsUsableProcedure( pAttach ) ||
	     !sirGLIsUsableProcedure( pCheck ) || !sirGLIsUsableProcedure( pDraw ) || !sirGLIsUsableProcedure( pRead ) ||
	     !sirGLIsUsableProcedure( pError ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGLFramebufferDispatch_t result {};
	result.pCreateFramebuffers = reinterpret_cast<void ( * )( int32_t, uint32_t * )>( pCreate );
	result.pDeleteFramebuffers = reinterpret_cast<void ( * )( int32_t, const uint32_t * )>( pDelete );
	result.pNamedFramebufferTexture = reinterpret_cast<void ( * )( uint32_t, uint32_t, uint32_t, int32_t )>( pAttach );
	result.pCheckNamedFramebufferStatus = reinterpret_cast<uint32_t ( * )( uint32_t, uint32_t )>( pCheck );
	result.pNamedFramebufferDrawBuffer = reinterpret_cast<void ( * )( uint32_t, uint32_t )>( pDraw );
	result.pNamedFramebufferReadBuffer = reinterpret_cast<void ( * )( uint32_t, uint32_t )>( pRead );
	result.pGetError = reinterpret_cast<uint32_t ( * )( void )>( pError );
	*pAPI = result;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult sirGLCreateNativeFramebuffer( const sirGLFramebufferDispatch_t *pAPI, const sirGLFramebufferDesc_t *pDesc,
                                                  uint32_t *pName ) noexcept
{
	if ( pAPI == nullptr || pDesc == nullptr || pName == nullptr || pAPI->pCreateFramebuffers == nullptr ||
	     pAPI->pDeleteFramebuffers == nullptr || pAPI->pNamedFramebufferTexture == nullptr ||
	     pAPI->pCheckNamedFramebufferStatus == nullptr || pAPI->pNamedFramebufferDrawBuffer == nullptr ||
	     pAPI->pNamedFramebufferReadBuffer == nullptr || pAPI->pGetError == nullptr ||
	     ( pDesc->nColorName == 0 && pDesc->nDepthName == 0 ) || pDesc->nColorMip > INT32_MAX || pDesc->nDepthMip > INT32_MAX ||
	     ( pDesc->nColorName == 0 && ( pDesc->nColorMip != 0 || pDesc->eColorFormat != SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_UNKNOWN ) ) ||
	     ( pDesc->nDepthName == 0 && ( pDesc->nDepthMip != 0 || pDesc->eDepthFormat != SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_UNKNOWN ) ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pDesc->nColorName != 0 && pDesc->nColorName == pDesc->nDepthName ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pDesc->nColorName != 0 && pDesc->eColorFormat != SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_RGBA8_UNORM &&
	     pDesc->eColorFormat != SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_RGBA8_SRGB && pDesc->eColorFormat != SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_R8_UNORM &&
	     pDesc->eColorFormat != SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_RG8_UNORM ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pDesc->nDepthName != 0 && pDesc->eDepthFormat != SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_D16_UNORM &&
	     pDesc->eDepthFormat != SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_D24_UNORM_S8_UINT && pDesc->eDepthFormat != SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_D32_FLOAT ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pAPI->pGetError() != SIR_GL_FBO_NO_ERROR ) {
		return SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR;
	}
	uint32_t nName = 0;
	pAPI->pCreateFramebuffers( 1, &nName );
	const uint32_t nCreateError = pAPI->pGetError();
	if ( nCreateError != SIR_GL_FBO_NO_ERROR || nName == 0 ) {
		if ( nName != 0 ) {
			pAPI->pDeleteFramebuffers( 1, &nName );
		}
		return sirGLFramebufferError( nCreateError );
	}
	if ( pDesc->nColorName != 0 ) {
		pAPI->pNamedFramebufferTexture( nName, SIR_GL_COLOR_ATTACHMENT0, pDesc->nColorName, static_cast<int32_t>(pDesc->nColorMip) );
	}
	if ( pDesc->nDepthName != 0 ) {
		const uint32_t nAttachment =
		    pDesc->eDepthFormat == SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_D24_UNORM_S8_UINT ? SIR_GL_DEPTH_STENCIL_ATTACHMENT : SIR_GL_DEPTH_ATTACHMENT;
		pAPI->pNamedFramebufferTexture( nName, nAttachment, pDesc->nDepthName, static_cast<int32_t>(pDesc->nDepthMip) );
	}
	if ( pDesc->nColorName == 0 ) {
		pAPI->pNamedFramebufferDrawBuffer( nName, SIR_GL_FBO_NONE );
		pAPI->pNamedFramebufferReadBuffer( nName, SIR_GL_FBO_NONE );
	}
	const uint32_t nAttachError = pAPI->pGetError();
	if ( nAttachError != SIR_GL_FBO_NO_ERROR ) {
		pAPI->pDeleteFramebuffers( 1, &nName );
		return sirGLFramebufferError( nAttachError );
	}
	const uint32_t nStatus = pAPI->pCheckNamedFramebufferStatus( nName, SIR_GL_FRAMEBUFFER );
	const uint32_t nCheckError = pAPI->pGetError();
	if ( nCheckError != SIR_GL_FBO_NO_ERROR || nStatus != SIR_GL_FRAMEBUFFER_COMPLETE ) {
		pAPI->pDeleteFramebuffers( 1, &nName );
		return nCheckError == SIR_GL_FBO_NO_ERROR ? SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED : sirGLFramebufferError( nCheckError );
	}
	*pName = nName;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult sirGLDeleteNativeFramebuffer( const sirGLFramebufferDispatch_t *pAPI, uint32_t nName ) noexcept
{
	if ( pAPI == nullptr || pAPI->pDeleteFramebuffers == nullptr || pAPI->pGetError == nullptr || nName == 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pAPI->pGetError() != SIR_GL_FBO_NO_ERROR ) {
		return SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR;
	}
	pAPI->pDeleteFramebuffers( 1, &nName );
	const uint32_t nError = pAPI->pGetError();
	return nError == SIR_GL_FBO_NO_ERROR ? SIRGraphicsResult::SIR_GRAPHICS_SUCCESS : sirGLFramebufferError( nError );
}
