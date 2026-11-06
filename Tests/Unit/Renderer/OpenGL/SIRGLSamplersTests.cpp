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
#include <limits>
#include <stdio.h>
#include <string.h>

static int s_nFailures = 0;
static uint32_t s_nError = 0;
static uint32_t s_nCreateError = 0;
static uint32_t s_nParameterError = 0;
static uint32_t s_nDeleteError = 0;
static uint32_t s_nCreated = 0;
static uint32_t s_nDeleted = 0;
static uint32_t s_nIntWrites = 0;
static uint32_t s_nFloatWrites = 0;
static int32_t s_nLastFilter = 0;
static int32_t s_nLastWrap = 0;
static float s_fLastLod = 0.0f;
static bool s_bZeroObject = false;
static const char *s_pszMissing = nullptr;

static void sirGLCheck( bool bOk, const char *pszName ) noexcept
{
	if ( !bOk ) {
		fprintf( stderr, "FAIL: %s\n", pszName );
		++s_nFailures;
	}
}

static void sirGLFakeCreate( int32_t, uint32_t *pName ) noexcept
{
	++s_nCreated;
	*pName = s_bZeroObject ? 0 : 19;
	s_nError = s_nCreateError;
}

static void sirGLFakeDelete( int32_t, const uint32_t * ) noexcept
{
	++s_nDeleted;
	s_nError = s_nDeleteError;
}

static void sirGLFakeParameteri( uint32_t, uint32_t nParameter, int32_t nValue ) noexcept
{
	++s_nIntWrites;
	if ( nParameter == 0x2801 ) {
		s_nLastFilter = nValue;
	}
	if ( nParameter == 0x8072 ) {
		s_nLastWrap = nValue;
	}
	s_nError = s_nParameterError;
}

static void sirGLFakeParameterf( uint32_t, uint32_t nParameter, float fValue ) noexcept
{
	++s_nFloatWrites;
	if ( nParameter == 0x813B ) {
		s_fLastLod = fValue;
	}
	s_nError = s_nParameterError;
}

static uint32_t sirGLFakeGetError( void ) noexcept
{
	const uint32_t nResult = s_nError;
	s_nError = 0;
	return nResult;
}

static void *sirGLFakeLookup( void *, const char *pszName ) noexcept
{
	if ( s_pszMissing != nullptr && strcmp( pszName, s_pszMissing ) == 0 ) {
		return nullptr;
	}
	if ( strcmp( pszName, "glCreateSamplers" ) == 0 ) {
		return ( void * )sirGLFakeCreate;
	}
	if ( strcmp( pszName, "glDeleteSamplers" ) == 0 ) {
		return ( void * )sirGLFakeDelete;
	}
	if ( strcmp( pszName, "glSamplerParameteri" ) == 0 ) {
		return ( void * )sirGLFakeParameteri;
	}
	if ( strcmp( pszName, "glSamplerParameterf" ) == 0 ) {
		return ( void * )sirGLFakeParameterf;
	}
	if ( strcmp( pszName, "glGetError" ) == 0 ) {
		return ( void * )sirGLFakeGetError;
	}
	return nullptr;
}

int main( void )
{
	sirGLSamplerDispatch_t api = {};
	sirGLCheck( sirGLLoadSamplerDispatch( nullptr, &api, &api ) == SIR_GRAPHICS_INVALID_ARGUMENT, "no loader" );
	sirGLCheck( sirGLLoadSamplerDispatch( sirGLFakeLookup, nullptr, &api ) == SIR_GRAPHICS_INVALID_ARGUMENT, "no context" );
	s_pszMissing = "glSamplerParameterf";
	sirGLCheck( sirGLLoadSamplerDispatch( sirGLFakeLookup, &api, &api ) == SIR_GRAPHICS_UNSUPPORTED, "missing entrypoint" );
	s_pszMissing = nullptr;
	sirGLCheck( sirGLLoadSamplerDispatch( sirGLFakeLookup, &api, &api ) == SIR_GRAPHICS_SUCCESS, "complete GL dispatch" );

	sirGraphicsSamplerDesc_t desc = {};
	desc.eFilter = SIR_GRAPHICS_FILTER_NEAREST;
	desc.eAddressU = SIR_GRAPHICS_ADDRESS_REPEAT;
	desc.eAddressV = SIR_GRAPHICS_ADDRESS_MIRROR;
	desc.eAddressW = SIR_GRAPHICS_ADDRESS_BORDER;
	desc.fMaximumAnisotropy = 1.0f;
	desc.fMinimumLod = -4.0f;
	desc.fMaximumLod = 6.0f;
	uint32_t nName = 0;
	sirGLCheck( sirGLCreateNativeSampler( &api, &desc, &nName ) == SIR_GRAPHICS_SUCCESS && nName == 19, "nearest sampler created" );
	sirGLCheck( s_nLastFilter == 0x2600 && s_nLastWrap == 0x812D && s_fLastLod == 6.0f && s_nIntWrites == 5 && s_nFloatWrites == 2,
	            "exact GL sampler parameters" );
	sirGLCheck( sirGLDeleteNativeSampler( &api, nName ) == SIR_GRAPHICS_SUCCESS && s_nDeleted == 1, "delete sampler" );
	desc.eFilter = SIR_GRAPHICS_FILTER_LINEAR;
	sirGLCheck( sirGLCreateNativeSampler( &api, &desc, &nName ) == SIR_GRAPHICS_SUCCESS && s_nLastFilter == 0x2601, "linear sampler" );
	desc.eFilter = SIR_GRAPHICS_FILTER_TRILINEAR;
	sirGLCheck( sirGLCreateNativeSampler( &api, &desc, &nName ) == SIR_GRAPHICS_SUCCESS && s_nLastFilter == 0x2703, "trilinear sampler" );
	desc.eFilter = ( sirGraphicsSamplerFilter_t )8;
	sirGLCheck( sirGLCreateNativeSampler( &api, &desc, &nName ) == SIR_GRAPHICS_INVALID_ARGUMENT, "invalid filter" );
	desc.eFilter = SIR_GRAPHICS_FILTER_ANISOTROPIC;
	sirGLCheck( sirGLCreateNativeSampler( &api, &desc, &nName ) == SIR_GRAPHICS_UNSUPPORTED, "anisotropy extension gated" );
	desc.eFilter = SIR_GRAPHICS_FILTER_LINEAR;
	desc.fMaximumAnisotropy = 4.0f;
	sirGLCheck( sirGLCreateNativeSampler( &api, &desc, &nName ) == SIR_GRAPHICS_UNSUPPORTED, "anisotropic request gated" );
	desc.fMaximumAnisotropy = 1.0f;
	desc.eAddressU = ( sirGraphicsAddressMode_t )9;
	sirGLCheck( sirGLCreateNativeSampler( &api, &desc, &nName ) == SIR_GRAPHICS_INVALID_ARGUMENT, "invalid wrap" );
	desc.eAddressU = SIR_GRAPHICS_ADDRESS_CLAMP;
	desc.fMinimumLod = std::numeric_limits<float>::quiet_NaN();
	sirGLCheck( sirGLCreateNativeSampler( &api, &desc, &nName ) == SIR_GRAPHICS_INVALID_ARGUMENT, "NaN LOD" );
	desc.fMinimumLod = 10.0f;
	sirGLCheck( sirGLCreateNativeSampler( &api, &desc, &nName ) == SIR_GRAPHICS_INVALID_ARGUMENT, "reversed LOD" );
	desc.fMinimumLod = 0.0f;
	s_nError = 0x0502;
	sirGLCheck( sirGLCreateNativeSampler( &api, &desc, &nName ) == SIR_GRAPHICS_DRIVER_ERROR, "existing error" );
	s_nCreateError = 0x0505;
	sirGLCheck( sirGLCreateNativeSampler( &api, &desc, &nName ) == SIR_GRAPHICS_OUT_OF_MEMORY, "create OOM" );
	s_nCreateError = 0;
	s_bZeroObject = true;
	sirGLCheck( sirGLCreateNativeSampler( &api, &desc, &nName ) == SIR_GRAPHICS_DRIVER_ERROR, "zero sampler name" );
	s_bZeroObject = false;
	s_nParameterError = 0x0502;
	sirGLCheck( sirGLCreateNativeSampler( &api, &desc, &nName ) == SIR_GRAPHICS_DRIVER_ERROR, "config error" );
	s_nParameterError = 0;
	sirGLCheck( sirGLDeleteNativeSampler( nullptr, 19 ) == SIR_GRAPHICS_INVALID_ARGUMENT, "null delete" );
	sirGLCheck( sirGLDeleteNativeSampler( &api, 0 ) == SIR_GRAPHICS_INVALID_ARGUMENT, "zero delete" );
	sirGLCheck( s_nDeleted == 3 && s_nCreated == 6, "failure cleanup and no invalid allocations" );
	if ( s_nFailures == 0 ) {
		printf( "GL46 native sampler operation tests passed\n" );
	}
	return s_nFailures == 0 ? 0 : 1;
}
