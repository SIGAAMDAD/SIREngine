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

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <stddef.h>
#include <string.h>
#include <wchar.h>

static sirFileSystemResult_t sirWindowsFileSystemError( DWORD nError ) noexcept
{
	switch ( nError ) {
	case ERROR_FILE_NOT_FOUND:
	case ERROR_PATH_NOT_FOUND:
	case ERROR_INVALID_NAME:
		return SIR_FILESYSTEM_NOT_FOUND;
	case ERROR_ALREADY_EXISTS:
	case ERROR_FILE_EXISTS:
		return SIR_FILESYSTEM_ALREADY_EXISTS;
	case ERROR_ACCESS_DENIED:
	case ERROR_SHARING_VIOLATION:
		return SIR_FILESYSTEM_ACCESS_DENIED;
	case ERROR_FILENAME_EXCED_RANGE:
	case ERROR_BUFFER_OVERFLOW:
		return SIR_FILESYSTEM_BUFFER_TOO_SMALL;
	default:
		return SIR_FILESYSTEM_IO_ERROR;
	}
}

static bool sirWindowsPathToWide( const char *pszPath, wchar_t *pszOutput, int nCapacity ) noexcept
{
	return pszPath != nullptr && pszPath[0] != '\0' &&
	       MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, pszPath, -1, pszOutput, nCapacity ) > 0;
}

static uint64_t sirWindowsFileTimeNanoseconds( const FILETIME &rTime ) noexcept
{
	const uint64_t nTicks = ( ( uint64_t )rTime.dwHighDateTime << 32 ) | ( uint64_t )rTime.dwLowDateTime;
	return nTicks >= 116444736000000000ULL ? ( nTicks - 116444736000000000ULL ) * 100ULL : 0;
}

static void sirWindowsFillFileInfo( DWORD nAttributes, DWORD nSizeHigh, DWORD nSizeLow, const FILETIME &rTime,
                                    sirFileSystemInfo_t *pInfo ) noexcept
{
	pInfo->nSizeBytes = ( ( uint64_t )nSizeHigh << 32 ) | nSizeLow;
	pInfo->nModifiedNanoseconds = sirWindowsFileTimeNanoseconds( rTime );
	pInfo->eType = ( nAttributes & FILE_ATTRIBUTE_REPARSE_POINT ) != 0 ? SIR_FILESYSTEM_SYMLINK
	               : ( nAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0   ? SIR_FILESYSTEM_DIRECTORY
	                                                                   : SIR_FILESYSTEM_REGULAR_FILE;
}

sirFileSystemResult_t SIRPlatformFileSystem::query( const char *pszPath, sirFileSystemInfo_t *pInfo ) noexcept
{
	if ( pszPath == nullptr || pszPath[0] == '\0' || pInfo == nullptr ) {
		return SIR_FILESYSTEM_INVALID_ARGUMENT;
	}
	wchar_t szPath[32768];
	if ( !sirWindowsPathToWide( pszPath, szPath, 32768 ) ) {
		return SIR_FILESYSTEM_INVALID_ARGUMENT;
	}
	WIN32_FILE_ATTRIBUTE_DATA data{};
	if ( !GetFileAttributesExW( szPath, GetFileExInfoStandard, &data ) ) {
		return sirWindowsFileSystemError( GetLastError() );
	}
	sirWindowsFillFileInfo( data.dwFileAttributes, data.nFileSizeHigh, data.nFileSizeLow, data.ftLastWriteTime, pInfo );
	return SIR_FILESYSTEM_OK;
}

sirFileSystemResult_t SIRPlatformFileSystem::createDirectory( const char *pszPath, bool bRecursive ) noexcept
{
	if ( pszPath == nullptr || pszPath[0] == '\0' ) {
		return SIR_FILESYSTEM_INVALID_ARGUMENT;
	}
	wchar_t szPath[32768];
	if ( !sirWindowsPathToWide( pszPath, szPath, 32768 ) ) {
		return SIR_FILESYSTEM_INVALID_ARGUMENT;
	}
	if ( !bRecursive ) {
		return CreateDirectoryW( szPath, nullptr ) ? SIR_FILESYSTEM_OK : sirWindowsFileSystemError( GetLastError() );
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
	wchar_t szPath[32768];
	if ( !sirWindowsPathToWide( pszPath, szPath, 32768 ) ) {
		return SIR_FILESYSTEM_INVALID_ARGUMENT;
	}
	return RemoveDirectoryW( szPath ) ? SIR_FILESYSTEM_OK : sirWindowsFileSystemError( GetLastError() );
}

sirFileSystemResult_t SIRPlatformFileSystem::renameReplace( const char *pszOldPath, const char *pszNewPath ) noexcept
{
	if ( pszOldPath == nullptr || pszNewPath == nullptr || pszOldPath[0] == '\0' || pszNewPath[0] == '\0' ) {
		return SIR_FILESYSTEM_INVALID_ARGUMENT;
	}
	wchar_t szOldPath[32768];
	wchar_t szNewPath[32768];
	if ( !sirWindowsPathToWide( pszOldPath, szOldPath, 32768 ) || !sirWindowsPathToWide( pszNewPath, szNewPath, 32768 ) ) {
		return SIR_FILESYSTEM_INVALID_ARGUMENT;
	}
	return MoveFileExW( szOldPath, szNewPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH )
	           ? SIR_FILESYSTEM_OK
			   : sirWindowsFileSystemError( GetLastError() );
}

sirFileSystemResult_t SIRPlatformFileSystem::enumerate( const char *pszDirectory, sirFileSystemVisitor_t pVisitor, void *pContext ) noexcept
{
	if ( pszDirectory == nullptr || pszDirectory[0] == '\0' || pVisitor == nullptr ) {
		return SIR_FILESYSTEM_INVALID_ARGUMENT;
	}
	wchar_t szPath[32768];
	if ( !sirWindowsPathToWide( pszDirectory, szPath, 32768 ) ) {
		return SIR_FILESYSTEM_INVALID_ARGUMENT;
	}
	size_t nLength = wcslen( szPath );
	if ( nLength + 3 >= 32768 ) {
		return SIR_FILESYSTEM_BUFFER_TOO_SMALL;
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
	sirFileSystemResult_t eResult = SIR_FILESYSTEM_OK;
	do {
		if ( wcscmp( entry.cFileName, L"." ) == 0 || wcscmp( entry.cFileName, L".." ) == 0 ) {
			continue;
		}
		char szName[2048];
		if ( WideCharToMultiByte( CP_UTF8, WC_ERR_INVALID_CHARS, entry.cFileName, -1, szName, ( int )sizeof( szName ), nullptr, nullptr ) ==
		     0 ) {
			eResult = SIR_FILESYSTEM_BUFFER_TOO_SMALL;
			break;
		}
		sirFileSystemInfo_t info{};
		sirWindowsFillFileInfo( entry.dwFileAttributes, entry.nFileSizeHigh, entry.nFileSizeLow, entry.ftLastWriteTime, &info );
		if ( !pVisitor( szName, &info, pContext ) ) {
			eResult = SIR_FILESYSTEM_STOPPED;
			break;
		}
	} while ( FindNextFileW( hFind, &entry ) );
	if ( eResult == SIR_FILESYSTEM_OK && GetLastError() != ERROR_NO_MORE_FILES ) {
		eResult = sirWindowsFileSystemError( GetLastError() );
	}
	FindClose( hFind );
	return eResult;
}
