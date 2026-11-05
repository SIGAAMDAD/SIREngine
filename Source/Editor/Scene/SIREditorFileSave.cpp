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
static sirEditorFileSaveResult_t sirEditorFileCheckExpected( const wchar_t *pszDestination,
                                                             const sirEditorFileWriteRequest_t &request ) noexcept
{
	if ( request.pExpectedBytes == nullptr ) {
		return SIR_EDITOR_FILE_SAVE_OK;
	}
	HANDLE hFile = CreateFileW( pszDestination, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
	                            OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr );
	if ( hFile == INVALID_HANDLE_VALUE ) {
		return GetLastError() == ERROR_FILE_NOT_FOUND ? SIR_EDITOR_FILE_SAVE_CONFLICT : SIR_EDITOR_FILE_SAVE_IO_ERROR;
	}
	BY_HANDLE_FILE_INFORMATION information = {};
	LARGE_INTEGER nLength = {};
	if ( !GetFileInformationByHandle( hFile, &information ) || !GetFileSizeEx( hFile, &nLength ) ) {
		CloseHandle( hFile );
		return SIR_EDITOR_FILE_SAVE_IO_ERROR;
	}
	if ( ( information.dwFileAttributes & ( FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DEVICE ) ) != 0 ) {
		CloseHandle( hFile );
		return SIR_EDITOR_FILE_SAVE_INVALID_PATH;
	}
	if ( nLength.QuadPart < 0 || ( uint64_t )nLength.QuadPart != ( uint64_t )request.nExpectedBytes ) {
		CloseHandle( hFile );
		return SIR_EDITOR_FILE_SAVE_CONFLICT;
	}
	char szChunk[4096] = {};
	size_t nRead = 0;
	while ( nRead < request.nExpectedBytes ) {
		const size_t nRemaining = request.nExpectedBytes - nRead;
		const DWORD nChunk = nRemaining < sizeof( szChunk ) ? ( DWORD )nRemaining : ( DWORD )sizeof( szChunk );
		DWORD nActual = 0;
		if ( !ReadFile( hFile, szChunk, nChunk, &nActual, nullptr ) || nActual == 0 ) {
			CloseHandle( hFile );
			return SIR_EDITOR_FILE_SAVE_IO_ERROR;
		}
		if ( memcmp( szChunk, request.pExpectedBytes + nRead, nActual ) != 0 ) {
			CloseHandle( hFile );
			return SIR_EDITOR_FILE_SAVE_CONFLICT;
		}
		nRead += nActual;
	}
	return CloseHandle( hFile ) ? SIR_EDITOR_FILE_SAVE_OK : SIR_EDITOR_FILE_SAVE_IO_ERROR;
}

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
	const DWORD nDestinationAttributes = GetFileAttributesW( szDestination );
	if ( nDestinationAttributes != INVALID_FILE_ATTRIBUTES ) {
		if ( ( nDestinationAttributes & FILE_ATTRIBUTE_REPARSE_POINT ) != 0 ) {
			return SIR_EDITOR_FILE_SAVE_INVALID_PATH;
		}
		if ( ( nDestinationAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0 ) {
			return SIR_EDITOR_FILE_SAVE_REPLACE_FAILED;
		}
		if ( ( nDestinationAttributes & FILE_ATTRIBUTE_DEVICE ) != 0 ) {
			return SIR_EDITOR_FILE_SAVE_INVALID_PATH;
		}
	} else if ( GetLastError() != ERROR_FILE_NOT_FOUND ) {
		return SIR_EDITOR_FILE_SAVE_IO_ERROR;
	}

	const sirEditorFileSaveResult_t eExpected = sirEditorFileCheckExpected( szDestination, request );
	if ( eExpected != SIR_EDITOR_FILE_SAVE_OK ) {
		return eExpected;
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
	const sirEditorFileSaveResult_t eRecheck = sirEditorFileCheckExpected( szDestination, request );
	if ( eRecheck != SIR_EDITOR_FILE_SAVE_OK ) {
		DeleteFileW( szTemporary );
		return eRecheck;
	}
	if ( !MoveFileExW( szTemporary, szDestination, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH ) ) {
		DeleteFileW( szTemporary );
		return SIR_EDITOR_FILE_SAVE_REPLACE_FAILED;
	}
	return SIR_EDITOR_FILE_SAVE_OK;
}
#else
static sirEditorFileSaveResult_t sirEditorFileCheckExpected( const char *pszDestination,
                                                             const sirEditorFileWriteRequest_t &request ) noexcept
{
	if ( request.pExpectedBytes == nullptr ) {
		return SIR_EDITOR_FILE_SAVE_OK;
	}
	const int nFile = open( pszDestination, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK );
	if ( nFile < 0 ) {
		return errno == ENOENT ? SIR_EDITOR_FILE_SAVE_CONFLICT : SIR_EDITOR_FILE_SAVE_IO_ERROR;
	}
	struct stat metadata = {};
	if ( fstat( nFile, &metadata ) != 0 ) {
		close( nFile );
		return SIR_EDITOR_FILE_SAVE_IO_ERROR;
	}
	if ( !S_ISREG( metadata.st_mode ) ) {
		close( nFile );
		return SIR_EDITOR_FILE_SAVE_INVALID_PATH;
	}
	if ( metadata.st_size < 0 || ( uint64_t )metadata.st_size != ( uint64_t )request.nExpectedBytes ) {
		close( nFile );
		return SIR_EDITOR_FILE_SAVE_CONFLICT;
	}
	char szChunk[4096] = {};
	size_t nRead = 0;
	while ( nRead < request.nExpectedBytes ) {
		const size_t nRemaining = request.nExpectedBytes - nRead;
		const size_t nChunk = nRemaining < sizeof( szChunk ) ? nRemaining : sizeof( szChunk );
		const ssize_t nActual = read( nFile, szChunk, nChunk );
		if ( nActual < 0 && errno == EINTR ) {
			continue;
		}
		if ( nActual <= 0 ) {
			close( nFile );
			return SIR_EDITOR_FILE_SAVE_IO_ERROR;
		}
		if ( memcmp( szChunk, request.pExpectedBytes + nRead, ( size_t )nActual ) != 0 ) {
			close( nFile );
			return SIR_EDITOR_FILE_SAVE_CONFLICT;
		}
		nRead += ( size_t )nActual;
	}
	return close( nFile ) == 0 ? SIR_EDITOR_FILE_SAVE_OK : SIR_EDITOR_FILE_SAVE_IO_ERROR;
}

static sirEditorFileSaveResult_t sirEditorFileWritePlatform( const sirEditorFileWriteRequest_t &request ) noexcept
{
	// Do not discard symlinks, devices or special files under an authored scene path.
	// A failed destination probe must not be treated as a new-file save.
	struct stat previous = {};
	const int nStat = lstat( request.pszDestination, &previous );
	if ( nStat == 0 ) {
		if ( S_ISDIR( previous.st_mode ) ) {
			return SIR_EDITOR_FILE_SAVE_REPLACE_FAILED;
		}
		if ( !S_ISREG( previous.st_mode ) ) {
			return SIR_EDITOR_FILE_SAVE_INVALID_PATH;
		}
	} else if ( errno != ENOENT ) {
		return SIR_EDITOR_FILE_SAVE_IO_ERROR;
	}

	const sirEditorFileSaveResult_t eExpected = sirEditorFileCheckExpected( request.pszDestination, request );
	if ( eExpected != SIR_EDITOR_FILE_SAVE_OK ) {
		return eExpected;
	}
	const int nFile = open( request.pszTemporary, O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, 0600 );
	if ( nFile < 0 ) {
		return errno == EEXIST ? SIR_EDITOR_FILE_SAVE_TEMP_EXISTS : SIR_EDITOR_FILE_SAVE_IO_ERROR;
	}
	bool bSuccess = true;
	// Preserve editable scene file permissions when replacing a regular file.
	if ( nStat == 0 && fchmod( nFile, previous.st_mode & 0777 ) != 0 ) {
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
	const sirEditorFileSaveResult_t eRecheck = sirEditorFileCheckExpected( request.pszDestination, request );
	if ( eRecheck != SIR_EDITOR_FILE_SAVE_OK ) {
		unlink( request.pszTemporary );
		return eRecheck;
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
	if ( request.pBytes == nullptr || request.nBytes == 0 || request.nBytes > SIR_EDITOR_FILE_MAX_BYTES ||
	     request.nExpectedBytes > SIR_EDITOR_FILE_MAX_BYTES || ( request.pExpectedBytes == nullptr && request.nExpectedBytes != 0 ) ) {
		return SIR_EDITOR_FILE_SAVE_INVALID_ARGUMENT;
	}
	if ( !sirEditorFileValidPaths( request.pszDestination, request.pszTemporary ) ) {
		return SIR_EDITOR_FILE_SAVE_INVALID_PATH;
	}
	return sirEditorFileWritePlatform( request );
}

sirEditorFileSaveResult_t sirEditorSaveBoundScene( const sirEditorSceneSaveRequest_t &request ) noexcept
{
	if ( request.pBridge == nullptr || request.pSourceScene == nullptr || request.pszScratch == nullptr || request.nScratchCapacity == 0 ) {
		return SIR_EDITOR_FILE_SAVE_INVALID_ARGUMENT;
	}
	const sirEditorTextSceneIndex_t &source = *request.pSourceScene;
	if ( request.pBridge->pDocument == nullptr || source.storage.pNodes == nullptr || source.nNodeCount == 0 ||
	     source.nNodeCount > source.storage.nNodeCapacity ) {
		return SIR_EDITOR_FILE_SAVE_INVALID_ARGUMENT;
	}
	uint32_t nAuthoredNodes = 0;
	uint64_t nRevision = 0;
	const sirEditorResult_t eCount = request.pBridge->pDocument->copySnapshot( nullptr, 0, &nAuthoredNodes, &nRevision );
	if ( eCount != SIR_EDITOR_OUT_OF_CAPACITY && eCount != SIR_EDITOR_OK ) {
		return SIR_EDITOR_FILE_SAVE_SERIALIZATION_FAILED;
	}
	if ( nAuthoredNodes != source.nNodeCount ) {
		return SIR_EDITOR_FILE_SAVE_UNSUPPORTED_EDIT;
	}
	for ( uint32_t iNode = 0; iNode < source.nNodeCount; ++iNode ) {
		const sirEditorNode_t &original = source.storage.pNodes[iNode].node;
		sirEditorNode_t current = {};
		if ( request.pBridge->pDocument->findNode( original.nId, &current ) != SIR_EDITOR_OK || current.nId != original.nId ||
		     current.nTypeId != original.nTypeId || current.nParentId != original.nParentId || current.nFlags != original.nFlags ||
		     original.nGeneration == UINT32_MAX || current.nGeneration != original.nGeneration + 1 ) {
			return SIR_EDITOR_FILE_SAVE_UNSUPPORTED_EDIT;
		}
	}
	// Only authored properties represented by this scalar binding table
	// can be persisted. Added/unbound properties require the full writer.
	uint32_t nAuthoredProperties = 0;
	const sirEditorResult_t eProperties = request.pBridge->pDocument->copyPropertySnapshot( nullptr, 0, &nAuthoredProperties );
	if ( eProperties != SIR_EDITOR_OUT_OF_CAPACITY && eProperties != SIR_EDITOR_OK ) {
		return SIR_EDITOR_FILE_SAVE_SERIALIZATION_FAILED;
	}
	if ( nAuthoredProperties != request.pBridge->nBindingCount ) {
		return SIR_EDITOR_FILE_SAVE_UNSUPPORTED_EDIT;
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
	sirEditorFileWriteRequest_t write = { request.pszDestination, request.pszTemporary, request.pszScratch, nOutputBytes };
	if ( request.bRejectExternalChanges ) {
		if ( request.pBridge->pszSource == nullptr || request.pBridge->nSourceBytes == 0 ) {
			return SIR_EDITOR_FILE_SAVE_INVALID_ARGUMENT;
		}
		write.pExpectedBytes = request.pBridge->pszSource;
		write.nExpectedBytes = request.pBridge->nSourceBytes;
	}
	return sirEditorWriteFileAtomically( write );
}
