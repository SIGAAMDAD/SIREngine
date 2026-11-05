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

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <winternl.h>

#include <string.h>

// These documented native create flags avoid rebuilding absolute paths
// between directory opens; FILE_OPEN_REPARSE_POINT permits inspecting the
// actual opened entry rather than traversing a reparse target.
constexpr ULONG SIR_ROOT_OPEN_REPARSE_POINT = 0x00200000UL;
constexpr ULONG SIR_ROOT_DIRECTORY_FILE = 0x00000001UL;
constexpr ULONG SIR_ROOT_NON_DIRECTORY_FILE = 0x00000040UL;
constexpr ULONG SIR_ROOT_DONT_REPARSE = 0x00001000UL;

typedef NTSTATUS( NTAPI *sirRootNtCreateFile_t )( PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PIO_STATUS_BLOCK, PLARGE_INTEGER, ULONG, ULONG,
                                                  ULONG, ULONG, PVOID, ULONG );

static sirFileResult_t sirRootErrorWindows( DWORD nError ) noexcept
{
	switch ( nError ) {
	case ERROR_FILE_NOT_FOUND:
	case ERROR_PATH_NOT_FOUND:
		return SIR_FILE_NOT_FOUND;
	case ERROR_ACCESS_DENIED:
	case ERROR_SHARING_VIOLATION:
	case ERROR_CANT_ACCESS_FILE:
		return SIR_FILE_ACCESS_DENIED;
	case ERROR_INVALID_NAME:
	case ERROR_INVALID_PARAMETER:
		return SIR_FILE_INVALID_ARGUMENT;
	case ERROR_FILENAME_EXCED_RANGE:
		return SIR_FILE_OUT_OF_RANGE;
	default:
		return SIR_FILE_IO_ERROR;
	}
}

static sirFileResult_t sirRootNativeErrorWindows( NTSTATUS nStatus ) noexcept
{
	// NTSTATUS values are intentionally tested as numbers: the normal Windows
	// SDK does not always make ntstatus.h constants available alongside Windows.h.
	if ( nStatus == ( NTSTATUS )0xC0000034L || nStatus == ( NTSTATUS )0xC000003AL ) {
		return SIR_FILE_NOT_FOUND;
	}
	if ( nStatus == ( NTSTATUS )0xC0000022L || nStatus == ( NTSTATUS )0xC000050BL || nStatus == ( NTSTATUS )0xC000000DL ||
	     nStatus == ( NTSTATUS )0xC0000103L || nStatus == ( NTSTATUS )0xC00000BAL ) {
		return SIR_FILE_ACCESS_DENIED;
	}
	return SIR_FILE_IO_ERROR;
}

static bool sirRootSafeAttributesWindows( HANDLE hObject, bool bDirectory ) noexcept
{
	FILE_ATTRIBUTE_TAG_INFO info = {};
	if ( !GetFileInformationByHandleEx( hObject, FileAttributeTagInfo, &info, sizeof( info ) ) ||
	     ( info.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT ) != 0 ) {
		return false;
	}
	return bDirectory ? ( info.FileAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0
	                  : ( info.FileAttributes & FILE_ATTRIBUTE_DIRECTORY ) == 0 && GetFileType( hObject ) == FILE_TYPE_DISK;
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
	wchar_t szRoot[32768];
	if ( MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, pszRootPath, -1, szRoot, 32768 ) == 0 ) {
		return SIR_FILE_INVALID_ARGUMENT;
	}
	HANDLE hRoot = CreateFileW( szRoot, FILE_TRAVERSE | FILE_READ_ATTRIBUTES | SYNCHRONIZE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
	                            OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr );
	if ( hRoot == INVALID_HANDLE_VALUE ) {
		return sirRootErrorWindows( GetLastError() );
	}
	if ( !sirRootSafeAttributesWindows( hRoot, true ) ) {
		CloseHandle( hRoot );
		return SIR_FILE_ACCESS_DENIED;
	}
	m_nNativeDirectory = ( uintptr_t )hRoot;
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
	HMODULE hNtdll = GetModuleHandleW( L"ntdll.dll" );
	if ( hNtdll == nullptr ) {
		return SIR_FILE_IO_ERROR;
	}
	const sirRootNtCreateFile_t pfnCreate = ( sirRootNtCreateFile_t )GetProcAddress( hNtdll, "NtCreateFile" );
	if ( pfnCreate == nullptr ) {
		return SIR_FILE_IO_ERROR;
	}

	const HANDLE hRoot = ( HANDLE )m_nNativeDirectory;
	HANDLE hParent = hRoot;
	uint32_t nStart = 0;
	for ( uint32_t i = 0;; ++i ) {
		if ( pszRelativePath[i] != '/' && pszRelativePath[i] != '\0' ) {
			continue;
		}
		const bool bFinal = pszRelativePath[i] == '\0';
		const uint32_t nCount = i - nStart;
		char szPart[SIR_PLATFORM_ROOTED_MAX_RELATIVE_BYTES];
		memcpy( szPart, pszRelativePath + nStart, nCount );
		szPart[nCount] = '\0';
		wchar_t szWide[SIR_PLATFORM_ROOTED_MAX_RELATIVE_BYTES];
		const int nWide = MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, szPart, -1, szWide, SIR_PLATFORM_ROOTED_MAX_RELATIVE_BYTES );
		if ( nWide <= 1 ) {
			if ( hParent != hRoot ) {
				CloseHandle( hParent );
			}
			return SIR_FILE_INVALID_ARGUMENT;
		}

		UNICODE_STRING component = {};
		component.Buffer = szWide;
		component.Length = ( USHORT )( ( nWide - 1 ) * sizeof( wchar_t ) );
		component.MaximumLength = ( USHORT )( nWide * sizeof( wchar_t ) );
		OBJECT_ATTRIBUTES attributes = {};
		attributes.Length = sizeof( attributes );
		attributes.RootDirectory = hParent;
		attributes.ObjectName = &component;
		attributes.Attributes = OBJ_CASE_INSENSITIVE | SIR_ROOT_DONT_REPARSE;
		IO_STATUS_BLOCK io = {};
		HANDLE hChild = nullptr;
		const ACCESS_MASK nAccess = ( bFinal ? FILE_READ_DATA : FILE_TRAVERSE ) | FILE_READ_ATTRIBUTES | SYNCHRONIZE;
		const ULONG nOptions = SIR_ROOT_OPEN_REPARSE_POINT | ( bFinal ? SIR_ROOT_NON_DIRECTORY_FILE : SIR_ROOT_DIRECTORY_FILE );
		const NTSTATUS nStatus = pfnCreate( &hChild, nAccess, &attributes, &io, nullptr, FILE_ATTRIBUTE_NORMAL,
		                                    FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_OPEN, nOptions, nullptr, 0 );
		if ( hParent != hRoot ) {
			CloseHandle( hParent );
		}
		if ( nStatus < 0 ) {
			return sirRootNativeErrorWindows( nStatus );
		}
		if ( hChild == nullptr || !sirRootSafeAttributesWindows( hChild, !bFinal ) ) {
			if ( hChild != nullptr ) {
				CloseHandle( hChild );
			}
			return SIR_FILE_ACCESS_DENIED;
		}
		if ( bFinal ) {
			pFile->m_nNativeHandle = ( uintptr_t )hChild;
			pFile->m_nFlags = SIR_FILE_OPEN_READ;
			pFile->m_nCursor = 0;
			return SIR_FILE_OK;
		}
		hParent = hChild;
		nStart = i + 1;
	}
}

sirFileResult_t SIRPlatformRootDirectory::close( void ) noexcept
{
	if ( !isOpen() ) {
		return SIR_FILE_OK;
	}
	HANDLE hRoot = ( HANDLE )m_nNativeDirectory;
	m_nNativeDirectory = UINTPTR_MAX;
	return CloseHandle( hRoot ) ? SIR_FILE_OK : sirRootErrorWindows( GetLastError() );
}
