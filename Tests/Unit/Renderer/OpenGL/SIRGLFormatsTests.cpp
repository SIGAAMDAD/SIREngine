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
#include <stdio.h>
#include <string.h>

static uint32_t s_nErrors = 0;
static uint32_t s_nFailures = 0;
static uint32_t s_nCalls = 0;
static uint32_t s_nLastTarget = 0;
static uint32_t s_nLastFormat = 0;
static uint32_t s_nLastParameter = 0;
static uint32_t s_nInjectedError = 0;
static int32_t s_nSupported = 1;
static const char *s_pszMissing = nullptr;

static void sirGLTestCheck( bool bResult, const char *pszCase ) noexcept
{
	if ( !bResult ) {
		fprintf( stderr, "FAIL: %s\n", pszCase );
		++s_nFailures;
	}
}

static void sirGLFakeQuery( uint32_t nTarget, uint32_t nFormat, uint32_t nParameter, int32_t nCount, int32_t *pValues ) noexcept
{
	++s_nCalls;
	s_nLastTarget = nTarget;
	s_nLastFormat = nFormat;
	s_nLastParameter = nParameter;
	if ( nCount == 1 ) {
		*pValues = s_nSupported;
	}
	s_nErrors = s_nInjectedError;
}

static uint32_t sirGLFakeError( void ) noexcept
{
	const uint32_t nResult = s_nErrors;
	s_nErrors = 0;
	return nResult;
}

static void *sirGLFakeLookup( void *, const char *pszName ) noexcept
{
	if ( s_pszMissing != nullptr && strcmp( pszName, s_pszMissing ) == 0 ) {
		return nullptr;
	}
	if ( strcmp( pszName, "glGetInternalformativ" ) == 0 ) {
		return ( void * )sirGLFakeQuery;
	}
	if ( strcmp( pszName, "glGetError" ) == 0 ) {
		return ( void * )sirGLFakeError;
	}
	return nullptr;
}

int main( void )
{
	sirGLFormatDispatch_t api = {};
	sirGLTestCheck( sirGLLoadFormatDispatch( nullptr, &api, &api ) == SIR_GRAPHICS_INVALID_ARGUMENT, "null resolver" );
	sirGLTestCheck( sirGLLoadFormatDispatch( sirGLFakeLookup, nullptr, &api ) == SIR_GRAPHICS_INVALID_ARGUMENT, "null context" );
	s_pszMissing = "glGetInternalformativ";
	sirGLTestCheck( sirGLLoadFormatDispatch( sirGLFakeLookup, &api, &api ) == SIR_GRAPHICS_UNSUPPORTED, "missing query" );
	s_pszMissing = "glGetError";
	sirGLTestCheck( sirGLLoadFormatDispatch( sirGLFakeLookup, &api, &api ) == SIR_GRAPHICS_UNSUPPORTED, "missing error" );
	s_pszMissing = nullptr;
	sirGLTestCheck( sirGLLoadFormatDispatch( sirGLFakeLookup, &api, &api ) == SIR_GRAPHICS_SUCCESS, "dispatch initialized" );

	const uint32_t szFormats[] = { SIR_GRAPHICS_FORMAT_RGBA8_UNORM, SIR_GRAPHICS_FORMAT_RGBA8_SRGB, SIR_GRAPHICS_FORMAT_R8_UNORM,
	                               SIR_GRAPHICS_FORMAT_RG8_UNORM };
	const uint32_t szGLFormats[] = { 0x8058, 0x8C43, 0x8229, 0x822B };
	for ( uint32_t i = 0; i < 4; ++i ) {
		sirGraphicsABIFormat_t format = {};
		format.nSize = sizeof( format );
		format.nFormat = szFormats[i];
		sirGLTestCheck( sirGLQueryNativeFormat( &api, &format ) == SIR_GRAPHICS_SUCCESS, "storage format supported" );
		sirGLTestCheck( format.nOptimalFeatures == ( SIR_GRAPHICS_ABI_FORMAT_SAMPLED |
		                    ( i < 2 ? SIR_GRAPHICS_ABI_FORMAT_COLOR_ATTACHMENT : 0U ) ) && format.nLinearFeatures == 0 &&
		                    format.nSampleCountMask == 1 && format.nReserved == 0,
		                "strict capability mask" );
		sirGLTestCheck( s_nLastTarget == 0x0DE1 && s_nLastParameter == 0x826F && s_nLastFormat == szGLFormats[i], "GL query arguments" );
	}
	sirGraphicsABIFormat_t format = {};
	format.nSize = sizeof( format );
	format.nFormat = SIR_GRAPHICS_FORMAT_BGRA8_UNORM;
	const uint32_t nCallsBefore = s_nCalls;
	sirGLTestCheck( sirGLQueryNativeFormat( &api, &format ) == SIR_GRAPHICS_UNSUPPORTED && s_nCalls == nCallsBefore,
	                "unimplemented format never probed" );
	format.nFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	format.nSize = 1;
	sirGLTestCheck( sirGLQueryNativeFormat( &api, &format ) == SIR_GRAPHICS_INVALID_ARGUMENT, "short ABI struct rejected" );
	format.nSize = sizeof( format );
	format.nReserved = 1;
	sirGLTestCheck( sirGLQueryNativeFormat( &api, &format ) == SIR_GRAPHICS_INVALID_ARGUMENT, "reserved field rejected" );
	format.nReserved = 0;
	s_nSupported = 0;
	sirGLTestCheck( sirGLQueryNativeFormat( &api, &format ) == SIR_GRAPHICS_UNSUPPORTED, "unsupported driver format" );
	s_nSupported = 1;
	s_nErrors = 0x0502;
	sirGLTestCheck( sirGLQueryNativeFormat( &api, &format ) == SIR_GRAPHICS_DRIVER_ERROR, "stale GL error rejected" );
	s_nInjectedError = 0x0505;
	sirGLTestCheck( sirGLQueryNativeFormat( &api, &format ) == SIR_GRAPHICS_DRIVER_ERROR, "runtime query error rejected" );
	s_nInjectedError = 0;
	sirGLTestCheck( sirGLQueryNativeFormat( nullptr, &format ) == SIR_GRAPHICS_INVALID_ARGUMENT, "null API" );
	sirGLTestCheck( sirGLQueryNativeFormat( &api, nullptr ) == SIR_GRAPHICS_INVALID_ARGUMENT, "null format" );
	if ( s_nFailures == 0 ) {
		printf( "OpenGL 4.6 live format query tests passed\n" );
	}
	return s_nFailures == 0 ? 0 : 1;
}
