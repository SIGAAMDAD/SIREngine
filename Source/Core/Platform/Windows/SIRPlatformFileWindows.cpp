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

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

static sirFileResult_t sirFileErrorWindows( DWORD nError ) noexcept
{
	switch ( nError ) {
	case ERROR_FILE_NOT_FOUND:
	case ERROR_PATH_NOT_FOUND:
		return SIR_FILE_NOT_FOUND;
	case ERROR_FILE_EXISTS:
	case ERROR_ALREADY_EXISTS:
		return SIR_FILE_ALREADY_EXISTS;
	case ERROR_ACCESS_DENIED:
	case ERROR_SHARING_VIOLATION:
	case ERROR_LOCK_VIOLATION:
		return SIR_FILE_ACCESS_DENIED;
	case ERROR_DISK_FULL:
	case ERROR_HANDLE_DISK_FULL:
		return SIR_FILE_NO_SPACE;
	case ERROR_HANDLE_EOF:
		return SIR_FILE_END_OF_FILE;
	case ERROR_NEGATIVE_SEEK:
	case ERROR_ARITHMETIC_OVERFLOW:
		return SIR_FILE_OUT_OF_RANGE;
	case ERROR_INVALID_PARAMETER:
	case ERROR_INVALID_NAME:
		return SIR_FILE_INVALID_ARGUMENT;
	default:
		return SIR_FILE_IO_ERROR;
	}
}

static bool sirFileValidFlagsWindows( uint32_t nFlags ) noexcept
{
	const uint32_t nKnown = SIR_FILE_OPEN_READ | SIR_FILE_OPEN_WRITE | SIR_FILE_OPEN_CREATE | SIR_FILE_OPEN_TRUNCATE |
	                        SIR_FILE_OPEN_APPEND | SIR_FILE_OPEN_EXCLUSIVE;
	return ( ( nFlags & ~nKnown ) == 0 ) && ( ( nFlags & ( SIR_FILE_OPEN_READ | SIR_FILE_OPEN_WRITE ) ) != 0 ) &&
	       ( ( nFlags & ( SIR_FILE_OPEN_CREATE | SIR_FILE_OPEN_TRUNCATE | SIR_FILE_OPEN_APPEND ) ) == 0 ||
	         ( nFlags & SIR_FILE_OPEN_WRITE ) != 0 ) &&
	       ( ( nFlags & SIR_FILE_OPEN_EXCLUSIVE ) == 0 || ( nFlags & SIR_FILE_OPEN_CREATE ) != 0 ) &&
	       ( ( nFlags & SIR_FILE_OPEN_APPEND ) == 0 || ( nFlags & SIR_FILE_OPEN_TRUNCATE ) == 0 );
}

static bool sirFileConvertPathWindows( const char *pszUtf8Path, wchar_t *pszOutput, int nCapacity ) noexcept
{
	return pszUtf8Path != nullptr && pszUtf8Path[0] != '\0' &&
	       MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, pszUtf8Path, -1, pszOutput, nCapacity ) != 0;
}

static bool sirFileApplyOffsetWindows( uint64_t nBase, int64_t nDelta, uint64_t *pResult ) noexcept
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

// Each positional I/O has its own OVERLAPPED/event; no shared file pointer.
static sirFileResult_t sirFileTransferWindows( HANDLE hFile, uint64_t nOffset, void *pBuffer, DWORD nBytes, bool bWrite, bool bAppend,
                                               DWORD *pTransferred ) noexcept
{
	OVERLAPPED request = {};
	request.Offset = bAppend ? MAXDWORD : ( DWORD )nOffset;
	request.OffsetHigh = bAppend ? MAXDWORD : ( DWORD )( nOffset >> 32 );
	request.hEvent = CreateEventW( nullptr, TRUE, FALSE, nullptr );
	if ( request.hEvent == nullptr ) {
		return sirFileErrorWindows( GetLastError() );
	}
	DWORD nResultBytes = 0;
	BOOL bSuccess =
	    bWrite ? WriteFile( hFile, pBuffer, nBytes, &nResultBytes, &request ) : ReadFile( hFile, pBuffer, nBytes, &nResultBytes, &request );
	DWORD nError = bSuccess ? ERROR_SUCCESS : GetLastError();
	if ( !bSuccess && nError == ERROR_IO_PENDING ) {
		bSuccess = GetOverlappedResult( hFile, &request, &nResultBytes, TRUE );
		nError = bSuccess ? ERROR_SUCCESS : GetLastError();
	}
	CloseHandle( request.hEvent );
	if ( !bSuccess ) {
		return sirFileErrorWindows( nError );
	}
	*pTransferred = nResultBytes;
	return SIR_FILE_OK;
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
	if ( isOpen() || !sirFileValidFlagsWindows( nFlags ) ) {
		return SIR_FILE_INVALID_ARGUMENT;
	}
	wchar_t szPath[32768];
	if ( !sirFileConvertPathWindows( pszUtf8Path, szPath, 32768 ) ) {
		return SIR_FILE_INVALID_ARGUMENT;
	}
	DWORD nAccess = 0;
	if ( nFlags & SIR_FILE_OPEN_READ ) {
		nAccess |= GENERIC_READ;
	}
	if ( nFlags & SIR_FILE_OPEN_WRITE ) {
		nAccess |= GENERIC_WRITE;
	}
	DWORD nDisposition = OPEN_EXISTING;
	if ( nFlags & SIR_FILE_OPEN_EXCLUSIVE ) {
		nDisposition = CREATE_NEW;
	} else if ( nFlags & SIR_FILE_OPEN_CREATE ) {
		nDisposition = ( nFlags & SIR_FILE_OPEN_TRUNCATE ) ? CREATE_ALWAYS : OPEN_ALWAYS;
	} else if ( nFlags & SIR_FILE_OPEN_TRUNCATE ) {
		nDisposition = TRUNCATE_EXISTING;
	}
	HANDLE hFile = CreateFileW( szPath, nAccess, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, nDisposition,
	                            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr );
	if ( hFile == INVALID_HANDLE_VALUE ) {
		return sirFileErrorWindows( GetLastError() );
	}
	LARGE_INTEGER nSize = {};
	if ( GetFileType( hFile ) != FILE_TYPE_DISK || !GetFileSizeEx( hFile, &nSize ) || nSize.QuadPart < 0 ) {
		CloseHandle( hFile );
		return SIR_FILE_IO_ERROR;
	}
	m_nNativeHandle = ( uintptr_t )hFile;
	m_nCursor = ( nFlags & SIR_FILE_OPEN_APPEND ) ? ( uint64_t )nSize.QuadPart : 0;
	m_nFlags = nFlags;
	return SIR_FILE_OK;
}

sirFileResult_t SIRPlatformFile::close( void ) noexcept
{
	if ( !isOpen() ) {
		return SIR_FILE_OK;
	}
	HANDLE hFile = ( HANDLE )m_nNativeHandle;
	m_nNativeHandle = UINTPTR_MAX;
	m_nCursor = 0;
	m_nFlags = 0;
	return CloseHandle( hFile ) ? SIR_FILE_OK : sirFileErrorWindows( GetLastError() );
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
		DWORD nStep = ( DWORD )( nRemaining < 1024 * 1024 ? nRemaining : 1024 * 1024 );
		DWORD nRead = 0;
		sirFileResult_t eResult = sirFileTransferWindows( ( HANDLE )m_nNativeHandle, nOffset + *pTransferred, pOutput + *pTransferred,
		                                                  nStep, false, false, &nRead );
		if ( eResult == SIR_FILE_END_OF_FILE ) {
			return SIR_FILE_END_OF_FILE;
		}
		if ( eResult != SIR_FILE_OK ) {
			return eResult;
		}
		if ( nRead == 0 ) {
			return SIR_FILE_END_OF_FILE;
		}
		*pTransferred += nRead;
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
		DWORD nStep = ( DWORD )( nRemaining < 1024 * 1024 ? nRemaining : 1024 * 1024 );
		DWORD nWritten = 0;
		sirFileResult_t eResult = sirFileTransferWindows( ( HANDLE )m_nNativeHandle, nOffset + *pTransferred,
		                                                  ( void * )( pInput + *pTransferred ), nStep, true, false, &nWritten );
		if ( eResult != SIR_FILE_OK ) {
			return eResult;
		}
		if ( nWritten == 0 ) {
			return SIR_FILE_IO_ERROR;
		}
		*pTransferred += nWritten;
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
		DWORD nStep = ( DWORD )( nRemaining < 1024 * 1024 ? nRemaining : 1024 * 1024 );
		DWORD nWritten = 0;
		sirFileResult_t eResult =
		    sirFileTransferWindows( ( HANDLE )m_nNativeHandle, 0, ( void * )( pInput + *pTransferred ), nStep, true, true, &nWritten );
		if ( eResult != SIR_FILE_OK ) {
			return eResult;
		}
		if ( nWritten == 0 ) {
			return SIR_FILE_IO_ERROR;
		}
		*pTransferred += nWritten;
	}
	uint64_t nSize = 0;
	if ( getSize( &nSize ) == SIR_FILE_OK ) {
		m_nCursor = nSize;
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
	if ( !sirFileApplyOffsetWindows( nBase, nOffset, &nNext ) ) {
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
	LARGE_INTEGER nSize = {};
	if ( !GetFileSizeEx( ( HANDLE )m_nNativeHandle, &nSize ) ) {
		return sirFileErrorWindows( GetLastError() );
	}
	if ( nSize.QuadPart < 0 ) {
		return SIR_FILE_OUT_OF_RANGE;
	}
	*pBytes = ( uint64_t )nSize.QuadPart;
	return SIR_FILE_OK;
}

sirFileResult_t SIRPlatformFile::flush( void ) const noexcept
{
	if ( !isOpen() ) {
		return SIR_FILE_CLOSED;
	}
	if ( !( m_nFlags & SIR_FILE_OPEN_WRITE ) ) {
		return SIR_FILE_OK;
	}
	return FlushFileBuffers( ( HANDLE )m_nNativeHandle ) ? SIR_FILE_OK : sirFileErrorWindows( GetLastError() );
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
	FILE_END_OF_FILE_INFO nEndInfo = {};
	nEndInfo.EndOfFile.QuadPart = ( LONGLONG )nBytes;
	return SetFileInformationByHandle( ( HANDLE )m_nNativeHandle, FileEndOfFileInfo, &nEndInfo, sizeof( nEndInfo ) )
	           ? SIR_FILE_OK
	           : sirFileErrorWindows( GetLastError() );
}

sirFileResult_t SIRPlatformFile::getInfo( const char *pszUtf8Path, sirFileInfo_t *pInfo ) noexcept
{
	if ( pInfo == nullptr ) {
		return SIR_FILE_INVALID_ARGUMENT;
	}
	wchar_t szPath[32768];
	if ( !sirFileConvertPathWindows( pszUtf8Path, szPath, 32768 ) ) {
		return SIR_FILE_INVALID_ARGUMENT;
	}
	WIN32_FILE_ATTRIBUTE_DATA data = {};
	if ( !GetFileAttributesExW( szPath, GetFileExInfoStandard, &data ) ) {
		return sirFileErrorWindows( GetLastError() );
	}
	sirFileInfo_t info = {};
	info.bDirectory = ( data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0;
	info.bRegularFile = !info.bDirectory;
	info.nSizeBytes = ( ( uint64_t )data.nFileSizeHigh << 32 ) | data.nFileSizeLow;
	ULARGE_INTEGER time = {};
	time.LowPart = data.ftLastWriteTime.dwLowDateTime;
	time.HighPart = data.ftLastWriteTime.dwHighDateTime;
	const uint64_t nUnixEpoch = UINT64_C( 116444736000000000 );
	if ( time.QuadPart >= nUnixEpoch ) {
		const uint64_t nTicks = time.QuadPart - nUnixEpoch;
		if ( nTicks <= UINT64_MAX / 100 ) {
			info.nModifiedUnixNanoseconds = nTicks * 100;
		}
	}
	*pInfo = info;
	return SIR_FILE_OK;
}

sirFileResult_t SIRPlatformFile::remove( const char *pszUtf8Path ) noexcept
{
	wchar_t szPath[32768];
	if ( !sirFileConvertPathWindows( pszUtf8Path, szPath, 32768 ) ) {
		return SIR_FILE_INVALID_ARGUMENT;
	}
	return DeleteFileW( szPath ) ? SIR_FILE_OK : sirFileErrorWindows( GetLastError() );
}

sirFileResult_t SIRPlatformFile::renameReplace( const char *pszOldPath, const char *pszNewPath ) noexcept
{
	wchar_t szOld[32768];
	wchar_t szNew[32768];
	if ( !sirFileConvertPathWindows( pszOldPath, szOld, 32768 ) || !sirFileConvertPathWindows( pszNewPath, szNew, 32768 ) ) {
		return SIR_FILE_INVALID_ARGUMENT;
	}
	return MoveFileExW( szOld, szNew, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH ) ? SIR_FILE_OK
	                                                                                       : sirFileErrorWindows( GetLastError() );
}
