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

#include <Scripting/Core/SIRScriptOptimizationProtocol.h>

#include <chrono>
#include <stdio.h>
#include <string.h>
#include <thread>
#if defined( _WIN32 )
#include <fcntl.h>
#include <io.h>
#endif

static bool fixtureReadExact( uint8_t *pData, uint32_t nBytes ) noexcept
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
	if ( nArguments < 2 ) {
		return 2;
	}
#if defined( _WIN32 )
	// Protocol frames contain arbitrary binary bytes. Win32 text streams
	// translate CR/LF and treat Ctrl-Z as EOF, corrupting framed messages.
	if ( _setmode( _fileno( stdin ), _O_BINARY ) == -1 || _setmode( _fileno( stdout ), _O_BINARY ) == -1 ) {
		return 8;
	}
#endif
	setvbuf( stdout, nullptr, _IONBF, 0 );
	if ( strcmp( pszArguments[1], "exit" ) == 0 ) {
		return 0;
	}
	for ( ;; ) {
		uint8_t szWire[SIRScriptOptimizationProtocol::REQUEST_BYTES] = {};
		if ( !fixtureReadExact( szWire, sizeof( szWire ) ) ) {
			return 0;
		}
		sirScriptOptimizationRequest_t request = {};
		if ( SIRScriptOptimizationProtocol::decodeRequest( szWire, sizeof( szWire ), &request ) != SIR_SCRIPT_OK ) {
			return 3;
		}
		if ( strcmp( pszArguments[1], "stall" ) == 0 ) {
			std::this_thread::sleep_for( std::chrono::seconds( 8 ) );
			return 0;
		}
		if ( strcmp( pszArguments[1], "truncate" ) == 0 ) {
			const uint8_t szTruncated[3] = { 1, 2, 3 };
			( void )fwrite( szTruncated, 1, sizeof( szTruncated ), stdout );
			return 0;
		}
		sirScriptOptimizationReply_t reply = {};
		reply.nJobId = request.nJobId;
		reply.eResult = SIR_SCRIPT_BACKEND_FAILURE;
		uint8_t szReply[SIRScriptOptimizationProtocol::REPLY_BYTES] = {};
		if ( SIRScriptOptimizationProtocol::encodeReply( reply, szReply ) != SIR_SCRIPT_OK ) {
			return 4;
		}
		if ( strcmp( pszArguments[1], "split" ) == 0 ) {
			if ( fwrite( szReply, 1, 17, stdout ) != 17 ) {
				return 5;
			}
			std::this_thread::sleep_for( std::chrono::milliseconds( 3 ) );
			if ( fwrite( szReply + 17, 1, sizeof( szReply ) - 17, stdout ) != sizeof( szReply ) - 17 ) {
				return 6;
			}
		} else if ( fwrite( szReply, 1, sizeof( szReply ), stdout ) != sizeof( szReply ) ) {
			return 7;
		}
	}
}
