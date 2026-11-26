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

#include <Core/Platform/Shared/Threading/SIRPlatformThreadControl.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

static sirThreadControlResult sirWindowsThreadError( DWORD nError ) noexcept
{
	return nError == ERROR_ACCESS_DENIED || nError == ERROR_PRIVILEGE_NOT_HELD ? sirThreadControlResult::SIR_THREAD_CONTROL_ACCESS_DENIED
	                                                                           : sirThreadControlResult::SIR_THREAD_CONTROL_OS_ERROR;
}

uint64_t SIRPlatformThreadControl::getCurrentThreadId( void ) noexcept
{
	return static_cast<uint64_t>(GetCurrentThreadId());
}

sirThreadControlResult SIRPlatformThreadControl::setCurrentName( const char *pszName ) noexcept
{
	if ( pszName == nullptr || pszName[0] == '\0' ) {
		return sirThreadControlResult::SIR_THREAD_CONTROL_INVALID_ARGUMENT;
	}
	wchar_t szWide[256];
	if ( MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, pszName, -1, szWide, 256 ) == 0 ) {
		return sirThreadControlResult::SIR_THREAD_CONTROL_INVALID_ARGUMENT;
	}
	const HRESULT nResult = SetThreadDescription( GetCurrentThread(), szWide );
	return SUCCEEDED( nResult ) ? sirThreadControlResult::SIR_THREAD_CONTROL_OK : sirThreadControlResult::SIR_THREAD_CONTROL_OS_ERROR;
}

sirThreadControlResult SIRPlatformThreadControl::getCurrentName( char *pszName, uint32_t nCapacity ) noexcept
{
	if ( pszName == nullptr || nCapacity == 0 || nCapacity > INT32_MAX ) {
		return sirThreadControlResult::SIR_THREAD_CONTROL_INVALID_ARGUMENT;
	}
	PWSTR pszWide = nullptr;
	const HRESULT nResult = GetThreadDescription( GetCurrentThread(), &pszWide );
	if ( FAILED( nResult ) ) {
		return sirThreadControlResult::SIR_THREAD_CONTROL_OS_ERROR;
	}
	const int nWritten = WideCharToMultiByte( CP_UTF8, WC_ERR_INVALID_CHARS, pszWide, -1, pszName, static_cast<int>(nCapacity), nullptr, nullptr );
	LocalFree( pszWide );
	return nWritten > 0 ? sirThreadControlResult::SIR_THREAD_CONTROL_OK : sirThreadControlResult::SIR_THREAD_CONTROL_INVALID_ARGUMENT;
}

sirThreadControlResult SIRPlatformThreadControl::setCurrentAffinity( const sirThreadAffinity_t& rAffinity ) noexcept
{
	if ( rAffinity.nMask == 0 ) {
		return sirThreadControlResult::SIR_THREAD_CONTROL_INVALID_ARGUMENT;
	}
	GROUP_AFFINITY affinity{};
	affinity.Group = static_cast<WORD>(rAffinity.nGroup);
	affinity.Mask = static_cast<KAFFINITY>(rAffinity.nMask);
	return SetThreadGroupAffinity( GetCurrentThread(), &affinity, nullptr ) ? sirThreadControlResult::SIR_THREAD_CONTROL_OK
	                                                                        : sirWindowsThreadError( GetLastError() );
}

sirThreadControlResult SIRPlatformThreadControl::getCurrentAffinity( sirThreadAffinity_t *pAffinity ) noexcept
{
	if ( pAffinity == nullptr ) {
		return sirThreadControlResult::SIR_THREAD_CONTROL_INVALID_ARGUMENT;
	}
	GROUP_AFFINITY affinity{};
	if ( !GetThreadGroupAffinity( GetCurrentThread(), &affinity ) ) {
		return sirWindowsThreadError( GetLastError() );
	}
	sirThreadAffinity_t result{};
	result.nMask = static_cast<uint64_t>(affinity.Mask);
	result.nGroup = static_cast<uint16_t>(affinity.Group);
	*pAffinity = result;
	return sirThreadControlResult::SIR_THREAD_CONTROL_OK;
}

sirThreadControlResult SIRPlatformThreadControl::setCurrentPriority( sirThreadPriority ePriority ) noexcept
{
	int nNativePriority = THREAD_PRIORITY_NORMAL;
	switch ( ePriority ) {
	case sirThreadPriority::SIR_THREAD_PRIORITY_BACKGROUND:
		nNativePriority = THREAD_PRIORITY_BELOW_NORMAL;
		break;
	case sirThreadPriority::SIR_THREAD_PRIORITY_NORMAL:
		nNativePriority = THREAD_PRIORITY_NORMAL;
		break;
	case sirThreadPriority::SIR_THREAD_PRIORITY_HIGH:
		nNativePriority = THREAD_PRIORITY_ABOVE_NORMAL;
		break;
	default:
		return sirThreadControlResult::SIR_THREAD_CONTROL_INVALID_ARGUMENT;
	}
	return SetThreadPriority( GetCurrentThread(), nNativePriority ) ? sirThreadControlResult::SIR_THREAD_CONTROL_OK : sirWindowsThreadError( GetLastError() );
}
