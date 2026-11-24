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

#include <Core/Platform/Shared/Library/SIRDynamicLibrary.h>

#include <dlfcn.h>
#include <string.h>

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

sirLibraryResult SIRDynamicLibrary::load( const char *pszPath ) noexcept
{
	if ( pszPath == nullptr || pszPath[0] == '\0' ) {
		return sirLibraryResult::SIR_LIBRARY_INVALID_ARGUMENT;
	}
	if ( isOpen() ) {
		return sirLibraryResult::SIR_LIBRARY_ALREADY_OPEN;
	}
	dlerror();
	void *pHandle = dlopen( pszPath, RTLD_NOW | RTLD_LOCAL );
	if ( pHandle == nullptr ) {
		return sirLibraryResult::SIR_LIBRARY_NOT_FOUND;
	}
	m_nHandle = reinterpret_cast<uintptr_t>( pHandle );
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
	dlerror();
	void *pSymbol = dlsym( reinterpret_cast<void *>( m_nHandle ), pszSymbol );
	if ( dlerror() != nullptr ) {
		return sirLibraryResult::SIR_LIBRARY_SYMBOL_NOT_FOUND;
	}
	*ppSymbol = pSymbol;
	return sirLibraryResult::SIR_LIBRARY_OK;
}

sirLibraryResult SIRDynamicLibrary::unload( void ) noexcept
{
	if ( !isOpen() ) {
		return sirLibraryResult::SIR_LIBRARY_CLOSED;
	}
	if ( dlclose( reinterpret_cast<void *>( m_nHandle ) ) != 0 ) {
		return sirLibraryResult::SIR_LIBRARY_OS_ERROR;
	}
	m_nHandle = 0;
	return sirLibraryResult::SIR_LIBRARY_OK;
}
