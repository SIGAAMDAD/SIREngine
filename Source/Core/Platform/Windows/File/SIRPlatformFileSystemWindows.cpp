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

#include <Core/Platform/Shared/File/SIRPlatformFileSystem.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <stddef.h>
#include <string.h>
#include <wchar.h>

static sirFileSystemResult sirWindowsFileSystemError( DWORD nError ) noexcept
{
	switch ( nError ) {
	case ERROR_FILE_NOT_FOUND:
	case ERROR_PATH_NOT_FOUND:
	case ERROR_INVALID_NAME:
		return sirFileSystemResult::SIR_FILESYSTEM_NOT_FOUND;
	case ERROR_ALREADY_EXISTS:
	case ERROR_FILE_EXISTS:
		return sirFileSystemResult::SIR_FILESYSTEM_ALREADY_EXISTS;
	case ERROR_ACCESS_DENIED:
	case ERROR_SHARING_VIOLATION:
		return sirFileSystemResult::SIR_FILESYSTEM_ACCESS_DENIED;
	case ERROR_FILENAME_EXCED_RANGE:
	case ERROR_BUFFER_OVERFLOW:
		return sirFileSystemResult::SIR_FILESYSTEM_BUFFER_TOO_SMALL;
	default:
		return sirFileSystemResult::SIR_FILESYSTEM_IO_ERROR;
	}
}

static bool sirWindowsPathToWide( const char *pszPath, wchar_t *pszOutput, int nCapacity ) noexcept
{
	return pszPath != nullptr && pszPath[0] != '\0' &&
	       MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, pszPath, -1, pszOutput, nCapacity ) > 0;
}

static uint64_t sirWindowsFileTimeNanoseconds( const FILETIME& rTime ) noexcept
{
	const uint64_t nTicks = ( static_cast<uint64_t>(rTime.dwHighDateTime) << 32 ) | static_cast<uint64_t>(rTime.dwLowDateTime);
	return nTicks >= 116444736000000000ULL ? ( nTicks - 116444736000000000ULL ) * 100ULL : 0;
}

static void sirWindowsFillFileInfo( DWORD nAttributes, DWORD nSizeHigh, DWORD nSizeLow, const FILETIME& rTime,
                                    sirFileSystemInfo_t *pInfo ) noexcept
{
	pInfo->nSizeBytes = ( static_cast<uint64_t>(nSizeHigh) << 32 ) | nSizeLow;
	pInfo->nModifiedNanoseconds = sirWindowsFileTimeNanoseconds( rTime );
	pInfo->eType = ( nAttributes & FILE_ATTRIBUTE_REPARSE_POINT ) != 0 ? sirFileSystemEntryType::SIR_FILESYSTEM_SYMLINK
	               : ( nAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0   ? sirFileSystemEntryType::SIR_FILESYSTEM_DIRECTORY
	                                                                   : sirFileSystemEntryType::SIR_FILESYSTEM_REGULAR_FILE;
}

sirFileSystemResult SIRPlatformFileSystem::query( const char *pszPath, sirFileSystemInfo_t *pInfo ) noexcept
{
	if ( pszPath == nullptr || pszPath[0] == '\0' || pInfo == nullptr ) {
		return sirFileSystemResult::SIR_FILESYSTEM_INVALID_ARGUMENT;
	}
	wchar_t szPath[32768];
	if ( !sirWindowsPathToWide( pszPath, szPath, 32768 ) ) {
		return sirFileSystemResult::SIR_FILESYSTEM_INVALID_ARGUMENT;
	}
	WIN32_FILE_ATTRIBUTE_DATA data{};
	if ( !GetFileAttributesExW( szPath, GetFileExInfoStandard, &data ) ) {
		return sirWindowsFileSystemError( GetLastError() );
	}
	sirWindowsFillFileInfo( data.dwFileAttributes, data.nFileSizeHigh, data.nFileSizeLow, data.ftLastWriteTime, pInfo );
	return sirFileSystemResult::SIR_FILESYSTEM_OK;
}

sirFileSystemResult SIRPlatformFileSystem::createDirectory( const char *pszPath, bool bRecursive ) noexcept
{
	if ( pszPath == nullptr || pszPath[0] == '\0' ) {
		return sirFileSystemResult::SIR_FILESYSTEM_INVALID_ARGUMENT;
	}
	wchar_t szPath[32768];
	if ( !sirWindowsPathToWide( pszPath, szPath, 32768 ) ) {
		return sirFileSystemResult::SIR_FILESYSTEM_INVALID_ARGUMENT;
	}
	if ( !bRecursive ) {
		return CreateDirectoryW( szPath, nullptr ) ? sirFileSystemResult::SIR_FILESYSTEM_OK : sirWindowsFileSystemError( GetLastError() );
	}
	const size_t nLength = wcslen( szPath );
	size_t nRoot = 0;
	if ( nLength >= 2 && szPath[1] == L':' ) {
		nRoot = nLength >= 3 && ( szPath[2] == L'\\' || szPath[2] == L'/' ) ? 3 : 2;
	} else if ( nLength > 2 && szPath[0] == L'\\' && szPath[1] == L'\\' ) {
		// Skip UNC server and share prefixes; those are not creatable directories.
		size_t i = 2;
		for ( uint32_t nPart = 0; nPart < 2 && i < nLength; ++nPart ) {
			while ( i < nLength && szPath[i] != L'\\' && szPath[i] != L'/' ) {
				++i;
			}
			if ( i < nLength ) {
				++i;
			}
		}
		nRoot = i;
	}
	for ( size_t i = nRoot; i <= nLength; ++i ) {
		if ( szPath[i] != L'\\' && szPath[i] != L'/' && szPath[i] != L'\0' ) {
			continue;
		}
		if ( i == nRoot || ( i > 0 && ( szPath[i - 1] == L'\\' || szPath[i - 1] == L'/' ) ) ) {
			continue;
		}
		const wchar_t cSaved = szPath[i];
		szPath[i] = L'\0';
		if ( !CreateDirectoryW( szPath, nullptr ) ) {
			const DWORD nError = GetLastError();
			if ( nError != ERROR_ALREADY_EXISTS ) {
				return sirWindowsFileSystemError( nError );
			}
			const DWORD nAttributes = GetFileAttributesW( szPath );
			if ( nAttributes == INVALID_FILE_ATTRIBUTES || ( nAttributes & FILE_ATTRIBUTE_DIRECTORY ) == 0 ) {
				return sirFileSystemResult::SIR_FILESYSTEM_ALREADY_EXISTS;
			}
		}
		szPath[i] = cSaved;
	}
	return sirFileSystemResult::SIR_FILESYSTEM_OK;
}

sirFileSystemResult SIRPlatformFileSystem::removeDirectory( const char *pszPath ) noexcept
{
	if ( pszPath == nullptr || pszPath[0] == '\0' ) {
		return sirFileSystemResult::SIR_FILESYSTEM_INVALID_ARGUMENT;
	}
	wchar_t szPath[32768];
	if ( !sirWindowsPathToWide( pszPath, szPath, 32768 ) ) {
		return sirFileSystemResult::SIR_FILESYSTEM_INVALID_ARGUMENT;
	}
	return RemoveDirectoryW( szPath ) ? sirFileSystemResult::SIR_FILESYSTEM_OK : sirWindowsFileSystemError( GetLastError() );
}

sirFileSystemResult SIRPlatformFileSystem::renameReplace( const char *pszOldPath, const char *pszNewPath ) noexcept
{
	if ( pszOldPath == nullptr || pszNewPath == nullptr || pszOldPath[0] == '\0' || pszNewPath[0] == '\0' ) {
		return sirFileSystemResult::SIR_FILESYSTEM_INVALID_ARGUMENT;
	}
	wchar_t szOldPath[32768];
	wchar_t szNewPath[32768];
	if ( !sirWindowsPathToWide( pszOldPath, szOldPath, 32768 ) || !sirWindowsPathToWide( pszNewPath, szNewPath, 32768 ) ) {
		return sirFileSystemResult::SIR_FILESYSTEM_INVALID_ARGUMENT;
	}
	return MoveFileExW( szOldPath, szNewPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH )
	           ? sirFileSystemResult::SIR_FILESYSTEM_OK
			   : sirWindowsFileSystemError( GetLastError() );
}

sirFileSystemResult SIRPlatformFileSystem::enumerate( const char *pszDirectory, sirFileSystemVisitor_t pVisitor, void *pContext ) noexcept
{
	if ( pszDirectory == nullptr || pszDirectory[0] == '\0' || pVisitor == nullptr ) {
		return sirFileSystemResult::SIR_FILESYSTEM_INVALID_ARGUMENT;
	}
	wchar_t szPath[32768];
	if ( !sirWindowsPathToWide( pszDirectory, szPath, 32768 ) ) {
		return sirFileSystemResult::SIR_FILESYSTEM_INVALID_ARGUMENT;
	}
	size_t nLength = wcslen( szPath );
	if ( nLength + 3 >= 32768 ) {
		return sirFileSystemResult::SIR_FILESYSTEM_BUFFER_TOO_SMALL;
	}
	if ( szPath[nLength - 1] != L'\\' && szPath[nLength - 1] != L'/' ) {
		szPath[nLength++] = L'\\';
	}
	szPath[nLength++] = L'*';
	szPath[nLength] = L'\0';
	WIN32_FIND_DATAW entry{};
	HANDLE hFind = FindFirstFileW( szPath, &entry );
	if ( hFind == INVALID_HANDLE_VALUE ) {
		return sirWindowsFileSystemError( GetLastError() );
	}
	sirFileSystemResult eResult = sirFileSystemResult::SIR_FILESYSTEM_OK;
	do {
		if ( wcscmp( entry.cFileName, L"." ) == 0 || wcscmp( entry.cFileName, L".." ) == 0 ) {
			continue;
		}
		char szName[2048];
		if ( WideCharToMultiByte( CP_UTF8, WC_ERR_INVALID_CHARS, entry.cFileName, -1, szName, static_cast<int>(sizeof( szName ), nullptr, nullptr )) ==
		     0 ) {
			eResult = sirFileSystemResult::SIR_FILESYSTEM_BUFFER_TOO_SMALL;
			break;
		}
		sirFileSystemInfo_t info{};
		sirWindowsFillFileInfo( entry.dwFileAttributes, entry.nFileSizeHigh, entry.nFileSizeLow, entry.ftLastWriteTime, &info );
		if ( !pVisitor( szName, &info, pContext ) ) {
			eResult = sirFileSystemResult::SIR_FILESYSTEM_STOPPED;
			break;
		}
	} while ( FindNextFileW( hFind, &entry ) );
	if ( eResult == sirFileSystemResult::SIR_FILESYSTEM_OK && GetLastError() != ERROR_NO_MORE_FILES ) {
		eResult = sirWindowsFileSystemError( GetLastError() );
	}
	FindClose( hFind );
	return eResult;
}

// Core-owned atomic scene replacement and bounded regular-file reads.
static sirPlatformAtomicResult sirFileSystemCheckExpected( const wchar_t *pszDestination,
                                                             const sirFileSystemAtomicWriteRequest_t& request ) noexcept
{
	if ( request.pExpectedBytes == nullptr ) {
		return sirPlatformAtomicResult::SIR_ATOMIC_SAVE_OK;
	}
	HANDLE hFile = CreateFileW( pszDestination, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
	                            OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr );
	if ( hFile == INVALID_HANDLE_VALUE ) {
		return GetLastError() == ERROR_FILE_NOT_FOUND ? sirPlatformAtomicResult::SIR_ATOMIC_SAVE_CONFLICT : sirPlatformAtomicResult::SIR_ATOMIC_SAVE_IO_ERROR;
	}
	BY_HANDLE_FILE_INFORMATION information {};
	LARGE_INTEGER nLength {};
	if ( !GetFileInformationByHandle( hFile, &information ) || !GetFileSizeEx( hFile, &nLength ) ) {
		CloseHandle( hFile );
		return sirPlatformAtomicResult::SIR_ATOMIC_SAVE_IO_ERROR;
	}
	if ( ( information.dwFileAttributes & ( FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DEVICE ) ) != 0 ) {
		CloseHandle( hFile );
		return sirPlatformAtomicResult::SIR_ATOMIC_SAVE_INVALID_PATH;
	}
	if ( nLength.QuadPart < 0 || static_cast<uint64_t>(nLength.QuadPart) != static_cast<uint64_t>(request.nExpectedBytes) ) {
		CloseHandle( hFile );
		return sirPlatformAtomicResult::SIR_ATOMIC_SAVE_CONFLICT;
	}
	char szChunk[4096];
	memset( szChunk, 0, sizeof( szChunk ) );
	size_t nRead = 0;
	while ( nRead < request.nExpectedBytes ) {
		const size_t nRemaining = request.nExpectedBytes - nRead;
		const DWORD nChunk = nRemaining < sizeof( szChunk ) ? static_cast<DWORD>(nRemaining) : static_cast<DWORD>(sizeof( szChunk ));
		DWORD nActual = 0;
		if ( !ReadFile( hFile, szChunk, nChunk, &nActual, nullptr ) || nActual == 0 ) {
			CloseHandle( hFile );
			return sirPlatformAtomicResult::SIR_ATOMIC_SAVE_IO_ERROR;
		}
		if ( memcmp( szChunk, request.pExpectedBytes + nRead, nActual ) != 0 ) {
			CloseHandle( hFile );
			return sirPlatformAtomicResult::SIR_ATOMIC_SAVE_CONFLICT;
		}
		nRead += nActual;
	}
	return CloseHandle( hFile ) ? sirPlatformAtomicResult::SIR_ATOMIC_SAVE_OK : sirPlatformAtomicResult::SIR_ATOMIC_SAVE_IO_ERROR;
}

static bool sirFileSystemToWindowsPath( const char *pszUtf8, wchar_t *pWide, int nCapacity ) noexcept
{
	const int nConverted = MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, pszUtf8, -1, pWide, nCapacity );
	return nConverted != 0;
}

sirFileSystemResult SIRPlatformFileSystem::readFilePlatform( const char *pszPath, char *pszOutput, size_t nCapacity,
                                                               size_t *pOutBytes ) noexcept
{
	wchar_t szPath[4096];
	memset( szPath, 0, sizeof( szPath ) );
	if ( !sirFileSystemToWindowsPath( pszPath, szPath, 4096 ) ) {
		return sirFileSystemResult::SIR_FILESYSTEM_INVALID_PATH;
	}
	HANDLE hFile = CreateFileW( szPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
	                            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr );
	if ( hFile == INVALID_HANDLE_VALUE ) {
		return sirFileSystemResult::SIR_FILESYSTEM_IO_ERROR;
	}
	BY_HANDLE_FILE_INFORMATION information {};
	LARGE_INTEGER nLength {};
	if ( !GetFileInformationByHandle( hFile, &information ) || !GetFileSizeEx( hFile, &nLength ) ) {
		CloseHandle( hFile );
		return sirFileSystemResult::SIR_FILESYSTEM_IO_ERROR;
	}
	if ( ( information.dwFileAttributes & ( FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_DEVICE | FILE_ATTRIBUTE_REPARSE_POINT ) ) != 0 ||
	     nLength.QuadPart < 0 ) {
		CloseHandle( hFile );
		return sirFileSystemResult::SIR_FILESYSTEM_INVALID_PATH;
	}
	if ( static_cast<uint64_t>(nLength.QuadPart) > SIZE_MAX ) {
		CloseHandle( hFile );
		return sirFileSystemResult::SIR_FILESYSTEM_OUT_OF_CAPACITY;
	}
	const size_t nFileBytes = static_cast<size_t>(nLength.QuadPart);
	*pOutBytes = nFileBytes;
	if ( pszOutput == nullptr ) {
		return CloseHandle( hFile ) ? sirFileSystemResult::SIR_FILESYSTEM_OK : sirFileSystemResult::SIR_FILESYSTEM_IO_ERROR;
	}
	if ( nCapacity < nFileBytes ) {
		CloseHandle( hFile );
		return sirFileSystemResult::SIR_FILESYSTEM_OUT_OF_CAPACITY;
	}
	size_t nRead = 0;
	while ( nRead < nFileBytes ) {
		const size_t nRemaining = nFileBytes - nRead;
		const DWORD nChunk = nRemaining > 1048576 ? 1048576 : static_cast<DWORD>(nRemaining);
		DWORD nActual = 0;
		if ( !ReadFile( hFile, pszOutput + nRead, nChunk, &nActual, nullptr ) || nActual == 0 ) {
			CloseHandle( hFile );
			return sirFileSystemResult::SIR_FILESYSTEM_IO_ERROR;
		}
		nRead += nActual;
	}
	char szExtra[1];
	memset( szExtra, 0, sizeof( szExtra ) );
	DWORD nExtra = 0;
	const bool bComplete = ReadFile( hFile, szExtra, 1, &nExtra, nullptr ) != 0 && nExtra == 0;
	const bool bClosed = CloseHandle( hFile ) != 0;
	return bComplete && bClosed ? sirFileSystemResult::SIR_FILESYSTEM_OK : sirFileSystemResult::SIR_FILESYSTEM_IO_ERROR;
}

sirPlatformAtomicResult SIRPlatformFileSystem::writeAtomicallyPlatform( const sirFileSystemAtomicWriteRequest_t& request ) noexcept
{
	wchar_t szDestination[4096];
	memset( szDestination, 0, sizeof( szDestination ) );
	wchar_t szTemporary[4096];
	memset( szTemporary, 0, sizeof( szTemporary ) );
	if ( !sirFileSystemToWindowsPath( request.pszDestination, szDestination, 4096 ) ||
	     !sirFileSystemToWindowsPath( request.pszTemporary, szTemporary, 4096 ) ) {
		return sirPlatformAtomicResult::SIR_ATOMIC_SAVE_INVALID_PATH;
	}
	const DWORD nDestinationAttributes = GetFileAttributesW( szDestination );
	if ( nDestinationAttributes != INVALID_FILE_ATTRIBUTES ) {
		if ( ( nDestinationAttributes & FILE_ATTRIBUTE_REPARSE_POINT ) != 0 ) {
			return sirPlatformAtomicResult::SIR_ATOMIC_SAVE_INVALID_PATH;
		}
		if ( ( nDestinationAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0 ) {
			return sirPlatformAtomicResult::SIR_ATOMIC_SAVE_REPLACE_FAILED;
		}
		if ( ( nDestinationAttributes & FILE_ATTRIBUTE_DEVICE ) != 0 ) {
			return sirPlatformAtomicResult::SIR_ATOMIC_SAVE_INVALID_PATH;
		}
	} else if ( GetLastError() != ERROR_FILE_NOT_FOUND ) {
		return sirPlatformAtomicResult::SIR_ATOMIC_SAVE_IO_ERROR;
	}

	const sirPlatformAtomicResult eExpected = sirFileSystemCheckExpected( szDestination, request );
	if ( eExpected != sirPlatformAtomicResult::SIR_ATOMIC_SAVE_OK ) {
		return eExpected;
	}
	HANDLE hFile = CreateFileW( szTemporary, GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr );
	if ( hFile == INVALID_HANDLE_VALUE ) {
		const DWORD nError = GetLastError();
		return ( nError == ERROR_FILE_EXISTS || nError == ERROR_ALREADY_EXISTS ) ? sirPlatformAtomicResult::SIR_ATOMIC_SAVE_TEMP_EXISTS : sirPlatformAtomicResult::SIR_ATOMIC_SAVE_IO_ERROR;
	}
	bool bSuccess = true;
	size_t nWritten = 0;
	while ( nWritten < request.nBytes ) {
		const size_t nRemaining = request.nBytes - nWritten;
		const DWORD nChunk = nRemaining > 1048576 ? 1048576 : static_cast<DWORD>(nRemaining);
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
		return sirPlatformAtomicResult::SIR_ATOMIC_SAVE_IO_ERROR;
	}
	const sirPlatformAtomicResult eRecheck = sirFileSystemCheckExpected( szDestination, request );
	if ( eRecheck != sirPlatformAtomicResult::SIR_ATOMIC_SAVE_OK ) {
		DeleteFileW( szTemporary );
		return eRecheck;
	}
	if ( !MoveFileExW( szTemporary, szDestination, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH ) ) {
		DeleteFileW( szTemporary );
		return sirPlatformAtomicResult::SIR_ATOMIC_SAVE_REPLACE_FAILED;
	}
	return sirPlatformAtomicResult::SIR_ATOMIC_SAVE_OK;
}
