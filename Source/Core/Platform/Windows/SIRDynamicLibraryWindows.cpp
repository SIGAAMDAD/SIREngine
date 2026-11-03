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

#include <Core/Platform/SIRDynamicLibrary.h>

#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

// Paths and module names arrive as UTF-8. Using explicit search flags avoids legacy CWD DLL probing.
bool SIRDynamicLibrary::load( const char *szPath, sirDynamicLibrary_t *pLibrary ) noexcept
{
	if ( szPath == nullptr || szPath[0] == '\0' || pLibrary == nullptr || pLibrary->pNativeHandle != nullptr ) {
		return false;
	}
	const int nCharacters = MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, szPath, -1, nullptr, 0 );
	if ( nCharacters <= 0 || nCharacters > 32767 ) {
		return false;
	}
	wchar_t szWidePath[32768];
	if ( MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, szPath, -1, szWidePath, nCharacters ) != nCharacters ) {
		return false;
	}
	// Named system DLLs use default safe search directories; explicit paths are made absolute.
	bool bHasDirectory = nCharacters > 2 && szWidePath[1] == L':';
	for ( int iCharacter = 0; iCharacter < nCharacters; ++iCharacter ) {
		if ( szWidePath[iCharacter] == L'\\' || szWidePath[iCharacter] == L'/' ) {
			bHasDirectory = true;
			break;
		}
	}
	wchar_t szAbsolutePath[32768];
	const wchar_t *szLoadPath = szWidePath;
	if ( bHasDirectory ) {
		const DWORD nLength = GetFullPathNameW( szWidePath, 32768, szAbsolutePath, nullptr );
		if ( nLength == 0 || nLength >= 32768 ) {
			return false;
		}
		szLoadPath = szAbsolutePath;
	}
	const DWORD nFlags = LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | ( bHasDirectory ? LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR : 0 );
	DWORD nPreviousMode = 0;
	const bool bModeChanged = SetThreadErrorMode( SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX, &nPreviousMode ) != FALSE;
	HMODULE hLibrary = LoadLibraryExW( szLoadPath, nullptr, nFlags );
	const DWORD nError = hLibrary == nullptr ? GetLastError() : ERROR_SUCCESS;
	if ( bModeChanged ) {
		SetThreadErrorMode( nPreviousMode, nullptr );
	}
	// Retain the loader failure even when SetThreadErrorMode changes the thread error.
	if ( hLibrary == nullptr ) {
		SetLastError( nError );
	}
	pLibrary->pNativeHandle = (void *)hLibrary;
	return hLibrary != nullptr;
}

void *SIRDynamicLibrary::findSymbol( const sirDynamicLibrary_t *pLibrary, const char *szSymbol ) noexcept
{
	if ( pLibrary == nullptr || pLibrary->pNativeHandle == nullptr || szSymbol == nullptr || szSymbol[0] == '\0' ) {
		return nullptr;
	}
	return (void *)GetProcAddress( (HMODULE)pLibrary->pNativeHandle, szSymbol );
}

bool SIRDynamicLibrary::unload( sirDynamicLibrary_t *pLibrary ) noexcept
{
	if ( pLibrary == nullptr || pLibrary->pNativeHandle == nullptr ) {
		return false;
	}
	if ( FreeLibrary( (HMODULE)pLibrary->pNativeHandle ) == FALSE ) {
		return false;
	}
	pLibrary->pNativeHandle = nullptr;
	return true;
}

bool SIRDynamicLibrary::getLastError( char *szOutput, size_t nCapacity ) noexcept
{
	if ( szOutput == nullptr || nCapacity == 0 ) {
		return false;
	}
	szOutput[0] = '\0';
	const DWORD nError = GetLastError();
	if ( nError == ERROR_SUCCESS ) {
		return false;
	}
	const DWORD nLength = FormatMessageA( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
		nError, 0, szOutput, (DWORD)nCapacity, nullptr );
	return nLength != 0;
}
