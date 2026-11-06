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

#include <Core/Platform/SIRFileLock.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

SIRFileLock::SIRFileLock( const char *pszLockPath ) noexcept : m_nHandle( -1 )
{
	if ( pszLockPath == nullptr || pszLockPath[0] == '\0' ) {
		return;
	}

	const int nCharacters = MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, pszLockPath, -1, nullptr, 0 );
	if ( nCharacters <= 0 || nCharacters > 4096 ) {
		return;
	}

	wchar_t szWidePath[4096];
	if ( MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, pszLockPath, -1, szWidePath, nCharacters ) != nCharacters ) {
		return;
	}

	const HANDLE pHandle = CreateFileW( szWidePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
	                                    FILE_ATTRIBUTE_NORMAL, nullptr );
	if ( pHandle == INVALID_HANDLE_VALUE ) {
		return;
	}

	OVERLAPPED overlapped = {};
	if ( LockFileEx( pHandle, LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0, &overlapped ) == FALSE ) {
		( void )CloseHandle( pHandle );
		return;
	}

	m_nHandle = ( intptr_t )pHandle;
}

SIRFileLock::~SIRFileLock( void ) noexcept
{
	if ( m_nHandle != -1 ) {
		OVERLAPPED overlapped = {};
		( void )UnlockFileEx( ( HANDLE )m_nHandle, 0, 1, 0, &overlapped );
		( void )CloseHandle( ( HANDLE )m_nHandle );
	}
}

bool SIRFileLock::isLocked( void ) const noexcept
{
	return m_nHandle != -1;
}
