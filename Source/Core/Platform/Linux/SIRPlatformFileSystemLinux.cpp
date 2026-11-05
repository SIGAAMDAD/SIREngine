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
