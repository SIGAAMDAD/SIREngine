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

#include <dlfcn.h>
#include <string.h>

bool SIRDynamicLibrary::load( const char *szPath, sirDynamicLibrary_t *pLibrary ) noexcept
{
	if ( szPath == nullptr || szPath[0] == '\0' || pLibrary == nullptr || pLibrary->pNativeHandle != nullptr ) {
		return false;
	}
	dlerror();
	pLibrary->pNativeHandle = dlopen( szPath, RTLD_NOW | RTLD_LOCAL );
	return pLibrary->pNativeHandle != nullptr;
}

void *SIRDynamicLibrary::findSymbol( const sirDynamicLibrary_t *pLibrary, const char *szSymbol ) noexcept
{
	if ( pLibrary == nullptr || pLibrary->pNativeHandle == nullptr || szSymbol == nullptr || szSymbol[0] == '\0' ) {
		return nullptr;
	}
	dlerror();
	return dlsym( pLibrary->pNativeHandle, szSymbol );
}

bool SIRDynamicLibrary::unload( sirDynamicLibrary_t *pLibrary ) noexcept
{
	if ( pLibrary == nullptr || pLibrary->pNativeHandle == nullptr || dlclose( pLibrary->pNativeHandle ) != 0 ) {
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
	const char *szError = dlerror();
	if ( szError == nullptr ) {
		return false;
	}
	const size_t nLength = strlen( szError );
	const size_t nCopy = nLength < nCapacity - 1 ? nLength : nCapacity - 1;
	memcpy( szOutput, szError, nCopy );
	szOutput[nCopy] = '\0';
	return true;
}
