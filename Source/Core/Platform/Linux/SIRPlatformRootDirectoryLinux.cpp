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

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static sirFileResult_t sirRootErrorLinux( int nError ) noexcept
{
	switch ( nError ) {
	case ENOENT:
		return SIR_FILE_NOT_FOUND;
	case EACCES:
	case EPERM:
	case ELOOP:
	case ENOTDIR:
	case EISDIR:
		return SIR_FILE_ACCESS_DENIED;
	case ENAMETOOLONG:
		return SIR_FILE_OUT_OF_RANGE;
	case EMFILE:
	case ENFILE:
	case ENOMEM:
		return SIR_FILE_NO_SPACE;
	case EINVAL:
		return SIR_FILE_INVALID_ARGUMENT;
	default:
		return SIR_FILE_IO_ERROR;
	}
}

SIRPlatformRootDirectory::SIRPlatformRootDirectory( void ) noexcept : m_nNativeDirectory( UINTPTR_MAX )
{
}

SIRPlatformRootDirectory::~SIRPlatformRootDirectory( void ) noexcept
{
	( void )close();
}

bool SIRPlatformRootDirectory::isOpen( void ) const noexcept
{
	return m_nNativeDirectory != UINTPTR_MAX;
}

sirFileResult_t SIRPlatformRootDirectory::openRoot( const char *pszRootPath ) noexcept
{
	if ( isOpen() || pszRootPath == nullptr || pszRootPath[0] == '\0' ) {
		return SIR_FILE_INVALID_ARGUMENT;
	}
	const int nRoot = ::open( pszRootPath, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC );
	if ( nRoot < 0 ) {
		return sirRootErrorLinux( errno );
	}
	m_nNativeDirectory = ( uintptr_t )nRoot;
	return SIR_FILE_OK;
}

sirFileResult_t SIRPlatformRootDirectory::openRead( const char *pszRelativePath, SIRPlatformFile *pFile ) const noexcept
{
	if ( pFile == nullptr || pFile->isOpen() || !validRelative( pszRelativePath ) ) {
		return SIR_FILE_INVALID_ARGUMENT;
	}
	if ( !isOpen() ) {
		return SIR_FILE_CLOSED;
	}

	const int nRoot = ( int )m_nNativeDirectory;
	int nParent = nRoot;
	uint32_t nStart = 0;
	for ( uint32_t i = 0;; ++i ) {
		if ( pszRelativePath[i] != '/' && pszRelativePath[i] != '\0' ) {
			continue;
		}
		const bool bFinal = pszRelativePath[i] == '\0';
		char szPart[SIR_PLATFORM_ROOTED_MAX_RELATIVE_BYTES];
		const uint32_t nCount = i - nStart;
		memcpy( szPart, pszRelativePath + nStart, nCount );
		szPart[nCount] = '\0';
		// Each syscall resolves exactly one component relative to a held
		// directory descriptor. No symlinks in *any* component are followed.
		const int nFlags = O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK | ( bFinal ? O_RDONLY : O_RDONLY | O_DIRECTORY );
		const int nChild = ::openat( nParent, szPart, nFlags );
		const int nError = errno;
		if ( nParent != nRoot ) {
			( void )::close( nParent );
		}
		if ( nChild < 0 ) {
			return sirRootErrorLinux( nError );
		}
		struct stat metadata = {};
		if ( ::fstat( nChild, &metadata ) != 0 || ( bFinal ? !S_ISREG( metadata.st_mode ) : !S_ISDIR( metadata.st_mode ) ) ||
		     ( bFinal && metadata.st_size < 0 ) ) {
			( void )::close( nChild );
			return SIR_FILE_ACCESS_DENIED;
		}
		if ( bFinal ) {
			pFile->m_nNativeHandle = ( uintptr_t )nChild;
			pFile->m_nFlags = SIR_FILE_OPEN_READ;
			pFile->m_nCursor = 0;
			return SIR_FILE_OK;
		}
		nParent = nChild;
		nStart = i + 1;
	}
}

sirFileResult_t SIRPlatformRootDirectory::close( void ) noexcept
{
	if ( !isOpen() ) {
		return SIR_FILE_OK;
	}
	const int nRoot = ( int )m_nNativeDirectory;
	m_nNativeDirectory = UINTPTR_MAX;
	return ::close( nRoot ) == 0 ? SIR_FILE_OK : sirRootErrorLinux( errno );
}
