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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static sirFileSystemResult_t sirFileSystemCheckExpected( const char *pszDestination,
                                                         const sirFileSystemAtomicWriteRequest_t &request ) noexcept
{
	if ( request.pExpectedBytes == nullptr ) {
		return SIR_FILE_SYSTEM_OK;
	}
	const int nFile = open( pszDestination, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK );
	if ( nFile < 0 ) {
		return errno == ENOENT ? SIR_FILE_SYSTEM_CONFLICT : SIR_FILE_SYSTEM_IO_ERROR;
	}
	struct stat metadata = {};
	if ( fstat( nFile, &metadata ) != 0 ) {
		close( nFile );
		return SIR_FILE_SYSTEM_IO_ERROR;
	}
	if ( !S_ISREG( metadata.st_mode ) ) {
		close( nFile );
		return SIR_FILE_SYSTEM_INVALID_PATH;
	}
	if ( metadata.st_size < 0 || ( uint64_t )metadata.st_size != ( uint64_t )request.nExpectedBytes ) {
		close( nFile );
		return SIR_FILE_SYSTEM_CONFLICT;
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
			return SIR_FILE_SYSTEM_IO_ERROR;
		}
		if ( memcmp( szChunk, request.pExpectedBytes + nRead, ( size_t )nActual ) != 0 ) {
			close( nFile );
			return SIR_FILE_SYSTEM_CONFLICT;
		}
		nRead += ( size_t )nActual;
	}
	return close( nFile ) == 0 ? SIR_FILE_SYSTEM_OK : SIR_FILE_SYSTEM_IO_ERROR;
}

sirFileSystemResult_t SIRFileSystem::writeAtomicallyPlatform( const sirFileSystemAtomicWriteRequest_t &request ) noexcept
{
	// Do not discard symlinks, devices or special files under an authored scene path.
	// A failed destination probe must not be treated as a new-file save.
	struct stat previous = {};
	const int nStat = lstat( request.pszDestination, &previous );
	if ( nStat == 0 ) {
		if ( S_ISDIR( previous.st_mode ) ) {
			return SIR_FILE_SYSTEM_REPLACE_FAILED;
		}
		if ( !S_ISREG( previous.st_mode ) ) {
			return SIR_FILE_SYSTEM_INVALID_PATH;
		}
	} else if ( errno != ENOENT ) {
		return SIR_FILE_SYSTEM_IO_ERROR;
	}

	const sirFileSystemResult_t eExpected = sirFileSystemCheckExpected( request.pszDestination, request );
	if ( eExpected != SIR_FILE_SYSTEM_OK ) {
		return eExpected;
	}
	const int nFile = open( request.pszTemporary, O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, 0600 );
	if ( nFile < 0 ) {
		return errno == EEXIST ? SIR_FILE_SYSTEM_TEMP_EXISTS : SIR_FILE_SYSTEM_IO_ERROR;
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
		return SIR_FILE_SYSTEM_IO_ERROR;
	}
	const sirFileSystemResult_t eRecheck = sirFileSystemCheckExpected( request.pszDestination, request );
	if ( eRecheck != SIR_FILE_SYSTEM_OK ) {
		unlink( request.pszTemporary );
		return eRecheck;
	}
	if ( rename( request.pszTemporary, request.pszDestination ) != 0 ) {
		unlink( request.pszTemporary );
		return SIR_FILE_SYSTEM_REPLACE_FAILED;
	}
	char szDirectory[SIR_FILE_SYSTEM_MAX_PATH_BYTES] = {};
	const char *pszSlash = lastSeparator( request.pszDestination );
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
		return SIR_FILE_SYSTEM_COMMITTED_NOT_DURABLE;
	}
	const bool bSynced = fsync( nDirectoryFd ) == 0;
	const bool bClosed = close( nDirectoryFd ) == 0;
	return bSynced && bClosed ? SIR_FILE_SYSTEM_OK : SIR_FILE_SYSTEM_COMMITTED_NOT_DURABLE;
}
