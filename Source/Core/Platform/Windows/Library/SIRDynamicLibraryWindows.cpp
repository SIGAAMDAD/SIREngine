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

#include <Core/Platform/Shared/Library/SIRDynamicLibrary.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

// Paths and module names arrive as UTF-8. Using explicit search flags avoids legacy CWD DLL probing.
SIRDynamicLibrary::SIRDynamicLibrary( void ) noexcept
    : m_nHandle( 0 )
{
}

SIRDynamicLibrary::SIRDynamicLibrary( SIRDynamicLibrary&& other ) noexcept
    : m_nHandle( other.m_nHandle )
{
	other.m_nHandle = 0;
}

SIRDynamicLibrary::~SIRDynamicLibrary() noexcept
{
	if ( isOpen() ) {
		static_cast<void>( unload() );
	}
}

bool SIRDynamicLibrary::isOpen( void ) const noexcept
{
	return m_nHandle != 0;
}

sirLibraryResult SIRDynamicLibrary::load( const char *szPath ) noexcept
{
	if ( szPath == nullptr || szPath[0] == '\0' ) {
		return sirLibraryResult::SIR_LIBRARY_INVALID_ARGUMENT;
	}
	if ( isOpen() ) {
		return sirLibraryResult::SIR_LIBRARY_ALREADY_OPEN;
	}
	const int nCharacters = MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, szPath, -1, nullptr, 0 );
	if ( nCharacters <= 0 || nCharacters > 32767 ) {
		return sirLibraryResult::SIR_LIBRARY_INVALID_ARGUMENT;
	}
	wchar_t szWidePath[32768];
	if ( MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, szPath, -1, szWidePath, nCharacters ) != nCharacters ) {
		return sirLibraryResult::SIR_LIBRARY_INVALID_ARGUMENT;
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
			return sirLibraryResult::SIR_LIBRARY_INVALID_ARGUMENT;
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
	if ( hLibrary == nullptr ) {
		return sirLibraryResult::SIR_LIBRARY_NOT_FOUND;
	}
	m_nHandle = reinterpret_cast<uintptr_t>( hLibrary );
	return sirLibraryResult::SIR_LIBRARY_OK;
}

sirLibraryResult SIRDynamicLibrary::findSymbol( const char *pszSymbol, void **ppSymbol ) const noexcept
{
	if ( pszSymbol == nullptr || pszSymbol[0] == '\0' || ppSymbol == nullptr ) {
		return sirLibraryResult::SIR_LIBRARY_INVALID_ARGUMENT;
	}
	if ( !isOpen() ) {
		return sirLibraryResult::SIR_LIBRARY_CLOSED;
	}
	FARPROC pSymbol = GetProcAddress( reinterpret_cast<HMODULE>( m_nHandle ), pszSymbol );
	if ( pSymbol == nullptr ) {
		return sirLibraryResult::SIR_LIBRARY_SYMBOL_NOT_FOUND;
	}
	*ppSymbol = reinterpret_cast<void *>( pSymbol );
	return sirLibraryResult::SIR_LIBRARY_OK;
}

sirLibraryResult SIRDynamicLibrary::unload( void ) noexcept
{
	if ( !isOpen() ) {
		return sirLibraryResult::SIR_LIBRARY_CLOSED;
	}
	if ( FreeLibrary( reinterpret_cast<HMODULE>( m_nHandle ) ) == FALSE ) {
		return sirLibraryResult::SIR_LIBRARY_OS_ERROR;
	}
	m_nHandle = 0;
	return sirLibraryResult::SIR_LIBRARY_OK;
}
