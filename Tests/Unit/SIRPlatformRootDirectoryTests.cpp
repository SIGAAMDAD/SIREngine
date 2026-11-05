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

#include <Core/Platform/SIRPlatformRootDirectory.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined( _WIN32 )
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

static void sirRootRequire( bool bOk, const char *pszReason )
{
	if ( !bOk ) {
		fprintf( stderr, "rooted directory test failed: %s\n", pszReason );
		abort();
	}
}

static void sirRootWriteFile( const char *pszPath, const char *pszBytes )
{
	FILE *pFile = fopen( pszPath, "wb" );
	sirRootRequire( pFile != nullptr, "fixture create file" );
	sirRootRequire( fwrite( pszBytes, 1, strlen( pszBytes ), pFile ) == strlen( pszBytes ), "fixture write file" );
	sirRootRequire( fclose( pFile ) == 0, "fixture close file" );
}

int main( void )
{
	char szTemp[512] = {};
#if defined( _WIN32 )
	const DWORD nTempBytes = GetTempPathA( sizeof( szTemp ), szTemp );
	sirRootRequire( nTempBytes > 0 && nTempBytes < sizeof( szTemp ), "get temp" );
	char szBase[640] = {};
	snprintf( szBase, sizeof( szBase ), "%ssir_vfs_root_%lu", szTemp, ( unsigned long )GetCurrentProcessId() );
	sirRootRequire( CreateDirectoryA( szBase, nullptr ), "fixture directory" );
#else
	strcpy( szTemp, "/tmp/sir_vfs_root_XXXXXX" );
	char *pszTemp = mkdtemp( szTemp );
	sirRootRequire( pszTemp != nullptr, "mkdtemp" );
	const char *szBase = szTemp;
#endif
	char szNested[768] = {};
	char szFile[768] = {};
	char szOutside[768] = {};
	char szLink[768] = {};
	char szLinkDir[768] = {};
	char szBaseLink[768] = {};
	snprintf( szNested, sizeof( szNested ), "%s/nested", szBase );
	snprintf( szFile, sizeof( szFile ), "%s/nested/file.bin", szBase );
	snprintf( szOutside, sizeof( szOutside ), "%s_outside.bin", szBase );
	snprintf( szLink, sizeof( szLink ), "%s/nested/link.bin", szBase );
	snprintf( szLinkDir, sizeof( szLinkDir ), "%s/junction", szBase );
	snprintf( szBaseLink, sizeof( szBaseLink ), "%s_link", szBase );
#if defined( _WIN32 )
	sirRootRequire( CreateDirectoryA( szNested, nullptr ), "mkdir nested" );
#else
	sirRootRequire( mkdir( szNested, 0700 ) == 0, "mkdir nested" );
#endif
	sirRootWriteFile( szFile, "ROOTED" );
	sirRootWriteFile( szOutside, "OUTSIDE" );

	SIRPlatformRootDirectory root;
	SIRPlatformFile file;
	sirRootRequire( !root.isOpen(), "initially closed" );
	sirRootRequire( root.openRead( "nested/file.bin", &file ) == SIR_FILE_CLOSED, "read before root open" );
	sirRootRequire( root.openRoot( szBase ) == SIR_FILE_OK, "root open" );
	sirRootRequire( root.isOpen(), "open state" );
	sirRootRequire( root.openRoot( szBase ) == SIR_FILE_INVALID_ARGUMENT, "double root open" );
	sirRootRequire( root.openRead( "nested/file.bin", &file ) == SIR_FILE_OK, "nested rooted file" );
	sirRootRequire( file.isOpen(), "file opened" );
	sirRootRequire( root.openRead( "nested/file.bin", &file ) == SIR_FILE_INVALID_ARGUMENT, "target already open" );
	uint64_t nBytes = 0;
	sirRootRequire( file.getSize( &nBytes ) == SIR_FILE_OK && nBytes == 6, "file size" );
	char szData[7] = {};
	sirRootRequire( file.readAt( 0, szData, 6, &nBytes ) == SIR_FILE_OK && nBytes == 6, "read retained handle" );
	sirRootRequire( strcmp( szData, "ROOTED" ) == 0, "expected bytes" );

	const char *szDenied[] = { "",
	                           "/etc/passwd",
	                           "../escape",
	                           "nested/../outside",
	                           "nested/./file.bin",
	                           "nested//file.bin",
	                           "nested/file.bin/",
	                           "nested\\file.bin",
	                           "nested/file.bin:stream",
	                           "nested/file.bin.",
	                           "CON.txt",
	                           "nested/%2e%2e",
	                           "nested/file.bin ",
	                           "nested/?",
	                           "C:/Windows" };
	SIRPlatformFile rejected;
	for ( uint32_t i = 0; i < sizeof( szDenied ) / sizeof( szDenied[0] ); ++i ) {
		sirRootRequire( root.openRead( szDenied[i], &rejected ) == SIR_FILE_INVALID_ARGUMENT, "deny malformed relative path" );
		sirRootRequire( !rejected.isOpen(), "denied leaves destination closed" );
	}
	sirRootRequire( root.openRead( "nested", &rejected ) == SIR_FILE_ACCESS_DENIED, "deny directory target" );
	sirRootRequire( root.openRead( "missing/file.bin", &rejected ) == SIR_FILE_NOT_FOUND, "missing intermediate" );
	sirRootRequire( root.openRead( "nested/missing.bin", &rejected ) == SIR_FILE_NOT_FOUND, "missing final" );

#if defined( _WIN32 )
	const bool bFileLink = CreateSymbolicLinkA( szLink, szOutside, SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE ) != 0;
	const bool bDirLink =
	    CreateSymbolicLinkA( szLinkDir, szNested, SYMBOLIC_LINK_FLAG_DIRECTORY | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE ) != 0;
	const bool bRootLink =
	    CreateSymbolicLinkA( szBaseLink, szBase, SYMBOLIC_LINK_FLAG_DIRECTORY | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE ) != 0;
#else
	const bool bFileLink = symlink( szOutside, szLink ) == 0;
	const bool bDirLink = symlink( szNested, szLinkDir ) == 0;
	const bool bRootLink = symlink( szBase, szBaseLink ) == 0;
#endif
#if defined( _WIN32 )
	// Hosted Windows agents may lack SeCreateSymbolicLinkPrivilege and Developer
	// Mode. The symlink tests run when the fixtures can be constructed.
	if ( !bFileLink || !bDirLink || !bRootLink ) {
		fprintf( stderr, "rooted directory: Windows symlink fixtures unavailable; reparse checks skipped\n" );
	}
#else
	sirRootRequire( bFileLink && bDirLink && bRootLink, "symlink fixtures created" );
#endif
	if ( bFileLink ) {
		sirRootRequire( root.openRead( "nested/link.bin", &rejected ) == SIR_FILE_ACCESS_DENIED, "deny final symlink" );
	}
	if ( bDirLink ) {
		sirRootRequire( root.openRead( "junction/file.bin", &rejected ) == SIR_FILE_ACCESS_DENIED, "deny symlink directory" );
	}
	if ( bRootLink ) {
		SIRPlatformRootDirectory linkedRoot;
		sirRootRequire( linkedRoot.openRoot( szBaseLink ) == SIR_FILE_ACCESS_DENIED, "deny linked root" );
	}

	sirRootRequire( root.close() == SIR_FILE_OK, "close root" );
	sirRootRequire( root.close() == SIR_FILE_OK, "double close root" );
	sirRootRequire( file.isOpen(), "file survives root close" );
	memset( szData, 0, sizeof( szData ) );
	sirRootRequire( file.readAt( 0, szData, 6, &nBytes ) == SIR_FILE_OK && strcmp( szData, "ROOTED" ) == 0,
	                "open file retains independent native handle" );
	sirRootRequire( file.close() == SIR_FILE_OK, "close file" );

#if defined( _WIN32 )
	if ( bFileLink ) {
		DeleteFileA( szLink );
	}
	if ( bDirLink ) {
		RemoveDirectoryA( szLinkDir );
	}
	if ( bRootLink ) {
		RemoveDirectoryA( szBaseLink );
	}
	DeleteFileA( szFile );
	DeleteFileA( szOutside );
	RemoveDirectoryA( szNested );
	RemoveDirectoryA( szBase );
#else
	unlink( szLink );
	unlink( szLinkDir );
	unlink( szBaseLink );
	unlink( szFile );
	unlink( szOutside );
	rmdir( szNested );
	rmdir( szBase );
#endif
	return 0;
}
