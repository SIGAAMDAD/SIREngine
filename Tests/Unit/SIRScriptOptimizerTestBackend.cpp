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

#include <Scripting/Core/SIRScriptOptimizerWorker.h>
#include <stdlib.h>
#include <string.h>
#if defined( _WIN32 )
#include <malloc.h>
#define SIR_SCRIPT_TEST_EXPORT __declspec( dllexport )
#else
#define SIR_SCRIPT_TEST_EXPORT __attribute__( ( visibility( "default" ) ) )
#endif

// A deterministic synthetic compiler fixture, never shipped as a production
// optimizer. It transforms input bytes so IPC success proves actual processing.
static sirScriptResult_t testCompile( void *pUser, const sirScriptOptimizerWorkView_t *pWork, sirScriptOptimizerOutput_t *pOutput ) noexcept
{
	( void )pUser;
	if ( pWork == nullptr || pOutput == nullptr || pWork->nInputBytes == 0 || pWork->nInputBytes > 4096 || pWork->pRecipe == nullptr ||
	     pWork->nRecipeBytes == 0 ) {
		return SIR_SCRIPT_INVALID_ARGUMENT;
	}
	const uint8_t *pInput = ( const uint8_t * )pWork->pInput;
	if ( pInput[0] == 0xff ) {
		return SIR_SCRIPT_BACKEND_FAILURE;
	}
	const size_t nAlignedBytes = ( size_t )( ( pWork->nInputBytes + 63 ) & ~( uint64_t )63 );
#if defined( _WIN32 )
	void *pData = _aligned_malloc( nAlignedBytes, 64 );
#else
	void *pData = aligned_alloc( 64, nAlignedBytes );
#endif
	if ( pData == nullptr ) {
		return SIR_SCRIPT_CAPACITY_EXCEEDED;
	}
	uint8_t *pCompiled = ( uint8_t * )pData;
	for ( uint64_t i = 0; i < pWork->nInputBytes; ++i ) {
		pCompiled[i] = ( uint8_t )( pInput[pWork->nInputBytes - 1 - i] ^ 0x5a );
	}
	*pOutput = { pData, pWork->nInputBytes, 64 };
	return SIR_SCRIPT_OK;
}

static void testRelease( void *pUser, sirScriptOptimizerOutput_t *pOutput ) noexcept
{
	( void )pUser;
	if ( pOutput == nullptr ) {
		return;
	}
#if defined( _WIN32 )
	_aligned_free( pOutput->pBytes );
#else
	free( pOutput->pBytes );
#endif
	*pOutput = {};
}

extern "C" SIR_SCRIPT_TEST_EXPORT const sirScriptOptimizerBackendAPI_t *SIRScriptOptimizerBackendGetAPI( void )
{
	static const sirScriptOptimizerBackendAPI_t s_API = { SIR_SCRIPT_OPT_BACKEND_ABI_REVISION, nullptr, testCompile, testRelease };
	return &s_API;
}
