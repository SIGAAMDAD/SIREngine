/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#include <Core/IO/SIRAsyncIOService.h>
#include <Core/Platform/SIRPlatformFile.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool sirIOFixtureCreate( const char *szPath ) noexcept
{
	SIRPlatformFile file;
	if ( file.open( szPath, SIR_FILE_OPEN_WRITE | SIR_FILE_OPEN_CREATE | SIR_FILE_OPEN_TRUNCATE ) != SIR_FILE_OK ) {
		return false;
	}
	uint8_t szBytes[1024];
	for ( uint32_t iByte = 0; iByte < sizeof( szBytes ); ++iByte ) {
		szBytes[iByte] = ( uint8_t )( iByte & 255 );
	}
	uint64_t nWritten = 0;
	const sirFileResult_t eResult = file.write( szBytes, sizeof( szBytes ), &nWritten );
	file.close();
	return eResult == SIR_FILE_OK && nWritten == sizeof( szBytes );
}

static bool sirIOAwait( SIRAsyncIOService& io, sirIOCompletion_t *pCompletion ) noexcept
{
	for ( uint32_t i = 0; i < 5000000; ++i ) {
		if ( io.poll( pCompletion, 1 ) == 1 ) {
			return true;
		}
	}
	return false;
}

int main( void )
{
	const char *szPath = "sir-async-io-fixture.bin";
	if ( !sirIOFixtureCreate( szPath ) ) {
		return 1;
	}

	SIRAsyncIOService io;
	const uint64_t nStorage = SIRAsyncIOService::requiredBytes( 3, 2, 2 );
	void *pStorage = malloc( ( size_t )nStorage );
	if ( pStorage == nullptr || !io.initialize( pStorage, nStorage, 3, 2, 2 ) ) {
		return 2;
	}

	sirIOSourceHandle_t hSource = {};
	sirIOBufferHandle_t hBuffer = {};
	uint8_t szBuffer[1024] = {};
	if ( io.openSource( szPath, &hSource ) != SIR_IO_OK || io.registerBuffer( szBuffer, sizeof( szBuffer ), &hBuffer ) != SIR_IO_OK ) {
		return 3;
	}

	for ( uint32_t iRequest = 0; iRequest < 1000; ++iRequest ) {
		sirIOReadRequest_t request = { hSource, hBuffer, 0, 0, 1024, 0, iRequest, 1 };
		sirIORequestHandle_t hRequest = {};
		if ( io.submit( request, &hRequest ) != SIR_IO_OK || io.closeSource( hSource ) != SIR_IO_BUSY ||
		     io.unregisterBuffer( hBuffer ) != SIR_IO_BUSY ) {
			return 4;
		}
		sirIOCompletion_t completion = {};
		if ( !sirIOAwait( io, &completion ) || completion.hRequest.nSlot != hRequest.nSlot ||
		     completion.hRequest.nGeneration != hRequest.nGeneration || completion.eResult != SIR_IO_OK ||
		     completion.nTransferred != 1024 || completion.nClientToken != iRequest ) {
			return 5;
		}
		for ( uint32_t iByte = 0; iByte < sizeof( szBuffer ); ++iByte ) {
			if ( szBuffer[iByte] != ( uint8_t )( iByte & 255 ) ) {
				return 6;
			}
		}
		if ( io.acknowledge( hRequest ) != SIR_IO_OK || io.cancel( hRequest ) != SIR_IO_INVALID_HANDLE ) {
			return 7;
		}
	}

	// A logical cancellation must retain both native source and borrowed buffer
	// until the terminal completion is consumed and acknowledged.
	sirIOReadRequest_t request = { hSource, hBuffer, 0, 0, 1024, 0, 77, 1 };
	sirIORequestHandle_t hCancelled = {};
	if ( io.submit( request, &hCancelled ) != SIR_IO_OK || io.cancel( hCancelled ) != SIR_IO_OK ||
	     io.unregisterBuffer( hBuffer ) != SIR_IO_BUSY ) {
		return 8;
	}
	sirIOCompletion_t cancelled = {};
	if ( !sirIOAwait( io, &cancelled ) || cancelled.eResult != SIR_IO_CANCELLED || io.acknowledge( hCancelled ) != SIR_IO_OK ) {
		return 9;
	}

	request.nOffset = 1000;
	request.nBytes = 100;
	sirIORequestHandle_t hInvalid = {};
	if ( io.submit( request, &hInvalid ) != SIR_IO_INVALID_ARGUMENT ) {
		return 10;
	}
	if ( io.unregisterBuffer( hBuffer ) != SIR_IO_OK || io.closeSource( hSource ) != SIR_IO_OK ||
	     io.registerBuffer( szBuffer, sizeof( szBuffer ), &hBuffer ) != SIR_IO_OK ||
	     io.submit( request, &hInvalid ) != SIR_IO_INVALID_HANDLE ) {
		return 11;
	}

	io.shutdown();
	free( pStorage );
	SIRPlatformFile::remove( szPath );
	puts( "SIRAsyncIOService: bounded generation, pin, cancellation and lifecycle PASS" );
	return 0;
}
