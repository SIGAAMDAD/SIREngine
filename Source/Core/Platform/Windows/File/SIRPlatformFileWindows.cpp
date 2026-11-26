/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Core/Platform/Shared/File/SIRPlatformFileSystem.h>
#include <Core/Platform/Shared/File/SIRPlatformFile.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

static sirFileResult sirFileErrorWindows( DWORD nError ) noexcept
{
	switch ( nError ) {
	case ERROR_FILE_NOT_FOUND:
	case ERROR_PATH_NOT_FOUND:
		return sirFileResult::SIR_FILE_NOT_FOUND;
	case ERROR_FILE_EXISTS:
	case ERROR_ALREADY_EXISTS:
		return sirFileResult::SIR_FILE_ALREADY_EXISTS;
	case ERROR_ACCESS_DENIED:
	case ERROR_SHARING_VIOLATION:
	case ERROR_LOCK_VIOLATION:
		return sirFileResult::SIR_FILE_ACCESS_DENIED;
	case ERROR_DISK_FULL:
	case ERROR_HANDLE_DISK_FULL:
		return sirFileResult::SIR_FILE_NO_SPACE;
	case ERROR_HANDLE_EOF:
		return sirFileResult::SIR_FILE_END_OF_FILE;
	case ERROR_NEGATIVE_SEEK:
	case ERROR_ARITHMETIC_OVERFLOW:
		return sirFileResult::SIR_FILE_OUT_OF_RANGE;
	case ERROR_INVALID_PARAMETER:
	case ERROR_INVALID_NAME:
		return sirFileResult::SIR_FILE_INVALID_ARGUMENT;
	default:
		return sirFileResult::SIR_FILE_IO_ERROR;
	}
}

static bool sirFileValidFlagsWindows( uint32_t nFlags ) noexcept
{
	const uint32_t nKnown = sirFileOpenFlag::SIR_FILE_OPEN_READ | sirFileOpenFlag::SIR_FILE_OPEN_WRITE | sirFileOpenFlag::SIR_FILE_OPEN_CREATE | sirFileOpenFlag::SIR_FILE_OPEN_TRUNCATE |
	                        sirFileOpenFlag::SIR_FILE_OPEN_APPEND | sirFileOpenFlag::SIR_FILE_OPEN_EXCLUSIVE;
	return ( ( nFlags & ~nKnown ) == 0 ) && ( ( nFlags & ( sirFileOpenFlag::SIR_FILE_OPEN_READ | sirFileOpenFlag::SIR_FILE_OPEN_WRITE ) ) != 0 ) &&
	       ( ( nFlags & ( sirFileOpenFlag::SIR_FILE_OPEN_CREATE | sirFileOpenFlag::SIR_FILE_OPEN_TRUNCATE | sirFileOpenFlag::SIR_FILE_OPEN_APPEND ) ) == 0 ||
	         ( nFlags & sirFileOpenFlag::SIR_FILE_OPEN_WRITE ) != 0 ) &&
	       ( ( nFlags & sirFileOpenFlag::SIR_FILE_OPEN_EXCLUSIVE ) == 0 || ( nFlags & sirFileOpenFlag::SIR_FILE_OPEN_CREATE ) != 0 ) &&
	       ( ( nFlags & sirFileOpenFlag::SIR_FILE_OPEN_APPEND ) == 0 || ( nFlags & sirFileOpenFlag::SIR_FILE_OPEN_TRUNCATE ) == 0 );
}

static bool sirFileConvertPathWindows( const char *pszUtf8Path, wchar_t *pszOutput, int nCapacity ) noexcept
{
	return pszUtf8Path != nullptr && pszUtf8Path[0] != '\0' &&
	       MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, pszUtf8Path, -1, pszOutput, nCapacity ) != 0;
}

static bool sirFileApplyOffsetWindows( uint64_t nBase, int64_t nDelta, uint64_t *pResult ) noexcept
{
	if ( nDelta >= 0 ) {
		if ( nBase > static_cast<uint64_t>( static_cast<INT64_MAX>( - static_cast ) )<uint64_t>(nDelta) ) {
			return false;
		}
		*pResult = nBase + static_cast<uint64_t>(nDelta);
		return true;
	}
	const uint64_t nMagnitude = static_cast<uint64_t>(  -( nDelta + 1 )  ) + 1;
	if ( nMagnitude > nBase ) {
		return false;
	}
	*pResult = nBase - nMagnitude;
	return true;
}

// Each positional I/O has its own OVERLAPPED/event; no shared file pointer.
static sirFileResult sirFileTransferWindows( HANDLE hFile, uint64_t nOffset, void *pBuffer, DWORD nBytes, bool bWrite, bool bAppend,
                                               DWORD *pTransferred ) noexcept
{
	OVERLAPPED request {};
	request.Offset = bAppend ? MAXDWORD : static_cast<DWORD>(nOffset);
	request.OffsetHigh = bAppend ? MAXDWORD : static_cast<DWORD>( nOffset >> 32 );
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
	return sirFileResult::SIR_FILE_OK;
}

SIRPlatformFile::SIRPlatformFile( void ) noexcept : m_nNativeHandle( UINTPTR_MAX ), m_nCursor( 0 ), m_nFlags( 0 )
{
}

SIRPlatformFile::SIRPlatformFile( SIRPlatformFile&& other ) noexcept
    : m_nNativeHandle( other.m_nNativeHandle ), m_nCursor( other.m_nCursor ), m_nFlags( other.m_nFlags )
{
	other.m_nNativeHandle = UINTPTR_MAX;
	other.m_nCursor = 0;
	other.m_nFlags = 0;
}

SIRPlatformFile::~SIRPlatformFile( void ) noexcept
{
	static_cast<void>( close() );
}

sirFileResult SIRPlatformFile::open( const char *pszUtf8Path, uint32_t nFlags ) noexcept
{
	if ( isOpen() || !sirFileValidFlagsWindows( nFlags ) ) {
		return sirFileResult::SIR_FILE_INVALID_ARGUMENT;
	}
	wchar_t szPath[32768];
	if ( !sirFileConvertPathWindows( pszUtf8Path, szPath, 32768 ) ) {
		return sirFileResult::SIR_FILE_INVALID_ARGUMENT;
	}
	DWORD nAccess = 0;
	if ( nFlags & sirFileOpenFlag::SIR_FILE_OPEN_READ ) {
		nAccess |= GENERIC_READ;
	}
	if ( nFlags & sirFileOpenFlag::SIR_FILE_OPEN_WRITE ) {
		nAccess |= GENERIC_WRITE;
	}
	DWORD nDisposition = OPEN_EXISTING;
	if ( nFlags & sirFileOpenFlag::SIR_FILE_OPEN_EXCLUSIVE ) {
		nDisposition = CREATE_NEW;
	} else if ( nFlags & sirFileOpenFlag::SIR_FILE_OPEN_CREATE ) {
		nDisposition = ( nFlags & sirFileOpenFlag::SIR_FILE_OPEN_TRUNCATE ) ? CREATE_ALWAYS : OPEN_ALWAYS;
	} else if ( nFlags & sirFileOpenFlag::SIR_FILE_OPEN_TRUNCATE ) {
		nDisposition = TRUNCATE_EXISTING;
	}
	HANDLE hFile = CreateFileW( szPath, nAccess, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, nDisposition,
	                            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr );
	if ( hFile == INVALID_HANDLE_VALUE ) {
		return sirFileErrorWindows( GetLastError() );
	}
	LARGE_INTEGER nSize {};
	if ( GetFileType( hFile ) != FILE_TYPE_DISK || !GetFileSizeEx( hFile, &nSize ) || nSize.QuadPart < 0 ) {
		CloseHandle( hFile );
		return sirFileResult::SIR_FILE_IO_ERROR;
	}
	m_nNativeHandle = static_cast<uintptr_t>(hFile);
	m_nCursor = ( nFlags & sirFileOpenFlag::SIR_FILE_OPEN_APPEND ) ? static_cast<uint64_t>(nSize.QuadPart) : 0;
	m_nFlags = nFlags;
	return sirFileResult::SIR_FILE_OK;
}

sirFileResult SIRPlatformFile::close( void ) noexcept
{
	if ( !isOpen() ) {
		return sirFileResult::SIR_FILE_OK;
	}
	HANDLE hFile = static_cast<HANDLE>(m_nNativeHandle);
	m_nNativeHandle = UINTPTR_MAX;
	m_nCursor = 0;
	m_nFlags = 0;
	return CloseHandle( hFile ) ? sirFileResult::SIR_FILE_OK : sirFileErrorWindows( GetLastError() );
}

bool SIRPlatformFile::isOpen( void ) const noexcept
{
	return m_nNativeHandle != UINTPTR_MAX;
}

sirFileResult SIRPlatformFile::readAt( uint64_t nOffset, void *pDestination, uint64_t nBytes, uint64_t *pTransferred ) const noexcept
{
	if ( pTransferred == nullptr ) {
		return sirFileResult::SIR_FILE_INVALID_ARGUMENT;
	}
	*pTransferred = 0;
	if ( !isOpen() ) {
		return sirFileResult::SIR_FILE_CLOSED;
	}
	if ( !( m_nFlags & sirFileOpenFlag::SIR_FILE_OPEN_READ ) ) {
		return sirFileResult::SIR_FILE_ACCESS_DENIED;
	}
	if ( nBytes != 0 && pDestination == nullptr ) {
		return sirFileResult::SIR_FILE_INVALID_ARGUMENT;
	}
	if ( nOffset > static_cast<uint64_t>(INT64_MAX) || nBytes > static_cast<uint64_t>( static_cast<INT64_MAX>( - nOffset ) ) ) {
		return sirFileResult::SIR_FILE_OUT_OF_RANGE;
	}
	uint8_t *pOutput = static_cast<uint8_t *>(pDestination);
	while ( *pTransferred < nBytes ) {
		uint64_t nRemaining = nBytes - *pTransferred;
		DWORD nStep = static_cast<DWORD>( nRemaining < 1024 * 1024 ? nRemaining : 1024 * 1024 );
		DWORD nRead = 0;
		sirFileResult eResult = sirFileTransferWindows( static_cast<HANDLE>(m_nNativeHandle), nOffset + *pTransferred, pOutput + *pTransferred,
		                                                  nStep, false, false, &nRead );
		if ( eResult == sirFileResult::SIR_FILE_END_OF_FILE ) {
			return sirFileResult::SIR_FILE_END_OF_FILE;
		}
		if ( eResult != sirFileResult::SIR_FILE_OK ) {
			return eResult;
		}
		if ( nRead == 0 ) {
			return sirFileResult::SIR_FILE_END_OF_FILE;
		}
		*pTransferred += nRead;
	}
	return sirFileResult::SIR_FILE_OK;
}

sirFileResult SIRPlatformFile::writeAt( uint64_t nOffset, const void *pSource, uint64_t nBytes, uint64_t *pTransferred ) const noexcept
{
	if ( pTransferred == nullptr ) {
		return sirFileResult::SIR_FILE_INVALID_ARGUMENT;
	}
	*pTransferred = 0;
	if ( !isOpen() ) {
		return sirFileResult::SIR_FILE_CLOSED;
	}
	if ( !( m_nFlags & sirFileOpenFlag::SIR_FILE_OPEN_WRITE ) ) {
		return sirFileResult::SIR_FILE_ACCESS_DENIED;
	}
	if ( m_nFlags & sirFileOpenFlag::SIR_FILE_OPEN_APPEND ) {
		return sirFileResult::SIR_FILE_INVALID_ARGUMENT;
	}
	if ( nBytes != 0 && pSource == nullptr ) {
		return sirFileResult::SIR_FILE_INVALID_ARGUMENT;
	}
	if ( nOffset > static_cast<uint64_t>(INT64_MAX) || nBytes > static_cast<uint64_t>( static_cast<INT64_MAX>( - nOffset ) ) ) {
		return sirFileResult::SIR_FILE_OUT_OF_RANGE;
	}
	const uint8_t *pInput = static_cast<const uint8_t *>(pSource);
	while ( *pTransferred < nBytes ) {
		uint64_t nRemaining = nBytes - *pTransferred;
		DWORD nStep = static_cast<DWORD>( nRemaining < 1024 * 1024 ? nRemaining : 1024 * 1024 );
		DWORD nWritten = 0;
		sirFileResult eResult = sirFileTransferWindows( static_cast<HANDLE>(m_nNativeHandle), nOffset + *pTransferred,
		                                                  static_cast<void *>( pInput + *pTransferred ), nStep, true, false, &nWritten );
		if ( eResult != sirFileResult::SIR_FILE_OK ) {
			return eResult;
		}
		if ( nWritten == 0 ) {
			return sirFileResult::SIR_FILE_IO_ERROR;
		}
		*pTransferred += nWritten;
	}
	return sirFileResult::SIR_FILE_OK;
}

sirFileResult SIRPlatformFile::read( void *pDestination, uint64_t nBytes, uint64_t *pTransferred ) noexcept
{
	sirFileResult eResult = readAt( m_nCursor, pDestination, nBytes, pTransferred );
	if ( pTransferred != nullptr ) {
		m_nCursor += *pTransferred;
	}
	return eResult;
}

sirFileResult SIRPlatformFile::write( const void *pSource, uint64_t nBytes, uint64_t *pTransferred ) noexcept
{
	if ( !( m_nFlags & sirFileOpenFlag::SIR_FILE_OPEN_APPEND ) ) {
		sirFileResult eResult = writeAt( m_nCursor, pSource, nBytes, pTransferred );
		if ( pTransferred != nullptr ) {
			m_nCursor += *pTransferred;
		}
		return eResult;
	}
	if ( pTransferred == nullptr ) {
		return sirFileResult::SIR_FILE_INVALID_ARGUMENT;
	}
	*pTransferred = 0;
	if ( !isOpen() ) {
		return sirFileResult::SIR_FILE_CLOSED;
	}
	if ( nBytes != 0 && pSource == nullptr ) {
		return sirFileResult::SIR_FILE_INVALID_ARGUMENT;
	}
	const uint8_t *pInput = static_cast<const uint8_t *>(pSource);
	while ( *pTransferred < nBytes ) {
		uint64_t nRemaining = nBytes - *pTransferred;
		DWORD nStep = static_cast<DWORD>( nRemaining < 1024 * 1024 ? nRemaining : 1024 * 1024 );
		DWORD nWritten = 0;
		sirFileResult eResult =
		    sirFileTransferWindows( static_cast<HANDLE>(m_nNativeHandle), 0, static_cast<void *>( pInput + *pTransferred ), nStep, true, true, &nWritten );
		if ( eResult != sirFileResult::SIR_FILE_OK ) {
			return eResult;
		}
		if ( nWritten == 0 ) {
			return sirFileResult::SIR_FILE_IO_ERROR;
		}
		*pTransferred += nWritten;
	}
	uint64_t nSize = 0;
	if ( getSize( &nSize ) == sirFileResult::SIR_FILE_OK ) {
		m_nCursor = nSize;
	}
	return sirFileResult::SIR_FILE_OK;
}

sirFileResult SIRPlatformFile::seek( int64_t nOffset, sirFileSeekOrigin eOrigin, uint64_t *pPosition ) noexcept
{
	if ( !isOpen() ) {
		return sirFileResult::SIR_FILE_CLOSED;
	}
	uint64_t nBase = 0;
	if ( eOrigin == sirFileSeekOrigin::SIR_FILE_SEEK_CURRENT ) {
		nBase = m_nCursor;
	} else if ( eOrigin == sirFileSeekOrigin::SIR_FILE_SEEK_END ) {
		sirFileResult eResult = getSize( &nBase );
		if ( eResult != sirFileResult::SIR_FILE_OK ) {
			return eResult;
		}
	} else if ( eOrigin != sirFileSeekOrigin::SIR_FILE_SEEK_BEGIN )
		return sirFileResult::SIR_FILE_INVALID_ARGUMENT;
	uint64_t nNext = 0;
	if ( !sirFileApplyOffsetWindows( nBase, nOffset, &nNext ) ) {
		return sirFileResult::SIR_FILE_OUT_OF_RANGE;
	}
	m_nCursor = nNext;
	if ( pPosition != nullptr ) {
		*pPosition = nNext;
	}
	return sirFileResult::SIR_FILE_OK;
}

sirFileResult SIRPlatformFile::tell( uint64_t *pPosition ) const noexcept
{
	if ( pPosition == nullptr ) {
		return sirFileResult::SIR_FILE_INVALID_ARGUMENT;
	}
	if ( !isOpen() ) {
		return sirFileResult::SIR_FILE_CLOSED;
	}
	*pPosition = m_nCursor;
	return sirFileResult::SIR_FILE_OK;
}

sirFileResult SIRPlatformFile::getSize( uint64_t *pBytes ) const noexcept
{
	if ( pBytes == nullptr ) {
		return sirFileResult::SIR_FILE_INVALID_ARGUMENT;
	}
	if ( !isOpen() ) {
		return sirFileResult::SIR_FILE_CLOSED;
	}
	LARGE_INTEGER nSize {};
	if ( !GetFileSizeEx( static_cast<HANDLE>(m_nNativeHandle), &nSize ) ) {
		return sirFileErrorWindows( GetLastError() );
	}
	if ( nSize.QuadPart < 0 ) {
		return sirFileResult::SIR_FILE_OUT_OF_RANGE;
	}
	*pBytes = static_cast<uint64_t>(nSize.QuadPart);
	return sirFileResult::SIR_FILE_OK;
}

sirFileResult SIRPlatformFile::flush( void ) const noexcept
{
	if ( !isOpen() ) {
		return sirFileResult::SIR_FILE_CLOSED;
	}
	if ( !( m_nFlags & sirFileOpenFlag::SIR_FILE_OPEN_WRITE ) ) {
		return sirFileResult::SIR_FILE_OK;
	}
	return FlushFileBuffers( static_cast<HANDLE>(m_nNativeHandle) ) ? sirFileResult::SIR_FILE_OK : sirFileErrorWindows( GetLastError() );
}

sirFileResult SIRPlatformFile::truncate( uint64_t nBytes ) noexcept
{
	if ( !isOpen() ) {
		return sirFileResult::SIR_FILE_CLOSED;
	}
	if ( !( m_nFlags & sirFileOpenFlag::SIR_FILE_OPEN_WRITE ) ) {
		return sirFileResult::SIR_FILE_ACCESS_DENIED;
	}
	if ( nBytes > static_cast<uint64_t>(INT64_MAX) ) {
		return sirFileResult::SIR_FILE_OUT_OF_RANGE;
	}
	FILE_END_OF_FILE_INFO nEndInfo {};
	nEndInfo.EndOfFile.QuadPart = static_cast<LONGLONG>(nBytes);
	return SetFileInformationByHandle( static_cast<HANDLE>(m_nNativeHandle), FileEndOfFileInfo, &nEndInfo, sizeof( nEndInfo ) )
	           ? sirFileResult::SIR_FILE_OK
			   : sirFileErrorWindows( GetLastError() );
}

sirFileResult SIRPlatformFileSystem::getFileInfo( const char *pszUtf8Path, sirFileInfo_t *pInfo ) noexcept
{
	if ( pInfo == nullptr ) {
		return sirFileResult::SIR_FILE_INVALID_ARGUMENT;
	}
	wchar_t szPath[32768];
	if ( !sirFileConvertPathWindows( pszUtf8Path, szPath, 32768 ) ) {
		return sirFileResult::SIR_FILE_INVALID_ARGUMENT;
	}
	WIN32_FILE_ATTRIBUTE_DATA data {};
	if ( !GetFileAttributesExW( szPath, GetFileExInfoStandard, &data ) ) {
		return sirFileErrorWindows( GetLastError() );
	}
	sirFileInfo_t info {};
	info.bDirectory = ( data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0;
	info.bRegularFile = !info.bDirectory;
	info.nSizeBytes = ( static_cast<uint64_t>(data.nFileSizeHigh) << 32 ) | data.nFileSizeLow;
	ULARGE_INTEGER time {};
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
	return sirFileResult::SIR_FILE_OK;
}

sirFileResult SIRPlatformFileSystem::removeFile( const char *pszUtf8Path ) noexcept
{
	wchar_t szPath[32768];
	if ( !sirFileConvertPathWindows( pszUtf8Path, szPath, 32768 ) ) {
		return sirFileResult::SIR_FILE_INVALID_ARGUMENT;
	}
	return DeleteFileW( szPath ) ? sirFileResult::SIR_FILE_OK : sirFileErrorWindows( GetLastError() );
}

sirFileResult SIRPlatformFileSystem::renameFileReplace( const char *pszOldPath, const char *pszNewPath ) noexcept
{
	wchar_t szOld[32768];
	wchar_t szNew[32768];
	if ( !sirFileConvertPathWindows( pszOldPath, szOld, 32768 ) || !sirFileConvertPathWindows( pszNewPath, szNew, 32768 ) ) {
		return sirFileResult::SIR_FILE_INVALID_ARGUMENT;
	}
	return MoveFileExW( szOld, szNew, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH ) ? sirFileResult::SIR_FILE_OK
	                                                                                       : sirFileErrorWindows( GetLastError() );
}
