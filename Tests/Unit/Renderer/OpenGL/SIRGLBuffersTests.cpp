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

#include <Renderer/Drivers/OpenGL/SIRGLBuffers.h>
#include <stdio.h>
#include <string.h>

static int s_nFailures = 0;
static uint32_t s_nError = 0;
static uint32_t s_nCreated = 0;
static uint32_t s_nDeleted = 0;
static intptr_t s_nStorageBytes = 0;
static uint32_t s_nCreateError = 0;
static uint32_t s_nStorageError = 0;
static bool s_bZeroObject = false;
static const char *s_pszMissing = nullptr;

static void sirGLCheck( bool bCondition, const char *pszMessage ) noexcept
{
	if ( !bCondition ) {
		fprintf( stderr, "FAIL %s\n", pszMessage );
		++s_nFailures;
	}
}

static void sirGLFakeCreate( int32_t, uint32_t *pName ) noexcept
{
	++s_nCreated;
	*pName = s_bZeroObject ? 0 : 17;
	s_nError = s_nCreateError;
}

static void sirGLFakeStorage( uint32_t, intptr_t nBytes, const void *, uint32_t ) noexcept
{
	s_nStorageBytes = nBytes;
	s_nError = s_nStorageError;
}

static void sirGLFakeDelete( int32_t, const uint32_t * ) noexcept
{
	++s_nDeleted;
}

static uint32_t sirGLFakeError( void ) noexcept
{
	uint32_t nValue = s_nError;
	s_nError = 0;
	return nValue;
}

static void *sirGLFakeLookup( void *, const char *pszName ) noexcept
{
	if ( s_pszMissing != nullptr && strcmp( s_pszMissing, pszName ) == 0 ) {
		return nullptr;
	}
	if ( strcmp( pszName, "glCreateBuffers" ) == 0 ) {
		return ( void * )sirGLFakeCreate;
	}
	if ( strcmp( pszName, "glNamedBufferStorage" ) == 0 ) {
		return ( void * )sirGLFakeStorage;
	}
	if ( strcmp( pszName, "glDeleteBuffers" ) == 0 ) {
		return ( void * )sirGLFakeDelete;
	}
	if ( strcmp( pszName, "glGetError" ) == 0 ) {
		return ( void * )sirGLFakeError;
	}
	return nullptr;
}

int main( void )
{
	sirGLBufferDispatch_t api = {};
	sirGLCheck( sirGLLoadBufferDispatch( nullptr, &api, &api ) == SIR_GRAPHICS_INVALID_ARGUMENT, "null lookup" );
	sirGLCheck( sirGLLoadBufferDispatch( sirGLFakeLookup, nullptr, &api ) == SIR_GRAPHICS_INVALID_ARGUMENT, "null context" );
	s_pszMissing = "glDeleteBuffers";
	sirGLCheck( sirGLLoadBufferDispatch( sirGLFakeLookup, &api, &api ) == SIR_GRAPHICS_UNSUPPORTED, "missing delete" );
	s_pszMissing = nullptr;
	sirGLCheck( sirGLLoadBufferDispatch( sirGLFakeLookup, &api, &api ) == SIR_GRAPHICS_SUCCESS, "load complete dispatch" );

	sirGraphicsBufferDesc_t desc = {};
	desc.nSizeBytes = 4096;
	desc.nAlignmentBytes = 256;
	desc.nUsage = SIR_GRAPHICS_USAGE_VERTEX | SIR_GRAPHICS_USAGE_COPY_DESTINATION;
	desc.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	uint32_t nName = 0;
	sirGLCheck( sirGLCreateNativeBuffer( &api, &desc, &nName ) == SIR_GRAPHICS_SUCCESS && nName == 17 && s_nStorageBytes == 4096,
	            "immutable storage" );
	sirGLCheck( sirGLDeleteNativeBuffer( &api, nName ) == SIR_GRAPHICS_SUCCESS && s_nDeleted == 1, "delete" );
	desc.nSizeBytes = 0;
	sirGLCheck( sirGLCreateNativeBuffer( &api, &desc, &nName ) == SIR_GRAPHICS_INVALID_ARGUMENT, "zero bytes" );
	desc.nSizeBytes = UINT64_MAX;
	sirGLCheck( sirGLCreateNativeBuffer( &api, &desc, &nName ) == SIR_GRAPHICS_INVALID_ARGUMENT, "too large" );
	desc.nSizeBytes = 4096;
	desc.nAlignmentBytes = 3;
	sirGLCheck( sirGLCreateNativeBuffer( &api, &desc, &nName ) == SIR_GRAPHICS_INVALID_ARGUMENT, "bad alignment" );
	desc.nAlignmentBytes = 256;
	desc.nUsage = SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT;
	sirGLCheck( sirGLCreateNativeBuffer( &api, &desc, &nName ) == SIR_GRAPHICS_INVALID_ARGUMENT, "texture usage" );
	desc.nUsage = SIR_GRAPHICS_USAGE_VERTEX;
	desc.eMemory = SIR_GRAPHICS_MEMORY_CPU_UPLOAD;
	sirGLCheck( sirGLCreateNativeBuffer( &api, &desc, &nName ) == SIR_GRAPHICS_UNSUPPORTED, "upload unavailable" );
	desc.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	s_nError = 0x0502;
	sirGLCheck( sirGLCreateNativeBuffer( &api, &desc, &nName ) == SIR_GRAPHICS_DRIVER_ERROR, "existing GL error" );
	s_nCreateError = 0x0505;
	sirGLCheck( sirGLCreateNativeBuffer( &api, &desc, &nName ) == SIR_GRAPHICS_OUT_OF_MEMORY, "create OOM" );
	s_nCreateError = 0;
	s_nStorageError = 0x0505;
	sirGLCheck( sirGLCreateNativeBuffer( &api, &desc, &nName ) == SIR_GRAPHICS_OUT_OF_MEMORY, "storage OOM" );
	sirGLCheck( s_nDeleted == 3, "failed creations cleaned" );
	s_nStorageError = 0;
	s_bZeroObject = true;
	sirGLCheck( sirGLCreateNativeBuffer( &api, &desc, &nName ) == SIR_GRAPHICS_DRIVER_ERROR, "zero native name" );
	s_bZeroObject = false;
	sirGLCheck( sirGLDeleteNativeBuffer( &api, 0 ) == SIR_GRAPHICS_INVALID_ARGUMENT, "zero name deletion" );
	sirGLCheck( s_nCreated == 4, "invalid inputs never allocate" );
	if ( s_nFailures == 0 ) {
		printf( "GL46 immutable buffer operation tests passed\n" );
	}
	return s_nFailures == 0 ? 0 : 1;
}
