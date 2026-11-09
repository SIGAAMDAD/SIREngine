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

#include <Editor/Scene/SIREditorFileSave.h>
#include <Core/Platform/SIRPlatformFileSystem.h>

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if defined( _WIN32 )
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

static void sirEditorTestReadExact( const char *pszPath, const char *pszExpected, size_t nExpected ) noexcept
{
	FILE *pFile = fopen( pszPath, "rb" );
	assert( pFile != nullptr );
	char szContents[1024] = {};
	assert( nExpected < sizeof( szContents ) );
	assert( fread( szContents, 1, nExpected, pFile ) == nExpected );
	assert( fgetc( pFile ) == EOF );
	assert( fclose( pFile ) == 0 );
	assert( memcmp( szContents, pszExpected, nExpected ) == 0 );
}

static void sirEditorTestWriteOriginal( const char *pszPath, const char *pszContent ) noexcept
{
	FILE *pFile = fopen( pszPath, "wb" );
	assert( pFile != nullptr );
	assert( fwrite( pszContent, 1, strlen( pszContent ), pFile ) == strlen( pszContent ) );
	assert( fclose( pFile ) == 0 );
}

static void sirEditorTestCommentedSceneRoundtrip( const char *pszDestination, const char *pszTemporary ) noexcept
{
	const char szOriginal[] = "[gd_scene format=3]\n"
	                          "[node name=\"Root\" type=\"Node\"]\n"
	                          "health = 12 # preserve this original comment\n"
	                          "enabled = true ; preserve this comment too\n"
	                          "metadata = {\"keys\": [1, # ] } ignored comment\n"
	                          "  2]}\n";
	sirEditorTextSceneNode_t nodes[1] = {};
	char szPaths[256] = {};
	sirEditorTextSceneIndex_t scene = {};
	scene.storage.pNodes = nodes;
	scene.storage.pszPaths = szPaths;
	scene.storage.nNodeCapacity = 1;
	scene.storage.nPathCapacity = sizeof( szPaths );
	assert( sirEditorScanTextScene( szOriginal, sizeof( szOriginal ) - 1, &scene ) == SIR_EDITOR_TEXT_OK );
	sirEditorTextProperty_t properties[3] = {};
	sirEditorTextPropertyIndex_t index = { properties, 3, 0, 0 };
	assert( sirEditorScanNodeTextProperties( szOriginal, sizeof( szOriginal ) - 1, scene, &index ) == SIR_EDITOR_PROPERTY_OK );
	assert( index.nCount == 3 );
	const sirEditorTextEdit_t changes[] = { { 0, "99", 2 }, { 1, "false", 5 } };
	char szEdited[512] = {};
	size_t nOutputBytes = 0;
	assert( sirEditorWriteScalarPatches( szOriginal, sizeof( szOriginal ) - 1, index, changes, 2, szEdited, sizeof( szEdited ),
	                                     &nOutputBytes ) == SIR_EDITOR_WRITE_OK );
	assert( strstr( szEdited, "health = 99 # preserve this original comment\n" ) != nullptr );
	assert( strstr( szEdited, "enabled = false ; preserve this comment too\n" ) != nullptr );
	assert( strstr( szEdited, "  2]}\n" ) != nullptr );
	sirEditorTestWriteOriginal( pszDestination, szOriginal );
	sirEditorFileWriteRequest_t write = { pszDestination, pszTemporary, szEdited, nOutputBytes };
	write.pExpectedBytes = szOriginal;
	write.nExpectedBytes = sizeof( szOriginal ) - 1;
	assert( sirEditorWriteFileAtomically( write ) == SIR_EDITOR_FILE_SAVE_OK );
	sirEditorTestReadExact( pszDestination, szEdited, nOutputBytes );
	assert( fopen( pszTemporary, "rb" ) == nullptr );
	// Close and reopen the actual saved file before validating its native
	// scene/index state; an in-memory copy is not a disk roundtrip.
	char szReopened[512] = {};
	FILE *pReopened = fopen( pszDestination, "rb" );
	assert( pReopened != nullptr );
	assert( fread( szReopened, 1, nOutputBytes, pReopened ) == nOutputBytes );
	assert( fgetc( pReopened ) == EOF );
	assert( fclose( pReopened ) == 0 );
	assert( sirEditorScanTextScene( szReopened, nOutputBytes, &scene ) == SIR_EDITOR_TEXT_OK );
	assert( sirEditorScanNodeTextProperties( szReopened, nOutputBytes, scene, &index ) == SIR_EDITOR_PROPERTY_OK );
	assert( index.nCount == 3 );
	assert( memcmp( szReopened + properties[0].nValueOffset, "99", 2 ) == 0 );
	assert( memcmp( szReopened + properties[1].nValueOffset, "false", 5 ) == 0 );
}

int main( void )
{
	char szDirectory[512] = {};
#if defined( _WIN32 )
	char szTempRoot[MAX_PATH] = {};
	assert( GetTempPathA( MAX_PATH, szTempRoot ) != 0 );
	assert( snprintf( szDirectory, sizeof( szDirectory ), "%ssir-editor-save-%lu", szTempRoot, ( unsigned long )GetCurrentProcessId() ) >
	        0 );
	assert( CreateDirectoryA( szDirectory, nullptr ) != 0 );
#else
	memcpy( szDirectory, "/tmp/sir-editor-save-XXXXXX", sizeof( "/tmp/sir-editor-save-XXXXXX" ) );
	assert( mkdtemp( szDirectory ) != nullptr );
#endif
	char szDestination[640] = {};
	char szTemporary[640] = {};
	char szOther[640] = {};
	assert( snprintf( szDestination, sizeof( szDestination ), "%s/scene.tscn", szDirectory ) > 0 );
	assert( snprintf( szTemporary, sizeof( szTemporary ), "%s/scene.tscn.tx", szDirectory ) > 0 );
	assert( snprintf( szOther, sizeof( szOther ), "%s/obstacle", szDirectory ) > 0 );

	sirEditorTestCommentedSceneRoundtrip( szDestination, szTemporary );
	const char *pszOriginal = "[gd_scene format=3]\n[node name=\"Old\" type=\"Node\"]\n";
	const char *pszReplacement = "[gd_scene format=3]\n[node name=\"New\" type=\"Node\"]\n";
	sirEditorTestWriteOriginal( szDestination, pszOriginal );
	// Platform service works independently of the editor authoring facade.
	sirFileSystemAtomicWriteRequest_t platform = { szDestination, szTemporary, pszOriginal, strlen( pszOriginal ) };
	platform.pExpectedBytes = pszOriginal;
	platform.nExpectedBytes = strlen( pszOriginal );
	assert( SIRPlatformFileSystem::writeAtomically( platform ) == SIR_ATOMIC_SAVE_OK );
	sirEditorTestReadExact( szDestination, pszOriginal, strlen( pszOriginal ) );
	platform.pExpectedBytes = "stale revision";
	platform.nExpectedBytes = strlen( "stale revision" );
	assert( SIRPlatformFileSystem::writeAtomically( platform ) == SIR_ATOMIC_SAVE_CONFLICT );
	sirEditorTestReadExact( szDestination, pszOriginal, strlen( pszOriginal ) );
	assert( fopen( szTemporary, "rb" ) == nullptr );
	sirEditorFileWriteRequest_t request = { szDestination, szTemporary, pszReplacement, strlen( pszReplacement ) };
	assert( sirEditorWriteFileAtomically( request ) == SIR_EDITOR_FILE_SAVE_OK );
	sirEditorTestReadExact( szDestination, pszReplacement, strlen( pszReplacement ) );
	assert( fopen( szTemporary, "rb" ) == nullptr );

	const char *pszUpdated = "[gd_scene format=3]\n[node name=\"Next\" type=\"Node\"]\n";
#if !defined( _WIN32 )
	// Atomic replacements must preserve existing authoring file permissions.
	assert( chmod( szDestination, 0644 ) == 0 );
#endif
	request.pBytes = pszUpdated;
	request.nBytes = strlen( pszUpdated );
	assert( sirEditorWriteFileAtomically( request ) == SIR_EDITOR_FILE_SAVE_OK );
	sirEditorTestReadExact( szDestination, pszUpdated, strlen( pszUpdated ) );
#if !defined( _WIN32 )
	struct stat filePermissions = {};
	assert( stat( szDestination, &filePermissions ) == 0 );
	assert( ( filePermissions.st_mode & 0777 ) == 0644 );
#endif

	// Stale sibling output prevents clobbering and preserves both files.
	sirEditorTestWriteOriginal( szTemporary, "owned by another process" );
	assert( sirEditorWriteFileAtomically( request ) == SIR_EDITOR_FILE_SAVE_TEMP_EXISTS );
	sirEditorTestReadExact( szDestination, pszUpdated, strlen( pszUpdated ) );
	sirEditorTestReadExact( szTemporary, "owned by another process", strlen( "owned by another process" ) );
	assert( remove( szTemporary ) == 0 );

	// Optimistic scene-save guard refuses stale disk revisions without staging.
	request.pExpectedBytes = pszUpdated;
	request.nExpectedBytes = strlen( pszUpdated );
	request.pBytes = pszReplacement;
	request.nBytes = strlen( pszReplacement );
	sirEditorTestWriteOriginal( szDestination, "changed outside editor" );
	assert( sirEditorWriteFileAtomically( request ) == SIR_EDITOR_FILE_SAVE_CONFLICT );
	sirEditorTestReadExact( szDestination, "changed outside editor", strlen( "changed outside editor" ) );
	assert( fopen( szTemporary, "rb" ) == nullptr );
	sirEditorTestWriteOriginal( szDestination, pszUpdated );
	request.nExpectedBytes -= 1;
	assert( sirEditorWriteFileAtomically( request ) == SIR_EDITOR_FILE_SAVE_CONFLICT );
	request.nExpectedBytes += 1;
	assert( sirEditorWriteFileAtomically( request ) == SIR_EDITOR_FILE_SAVE_OK );
	sirEditorTestReadExact( szDestination, pszReplacement, strlen( pszReplacement ) );

	// No implicit Save As when an opened destination has been deleted.
	assert( remove( szDestination ) == 0 );
	assert( sirEditorWriteFileAtomically( request ) == SIR_EDITOR_FILE_SAVE_CONFLICT );
	assert( fopen( szTemporary, "rb" ) == nullptr );
	sirEditorTestWriteOriginal( szDestination, pszUpdated );
	request.pExpectedBytes = nullptr;
	request.nExpectedBytes = 0;
	request.pBytes = pszUpdated;
	request.nBytes = strlen( pszUpdated );

	// Exercise binary zero bytes and a payload exceeding the Windows 1 MiB
	// WriteFile chunk bound, including exact byte count and staging cleanup.
	const size_t nBinaryBytes = 1024 * 1024 + 17;
	char *pBinary = ( char * )malloc( nBinaryBytes );
	assert( pBinary != nullptr );
	for ( size_t iByte = 0; iByte < nBinaryBytes; ++iByte ) {
		pBinary[iByte] = ( char )( iByte * 17u );
	}
	request.pBytes = pBinary;
	request.nBytes = nBinaryBytes;
	assert( sirEditorWriteFileAtomically( request ) == SIR_EDITOR_FILE_SAVE_OK );
	FILE *pBinaryFile = fopen( szDestination, "rb" );
	assert( pBinaryFile != nullptr );
	char szChunk[4096] = {};
	for ( size_t iByte = 0; iByte < nBinaryBytes; ) {
		const size_t nCount = nBinaryBytes - iByte < sizeof( szChunk ) ? nBinaryBytes - iByte : sizeof( szChunk );
		assert( fread( szChunk, 1, nCount, pBinaryFile ) == nCount );
		assert( memcmp( szChunk, pBinary + iByte, nCount ) == 0 );
		iByte += nCount;
	}
	assert( fgetc( pBinaryFile ) == EOF );
	assert( fclose( pBinaryFile ) == 0 );
	assert( fopen( szTemporary, "rb" ) == nullptr );
	free( pBinary );
	request.pBytes = pszUpdated;
	request.nBytes = strlen( pszUpdated );
	assert( sirEditorWriteFileAtomically( request ) == SIR_EDITOR_FILE_SAVE_OK );

	const sirEditorFileWriteRequest_t invalid[] = {
	    { szDestination, szDestination, pszReplacement, strlen( pszReplacement ) },
	    { szDestination, "other-directory/scene.tscn.tx", pszReplacement, strlen( pszReplacement ) },
	    { szDestination, szTemporary, nullptr, 15 },
	    { szDestination, szTemporary, pszReplacement, 0 },
	    { szDestination, szTemporary, pszReplacement, 32ULL * 1024 * 1024 + 1 },
	    { nullptr, szTemporary, pszReplacement, strlen( pszReplacement ) },
	    { szDestination, nullptr, pszReplacement, strlen( pszReplacement ) } };
	for ( uint32_t iCase = 0; iCase < sizeof( invalid ) / sizeof( invalid[0] ); ++iCase ) {
		assert( sirEditorWriteFileAtomically( invalid[iCase] ) != SIR_EDITOR_FILE_SAVE_OK );
		sirEditorTestReadExact( szDestination, pszUpdated, strlen( pszUpdated ) );
	}

	// Rename over an existing nonempty directory fails, leaving that directory
	// unchanged and cleaning up the exclusively created staging file.
#if defined( _WIN32 )
	assert( CreateDirectoryA( szOther, nullptr ) != 0 );
#else
	assert( mkdir( szOther, 0700 ) == 0 );
#endif
	const sirEditorFileWriteRequest_t blocked = { szOther, szTemporary, pszReplacement, strlen( pszReplacement ) };
	assert( sirEditorWriteFileAtomically( blocked ) == SIR_EDITOR_FILE_SAVE_REPLACE_FAILED );
	assert( fopen( szTemporary, "rb" ) == nullptr );
	sirEditorTestReadExact( szDestination, pszUpdated, strlen( pszUpdated ) );
#if defined( _WIN32 )
	assert( RemoveDirectoryA( szOther ) != 0 );
#else
	assert( rmdir( szOther ) == 0 );
#endif
#if !defined( _WIN32 )
	// Scene saves must not silently replace a symlink or a special file.
	char szLinkedTarget[640] = {};
	assert( snprintf( szLinkedTarget, sizeof( szLinkedTarget ), "%s/linked-target.tscn", szDirectory ) > 0 );
	sirEditorTestWriteOriginal( szLinkedTarget, "external owner" );
	assert( remove( szDestination ) == 0 );
	assert( symlink( szLinkedTarget, szDestination ) == 0 );
	assert( sirEditorWriteFileAtomically( request ) == SIR_EDITOR_FILE_SAVE_INVALID_PATH );
	sirEditorTestReadExact( szLinkedTarget, "external owner", strlen( "external owner" ) );
	struct stat linkState = {};
	assert( lstat( szDestination, &linkState ) == 0 );
	assert( S_ISLNK( linkState.st_mode ) );
	assert( fopen( szTemporary, "rb" ) == nullptr );
	assert( unlink( szDestination ) == 0 );

	assert( mkfifo( szDestination, 0600 ) == 0 );
	assert( sirEditorWriteFileAtomically( request ) == SIR_EDITOR_FILE_SAVE_INVALID_PATH );
	assert( lstat( szDestination, &linkState ) == 0 );
	assert( S_ISFIFO( linkState.st_mode ) );
	assert( fopen( szTemporary, "rb" ) == nullptr );
	assert( unlink( szDestination ) == 0 );
	assert( remove( szLinkedTarget ) == 0 );
	sirEditorTestWriteOriginal( szDestination, pszUpdated );
#endif
	assert( remove( szDestination ) == 0 );
#if defined( _WIN32 )
	assert( RemoveDirectoryA( szDirectory ) != 0 );
#else
	assert( rmdir( szDirectory ) == 0 );
#endif
	return 0;
}
