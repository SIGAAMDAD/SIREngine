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

#include <Core/Platform/SIRPlatformFileSystem.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static sirFileSystemResult_t sirFileSystemError( int nError ) noexcept
{
	switch ( nError ) {
	case ENOENT:
	case ENOTDIR:
		return SIR_FILESYSTEM_NOT_FOUND;
	case EEXIST:
		return SIR_FILESYSTEM_ALREADY_EXISTS;
	case EACCES:
	case EPERM:
		return SIR_FILESYSTEM_ACCESS_DENIED;
	case ENAMETOOLONG:
		return SIR_FILESYSTEM_BUFFER_TOO_SMALL;
	default:
		return SIR_FILESYSTEM_IO_ERROR;
	}
}

static void sirFileSystemStatInfo( const struct stat& rStat, sirFileSystemInfo_t *pInfo ) noexcept
{
	pInfo->nSizeBytes = rStat.st_size > 0 ? ( uint64_t )rStat.st_size : 0;
	const uint64_t nSeconds = rStat.st_mtim.tv_sec >= 0 ? ( uint64_t )rStat.st_mtim.tv_sec : 0;
	pInfo->nModifiedNanoseconds = nSeconds <= ( UINT64_MAX - 999999999ULL ) / 1000000000ULL
	                                  ? nSeconds * 1000000000ULL + ( rStat.st_mtim.tv_sec >= 0 ? ( uint64_t )rStat.st_mtim.tv_nsec : 0 )
	                                  : UINT64_MAX;
	pInfo->eType = S_ISREG( rStat.st_mode )   ? SIR_FILESYSTEM_REGULAR_FILE
	               : S_ISDIR( rStat.st_mode ) ? SIR_FILESYSTEM_DIRECTORY
	               : S_ISLNK( rStat.st_mode ) ? SIR_FILESYSTEM_SYMLINK
	                                          : SIR_FILESYSTEM_OTHER;
}

sirFileSystemResult_t SIRPlatformFileSystem::query( const char *pszPath, sirFileSystemInfo_t *pInfo ) noexcept
{
	if ( pszPath == nullptr || pszPath[0] == '\0' || pInfo == nullptr ) {
		return SIR_FILESYSTEM_INVALID_ARGUMENT;
	}
	struct stat metadata;
	if ( lstat( pszPath, &metadata ) != 0 ) {
		return sirFileSystemError( errno );
	}
	sirFileSystemStatInfo( metadata, pInfo );
	return SIR_FILESYSTEM_OK;
}

sirFileSystemResult_t SIRPlatformFileSystem::createDirectory( const char *pszPath, bool bRecursive ) noexcept
{
	if ( pszPath == nullptr || pszPath[0] == '\0' ) {
		return SIR_FILESYSTEM_INVALID_ARGUMENT;
	}
	if ( !bRecursive ) {
		if ( mkdir( pszPath, 0755 ) == 0 ) {
			return SIR_FILESYSTEM_OK;
		}
		return sirFileSystemError( errno );
	}
	const size_t nLength = strlen( pszPath );
	if ( nLength >= PATH_MAX ) {
		return SIR_FILESYSTEM_BUFFER_TOO_SMALL;
	}
	char szPath[PATH_MAX];
	memcpy( szPath, pszPath, nLength + 1 );
	for ( size_t i = 1; i <= nLength; ++i ) {
		if ( szPath[i] != '/' && szPath[i] != '\0' ) {
			continue;
		}
		const char cSaved = szPath[i];
		szPath[i] = '\0';
		if ( szPath[0] != '\0' && mkdir( szPath, 0755 ) != 0 ) {
			if ( errno != EEXIST ) {
				return sirFileSystemError( errno );
			}
			struct stat metadata;
			if ( stat( szPath, &metadata ) != 0 || !S_ISDIR( metadata.st_mode ) ) {
				return SIR_FILESYSTEM_ALREADY_EXISTS;
			}
		}
		szPath[i] = cSaved;
	}
	return SIR_FILESYSTEM_OK;
}

sirFileSystemResult_t SIRPlatformFileSystem::removeDirectory( const char *pszPath ) noexcept
{
	if ( pszPath == nullptr || pszPath[0] == '\0' ) {
		return SIR_FILESYSTEM_INVALID_ARGUMENT;
	}
	return rmdir( pszPath ) == 0 ? SIR_FILESYSTEM_OK : sirFileSystemError( errno );
}

sirFileSystemResult_t SIRPlatformFileSystem::renameReplace( const char *pszOldPath, const char *pszNewPath ) noexcept
{
	if ( pszOldPath == nullptr || pszOldPath[0] == '\0' || pszNewPath == nullptr || pszNewPath[0] == '\0' ) {
		return SIR_FILESYSTEM_INVALID_ARGUMENT;
	}
	// Atomic within one filesystem; caller is responsible for file/directory fsync durability.
	return renameat( AT_FDCWD, pszOldPath, AT_FDCWD, pszNewPath ) == 0 ? SIR_FILESYSTEM_OK : sirFileSystemError( errno );
}

sirFileSystemResult_t SIRPlatformFileSystem::enumerate( const char *pszDirectory, sirFileSystemVisitor_t pVisitor, void *pContext ) noexcept
{
	if ( pszDirectory == nullptr || pszDirectory[0] == '\0' || pVisitor == nullptr ) {
		return SIR_FILESYSTEM_INVALID_ARGUMENT;
	}
	DIR *pDirectory = opendir( pszDirectory );
	if ( pDirectory == nullptr ) {
		return sirFileSystemError( errno );
	}
	sirFileSystemResult_t eResult = SIR_FILESYSTEM_OK;
	for ( ;; ) {
		errno = 0;
		const struct dirent *pEntry = readdir( pDirectory );
		if ( pEntry == nullptr ) {
			if ( errno != 0 ) {
				eResult = sirFileSystemError( errno );
			}
			break;
		}
		if ( strcmp( pEntry->d_name, "." ) == 0 || strcmp( pEntry->d_name, ".." ) == 0 ) {
			continue;
		}
		struct stat metadata;
		if ( fstatat( dirfd( pDirectory ), pEntry->d_name, &metadata, AT_SYMLINK_NOFOLLOW ) != 0 ) {
			eResult = sirFileSystemError( errno );
			break;
		}
		sirFileSystemInfo_t info{};
		sirFileSystemStatInfo( metadata, &info );
		if ( !pVisitor( pEntry->d_name, &info, pContext ) ) {
			eResult = SIR_FILESYSTEM_STOPPED;
			break;
		}
	}
	if ( closedir( pDirectory ) != 0 && eResult == SIR_FILESYSTEM_OK ) {
		eResult = SIR_FILESYSTEM_IO_ERROR;
	}
	return eResult;
}

// Core-owned atomic scene replacement and bounded regular-file reads.
static sirFileSystemResult_t sirFileSystemCheckExpected( const char *pszDestination,
                                                         const sirFileSystemAtomicWriteRequest_t &request ) noexcept
{
	if ( request.pExpectedBytes == nullptr ) {
		return SIR_FILESYSTEM_OK;
	}
	const int nFile = open( pszDestination, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK );
	if ( nFile < 0 ) {
		return errno == ENOENT ? SIR_FILESYSTEM_CONFLICT : SIR_FILESYSTEM_IO_ERROR;
	}
	struct stat metadata = {};
	if ( fstat( nFile, &metadata ) != 0 ) {
		close( nFile );
		return SIR_FILESYSTEM_IO_ERROR;
	}
	if ( !S_ISREG( metadata.st_mode ) ) {
		close( nFile );
		return SIR_FILESYSTEM_INVALID_PATH;
	}
	if ( metadata.st_size < 0 || ( uint64_t )metadata.st_size != ( uint64_t )request.nExpectedBytes ) {
		close( nFile );
		return SIR_FILESYSTEM_CONFLICT;
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
			return SIR_FILESYSTEM_IO_ERROR;
		}
		if ( memcmp( szChunk, request.pExpectedBytes + nRead, ( size_t )nActual ) != 0 ) {
			close( nFile );
			return SIR_FILESYSTEM_CONFLICT;
		}
		nRead += ( size_t )nActual;
	}
	return close( nFile ) == 0 ? SIR_FILESYSTEM_OK : SIR_FILESYSTEM_IO_ERROR;
}

sirFileSystemResult_t SIRPlatformFileSystem::readFilePlatform( const char *pszPath, char *pszOutput, size_t nCapacity, size_t *pOutBytes ) noexcept
{
	const int nFile = open( pszPath, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK );
	if ( nFile < 0 ) {
		return errno == ELOOP ? SIR_FILESYSTEM_INVALID_PATH : SIR_FILESYSTEM_IO_ERROR;
	}
	struct stat metadata = {};
	if ( fstat( nFile, &metadata ) != 0 ) {
		close( nFile );
		return SIR_FILESYSTEM_IO_ERROR;
	}
	if ( !S_ISREG( metadata.st_mode ) || metadata.st_size < 0 ) {
		close( nFile );
		return SIR_FILESYSTEM_INVALID_PATH;
	}
	const uint64_t nFileBytes = ( uint64_t )metadata.st_size;
	if ( nFileBytes > SIZE_MAX ) {
		close( nFile );
		return SIR_FILESYSTEM_OUT_OF_CAPACITY;
	}
	*pOutBytes = ( size_t )nFileBytes;
	if ( pszOutput == nullptr ) {
		return close( nFile ) == 0 ? SIR_FILESYSTEM_OK : SIR_FILESYSTEM_IO_ERROR;
	}
	if ( nCapacity < nFileBytes ) {
		close( nFile );
		return SIR_FILESYSTEM_OUT_OF_CAPACITY;
	}
	size_t nRead = 0;
	while ( nRead < nFileBytes ) {
		const ssize_t nActual = read( nFile, pszOutput + nRead, ( size_t )nFileBytes - nRead );
		if ( nActual < 0 && errno == EINTR ) {
			continue;
		}
		if ( nActual <= 0 ) {
			close( nFile );
			return SIR_FILESYSTEM_IO_ERROR;
		}
		nRead += ( size_t )nActual;
	}
	char szExtra[1] = {};
	const ssize_t nExtra = read( nFile, szExtra, sizeof( szExtra ) );
	const bool bComplete = nExtra == 0;
	const bool bClosed = close( nFile ) == 0;
	return bComplete && bClosed ? SIR_FILESYSTEM_OK : SIR_FILESYSTEM_IO_ERROR;
}

sirFileSystemResult_t SIRPlatformFileSystem::writeAtomicallyPlatform( const sirFileSystemAtomicWriteRequest_t &request ) noexcept
{
	// Do not discard symlinks, devices or special files under an authored scene path.
	// A failed destination probe must not be treated as a new-file save.
	struct stat previous = {};
	const int nStat = lstat( request.pszDestination, &previous );
	if ( nStat == 0 ) {
		if ( S_ISDIR( previous.st_mode ) ) {
			return SIR_FILESYSTEM_REPLACE_FAILED;
		}
		if ( !S_ISREG( previous.st_mode ) ) {
			return SIR_FILESYSTEM_INVALID_PATH;
		}
	} else if ( errno != ENOENT ) {
		return SIR_FILESYSTEM_IO_ERROR;
	}

	const sirFileSystemResult_t eExpected = sirFileSystemCheckExpected( request.pszDestination, request );
	if ( eExpected != SIR_FILESYSTEM_OK ) {
		return eExpected;
	}
	const int nFile = open( request.pszTemporary, O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, 0600 );
	if ( nFile < 0 ) {
		return errno == EEXIST ? SIR_FILESYSTEM_TEMP_EXISTS : SIR_FILESYSTEM_IO_ERROR;
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
		return SIR_FILESYSTEM_IO_ERROR;
	}
	const sirFileSystemResult_t eRecheck = sirFileSystemCheckExpected( request.pszDestination, request );
	if ( eRecheck != SIR_FILESYSTEM_OK ) {
		unlink( request.pszTemporary );
		return eRecheck;
	}
	if ( rename( request.pszTemporary, request.pszDestination ) != 0 ) {
		unlink( request.pszTemporary );
		return SIR_FILESYSTEM_REPLACE_FAILED;
	}
	char szDirectory[SIR_FILESYSTEM_MAX_PATH_BYTES] = {};
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
		return SIR_FILESYSTEM_COMMITTED_NOT_DURABLE;
	}
	const bool bSynced = fsync( nDirectoryFd ) == 0;
	const bool bClosed = close( nDirectoryFd ) == 0;
	return bSynced && bClosed ? SIR_FILESYSTEM_OK : SIR_FILESYSTEM_COMMITTED_NOT_DURABLE;
}
