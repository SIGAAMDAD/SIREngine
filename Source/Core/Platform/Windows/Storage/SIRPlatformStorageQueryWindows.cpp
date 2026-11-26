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

#include <Core/Platform/Shared/Storage/SIRPlatformStorageQuery.h>

#if defined( _WIN32 )
#include <Core/Platform/Shared/Diagnostics/SIRPlatformTime.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

class SIRWindowsStorageObservation final
{
public:
	explicit SIRWindowsStorageObservation( void ) = delete;
	explicit SIRWindowsStorageObservation( const SIRWindowsStorageObservation& ) = delete;
	explicit SIRWindowsStorageObservation( SIRWindowsStorageObservation&& ) = delete;
	~SIRWindowsStorageObservation() = delete;

	SIRWindowsStorageObservation& operator=( const SIRWindowsStorageObservation& ) = delete;
	SIRWindowsStorageObservation& operator=( SIRWindowsStorageObservation&& ) = delete;

	static sirPlatformStorageResult reportError( DWORD nError, sirPlatformStorageObservation_t *pObservation ) noexcept;
};

sirPlatformStorageResult SIRWindowsStorageObservation::reportError( DWORD nError, sirPlatformStorageObservation_t *pObservation ) noexcept
{
	pObservation->nNativeError = static_cast<uint32_t>( nError );
	switch ( nError ) {
	case ERROR_ACCESS_DENIED:
		return sirPlatformStorageResult::SIR_PLATFORM_STORAGE_ACCESS_DENIED;
	case ERROR_FILE_NOT_FOUND:
	case ERROR_PATH_NOT_FOUND:
		return sirPlatformStorageResult::SIR_PLATFORM_STORAGE_UNKNOWN;
	case ERROR_INVALID_HANDLE:
	case ERROR_INVALID_NAME:
	case ERROR_NO_UNICODE_TRANSLATION:
		return sirPlatformStorageResult::SIR_PLATFORM_STORAGE_INVALID;
	case ERROR_NOT_SUPPORTED:
	case ERROR_INVALID_FUNCTION:
		return sirPlatformStorageResult::SIR_PLATFORM_STORAGE_UNSUPPORTED;
	default:
		return sirPlatformStorageResult::SIR_PLATFORM_STORAGE_OS_ERROR;
	}
}

sirPlatformStorageResult SIRPlatformStorageQuery::queryPath( const char *pszUtf8Path, sirPlatformStorageObservation_t *pObservation ) noexcept
{
	if ( pObservation == nullptr ) {
		return sirPlatformStorageResult::SIR_PLATFORM_STORAGE_INVALID;
	}
	*pObservation = {};
	if ( pszUtf8Path == nullptr || pszUtf8Path[0] == '\0' ) {
		return sirPlatformStorageResult::SIR_PLATFORM_STORAGE_INVALID;
	}

	wchar_t szPath[32768];
	if ( MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, pszUtf8Path, -1, szPath, static_cast<int32_t>( sizeof( szPath ) / sizeof( szPath[0] ) ) ) == 0 ) {
		return SIRWindowsStorageObservation::reportError( GetLastError(), pObservation );
	}
	const HANDLE hFile = CreateFileW(
	    szPath, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr );
	if ( hFile == INVALID_HANDLE_VALUE ) {
		return SIRWindowsStorageObservation::reportError( GetLastError(), pObservation );
	}

	// The opaque integer transports the Win32 HANDLE bit pattern.
	const sirPlatformStorageResult eResult = queryNativeFile( reinterpret_cast<uintptr_t>( hFile ), pObservation );
	CloseHandle( hFile );
	if ( pObservation->nValidFields != 0 ) {
		pObservation->nSourceFlags = static_cast<uint32_t>( sirStorageSource::SIR_STORAGE_SOURCE_ADVISORY_PATH );
	}
	return eResult;
}

sirPlatformStorageResult SIRPlatformStorageQuery::queryNativeFile( uintptr_t nHandle, sirPlatformStorageObservation_t *pObservation ) noexcept
{
	if ( pObservation == nullptr ) {
		return sirPlatformStorageResult::SIR_PLATFORM_STORAGE_INVALID;
	}
	*pObservation = {};
	if ( nHandle == 0 || nHandle == UINTPTR_MAX ) {
		return sirPlatformStorageResult::SIR_PLATFORM_STORAGE_INVALID;
	}

	// Reconstruct the borrowed Win32 HANDLE at the ABI boundary.
	const HANDLE hFile = reinterpret_cast<HANDLE>( nHandle );
	SetLastError( ERROR_SUCCESS );
	const DWORD nType = GetFileType( hFile );
	if ( nType == FILE_TYPE_UNKNOWN ) {
		const DWORD nError = GetLastError();
		if ( nError != ERROR_SUCCESS ) {
			return SIRWindowsStorageObservation::reportError( nError, pObservation );
		}
	}
	if ( nType != FILE_TYPE_DISK ) {
		return sirPlatformStorageResult::SIR_PLATFORM_STORAGE_UNSUPPORTED;
	}

	wchar_t szType[32]{};
	DWORD nFlags{};
	if ( !GetVolumeInformationByHandleW(
	         hFile, nullptr, 0, nullptr, nullptr, &nFlags, szType, static_cast<DWORD>( sizeof( szType ) / sizeof( szType[0] ) ) ) ) {
		return SIRWindowsStorageObservation::reportError( GetLastError(), pObservation );
	}
	pObservation->nSourceFlags = static_cast<uint32_t>( sirStorageSource::SIR_STORAGE_SOURCE_OPEN_HANDLE );
	pObservation->nValidFields = static_cast<uint32_t>( sirStorageField::SIR_STORAGE_READ_ONLY );
	if ( ( nFlags & FILE_READ_ONLY_VOLUME ) != 0 ) {
		pObservation->nVolumeAttributes |= static_cast<uint32_t>( sirStorageVolumeAttribute::SIR_STORAGE_VOLUME_READ_ONLY );
	}
	if ( szType[0] != L'\0' && WideCharToMultiByte( CP_UTF8,
	                                                WC_ERR_INVALID_CHARS,
	                                                szType,
	                                                -1,
	                                                pObservation->szFileSystemType,
	                                                static_cast<int32_t>( sizeof( pObservation->szFileSystemType ) ),
	                                                nullptr,
	                                                nullptr ) != 0 ) {
		pObservation->nValidFields |= static_cast<uint32_t>( sirStorageField::SIR_STORAGE_FILE_SYSTEM_TYPE );
	}
	pObservation->nObservedMonotonicNanoseconds = SIRPlatformTime::getMonotonicNanoseconds();
	if ( pObservation->nObservedMonotonicNanoseconds != 0 ) {
		pObservation->nValidFields |= static_cast<uint32_t>( sirStorageField::SIR_STORAGE_OBSERVATION_TIME );
	}
	return sirPlatformStorageResult::SIR_PLATFORM_STORAGE_PARTIAL;
}
#endif // _WIN32
