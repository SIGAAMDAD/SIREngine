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

#include <Core/Platform/SIRFileSystem.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined( _WIN32 )
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

static void sirTestWrite( const char *pszPath, const char *pBytes, size_t nBytes ) noexcept
{
	FILE *pFile = fopen( pszPath, "wb" );
	assert( pFile != nullptr );
	assert( fwrite( pBytes, 1, nBytes, pFile ) == nBytes );
	assert( fclose( pFile ) == 0 );
}

int main( void )
{
	char szDirectory[512] = {};
#if defined( _WIN32 )
	char szRoot[MAX_PATH] = {};
	assert( GetTempPathA( MAX_PATH, szRoot ) != 0 );
	assert( snprintf( szDirectory, sizeof( szDirectory ), "%ssir-file-read-%lu", szRoot, ( unsigned long )GetCurrentProcessId() ) > 0 );
	assert( CreateDirectoryA( szDirectory, nullptr ) != 0 );
#else
	memcpy( szDirectory, "/tmp/sir-file-read-XXXXXX", sizeof( "/tmp/sir-file-read-XXXXXX" ) );
	assert( mkdtemp( szDirectory ) != nullptr );
#endif
	char szPath[640] = {};
	char szMissing[640] = {};
	assert( snprintf( szPath, sizeof( szPath ), "%s/data.bin", szDirectory ) > 0 );
	assert( snprintf( szMissing, sizeof( szMissing ), "%s/missing.bin", szDirectory ) > 0 );

	const char szData[] = { 'A', '\0', '\n', 'B', '\0', 'C' };
	sirTestWrite( szPath, szData, sizeof( szData ) );
	size_t nBytes = 99;
	assert( SIRFileSystem::readFile( szPath, nullptr, 0, &nBytes ) == SIR_FILE_SYSTEM_OK );
	assert( nBytes == sizeof( szData ) );
	char szSmall[5] = { 'X', 'X', 'X', 'X', 'X' };
	assert( SIRFileSystem::readFile( szPath, szSmall, sizeof( szSmall ), &nBytes ) == SIR_FILE_SYSTEM_OUT_OF_CAPACITY );
	assert( nBytes == sizeof( szData ) );
	for ( size_t i = 0; i < sizeof( szSmall ); ++i ) {
		assert( szSmall[i] == 'X' );
	}
	char szOutput[16] = {};
	assert( SIRFileSystem::readFile( szPath, szOutput, sizeof( szData ), &nBytes ) == SIR_FILE_SYSTEM_OK );
	assert( nBytes == sizeof( szData ) );
	assert( memcmp( szOutput, szData, nBytes ) == 0 );

	// Invalid inputs fail without writing a caller's existing buffer.
	assert( SIRFileSystem::readFile( nullptr, nullptr, 0, &nBytes ) == SIR_FILE_SYSTEM_INVALID_PATH );
	assert( SIRFileSystem::readFile( szPath, nullptr, 1, &nBytes ) == SIR_FILE_SYSTEM_INVALID_ARGUMENT );
	assert( SIRFileSystem::readFile( szPath, szOutput, 0, &nBytes ) == SIR_FILE_SYSTEM_INVALID_ARGUMENT );
	assert( SIRFileSystem::readFile( szPath, szOutput, sizeof( szOutput ), nullptr ) == SIR_FILE_SYSTEM_INVALID_ARGUMENT );
	assert( SIRFileSystem::readFile( szMissing, nullptr, 0, &nBytes ) == SIR_FILE_SYSTEM_IO_ERROR );
	assert( SIRFileSystem::readFile( szDirectory, nullptr, 0, &nBytes ) == SIR_FILE_SYSTEM_INVALID_PATH );
	assert( SIRFileSystem::readFile( "", nullptr, 0, &nBytes ) == SIR_FILE_SYSTEM_INVALID_PATH );
	char szTooLong[SIR_FILE_SYSTEM_MAX_PATH_BYTES + 1] = {};
	memset( szTooLong, 'z', SIR_FILE_SYSTEM_MAX_PATH_BYTES );
	assert( SIRFileSystem::readFile( szTooLong, nullptr, 0, &nBytes ) == SIR_FILE_SYSTEM_INVALID_PATH );

#if !defined( _WIN32 )
	char szLink[640] = {};
	char szFifo[640] = {};
	assert( snprintf( szLink, sizeof( szLink ), "%s/data.link", szDirectory ) > 0 );
	assert( snprintf( szFifo, sizeof( szFifo ), "%s/data.fifo", szDirectory ) > 0 );
	assert( symlink( szPath, szLink ) == 0 );
	assert( SIRFileSystem::readFile( szLink, nullptr, 0, &nBytes ) == SIR_FILE_SYSTEM_INVALID_PATH );
	assert( unlink( szLink ) == 0 );
	assert( mkfifo( szFifo, 0600 ) == 0 );
	assert( SIRFileSystem::readFile( szFifo, nullptr, 0, &nBytes ) == SIR_FILE_SYSTEM_INVALID_PATH );
	assert( unlink( szFifo ) == 0 );
#endif

	// Empty source files measure as zero; higher-level scene validation rejects them.
	sirTestWrite( szPath, "", 0 );
	assert( SIRFileSystem::readFile( szPath, nullptr, 0, &nBytes ) == SIR_FILE_SYSTEM_OK );
	assert( nBytes == 0 );
	assert( SIRFileSystem::readFile( szPath, szOutput, sizeof( szOutput ), &nBytes ) == SIR_FILE_SYSTEM_OK );

	// Exercise reads spanning Windows 1 MiB I/O chunks and Linux short-read loops.
	const size_t nLarge = 1024 * 1024 + 29;
	char *pLarge = ( char * )malloc( nLarge );
	char *pRead = ( char * )malloc( nLarge );
	assert( pLarge != nullptr && pRead != nullptr );
	for ( size_t i = 0; i < nLarge; ++i ) {
		pLarge[i] = ( char )( i * 13u );
	}
	sirTestWrite( szPath, pLarge, nLarge );
	assert( SIRFileSystem::readFile( szPath, nullptr, 0, &nBytes ) == SIR_FILE_SYSTEM_OK );
	assert( nBytes == nLarge );
	assert( SIRFileSystem::readFile( szPath, pRead, nLarge, &nBytes ) == SIR_FILE_SYSTEM_OK );
	assert( nBytes == nLarge && memcmp( pRead, pLarge, nLarge ) == 0 );
	free( pRead );
	free( pLarge );

	assert( remove( szPath ) == 0 );
#if defined( _WIN32 )
	assert( RemoveDirectoryA( szDirectory ) != 0 );
#else
	assert( rmdir( szDirectory ) == 0 );
#endif
	return 0;
}
