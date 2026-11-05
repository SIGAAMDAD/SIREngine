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

#include <Core/VFS/SIRVFSPath.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void sirVFSTestOk( const char *pszInput, const char *pszExpected )
{
	sirVFSPath_t path = {};
	assert( SIRVFSPath::normalize( pszInput, ( uint32_t )strlen( pszInput ), &path ) == SIR_VFS_PATH_OK );
	assert( strcmp( path.szPath, pszExpected ) == 0 );
	assert( path.nLength == strlen( pszExpected ) );
}

static void sirVFSTestFailure( const char *pszInput, uint32_t nInputBytes, sirVFSPathResult_t eExpected )
{
	sirVFSPath_t path = {};
	strcpy( path.szPath, "unchanged" );
	const sirVFSPathResult_t actual = SIRVFSPath::normalize( pszInput, nInputBytes, &path );
	if ( actual != eExpected ) {
		fprintf( stderr, "Failed: %s got %u expected %u\n", pszInput, ( unsigned )actual, ( unsigned )eExpected );
	}
	assert( actual == eExpected );
	assert( strcmp( path.szPath, "unchanged" ) == 0 );
}

int main( void )
{
	sirVFSTestOk( "game:/foo/bar", "game:/foo/bar" );
	sirVFSTestOk( "GAME:/foo//./bar/", "game:/foo/bar" );
	sirVFSTestOk( "user:/save\\slot1", "user:/save/slot1" );
	sirVFSTestOk( "project:/a/b/../c", "project:/a/c" );
	sirVFSTestOk( "engine:/", "engine:/" );
	sirVFSTestOk( "mods:/textures/\xc3\xa9", "mods:/textures/\xc3\xa9" );
	sirVFSTestOk( "user:/a/..", "user:/" );
	sirVFSTestFailure( "game:/../../escape", ( uint32_t )strlen( "game:/../../escape" ), SIR_VFS_PATH_FORBIDDEN );
	sirVFSTestFailure( "user:/../secret", ( uint32_t )strlen( "user:/../secret" ), SIR_VFS_PATH_FORBIDDEN );
	sirVFSTestFailure( "C:\\Windows", ( uint32_t )strlen( "C:\\Windows" ), SIR_VFS_PATH_INVALID_NAMESPACE );
	sirVFSTestFailure( "//server/share", ( uint32_t )strlen( "//server/share" ), SIR_VFS_PATH_INVALID_NAMESPACE );
	sirVFSTestFailure( "game:/CON.txt", ( uint32_t )strlen( "game:/CON.txt" ), SIR_VFS_PATH_INVALID_SEGMENT );
	sirVFSTestFailure( "game:/COM1", ( uint32_t )strlen( "game:/COM1" ), SIR_VFS_PATH_INVALID_SEGMENT );
	sirVFSTestFailure( "game:/foo.", ( uint32_t )strlen( "game:/foo." ), SIR_VFS_PATH_INVALID_SEGMENT );
	sirVFSTestFailure( "game:/foo ", ( uint32_t )strlen( "game:/foo " ), SIR_VFS_PATH_INVALID_SEGMENT );
	sirVFSTestFailure( "game:/a:b", ( uint32_t )strlen( "game:/a:b" ), SIR_VFS_PATH_FORBIDDEN );
	sirVFSTestFailure( "game:/%2e%2e", ( uint32_t )strlen( "game:/%2e%2e" ), SIR_VFS_PATH_FORBIDDEN );
	sirVFSTestFailure( "game://bad?", ( uint32_t )strlen( "game://bad?" ), SIR_VFS_PATH_FORBIDDEN );
	const char szEmbeddedNul[] = { 'g', 'a', 'm', 'e', ':', '/', 'a', '\0', 'b' };
	sirVFSTestFailure( szEmbeddedNul, sizeof( szEmbeddedNul ), SIR_VFS_PATH_INVALID_ENCODING );
	const char szOverlong[] = { 'g', 'a', 'm', 'e', ':', '/', ( char )0xc0, ( char )0xaf };
	sirVFSTestFailure( szOverlong, sizeof( szOverlong ), SIR_VFS_PATH_INVALID_ENCODING );
	const char szSurrogate[] = { 'g', 'a', 'm', 'e', ':', '/', ( char )0xed, ( char )0xa0, ( char )0x80 };
	sirVFSTestFailure( szSurrogate, sizeof( szSurrogate ), SIR_VFS_PATH_INVALID_ENCODING );
	const char szTooLong[SIR_VFS_MAX_PATH_BYTES] = {};
	sirVFSTestFailure( szTooLong, sizeof( szTooLong ), SIR_VFS_PATH_TOO_LONG );
	sirVFSPath_t path = {};
	assert( SIRVFSPath::normalize( nullptr, 3, &path ) == SIR_VFS_PATH_INVALID_ARGUMENT );
	assert( SIRVFSPath::normalize( "game:/a", 7, nullptr ) == SIR_VFS_PATH_INVALID_ARGUMENT );
	return 0;
}
