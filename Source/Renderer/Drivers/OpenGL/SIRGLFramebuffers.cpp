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

#include <Renderer/Drivers/OpenGL/SIRGLFramebuffers.h>

static constexpr uint32_t SIR_GL_FBO_NONE = 0;
static constexpr uint32_t SIR_GL_FBO_NO_ERROR = 0;
static constexpr uint32_t SIR_GL_FBO_OUT_OF_MEMORY = 0x0505;
static constexpr uint32_t SIR_GL_FRAMEBUFFER = 0x8D40;
static constexpr uint32_t SIR_GL_FRAMEBUFFER_COMPLETE = 0x8CD5;
static constexpr uint32_t SIR_GL_COLOR_ATTACHMENT0 = 0x8CE0;
static constexpr uint32_t SIR_GL_DEPTH_ATTACHMENT = 0x8D00;
static constexpr uint32_t SIR_GL_DEPTH_STENCIL_ATTACHMENT = 0x821A;

static sirGraphicsResult_t sirGLFramebufferError( uint32_t nError ) noexcept
{
	return nError == SIR_GL_FBO_OUT_OF_MEMORY ? SIR_GRAPHICS_OUT_OF_MEMORY : SIR_GRAPHICS_DRIVER_ERROR;
}

sirGraphicsResult_t sirGLLoadFramebufferDispatch( sirGLProcedureLookup_t pLookup, void *pContext,
                                                  sirGLFramebufferDispatch_t *pAPI ) noexcept
{
	if ( pLookup == nullptr || pContext == nullptr || pAPI == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
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
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	sirGLFramebufferDispatch_t result = {};
	result.pCreateFramebuffers = ( decltype( result.pCreateFramebuffers ) )pCreate;
	result.pDeleteFramebuffers = ( decltype( result.pDeleteFramebuffers ) )pDelete;
	result.pNamedFramebufferTexture = ( decltype( result.pNamedFramebufferTexture ) )pAttach;
	result.pCheckNamedFramebufferStatus = ( decltype( result.pCheckNamedFramebufferStatus ) )pCheck;
	result.pNamedFramebufferDrawBuffer = ( decltype( result.pNamedFramebufferDrawBuffer ) )pDraw;
	result.pNamedFramebufferReadBuffer = ( decltype( result.pNamedFramebufferReadBuffer ) )pRead;
	result.pGetError = ( decltype( result.pGetError ) )pError;
	*pAPI = result;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirGLCreateNativeFramebuffer( const sirGLFramebufferDispatch_t *pAPI, const sirGLFramebufferDesc_t *pDesc,
                                                  uint32_t *pName ) noexcept
{
	if ( pAPI == nullptr || pDesc == nullptr || pName == nullptr || pAPI->pCreateFramebuffers == nullptr ||
	     pAPI->pDeleteFramebuffers == nullptr || pAPI->pNamedFramebufferTexture == nullptr ||
	     pAPI->pCheckNamedFramebufferStatus == nullptr || pAPI->pNamedFramebufferDrawBuffer == nullptr ||
	     pAPI->pNamedFramebufferReadBuffer == nullptr || pAPI->pGetError == nullptr ||
	     ( pDesc->nColorName == 0 && pDesc->nDepthName == 0 ) || pDesc->nColorMip > INT32_MAX || pDesc->nDepthMip > INT32_MAX ||
	     ( pDesc->nColorName == 0 && ( pDesc->nColorMip != 0 || pDesc->eColorFormat != SIR_GRAPHICS_FORMAT_UNKNOWN ) ) ||
	     ( pDesc->nDepthName == 0 && ( pDesc->nDepthMip != 0 || pDesc->eDepthFormat != SIR_GRAPHICS_FORMAT_UNKNOWN ) ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pDesc->nColorName != 0 && pDesc->nColorName == pDesc->nDepthName ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pDesc->nColorName != 0 && pDesc->eColorFormat != SIR_GRAPHICS_FORMAT_RGBA8_UNORM &&
	     pDesc->eColorFormat != SIR_GRAPHICS_FORMAT_RGBA8_SRGB && pDesc->eColorFormat != SIR_GRAPHICS_FORMAT_R8_UNORM &&
	     pDesc->eColorFormat != SIR_GRAPHICS_FORMAT_RG8_UNORM ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pDesc->nDepthName != 0 && pDesc->eDepthFormat != SIR_GRAPHICS_FORMAT_D16_UNORM &&
	     pDesc->eDepthFormat != SIR_GRAPHICS_FORMAT_D24_UNORM_S8_UINT && pDesc->eDepthFormat != SIR_GRAPHICS_FORMAT_D32_FLOAT ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( pAPI->pGetError() != SIR_GL_FBO_NO_ERROR ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
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
		pAPI->pNamedFramebufferTexture( nName, SIR_GL_COLOR_ATTACHMENT0, pDesc->nColorName, ( int32_t )pDesc->nColorMip );
	}
	if ( pDesc->nDepthName != 0 ) {
		const uint32_t nAttachment =
		    pDesc->eDepthFormat == SIR_GRAPHICS_FORMAT_D24_UNORM_S8_UINT ? SIR_GL_DEPTH_STENCIL_ATTACHMENT : SIR_GL_DEPTH_ATTACHMENT;
		pAPI->pNamedFramebufferTexture( nName, nAttachment, pDesc->nDepthName, ( int32_t )pDesc->nDepthMip );
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
		return nCheckError == SIR_GL_FBO_NO_ERROR ? SIR_GRAPHICS_UNSUPPORTED : sirGLFramebufferError( nCheckError );
	}
	*pName = nName;
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t sirGLDeleteNativeFramebuffer( const sirGLFramebufferDispatch_t *pAPI, uint32_t nName ) noexcept
{
	if ( pAPI == nullptr || pAPI->pDeleteFramebuffers == nullptr || pAPI->pGetError == nullptr || nName == 0 ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( pAPI->pGetError() != SIR_GL_FBO_NO_ERROR ) {
		return SIR_GRAPHICS_DRIVER_ERROR;
	}
	pAPI->pDeleteFramebuffers( 1, &nName );
	const uint32_t nError = pAPI->pGetError();
	return nError == SIR_GL_FBO_NO_ERROR ? SIR_GRAPHICS_SUCCESS : sirGLFramebufferError( nError );
}
