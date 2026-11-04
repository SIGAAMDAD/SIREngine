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

#include <stdio.h>
#include <string.h>
#if defined( _WIN32 )
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <fcntl.h>
#include <io.h>
#else
#include <dlfcn.h>
#endif

static bool workerReadExact( uint8_t *pData, uint32_t nBytes ) noexcept
{
	uint32_t nOffset = 0;
	while ( nOffset < nBytes ) {
		const size_t nRead = fread( pData + nOffset, 1, nBytes - nOffset, stdin );
		if ( nRead == 0 ) {
			return false;
		}
		nOffset += ( uint32_t )nRead;
	}
	return true;
}

int main( int nArguments, char **pszArguments )
{
	if ( nArguments != 4 ) {
		fprintf( stderr, "SIRScriptOptimizerWorker: usage <input-root> <cache-root> <backend-library>\n" );
		return 2;
	}
#if defined( _WIN32 )
	( void )_setmode( _fileno( stdin ), _O_BINARY );
	( void )_setmode( _fileno( stdout ), _O_BINARY );
	HMODULE hLibrary = LoadLibraryA( pszArguments[3] );
	const auto pGetAPI =
	    ( sirScriptOptimizerGetBackendAPI_t )( hLibrary != nullptr ? GetProcAddress( hLibrary, SIR_SCRIPT_OPT_BACKEND_ENTRYPOINT )
		                                                           : nullptr );
#else
	void *hLibrary = dlopen( pszArguments[3], RTLD_NOW | RTLD_LOCAL );
	const auto pGetAPI =
	    ( sirScriptOptimizerGetBackendAPI_t )( hLibrary != nullptr ? dlsym( hLibrary, SIR_SCRIPT_OPT_BACKEND_ENTRYPOINT ) : nullptr );
#endif
	if ( pGetAPI == nullptr ) {
		fprintf( stderr, "SIRScriptOptimizerWorker: compiler backend unavailable\n" );
		return 3;
	}
	const sirScriptOptimizerBackendAPI_t *pBackend = pGetAPI();
	if ( pBackend == nullptr ) {
		return 4;
	}
	SIRScriptOptimizerInputStore inputs;
	SIRScriptArtifactCache cache;
	SIRScriptOptimizerWorker worker;
	if ( inputs.configure( pszArguments[1], 64 * 1024 * 1024, 65536 ) != SIR_SCRIPT_OK ||
	     cache.configure( pszArguments[2], 64 * 1024 * 1024 ) != SIR_SCRIPT_OK ||
	     worker.configure( &inputs, &cache, *pBackend, 64 * 1024 * 1024 ) != SIR_SCRIPT_OK ) {
		fprintf( stderr, "SIRScriptOptimizerWorker: invalid configuration\n" );
		return 5;
	}
	setvbuf( stdout, nullptr, _IONBF, 0 );
	for ( ;; ) {
		uint8_t szWire[SIRScriptOptimizationProtocol::REQUEST_BYTES] = {};
		if ( !workerReadExact( szWire, sizeof( szWire ) ) ) {
			return ferror( stdin ) ? 6 : 0;
		}
		sirScriptOptimizationRequest_t request = {};
		if ( SIRScriptOptimizationProtocol::decodeRequest( szWire, sizeof( szWire ), &request ) != SIR_SCRIPT_OK ) {
			return 7;
		}
		sirScriptOptimizationReply_t reply = {};
		if ( worker.process( request, &reply ) != SIR_SCRIPT_OK ) {
			return 8;
		}
		uint8_t szReply[SIRScriptOptimizationProtocol::REPLY_BYTES] = {};
		if ( SIRScriptOptimizationProtocol::encodeReply( reply, szReply ) != SIR_SCRIPT_OK ||
		     fwrite( szReply, 1, sizeof( szReply ), stdout ) != sizeof( szReply ) ) {
			return 9;
		}
	}
}
