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

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <string.h>

static sirFileSystemResult_t sirFileSystemCheckExpected( const wchar_t *pszDestination,
                                                         const sirFileSystemAtomicWriteRequest_t &request ) noexcept
{
	if ( request.pExpectedBytes == nullptr ) {
		return SIR_FILE_SYSTEM_OK;
	}
	HANDLE hFile = CreateFileW( pszDestination, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
	                            OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr );
	if ( hFile == INVALID_HANDLE_VALUE ) {
		return GetLastError() == ERROR_FILE_NOT_FOUND ? SIR_FILE_SYSTEM_CONFLICT : SIR_FILE_SYSTEM_IO_ERROR;
	}
	BY_HANDLE_FILE_INFORMATION information = {};
	LARGE_INTEGER nLength = {};
	if ( !GetFileInformationByHandle( hFile, &information ) || !GetFileSizeEx( hFile, &nLength ) ) {
		CloseHandle( hFile );
		return SIR_FILE_SYSTEM_IO_ERROR;
	}
	if ( ( information.dwFileAttributes & ( FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DEVICE ) ) != 0 ) {
		CloseHandle( hFile );
		return SIR_FILE_SYSTEM_INVALID_PATH;
	}
	if ( nLength.QuadPart < 0 || ( uint64_t )nLength.QuadPart != ( uint64_t )request.nExpectedBytes ) {
		CloseHandle( hFile );
		return SIR_FILE_SYSTEM_CONFLICT;
	}
	char szChunk[4096] = {};
	size_t nRead = 0;
	while ( nRead < request.nExpectedBytes ) {
		const size_t nRemaining = request.nExpectedBytes - nRead;
		const DWORD nChunk = nRemaining < sizeof( szChunk ) ? ( DWORD )nRemaining : ( DWORD )sizeof( szChunk );
		DWORD nActual = 0;
		if ( !ReadFile( hFile, szChunk, nChunk, &nActual, nullptr ) || nActual == 0 ) {
			CloseHandle( hFile );
			return SIR_FILE_SYSTEM_IO_ERROR;
		}
		if ( memcmp( szChunk, request.pExpectedBytes + nRead, nActual ) != 0 ) {
			CloseHandle( hFile );
			return SIR_FILE_SYSTEM_CONFLICT;
		}
		nRead += nActual;
	}
	return CloseHandle( hFile ) ? SIR_FILE_SYSTEM_OK : SIR_FILE_SYSTEM_IO_ERROR;
}

static bool sirFileSystemToWindowsPath( const char *pszUtf8, wchar_t *pWide, int nCapacity ) noexcept
{
	const int nConverted = MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, pszUtf8, -1, pWide, nCapacity );
	return nConverted != 0;
}

sirFileSystemResult_t SIRFileSystem::writeAtomicallyPlatform( const sirFileSystemAtomicWriteRequest_t &request ) noexcept
{
	wchar_t szDestination[4096] = {};
	wchar_t szTemporary[4096] = {};
	if ( !sirFileSystemToWindowsPath( request.pszDestination, szDestination, 4096 ) ||
	     !sirFileSystemToWindowsPath( request.pszTemporary, szTemporary, 4096 ) ) {
		return SIR_FILE_SYSTEM_INVALID_PATH;
	}
	const DWORD nDestinationAttributes = GetFileAttributesW( szDestination );
	if ( nDestinationAttributes != INVALID_FILE_ATTRIBUTES ) {
		if ( ( nDestinationAttributes & FILE_ATTRIBUTE_REPARSE_POINT ) != 0 ) {
			return SIR_FILE_SYSTEM_INVALID_PATH;
		}
		if ( ( nDestinationAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0 ) {
			return SIR_FILE_SYSTEM_REPLACE_FAILED;
		}
		if ( ( nDestinationAttributes & FILE_ATTRIBUTE_DEVICE ) != 0 ) {
			return SIR_FILE_SYSTEM_INVALID_PATH;
		}
	} else if ( GetLastError() != ERROR_FILE_NOT_FOUND ) {
		return SIR_FILE_SYSTEM_IO_ERROR;
	}

	const sirFileSystemResult_t eExpected = sirFileSystemCheckExpected( szDestination, request );
	if ( eExpected != SIR_FILE_SYSTEM_OK ) {
		return eExpected;
	}
	HANDLE hFile = CreateFileW( szTemporary, GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr );
	if ( hFile == INVALID_HANDLE_VALUE ) {
		const DWORD nError = GetLastError();
		return ( nError == ERROR_FILE_EXISTS || nError == ERROR_ALREADY_EXISTS ) ? SIR_FILE_SYSTEM_TEMP_EXISTS : SIR_FILE_SYSTEM_IO_ERROR;
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
		return SIR_FILE_SYSTEM_IO_ERROR;
	}
	const sirFileSystemResult_t eRecheck = sirFileSystemCheckExpected( szDestination, request );
	if ( eRecheck != SIR_FILE_SYSTEM_OK ) {
		DeleteFileW( szTemporary );
		return eRecheck;
	}
	if ( !MoveFileExW( szTemporary, szDestination, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH ) ) {
		DeleteFileW( szTemporary );
		return SIR_FILE_SYSTEM_REPLACE_FAILED;
	}
	return SIR_FILE_SYSTEM_OK;
}
