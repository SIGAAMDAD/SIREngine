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

#include <Core/Platform/SIRPlatformFile.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static_assert( sizeof( off_t ) >= sizeof( int64_t ), "SIREngine requires 64-bit file offsets" );

static sirFileResult_t sirFileErrorLinux( int nError ) noexcept
{
	switch ( nError ) {
	case ENOENT:
		return SIR_FILE_NOT_FOUND;
	case EEXIST:
		return SIR_FILE_ALREADY_EXISTS;
	case EACCES:
	case EPERM:
	case EISDIR:
		return SIR_FILE_ACCESS_DENIED;
	case ENOSPC:
	case EDQUOT:
		return SIR_FILE_NO_SPACE;
	case EINVAL:
		return SIR_FILE_INVALID_ARGUMENT;
	case EOVERFLOW:
	case EFBIG:
		return SIR_FILE_OUT_OF_RANGE;
	default:
		return SIR_FILE_IO_ERROR;
	}
}

static bool sirFileValidFlags( uint32_t nFlags ) noexcept
{
	const uint32_t nKnown = SIR_FILE_OPEN_READ | SIR_FILE_OPEN_WRITE | SIR_FILE_OPEN_CREATE | SIR_FILE_OPEN_TRUNCATE |
	                        SIR_FILE_OPEN_APPEND | SIR_FILE_OPEN_EXCLUSIVE;
	return ( ( nFlags & ~nKnown ) == 0 ) && ( ( nFlags & ( SIR_FILE_OPEN_READ | SIR_FILE_OPEN_WRITE ) ) != 0 ) &&
	       ( ( nFlags & ( SIR_FILE_OPEN_CREATE | SIR_FILE_OPEN_TRUNCATE | SIR_FILE_OPEN_APPEND ) ) == 0 ||
	         ( nFlags & SIR_FILE_OPEN_WRITE ) != 0 ) &&
	       ( ( nFlags & SIR_FILE_OPEN_EXCLUSIVE ) == 0 || ( nFlags & SIR_FILE_OPEN_CREATE ) != 0 ) &&
	       ( ( nFlags & SIR_FILE_OPEN_APPEND ) == 0 || ( nFlags & SIR_FILE_OPEN_TRUNCATE ) == 0 );
}

static bool sirFileApplyOffset( uint64_t nBase, int64_t nDelta, uint64_t *pResult ) noexcept
{
	if ( nDelta >= 0 ) {
		if ( nBase > ( uint64_t )INT64_MAX - ( uint64_t )nDelta ) {
			return false;
		}
		*pResult = nBase + ( uint64_t )nDelta;
		return true;
	}
	const uint64_t nMagnitude = ( uint64_t )( -( nDelta + 1 ) ) + 1;
	if ( nMagnitude > nBase ) {
		return false;
	}
	*pResult = nBase - nMagnitude;
	return true;
}

SIRPlatformFile::SIRPlatformFile( void ) noexcept : m_nNativeHandle( UINTPTR_MAX ), m_nCursor( 0 ), m_nFlags( 0 )
{
}

SIRPlatformFile::~SIRPlatformFile( void ) noexcept
{
	close();
}

sirFileResult_t SIRPlatformFile::open( const char *pszUtf8Path, uint32_t nFlags ) noexcept
{
	if ( isOpen() || pszUtf8Path == nullptr || pszUtf8Path[0] == '\0' || !sirFileValidFlags( nFlags ) ) {
		return SIR_FILE_INVALID_ARGUMENT;
	}
	int nNativeFlags = O_CLOEXEC;
	if ( ( nFlags & SIR_FILE_OPEN_READ ) && ( nFlags & SIR_FILE_OPEN_WRITE ) ) {
		nNativeFlags |= O_RDWR;
	} else {
		nNativeFlags |= ( nFlags & SIR_FILE_OPEN_WRITE ) ? O_WRONLY : O_RDONLY;
	}
	if ( nFlags & SIR_FILE_OPEN_CREATE ) {
		nNativeFlags |= O_CREAT;
	}
	if ( nFlags & SIR_FILE_OPEN_TRUNCATE ) {
		nNativeFlags |= O_TRUNC;
	}
	if ( nFlags & SIR_FILE_OPEN_EXCLUSIVE ) {
		nNativeFlags |= O_EXCL;
	}
	if ( nFlags & SIR_FILE_OPEN_APPEND ) {
		nNativeFlags |= O_APPEND;
	}

	const int nHandle = ::open( pszUtf8Path, nNativeFlags, 0666 );
	if ( nHandle < 0 ) {
		return sirFileErrorLinux( errno );
	}
	struct stat info = {};
	if ( fstat( nHandle, &info ) != 0 || !S_ISREG( info.st_mode ) || info.st_size < 0 ) {
		::close( nHandle );
		return SIR_FILE_IO_ERROR;
	}
	m_nNativeHandle = ( uintptr_t )nHandle;
	m_nCursor = ( nFlags & SIR_FILE_OPEN_APPEND ) ? ( uint64_t )info.st_size : 0;
	m_nFlags = nFlags;
	return SIR_FILE_OK;
}

sirFileResult_t SIRPlatformFile::close( void ) noexcept
{
	if ( !isOpen() ) {
		return SIR_FILE_OK;
	}
	const int nHandle = ( int )m_nNativeHandle;
	m_nNativeHandle = UINTPTR_MAX;
	m_nFlags = 0;
	m_nCursor = 0;
	// Do not retry close on EINTR; the descriptor may already have been reused.
	return ::close( nHandle ) == 0 ? SIR_FILE_OK : sirFileErrorLinux( errno );
}

bool SIRPlatformFile::isOpen( void ) const noexcept
{
	return m_nNativeHandle != UINTPTR_MAX;
}

sirFileResult_t SIRPlatformFile::readAt( uint64_t nOffset, void *pDestination, uint64_t nBytes, uint64_t *pTransferred ) const noexcept
{
	if ( pTransferred == nullptr ) {
		return SIR_FILE_INVALID_ARGUMENT;
	}
	*pTransferred = 0;
	if ( !isOpen() ) {
		return SIR_FILE_CLOSED;
	}
	if ( !( m_nFlags & SIR_FILE_OPEN_READ ) ) {
		return SIR_FILE_ACCESS_DENIED;
	}
	if ( nBytes != 0 && pDestination == nullptr ) {
		return SIR_FILE_INVALID_ARGUMENT;
	}
	if ( nOffset > ( uint64_t )INT64_MAX || nBytes > ( uint64_t )INT64_MAX - nOffset ) {
		return SIR_FILE_OUT_OF_RANGE;
	}
	uint8_t *pOutput = ( uint8_t * )pDestination;
	while ( *pTransferred < nBytes ) {
		uint64_t nRemaining = nBytes - *pTransferred;
		size_t nStep = ( size_t )( nRemaining < 1024 * 1024 ? nRemaining : 1024 * 1024 );
		const ssize_t nRead = pread( ( int )m_nNativeHandle, pOutput + *pTransferred, nStep, ( off_t )( nOffset + *pTransferred ) );
		if ( nRead < 0 && errno == EINTR ) {
			continue;
		}
		if ( nRead < 0 ) {
			return sirFileErrorLinux( errno );
		}
		if ( nRead == 0 ) {
			return SIR_FILE_END_OF_FILE;
		}
		*pTransferred += ( uint64_t )nRead;
	}
	return SIR_FILE_OK;
}

sirFileResult_t SIRPlatformFile::writeAt( uint64_t nOffset, const void *pSource, uint64_t nBytes, uint64_t *pTransferred ) const noexcept
{
	if ( pTransferred == nullptr ) {
		return SIR_FILE_INVALID_ARGUMENT;
	}
	*pTransferred = 0;
	if ( !isOpen() ) {
		return SIR_FILE_CLOSED;
	}
	if ( !( m_nFlags & SIR_FILE_OPEN_WRITE ) ) {
		return SIR_FILE_ACCESS_DENIED;
	}
	if ( m_nFlags & SIR_FILE_OPEN_APPEND ) {
		return SIR_FILE_INVALID_ARGUMENT;
	}
	if ( nBytes != 0 && pSource == nullptr ) {
		return SIR_FILE_INVALID_ARGUMENT;
	}
	if ( nOffset > ( uint64_t )INT64_MAX || nBytes > ( uint64_t )INT64_MAX - nOffset ) {
		return SIR_FILE_OUT_OF_RANGE;
	}
	const uint8_t *pInput = ( const uint8_t * )pSource;
	while ( *pTransferred < nBytes ) {
		uint64_t nRemaining = nBytes - *pTransferred;
		size_t nStep = ( size_t )( nRemaining < 1024 * 1024 ? nRemaining : 1024 * 1024 );
		const ssize_t nWritten = pwrite( ( int )m_nNativeHandle, pInput + *pTransferred, nStep, ( off_t )( nOffset + *pTransferred ) );
		if ( nWritten < 0 && errno == EINTR ) {
			continue;
		}
		if ( nWritten < 0 ) {
			return sirFileErrorLinux( errno );
		}
		if ( nWritten == 0 ) {
			return SIR_FILE_IO_ERROR;
		}
		*pTransferred += ( uint64_t )nWritten;
	}
	return SIR_FILE_OK;
}

sirFileResult_t SIRPlatformFile::read( void *pDestination, uint64_t nBytes, uint64_t *pTransferred ) noexcept
{
	sirFileResult_t eResult = readAt( m_nCursor, pDestination, nBytes, pTransferred );
	if ( pTransferred != nullptr ) {
		m_nCursor += *pTransferred;
	}
	return eResult;
}

sirFileResult_t SIRPlatformFile::write( const void *pSource, uint64_t nBytes, uint64_t *pTransferred ) noexcept
{
	if ( !( m_nFlags & SIR_FILE_OPEN_APPEND ) ) {
		sirFileResult_t eResult = writeAt( m_nCursor, pSource, nBytes, pTransferred );
		if ( pTransferred != nullptr ) {
			m_nCursor += *pTransferred;
		}
		return eResult;
	}
	if ( pTransferred == nullptr ) {
		return SIR_FILE_INVALID_ARGUMENT;
	}
	*pTransferred = 0;
	if ( !isOpen() ) {
		return SIR_FILE_CLOSED;
	}
	if ( nBytes != 0 && pSource == nullptr ) {
		return SIR_FILE_INVALID_ARGUMENT;
	}
	const uint8_t *pInput = ( const uint8_t * )pSource;
	while ( *pTransferred < nBytes ) {
		uint64_t nRemaining = nBytes - *pTransferred;
		size_t nStep = ( size_t )( nRemaining < 1024 * 1024 ? nRemaining : 1024 * 1024 );
		const ssize_t nWritten = ::write( ( int )m_nNativeHandle, pInput + *pTransferred, nStep );
		if ( nWritten < 0 && errno == EINTR ) {
			continue;
		}
		if ( nWritten < 0 ) {
			return sirFileErrorLinux( errno );
		}
		if ( nWritten == 0 ) {
			return SIR_FILE_IO_ERROR;
		}
		*pTransferred += ( uint64_t )nWritten;
	}
	uint64_t nFileBytes = 0;
	if ( getSize( &nFileBytes ) == SIR_FILE_OK ) {
		m_nCursor = nFileBytes;
	}
	return SIR_FILE_OK;
}

sirFileResult_t SIRPlatformFile::seek( int64_t nOffset, sirFileSeekOrigin_t eOrigin, uint64_t *pPosition ) noexcept
{
	if ( !isOpen() ) {
		return SIR_FILE_CLOSED;
	}
	uint64_t nBase = 0;
	if ( eOrigin == SIR_FILE_SEEK_CURRENT ) {
		nBase = m_nCursor;
	} else if ( eOrigin == SIR_FILE_SEEK_END ) {
		sirFileResult_t eResult = getSize( &nBase );
		if ( eResult != SIR_FILE_OK ) {
			return eResult;
		}
	} else if ( eOrigin != SIR_FILE_SEEK_BEGIN )
		return SIR_FILE_INVALID_ARGUMENT;
	uint64_t nNext = 0;
	if ( !sirFileApplyOffset( nBase, nOffset, &nNext ) ) {
		return SIR_FILE_OUT_OF_RANGE;
	}
	m_nCursor = nNext;
	if ( pPosition != nullptr ) {
		*pPosition = nNext;
	}
	return SIR_FILE_OK;
}

sirFileResult_t SIRPlatformFile::tell( uint64_t *pPosition ) const noexcept
{
	if ( pPosition == nullptr ) {
		return SIR_FILE_INVALID_ARGUMENT;
	}
	if ( !isOpen() ) {
		return SIR_FILE_CLOSED;
	}
	*pPosition = m_nCursor;
	return SIR_FILE_OK;
}

sirFileResult_t SIRPlatformFile::getSize( uint64_t *pBytes ) const noexcept
{
	if ( pBytes == nullptr ) {
		return SIR_FILE_INVALID_ARGUMENT;
	}
	if ( !isOpen() ) {
		return SIR_FILE_CLOSED;
	}
	struct stat info = {};
	if ( fstat( ( int )m_nNativeHandle, &info ) != 0 ) {
		return sirFileErrorLinux( errno );
	}
	if ( info.st_size < 0 ) {
		return SIR_FILE_OUT_OF_RANGE;
	}
	*pBytes = ( uint64_t )info.st_size;
	return SIR_FILE_OK;
}

sirFileResult_t SIRPlatformFile::flush( void ) const noexcept
{
	if ( !isOpen() ) {
		return SIR_FILE_CLOSED;
	}
	return fsync( ( int )m_nNativeHandle ) == 0 ? SIR_FILE_OK : sirFileErrorLinux( errno );
}

sirFileResult_t SIRPlatformFile::truncate( uint64_t nBytes ) noexcept
{
	if ( !isOpen() ) {
		return SIR_FILE_CLOSED;
	}
	if ( !( m_nFlags & SIR_FILE_OPEN_WRITE ) ) {
		return SIR_FILE_ACCESS_DENIED;
	}
	if ( nBytes > ( uint64_t )INT64_MAX ) {
		return SIR_FILE_OUT_OF_RANGE;
	}
	return ftruncate( ( int )m_nNativeHandle, ( off_t )nBytes ) == 0 ? SIR_FILE_OK : sirFileErrorLinux( errno );
}

sirFileResult_t SIRPlatformFile::getInfo( const char *pszUtf8Path, sirFileInfo_t *pInfo ) noexcept
{
	if ( pszUtf8Path == nullptr || pszUtf8Path[0] == '\0' || pInfo == nullptr ) {
		return SIR_FILE_INVALID_ARGUMENT;
	}
	struct stat info = {};
	if ( stat( pszUtf8Path, &info ) != 0 ) {
		return sirFileErrorLinux( errno );
	}
	sirFileInfo_t result = {};
	if ( info.st_size >= 0 ) {
		result.nSizeBytes = ( uint64_t )info.st_size;
	}
	if ( info.st_mtim.tv_sec >= 0 && ( uint64_t )info.st_mtim.tv_sec <= UINT64_MAX / 1000000000u ) {
		result.nModifiedUnixNanoseconds = ( uint64_t )info.st_mtim.tv_sec * 1000000000u + ( uint64_t )info.st_mtim.tv_nsec;
	}
	result.bRegularFile = S_ISREG( info.st_mode );
	result.bDirectory = S_ISDIR( info.st_mode );
	*pInfo = result;
	return SIR_FILE_OK;
}

sirFileResult_t SIRPlatformFile::remove( const char *pszUtf8Path ) noexcept
{
	if ( pszUtf8Path == nullptr || pszUtf8Path[0] == '\0' ) {
		return SIR_FILE_INVALID_ARGUMENT;
	}
	return unlink( pszUtf8Path ) == 0 ? SIR_FILE_OK : sirFileErrorLinux( errno );
}

sirFileResult_t SIRPlatformFile::renameReplace( const char *pszOldPath, const char *pszNewPath ) noexcept
{
	if ( pszOldPath == nullptr || pszNewPath == nullptr || pszOldPath[0] == '\0' || pszNewPath[0] == '\0' ) {
		return SIR_FILE_INVALID_ARGUMENT;
	}
	return renameat( AT_FDCWD, pszOldPath, AT_FDCWD, pszNewPath ) == 0 ? SIR_FILE_OK : sirFileErrorLinux( errno );
}
