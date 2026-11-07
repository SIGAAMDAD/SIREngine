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

#include <Core/Platform/SIRPlatformThreadControl.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

static sirThreadControlResult_t sirWindowsThreadError( DWORD nError ) noexcept
{
	return nError == ERROR_ACCESS_DENIED || nError == ERROR_PRIVILEGE_NOT_HELD ? SIR_THREAD_CONTROL_ACCESS_DENIED
	                                                                           : SIR_THREAD_CONTROL_OS_ERROR;
}

uint64_t SIRPlatformThreadControl::getCurrentThreadId( void ) noexcept
{
	return ( uint64_t )GetCurrentThreadId();
}

sirThreadControlResult_t SIRPlatformThreadControl::setCurrentName( const char *pszName ) noexcept
{
	if ( pszName == nullptr || pszName[0] == '\0' ) {
		return SIR_THREAD_CONTROL_INVALID_ARGUMENT;
	}
	wchar_t szWide[256];
	if ( MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, pszName, -1, szWide, 256 ) == 0 ) {
		return SIR_THREAD_CONTROL_INVALID_ARGUMENT;
	}
	const HRESULT nResult = SetThreadDescription( GetCurrentThread(), szWide );
	return SUCCEEDED( nResult ) ? SIR_THREAD_CONTROL_OK : SIR_THREAD_CONTROL_OS_ERROR;
}

sirThreadControlResult_t SIRPlatformThreadControl::getCurrentName( char *pszName, uint32_t nCapacity ) noexcept
{
	if ( pszName == nullptr || nCapacity == 0 || nCapacity > INT32_MAX ) {
		return SIR_THREAD_CONTROL_INVALID_ARGUMENT;
	}
	PWSTR pszWide = nullptr;
	const HRESULT nResult = GetThreadDescription( GetCurrentThread(), &pszWide );
	if ( FAILED( nResult ) ) {
		return SIR_THREAD_CONTROL_OS_ERROR;
	}
	const int nWritten = WideCharToMultiByte( CP_UTF8, WC_ERR_INVALID_CHARS, pszWide, -1, pszName, ( int )nCapacity, nullptr, nullptr );
	LocalFree( pszWide );
	return nWritten > 0 ? SIR_THREAD_CONTROL_OK : SIR_THREAD_CONTROL_INVALID_ARGUMENT;
}

sirThreadControlResult_t SIRPlatformThreadControl::setCurrentAffinity( const sirThreadAffinity_t& rAffinity ) noexcept
{
	if ( rAffinity.nMask == 0 ) {
		return SIR_THREAD_CONTROL_INVALID_ARGUMENT;
	}
	GROUP_AFFINITY affinity{};
	affinity.Group = ( WORD )rAffinity.nGroup;
	affinity.Mask = ( KAFFINITY )rAffinity.nMask;
	return SetThreadGroupAffinity( GetCurrentThread(), &affinity, nullptr ) ? SIR_THREAD_CONTROL_OK
	                                                                        : sirWindowsThreadError( GetLastError() );
}

sirThreadControlResult_t SIRPlatformThreadControl::getCurrentAffinity( sirThreadAffinity_t *pAffinity ) noexcept
{
	if ( pAffinity == nullptr ) {
		return SIR_THREAD_CONTROL_INVALID_ARGUMENT;
	}
	GROUP_AFFINITY affinity{};
	if ( !GetThreadGroupAffinity( GetCurrentThread(), &affinity ) ) {
		return sirWindowsThreadError( GetLastError() );
	}
	sirThreadAffinity_t result{};
	result.nMask = ( uint64_t )affinity.Mask;
	result.nGroup = ( uint16_t )affinity.Group;
	*pAffinity = result;
	return SIR_THREAD_CONTROL_OK;
}

sirThreadControlResult_t SIRPlatformThreadControl::setCurrentPriority( sirThreadPriority_t ePriority ) noexcept
{
	int nNativePriority = THREAD_PRIORITY_NORMAL;
	switch ( ePriority ) {
	case SIR_THREAD_PRIORITY_BACKGROUND:
		nNativePriority = THREAD_PRIORITY_BELOW_NORMAL;
		break;
	case SIR_THREAD_PRIORITY_NORMAL:
		nNativePriority = THREAD_PRIORITY_NORMAL;
		break;
	case SIR_THREAD_PRIORITY_HIGH:
		nNativePriority = THREAD_PRIORITY_ABOVE_NORMAL;
		break;
	default:
		return SIR_THREAD_CONTROL_INVALID_ARGUMENT;
	}
	return SetThreadPriority( GetCurrentThread(), nNativePriority ) ? SIR_THREAD_CONTROL_OK : sirWindowsThreadError( GetLastError() );
}
