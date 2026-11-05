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
#include <stdio.h>
#include <string.h>

static uint32_t s_nFailures = 0;
static uint32_t s_nError = 0;
static uint32_t s_nCreateError = 0;
static uint32_t s_nAttachError = 0;
static uint32_t s_nStatusError = 0;
static uint32_t s_nStatus = 0x8CD5;
static uint32_t s_nCreated = 0;
static uint32_t s_nDeleted = 0;
static uint32_t s_nAttachedColor = 0;
static uint32_t s_nAttachedDepth = 0;
static uint32_t s_nDepthAttachment = 0;
static uint32_t s_nDrawNone = 0;
static uint32_t s_nReadNone = 0;
static int32_t s_nColorMip = -1;
static bool s_bZeroName = false;
static const char *s_pszMissing = nullptr;

static void sirGLCheck( bool bValid, const char *pszCase ) noexcept
{
	if ( !bValid ) {
		fprintf( stderr, "FAIL: %s\n", pszCase );
		++s_nFailures;
	}
}

static void sirGLFakeCreate( int32_t, uint32_t *pName ) noexcept
{
	++s_nCreated;
	*pName = s_bZeroName ? 0 : 23;
	s_nError = s_nCreateError;
}

static void sirGLFakeDelete( int32_t, const uint32_t * ) noexcept
{
	++s_nDeleted;
}

static void sirGLFakeAttach( uint32_t, uint32_t nAttachment, uint32_t nTexture, int32_t nMip ) noexcept
{
	if ( nAttachment == 0x8CE0 ) {
		s_nAttachedColor = nTexture;
		s_nColorMip = nMip;
	} else {
		s_nDepthAttachment = nAttachment;
		s_nAttachedDepth = nTexture;
	}
	s_nError = s_nAttachError;
}

static uint32_t sirGLFakeCheck( uint32_t, uint32_t nTarget ) noexcept
{
	if ( nTarget != 0x8D40 ) {
		s_nError = 0x0502;
	} else {
		s_nError = s_nStatusError;
	}
	return s_nStatus;
}

static void sirGLFakeDraw( uint32_t, uint32_t nMode ) noexcept
{
	if ( nMode == 0 ) {
		++s_nDrawNone;
	}
}

static void sirGLFakeRead( uint32_t, uint32_t nMode ) noexcept
{
	if ( nMode == 0 ) {
		++s_nReadNone;
	}
}

static uint32_t sirGLFakeError( void ) noexcept
{
	const uint32_t nResult = s_nError;
	s_nError = 0;
	return nResult;
}

static void *sirGLFakeLookup( void *, const char *pszName ) noexcept
{
	if ( s_pszMissing != nullptr && strcmp( s_pszMissing, pszName ) == 0 ) {
		return nullptr;
	}
	if ( strcmp( pszName, "glCreateFramebuffers" ) == 0 ) {
		return ( void * )sirGLFakeCreate;
	}
	if ( strcmp( pszName, "glDeleteFramebuffers" ) == 0 ) {
		return ( void * )sirGLFakeDelete;
	}
	if ( strcmp( pszName, "glNamedFramebufferTexture" ) == 0 ) {
		return ( void * )sirGLFakeAttach;
	}
	if ( strcmp( pszName, "glCheckNamedFramebufferStatus" ) == 0 ) {
		return ( void * )sirGLFakeCheck;
	}
	if ( strcmp( pszName, "glNamedFramebufferDrawBuffer" ) == 0 ) {
		return ( void * )sirGLFakeDraw;
	}
	if ( strcmp( pszName, "glNamedFramebufferReadBuffer" ) == 0 ) {
		return ( void * )sirGLFakeRead;
	}
	if ( strcmp( pszName, "glGetError" ) == 0 ) {
		return ( void * )sirGLFakeError;
	}
	return nullptr;
}

int main( void )
{
	sirGLFramebufferDispatch_t api = {};
	sirGLCheck( sirGLLoadFramebufferDispatch( nullptr, &api, &api ) == SIR_GRAPHICS_INVALID_ARGUMENT, "no lookup" );
	sirGLCheck( sirGLLoadFramebufferDispatch( sirGLFakeLookup, nullptr, &api ) == SIR_GRAPHICS_INVALID_ARGUMENT, "no context" );
	s_pszMissing = "glCheckNamedFramebufferStatus";
	sirGLCheck( sirGLLoadFramebufferDispatch( sirGLFakeLookup, &api, &api ) == SIR_GRAPHICS_UNSUPPORTED, "missing completeness" );
	s_pszMissing = "glNamedFramebufferReadBuffer";
	sirGLCheck( sirGLLoadFramebufferDispatch( sirGLFakeLookup, &api, &api ) == SIR_GRAPHICS_UNSUPPORTED, "missing read mode" );
	s_pszMissing = nullptr;
	sirGLCheck( sirGLLoadFramebufferDispatch( sirGLFakeLookup, &api, &api ) == SIR_GRAPHICS_SUCCESS, "dispatch loaded" );

	sirGLFramebufferDesc_t desc = {};
	uint32_t nFBO = 0;
	sirGLCheck( sirGLCreateNativeFramebuffer( &api, &desc, &nFBO ) == SIR_GRAPHICS_INVALID_ARGUMENT, "empty attachments" );
	desc.nColorName = 41;
	desc.nColorMip = 2;
	desc.eColorFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	sirGLCheck( sirGLCreateNativeFramebuffer( &api, &desc, &nFBO ) == SIR_GRAPHICS_SUCCESS && nFBO == 23 && s_nAttachedColor == 41 &&
	                s_nColorMip == 2 && s_nDrawNone == 0 && s_nReadNone == 0,
	            "color attachment complete" );
	sirGLCheck( sirGLDeleteNativeFramebuffer( &api, nFBO ) == SIR_GRAPHICS_SUCCESS, "framebuffer deleted" );
	desc.nDepthName = 42;
	desc.eDepthFormat = SIR_GRAPHICS_FORMAT_D24_UNORM_S8_UINT;
	sirGLCheck( sirGLCreateNativeFramebuffer( &api, &desc, &nFBO ) == SIR_GRAPHICS_SUCCESS && s_nAttachedDepth == 42 &&
	                s_nDepthAttachment == 0x821A,
	            "color and depth-stencil attachments" );
	sirGLCheck( sirGLDeleteNativeFramebuffer( &api, nFBO ) == SIR_GRAPHICS_SUCCESS, "mixed attachments deleted" );
	desc.nColorName = 0;
	desc.nColorMip = 0;
	desc.eColorFormat = SIR_GRAPHICS_FORMAT_UNKNOWN;
	desc.eDepthFormat = SIR_GRAPHICS_FORMAT_D32_FLOAT;
	sirGLCheck( sirGLCreateNativeFramebuffer( &api, &desc, &nFBO ) == SIR_GRAPHICS_SUCCESS && s_nDepthAttachment == 0x8D00 &&
	                s_nDrawNone == 1 && s_nReadNone == 1,
	            "depth-only FBO sets GL_NONE draw/read" );
	sirGLCheck( sirGLDeleteNativeFramebuffer( &api, nFBO ) == SIR_GRAPHICS_SUCCESS, "depth-only FBO deleted" );
	desc.eDepthFormat = SIR_GRAPHICS_FORMAT_R8_UNORM;
	sirGLCheck( sirGLCreateNativeFramebuffer( &api, &desc, &nFBO ) == SIR_GRAPHICS_UNSUPPORTED, "invalid depth attachment format" );
	desc.eDepthFormat = SIR_GRAPHICS_FORMAT_D16_UNORM;
	desc.nColorName = 42;
	desc.eColorFormat = SIR_GRAPHICS_FORMAT_R8_UNORM;
	sirGLCheck( sirGLCreateNativeFramebuffer( &api, &desc, &nFBO ) == SIR_GRAPHICS_INVALID_ARGUMENT, "self attachment rejected" );
	desc.nColorName = 41;
	desc.eColorFormat = SIR_GRAPHICS_FORMAT_D16_UNORM;
	sirGLCheck( sirGLCreateNativeFramebuffer( &api, &desc, &nFBO ) == SIR_GRAPHICS_UNSUPPORTED, "invalid color format" );
	desc.eColorFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	s_nError = 0x0502;
	sirGLCheck( sirGLCreateNativeFramebuffer( &api, &desc, &nFBO ) == SIR_GRAPHICS_DRIVER_ERROR, "pending error rejected" );
	s_nCreateError = 0x0505;
	sirGLCheck( sirGLCreateNativeFramebuffer( &api, &desc, &nFBO ) == SIR_GRAPHICS_OUT_OF_MEMORY, "create OOM" );
	s_nCreateError = 0;
	s_bZeroName = true;
	sirGLCheck( sirGLCreateNativeFramebuffer( &api, &desc, &nFBO ) == SIR_GRAPHICS_DRIVER_ERROR, "zero native FBO" );
	s_bZeroName = false;
	s_nAttachError = 0x0502;
	sirGLCheck( sirGLCreateNativeFramebuffer( &api, &desc, &nFBO ) == SIR_GRAPHICS_DRIVER_ERROR, "attach GL error" );
	s_nAttachError = 0;
	s_nStatus = 0x8CD6;
	sirGLCheck( sirGLCreateNativeFramebuffer( &api, &desc, &nFBO ) == SIR_GRAPHICS_UNSUPPORTED, "incomplete attachment cleaned" );
	s_nStatus = 0x8CD5;
	s_nStatusError = 0x0502;
	sirGLCheck( sirGLCreateNativeFramebuffer( &api, &desc, &nFBO ) == SIR_GRAPHICS_DRIVER_ERROR, "completeness GL error" );
	s_nStatusError = 0;
	sirGLCheck( sirGLDeleteNativeFramebuffer( &api, 0 ) == SIR_GRAPHICS_INVALID_ARGUMENT, "zero FBO rejected" );
	sirGLCheck( s_nCreated == 8 && s_nDeleted == 7, "failed native names retired" );
	if ( s_nFailures == 0 ) {
		printf( "GL46 framebuffer DSA tests passed\n" );
	}
	return s_nFailures == 0 ? 0 : 1;
}
