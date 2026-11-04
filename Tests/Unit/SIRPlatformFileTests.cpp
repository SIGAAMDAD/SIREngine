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

#include <Core/Platform/SIRPlatformFile.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

#define SIR_REQUIRE( condition )                                                                                                           \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "%s:%d %s\n", __FILE__, __LINE__, #condition );                                                               \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( 0 )

static void sirTestPlatformFile( void )
{
	const char *pszPath = "SIRPlatformFileTests.bin";
	const char *pszRenamed = "SIRPlatformFileTests-renamed.bin";
	const char *pszUtf8 = "SIRPlatformFileTests-\xE9\x9B\xAA.bin";
	SIRPlatformFile::remove( pszPath );
	SIRPlatformFile::remove( pszRenamed );
	SIRPlatformFile::remove( pszUtf8 );

	uint8_t szWritten[4096];
	for ( uint32_t i = 0; i < sizeof( szWritten ); ++i ) {
		szWritten[i] = ( uint8_t )( i ^ ( i >> 5 ) );
	}

	SIRPlatformFile file;
	uint64_t nTransferred = 0;
	SIR_REQUIRE( !file.isOpen() );
	SIR_REQUIRE( file.read( szWritten, 1, &nTransferred ) == SIR_FILE_CLOSED );
	SIR_REQUIRE( file.open( nullptr, SIR_FILE_OPEN_READ ) == SIR_FILE_INVALID_ARGUMENT );
	SIR_REQUIRE( file.open( pszPath, 0 ) == SIR_FILE_INVALID_ARGUMENT );
	SIR_REQUIRE( file.open( pszPath, SIR_FILE_OPEN_CREATE | SIR_FILE_OPEN_READ ) == SIR_FILE_INVALID_ARGUMENT );
	SIR_REQUIRE( file.open( pszPath, SIR_FILE_OPEN_READ ) == SIR_FILE_NOT_FOUND );

	const uint32_t nCreate = SIR_FILE_OPEN_READ | SIR_FILE_OPEN_WRITE | SIR_FILE_OPEN_CREATE | SIR_FILE_OPEN_EXCLUSIVE;
	SIR_REQUIRE( file.open( pszPath, nCreate ) == SIR_FILE_OK );
	SIR_REQUIRE( file.open( pszPath, nCreate ) == SIR_FILE_INVALID_ARGUMENT );
	SIR_REQUIRE( file.write( szWritten, sizeof( szWritten ), &nTransferred ) == SIR_FILE_OK );
	SIR_REQUIRE( nTransferred == sizeof( szWritten ) );
	uint64_t nSize = 0;
	SIR_REQUIRE( file.getSize( &nSize ) == SIR_FILE_OK && nSize == sizeof( szWritten ) );
	uint64_t nPosition = 0;
	SIR_REQUIRE( file.tell( &nPosition ) == SIR_FILE_OK && nPosition == sizeof( szWritten ) );
	SIR_REQUIRE( file.seek( 0, SIR_FILE_SEEK_BEGIN, &nPosition ) == SIR_FILE_OK && nPosition == 0 );

	uint8_t szRead[4096] = {};
	SIR_REQUIRE( file.read( szRead, sizeof( szRead ), &nTransferred ) == SIR_FILE_OK );
	SIR_REQUIRE( nTransferred == sizeof( szRead ) && memcmp( szWritten, szRead, sizeof( szRead ) ) == 0 );
	SIR_REQUIRE( file.read( szRead, 1, &nTransferred ) == SIR_FILE_END_OF_FILE && nTransferred == 0 );
	SIR_REQUIRE( file.seek( -5, SIR_FILE_SEEK_END, &nPosition ) == SIR_FILE_OK && nPosition == 4091 );
	SIR_REQUIRE( file.read( szRead, 8, &nTransferred ) == SIR_FILE_END_OF_FILE && nTransferred == 5 );
	SIR_REQUIRE( memcmp( szRead, szWritten + 4091, 5 ) == 0 );
	SIR_REQUIRE( file.seek( INT64_MIN, SIR_FILE_SEEK_CURRENT, nullptr ) == SIR_FILE_OUT_OF_RANGE );
	SIR_REQUIRE( file.readAt( UINT64_MAX, szRead, 1, &nTransferred ) == SIR_FILE_OUT_OF_RANGE );
	SIR_REQUIRE( file.writeAt( 0, nullptr, 1, &nTransferred ) == SIR_FILE_INVALID_ARGUMENT );
	SIR_REQUIRE( file.readAt( 0, nullptr, 1, &nTransferred ) == SIR_FILE_INVALID_ARGUMENT );
	SIR_REQUIRE( file.readAt( 4096, nullptr, 0, &nTransferred ) == SIR_FILE_OK );
	SIR_REQUIRE( file.readAt( 4096, szRead, 1, &nTransferred ) == SIR_FILE_END_OF_FILE && nTransferred == 0 );

	const uint8_t nReplacement = 0xA5;
	SIR_REQUIRE( file.writeAt( 12, &nReplacement, 1, &nTransferred ) == SIR_FILE_OK && nTransferred == 1 );
	szWritten[12] = nReplacement;
	SIR_REQUIRE( file.flush() == SIR_FILE_OK );
	// Exercise 64-bit positional I/O without allocating a multi-gigabyte buffer.
	const uint64_t nSparseOffset = ( UINT64_C( 1 ) << 32 ) + 123;
	SIR_REQUIRE( file.writeAt( nSparseOffset, &nReplacement, 1, &nTransferred ) == SIR_FILE_OK );
	SIR_REQUIRE( file.getSize( &nSize ) == SIR_FILE_OK && nSize == nSparseOffset + 1 );
	uint8_t nSparseByte = 0;
	SIR_REQUIRE( file.readAt( nSparseOffset, &nSparseByte, 1, &nTransferred ) == SIR_FILE_OK );
	SIR_REQUIRE( nSparseByte == nReplacement && nTransferred == 1 );
	SIR_REQUIRE( file.truncate( 4000 ) == SIR_FILE_OK );
	SIR_REQUIRE( file.getSize( &nSize ) == SIR_FILE_OK && nSize == 4000 );
	SIR_REQUIRE( file.close() == SIR_FILE_OK );
	SIR_REQUIRE( file.close() == SIR_FILE_OK );

	SIR_REQUIRE( file.open( pszPath, nCreate ) == SIR_FILE_ALREADY_EXISTS );
	SIR_REQUIRE( file.open( pszPath, SIR_FILE_OPEN_READ ) == SIR_FILE_OK );
	SIR_REQUIRE( file.write( &nReplacement, 1, &nTransferred ) == SIR_FILE_ACCESS_DENIED );
	SIR_REQUIRE( file.truncate( 2 ) == SIR_FILE_ACCESS_DENIED );
	SIR_REQUIRE( file.readAt( 0, szRead, 16, &nTransferred ) == SIR_FILE_OK && nTransferred == 16 );
	SIR_REQUIRE( memcmp( szRead, szWritten, 16 ) == 0 );
	SIR_REQUIRE( file.close() == SIR_FILE_OK );

	SIR_REQUIRE( file.open( pszPath, SIR_FILE_OPEN_WRITE | SIR_FILE_OPEN_APPEND ) == SIR_FILE_OK );
	SIR_REQUIRE( file.writeAt( 0, &nReplacement, 1, &nTransferred ) == SIR_FILE_INVALID_ARGUMENT );
	SIR_REQUIRE( file.write( &nReplacement, 1, &nTransferred ) == SIR_FILE_OK && nTransferred == 1 );
	SIR_REQUIRE( file.close() == SIR_FILE_OK );

	sirFileInfo_t info = {};
	SIR_REQUIRE( SIRPlatformFile::getInfo( pszPath, &info ) == SIR_FILE_OK );
	SIR_REQUIRE( info.bRegularFile && !info.bDirectory && info.nSizeBytes == 4001 );
	// renameReplace must atomically overwrite a pre-existing destination.
	SIRPlatformFile destination;
	SIR_REQUIRE( destination.open( pszRenamed, SIR_FILE_OPEN_CREATE | SIR_FILE_OPEN_WRITE | SIR_FILE_OPEN_TRUNCATE ) == SIR_FILE_OK );
	SIR_REQUIRE( destination.write( &nReplacement, 1, &nTransferred ) == SIR_FILE_OK );
	SIR_REQUIRE( destination.close() == SIR_FILE_OK );
	SIR_REQUIRE( SIRPlatformFile::renameReplace( pszPath, pszRenamed ) == SIR_FILE_OK );
	SIR_REQUIRE( SIRPlatformFile::getInfo( pszPath, &info ) == SIR_FILE_NOT_FOUND );
	SIR_REQUIRE( SIRPlatformFile::getInfo( pszRenamed, &info ) == SIR_FILE_OK && info.nSizeBytes == 4001 );

	SIR_REQUIRE( file.open( pszRenamed, SIR_FILE_OPEN_READ ) == SIR_FILE_OK );
	std::atomic<uint32_t> nValid{ 0 };
	std::thread workers[4];
	for ( uint32_t iWorker = 0; iWorker < 4; ++iWorker ) {
		workers[iWorker] = std::thread( [&file, &nValid, &szWritten, iWorker]() {
			uint8_t szLocal[32] = {};
			for ( uint32_t iRead = 0; iRead < 256; ++iRead ) {
				const uint64_t nOffset = ( iRead * 37u + iWorker * 53u ) % 3969u;
				uint64_t nBytes = 0;
				if ( file.readAt( nOffset, szLocal, sizeof( szLocal ), &nBytes ) == SIR_FILE_OK && nBytes == sizeof( szLocal ) &&
				     memcmp( szLocal, szWritten + nOffset, sizeof( szLocal ) ) == 0 ) {
					nValid.fetch_add( 1, std::memory_order_relaxed );
				}
			}
		} );
	}
	for ( std::thread &rWorker : workers ) {
		rWorker.join();
	}
	SIR_REQUIRE( nValid.load( std::memory_order_relaxed ) == 1024 );
	SIR_REQUIRE( file.close() == SIR_FILE_OK );

	SIR_REQUIRE( file.open( pszUtf8, SIR_FILE_OPEN_WRITE | SIR_FILE_OPEN_CREATE | SIR_FILE_OPEN_TRUNCATE ) == SIR_FILE_OK );
	SIR_REQUIRE( file.write( &nReplacement, 1, &nTransferred ) == SIR_FILE_OK );
	SIR_REQUIRE( file.close() == SIR_FILE_OK );
	SIR_REQUIRE( SIRPlatformFile::getInfo( pszUtf8, &info ) == SIR_FILE_OK && info.nSizeBytes == 1 );
	SIR_REQUIRE( SIRPlatformFile::remove( pszUtf8 ) == SIR_FILE_OK );
	SIR_REQUIRE( SIRPlatformFile::remove( pszRenamed ) == SIR_FILE_OK );
	SIR_REQUIRE( SIRPlatformFile::remove( pszRenamed ) == SIR_FILE_NOT_FOUND );
}

int main( void )
{
	sirTestPlatformFile();
	return 0;
}
