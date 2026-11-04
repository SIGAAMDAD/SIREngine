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

#include <errno.h>
#include <stdio.h>
#include <string.h>

#if defined( _WIN32 )
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

static constexpr size_t SIR_EDITOR_FILE_MAX_BYTES = 32ULL * 1024ULL * 1024ULL;
static constexpr size_t SIR_EDITOR_FILE_MAX_PATH_BYTES = 4096;

static const char *sirEditorFileLastSeparator( const char *pszPath ) noexcept
{
	const char *pszLast = nullptr;
	for ( const char *pszCurrent = pszPath; *pszCurrent != '\0'; ++pszCurrent ) {
		if ( *pszCurrent == '/'
#if defined( _WIN32 )
		     || *pszCurrent == '\\'
#endif
		) {
			pszLast = pszCurrent;
		}
	}
	return pszLast;
}

static size_t sirEditorFileBoundedLength( const char *pszText ) noexcept
{
	size_t nBytes = 0;
	while ( nBytes < SIR_EDITOR_FILE_MAX_PATH_BYTES && pszText[nBytes] != '\0' ) {
		++nBytes;
	}
	return nBytes;
}

static bool sirEditorFileValidPaths( const char *pszDestination, const char *pszTemporary ) noexcept
{
	if ( pszDestination == nullptr || pszTemporary == nullptr ) {
		return false;
	}
	const size_t nDestination = sirEditorFileBoundedLength( pszDestination );
	const size_t nTemporary = sirEditorFileBoundedLength( pszTemporary );
	if ( nDestination == 0 || nTemporary == 0 || nDestination == SIR_EDITOR_FILE_MAX_PATH_BYTES ||
	     nTemporary == SIR_EDITOR_FILE_MAX_PATH_BYTES || strcmp( pszDestination, pszTemporary ) == 0 ) {
		return false;
	}
	const char *pszDestinationEnd = sirEditorFileLastSeparator( pszDestination );
	const char *pszTemporaryEnd = sirEditorFileLastSeparator( pszTemporary );
	if ( ( pszDestinationEnd == nullptr ) != ( pszTemporaryEnd == nullptr ) ) {
		return false;
	}
	if ( pszDestinationEnd == nullptr ) {
		return true;
	}
	const size_t nDestinationParent = ( size_t )( pszDestinationEnd - pszDestination );
	const size_t nTemporaryParent = ( size_t )( pszTemporaryEnd - pszTemporary );
	return nDestinationParent == nTemporaryParent && memcmp( pszDestination, pszTemporary, nDestinationParent ) == 0 &&
	       pszDestinationEnd[1] != '\0' && pszTemporaryEnd[1] != '\0';
}

#if defined( _WIN32 )
static bool sirEditorFileToWindowsPath( const char *pszUtf8, wchar_t *pWide, int nCapacity ) noexcept
{
	const int nConverted = MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, pszUtf8, -1, pWide, nCapacity );
	return nConverted != 0;
}

static sirEditorFileSaveResult_t sirEditorFileWritePlatform( const sirEditorFileWriteRequest_t &request ) noexcept
{
	wchar_t szDestination[4096] = {};
	wchar_t szTemporary[4096] = {};
	if ( !sirEditorFileToWindowsPath( request.pszDestination, szDestination, 4096 ) ||
	     !sirEditorFileToWindowsPath( request.pszTemporary, szTemporary, 4096 ) ) {
		return SIR_EDITOR_FILE_SAVE_INVALID_PATH;
	}
	HANDLE hFile = CreateFileW( szTemporary, GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr );
	if ( hFile == INVALID_HANDLE_VALUE ) {
		const DWORD nError = GetLastError();
		return ( nError == ERROR_FILE_EXISTS || nError == ERROR_ALREADY_EXISTS ) ? SIR_EDITOR_FILE_SAVE_TEMP_EXISTS
		                                                                         : SIR_EDITOR_FILE_SAVE_IO_ERROR;
	}
	bool bSuccess = true;
	size_t nWritten = 0;
	while ( nWritten < request.nBytes ) {
		const size_t nRemaining = request.nBytes - nWritten;
		const DWORD nChunk = nRemaining > 1048576 ? 1048576 : ( DWORD )nRemaining;
		DWORD nActual = 0;
		if ( !WriteFile( hFile, request.pBytes + nWritten, nChunk, &nActual, nullptr ) || nActual == 0 ) {
			bSuccess = false;
			break;
		}
		nWritten += nActual;
	}
	if ( bSuccess ) {
		bSuccess = FlushFileBuffers( hFile ) != 0;
	}
	if ( !CloseHandle( hFile ) ) {
		bSuccess = false;
	}
	if ( !bSuccess ) {
		DeleteFileW( szTemporary );
		return SIR_EDITOR_FILE_SAVE_IO_ERROR;
	}
	if ( !MoveFileExW( szTemporary, szDestination, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH ) ) {
		DeleteFileW( szTemporary );
		return SIR_EDITOR_FILE_SAVE_REPLACE_FAILED;
	}
	return SIR_EDITOR_FILE_SAVE_OK;
}
#else
static sirEditorFileSaveResult_t sirEditorFileWritePlatform( const sirEditorFileWriteRequest_t &request ) noexcept
{
	const int nFile = open( request.pszTemporary, O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, 0600 );
	if ( nFile < 0 ) {
		return errno == EEXIST ? SIR_EDITOR_FILE_SAVE_TEMP_EXISTS : SIR_EDITOR_FILE_SAVE_IO_ERROR;
	}
	bool bSuccess = true;
	// Preserve editable scene file permissions when replacing a regular file.
	struct stat previous = {};
	if ( lstat( request.pszDestination, &previous ) == 0 && S_ISREG( previous.st_mode ) && fchmod( nFile, previous.st_mode & 0777 ) != 0 ) {
		bSuccess = false;
	}
	size_t nWritten = 0;
	while ( bSuccess && nWritten < request.nBytes ) {
		const ssize_t nActual = write( nFile, request.pBytes + nWritten, request.nBytes - nWritten );
		if ( nActual < 0 && errno == EINTR ) {
			continue;
		}
		if ( nActual <= 0 ) {
			bSuccess = false;
			break;
		}
		nWritten += ( size_t )nActual;
	}
	if ( bSuccess && fsync( nFile ) != 0 ) {
		bSuccess = false;
	}
	if ( close( nFile ) != 0 ) {
		bSuccess = false;
	}
	if ( !bSuccess ) {
		unlink( request.pszTemporary );
		return SIR_EDITOR_FILE_SAVE_IO_ERROR;
	}
	if ( rename( request.pszTemporary, request.pszDestination ) != 0 ) {
		unlink( request.pszTemporary );
		return SIR_EDITOR_FILE_SAVE_REPLACE_FAILED;
	}
	char szDirectory[SIR_EDITOR_FILE_MAX_PATH_BYTES] = {};
	const char *pszSlash = sirEditorFileLastSeparator( request.pszDestination );
	if ( pszSlash == nullptr ) {
		memcpy( szDirectory, ".", 2 );
	} else {
		const size_t nDirectory = ( size_t )( pszSlash - request.pszDestination );
		if ( nDirectory == 0 ) {
			memcpy( szDirectory, "/", 2 );
		} else {
			memcpy( szDirectory, request.pszDestination, nDirectory );
			szDirectory[nDirectory] = '\0';
		}
	}
	const int nDirectoryFd = open( szDirectory, O_RDONLY | O_DIRECTORY | O_CLOEXEC );
	if ( nDirectoryFd < 0 ) {
		return SIR_EDITOR_FILE_SAVE_COMMITTED_NOT_DURABLE;
	}
	const bool bSynced = fsync( nDirectoryFd ) == 0;
	const bool bClosed = close( nDirectoryFd ) == 0;
	return bSynced && bClosed ? SIR_EDITOR_FILE_SAVE_OK : SIR_EDITOR_FILE_SAVE_COMMITTED_NOT_DURABLE;
}
#endif

sirEditorFileSaveResult_t sirEditorWriteFileAtomically( const sirEditorFileWriteRequest_t &request ) noexcept
{
	if ( request.pBytes == nullptr || request.nBytes == 0 || request.nBytes > SIR_EDITOR_FILE_MAX_BYTES ) {
		return SIR_EDITOR_FILE_SAVE_INVALID_ARGUMENT;
	}
	if ( !sirEditorFileValidPaths( request.pszDestination, request.pszTemporary ) ) {
		return SIR_EDITOR_FILE_SAVE_INVALID_PATH;
	}
	return sirEditorFileWritePlatform( request );
}

sirEditorFileSaveResult_t sirEditorSaveBoundScene( const sirEditorSceneSaveRequest_t &request ) noexcept
{
	if ( request.pBridge == nullptr || request.pszScratch == nullptr || request.nScratchCapacity == 0 ) {
		return SIR_EDITOR_FILE_SAVE_INVALID_ARGUMENT;
	}
	size_t nOutputBytes = 0;
	if ( sirEditorExportBoundScalars( *request.pBridge, nullptr, 0, &nOutputBytes ) != SIR_EDITOR_SCALAR_BRIDGE_OK ) {
		return SIR_EDITOR_FILE_SAVE_SERIALIZATION_FAILED;
	}
	if ( nOutputBytes == 0 || request.nScratchCapacity <= nOutputBytes ) {
		return SIR_EDITOR_FILE_SAVE_OUT_OF_CAPACITY;
	}
	if ( sirEditorExportBoundScalars( *request.pBridge, request.pszScratch, request.nScratchCapacity, &nOutputBytes ) !=
	     SIR_EDITOR_SCALAR_BRIDGE_OK ) {
		return SIR_EDITOR_FILE_SAVE_SERIALIZATION_FAILED;
	}
	const sirEditorFileWriteRequest_t write = { request.pszDestination, request.pszTemporary, request.pszScratch, nOutputBytes };
	return sirEditorWriteFileAtomically( write );
}
