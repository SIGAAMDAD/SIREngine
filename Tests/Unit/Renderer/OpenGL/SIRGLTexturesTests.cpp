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
#include <stdio.h>
#include <string.h>

static int s_nFailures = 0;
static uint32_t s_nError = 0;
static uint32_t s_nCreateError = 0;
static uint32_t s_nStorageError = 0;
static uint32_t s_nGenError = 0;
static uint32_t s_nViewError = 0;
static uint32_t s_nGenerated = 0;
static uint32_t s_nViewed = 0;
static uint32_t s_nViewParent = 0;
static uint32_t s_nViewMip = 0;
static uint32_t s_nViewLevels = 0;
static uint32_t s_nCreated = 0;
static uint32_t s_nDeleted = 0;
static int32_t s_nLevels = 0;
static uint32_t s_nLastFormat = 0;
static uint32_t s_nLastTarget = 0;
static bool s_bZeroName = false;
static const char *s_pszMissing = nullptr;

static void sirGLCheck( bool bGood, const char *pszCase ) noexcept
{
	if ( !bGood ) {
		fprintf( stderr, "FAIL: %s\n", pszCase );
		++s_nFailures;
	}
}

static void sirGLFakeCreate( uint32_t nTarget, int32_t, uint32_t *pName ) noexcept
{
	++s_nCreated;
	s_nLastTarget = nTarget;
	*pName = s_bZeroName ? 0 : 53;
	s_nError = s_nCreateError;
}

static void sirGLFakeGen( int32_t, uint32_t *pName ) noexcept
{
	++s_nGenerated;
	*pName = s_bZeroName ? 0 : 91;
	s_nError = s_nGenError;
}

static void sirGLFakeView( uint32_t, uint32_t nTarget, uint32_t nParent, uint32_t, uint32_t nBaseMip, uint32_t nLevels, uint32_t nBaseLayer,
                           uint32_t nLayers ) noexcept
{
	++s_nViewed;
	s_nLastTarget = nTarget;
	s_nViewParent = nParent;
	s_nViewMip = nBaseMip;
	s_nViewLevels = nLevels;
	if ( nBaseLayer != 0 || nLayers != 1 ) {
		s_nError = 0x0502;
	} else {
		s_nError = s_nViewError;
	}
}

static void sirGLFakeStorage( uint32_t, int32_t nLevels, uint32_t nFormat, int32_t, int32_t ) noexcept
{
	s_nLevels = nLevels;
	s_nLastFormat = nFormat;
	s_nError = s_nStorageError;
}

static void sirGLFakeDelete( int32_t, const uint32_t * ) noexcept
{
	++s_nDeleted;
}

static uint32_t sirGLFakeGetError( void ) noexcept
{
	const uint32_t nError = s_nError;
	s_nError = 0;
	return nError;
}

static void *sirGLFakeLookup( void *, const char *pszName ) noexcept
{
	if ( s_pszMissing != nullptr && strcmp( s_pszMissing, pszName ) == 0 ) {
		return nullptr;
	}
	if ( strcmp( pszName, "glCreateTextures" ) == 0 ) {
		return ( void * )sirGLFakeCreate;
	}
	if ( strcmp( pszName, "glGenTextures" ) == 0 ) {
		return ( void * )sirGLFakeGen;
	}
	if ( strcmp( pszName, "glTextureView" ) == 0 ) {
		return ( void * )sirGLFakeView;
	}
	if ( strcmp( pszName, "glTextureStorage2D" ) == 0 ) {
		return ( void * )sirGLFakeStorage;
	}
	if ( strcmp( pszName, "glDeleteTextures" ) == 0 ) {
		return ( void * )sirGLFakeDelete;
	}
	if ( strcmp( pszName, "glGetError" ) == 0 ) {
		return ( void * )sirGLFakeGetError;
	}
	return nullptr;
}

int main( void )
{
	sirGLTextureDispatch_t api = {};
	sirGLCheck( sirGLLoadTextureDispatch( nullptr, &api, &api ) == SIR_GRAPHICS_INVALID_ARGUMENT, "null loader" );
	s_pszMissing = "glTextureStorage2D";
	sirGLCheck( sirGLLoadTextureDispatch( sirGLFakeLookup, &api, &api ) == SIR_GRAPHICS_UNSUPPORTED, "missing storage function" );
	s_pszMissing = "glTextureView";
	sirGLCheck( sirGLLoadTextureDispatch( sirGLFakeLookup, &api, &api ) == SIR_GRAPHICS_UNSUPPORTED, "missing texture view" );
	s_pszMissing = nullptr;
	sirGLCheck( sirGLLoadTextureDispatch( sirGLFakeLookup, &api, &api ) == SIR_GRAPHICS_SUCCESS, "load texture functions" );

	sirGraphicsTextureDesc_t desc = {};
	desc.nWidth = 16;
	desc.nHeight = 8;
	desc.nDepth = 1;
	desc.nArrayLayers = 1;
	desc.nMipLevels = 3;
	desc.nSampleCount = 1;
	desc.nUsage = SIR_GRAPHICS_USAGE_SAMPLED | SIR_GRAPHICS_USAGE_COPY_DESTINATION;
	desc.eDimension = SIR_GRAPHICS_TEXTURE_2D;
	desc.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	desc.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	sirGLNativeTexture_t texture = {};
	sirGLCheck( sirGLDescribeTexture2D( &desc, 4096, &texture ) == SIR_GRAPHICS_SUCCESS && texture.nRequestedBytes == 672 &&
	                texture.nInternalFormat == 0x8058,
	            "2D RGBA mip storage size" );
	sirGLCheck( sirGLCreateNativeTexture2D( &api, &desc, &texture ) == SIR_GRAPHICS_SUCCESS && texture.nName == 53 && s_nLevels == 3 &&
	                s_nLastTarget == 0x0DE1 && s_nLastFormat == 0x8058,
	            "immutable texture allocation" );
	sirGLCheck( sirGLDeleteNativeTexture( &api, texture.nName ) == SIR_GRAPHICS_SUCCESS, "delete texture" );
	desc.eFormat = SIR_GRAPHICS_FORMAT_R8_UNORM;
	sirGLCheck( sirGLDescribeTexture2D( &desc, 4096, &texture ) == SIR_GRAPHICS_SUCCESS && texture.nRequestedBytes == 168 &&
	                texture.nInternalFormat == 0x8229,
	            "R8 mip allocation" );
	desc.eFormat = SIR_GRAPHICS_FORMAT_RG8_UNORM;
	sirGLCheck( sirGLDescribeTexture2D( &desc, 4096, &texture ) == SIR_GRAPHICS_SUCCESS && texture.nRequestedBytes == 336,
	            "RG8 mip allocation" );
	desc.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_SRGB;
	sirGLCheck( sirGLDescribeTexture2D( &desc, 4096, &texture ) == SIR_GRAPHICS_SUCCESS && texture.nInternalFormat == 0x8C43,
	            "sRGB storage" );
	desc.eFormat = SIR_GRAPHICS_FORMAT_BGRA8_UNORM;
	sirGLCheck( sirGLDescribeTexture2D( &desc, 4096, &texture ) == SIR_GRAPHICS_UNSUPPORTED, "unqualified format rejected" );
	desc.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	desc.nWidth = 0;
	sirGLCheck( sirGLDescribeTexture2D( &desc, 4096, &texture ) == SIR_GRAPHICS_INVALID_ARGUMENT, "zero width" );
	desc.nWidth = 16;
	desc.nMipLevels = 6;
	sirGLCheck( sirGLDescribeTexture2D( &desc, 4096, &texture ) == SIR_GRAPHICS_INVALID_ARGUMENT, "too many mips" );
	desc.nMipLevels = 3;
	desc.nSampleCount = 4;
	sirGLCheck( sirGLDescribeTexture2D( &desc, 4096, &texture ) == SIR_GRAPHICS_UNSUPPORTED, "multisample pending" );
	desc.nSampleCount = 1;
	desc.nUsage = SIR_GRAPHICS_USAGE_STORAGE;
	sirGLCheck( sirGLDescribeTexture2D( &desc, 4096, &texture ) == SIR_GRAPHICS_UNSUPPORTED, "storage view pending" );
	desc.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	desc.eMemory = SIR_GRAPHICS_MEMORY_CPU_UPLOAD;
	sirGLCheck( sirGLDescribeTexture2D( &desc, 4096, &texture ) == SIR_GRAPHICS_UNSUPPORTED, "CPU upload pending" );
	desc.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	sirGLCheck( sirGLDescribeTexture2D( &desc, 8, &texture ) == SIR_GRAPHICS_UNSUPPORTED, "dimension limit" );
	sirGLCheck( sirGLDescribeTexture2D( &desc, 4096, &texture ) == SIR_GRAPHICS_SUCCESS, "restore valid descriptor" );
	s_nError = 0x0502;
	sirGLCheck( sirGLCreateNativeTexture2D( &api, &desc, &texture ) == SIR_GRAPHICS_DRIVER_ERROR, "existing GL error" );
	s_nCreateError = 0x0505;
	sirGLCheck( sirGLCreateNativeTexture2D( &api, &desc, &texture ) == SIR_GRAPHICS_OUT_OF_MEMORY, "create OOM" );
	s_nCreateError = 0;
	s_bZeroName = true;
	sirGLCheck( sirGLCreateNativeTexture2D( &api, &desc, &texture ) == SIR_GRAPHICS_DRIVER_ERROR, "zero native texture" );
	s_bZeroName = false;
	s_nStorageError = 0x0505;
	sirGLCheck( sirGLCreateNativeTexture2D( &api, &desc, &texture ) == SIR_GRAPHICS_OUT_OF_MEMORY, "storage OOM" );
	sirGLCheck( s_nDeleted == 3 && s_nCreated == 4, "failure rollback" );
	sirGLCheck( sirGLDeleteNativeTexture( &api, 0 ) == SIR_GRAPHICS_INVALID_ARGUMENT, "zero delete" );
	sirGLTextureViewCreateInfo_t view = { 53, 0x8058, 1, 2 };
	uint32_t nViewName = 0;
	sirGLCheck( sirGLCreateNativeTextureView( &api, &view, &nViewName ) == SIR_GRAPHICS_SUCCESS && nViewName == 91 && s_nViewParent == 53 &&
	                s_nLastTarget == 0x0DE1 && s_nViewMip == 1 && s_nViewLevels == 2,
	            "2D immutable texture view" );
	sirGLCheck( sirGLDeleteNativeTexture( &api, nViewName ) == SIR_GRAPHICS_SUCCESS, "delete texture view" );
	view.nMipLevels = 0;
	sirGLCheck( sirGLCreateNativeTextureView( &api, &view, &nViewName ) == SIR_GRAPHICS_INVALID_ARGUMENT, "empty view" );
	view.nMipLevels = 2;
	s_nError = 0x0502;
	sirGLCheck( sirGLCreateNativeTextureView( &api, &view, &nViewName ) == SIR_GRAPHICS_DRIVER_ERROR, "pending view error" );
	s_nGenError = 0x0505;
	sirGLCheck( sirGLCreateNativeTextureView( &api, &view, &nViewName ) == SIR_GRAPHICS_OUT_OF_MEMORY, "view name OOM" );
	s_nGenError = 0;
	s_bZeroName = true;
	sirGLCheck( sirGLCreateNativeTextureView( &api, &view, &nViewName ) == SIR_GRAPHICS_DRIVER_ERROR, "zero view name" );
	s_bZeroName = false;
	s_nViewError = 0x0502;
	sirGLCheck( sirGLCreateNativeTextureView( &api, &view, &nViewName ) == SIR_GRAPHICS_DRIVER_ERROR, "view invalid operation" );
	s_nViewError = 0;
	sirGLCheck( s_nGenerated == 4 && s_nViewed == 2 && s_nDeleted == 6, "view failures release native names" );
	if ( s_nFailures == 0 ) {
		printf( "GL46 native texture storage tests passed\n" );
	}
	return s_nFailures == 0 ? 0 : 1;
}
