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

#include <Renderer/Drivers/OpenGL/SIRGLFunctionLoader.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct sirGLTestLookup_s
{
	const char *pszMissing;
	uintptr_t nReturn;
	uint32_t nQueries;
} sirGLTestLookup_t;

static void *sirGLTestLookup( void *pContext, const char *pszName ) noexcept
{
	sirGLTestLookup_t *pLookup = ( sirGLTestLookup_t * )pContext;
	++pLookup->nQueries;
	return pLookup->pszMissing != nullptr && strcmp( pszName, pLookup->pszMissing ) == 0 ? nullptr : ( void * )pLookup->nReturn;
}

static int s_nFailures = 0;
static void sirGLCheck( bool bCondition, const char *pszCase ) noexcept
{
	if ( !bCondition ) {
		fprintf( stderr, "FAIL %s\n", pszCase );
		++s_nFailures;
	}
}

int main( void )
{
	sirGLCheck( !sirGLIsUsableProcedure( nullptr ), "null rejected" );
	for ( uintptr_t nValue = 1; nValue <= 3; ++nValue ) {
		sirGLCheck( !sirGLIsUsableProcedure( ( void * )nValue ), "WGL sentinel rejected" );
	}
	sirGLCheck( !sirGLIsUsableProcedure( ( void * )UINTPTR_MAX ), "WGL -1 sentinel rejected" );
	sirGLCheck( sirGLIsUsableProcedure( ( void * )( uintptr_t )0x1000 ), "non-sentinel address admitted" );
	sirGLCheck( !sirGLValidateRequired46Functions( nullptr, nullptr ), "missing lookup rejected" );
	sirGLCheck( !sirGLValidateRequired46Functions( sirGLTestLookup, nullptr ), "missing resolver context rejected" );
	sirGLTestLookup_t lookup = {};
	lookup.nReturn = 0x1000;
	sirGLCheck( sirGLValidateRequired46Functions( sirGLTestLookup, &lookup ), "complete dispatch accepted" );
	sirGLCheck( lookup.nQueries >= 25, "complete required function set queried" );
	lookup.nQueries = 0;
	lookup.pszMissing = "glNamedBufferStorage";
	sirGLCheck( !sirGLValidateRequired46Functions( sirGLTestLookup, &lookup ), "missing DSA storage rejected" );
	sirGLCheck( lookup.nQueries >= 4, "missing function detected at expected position" );
	lookup.pszMissing = "glMemoryBarrier";
	sirGLCheck( !sirGLValidateRequired46Functions( sirGLTestLookup, &lookup ), "missing memory barrier rejected" );
	lookup.pszMissing = nullptr;
	lookup.nReturn = 1;
	sirGLCheck( !sirGLValidateRequired46Functions( sirGLTestLookup, &lookup ), "resolver sentinel rejected" );
	if ( s_nFailures == 0 ) {
		printf( "GL46 native function loader tests passed\n" );
	}
	return s_nFailures == 0 ? 0 : 1;
}
