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

#ifndef __SIR_DYNAMICLIBRARY_H__
#define __SIR_DYNAMICLIBRARY_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

enum class sirLibraryResult : uint8_t
{
	SIR_LIBRARY_OK,
	SIR_LIBRARY_INVALID_ARGUMENT,
	SIR_LIBRARY_ALREADY_OPEN,
	SIR_LIBRARY_CLOSED,
	SIR_LIBRARY_NOT_FOUND,
	SIR_LIBRARY_SYMBOL_NOT_FOUND,
	SIR_LIBRARY_OS_ERROR
};

class SIRDynamicLibrary final
{
public:
	explicit SIRDynamicLibrary( void ) noexcept;
	explicit SIRDynamicLibrary( const SIRDynamicLibrary& ) = delete;
	explicit SIRDynamicLibrary( SIRDynamicLibrary&& other ) noexcept;
	~SIRDynamicLibrary() noexcept;

	SIRDynamicLibrary& operator=( const SIRDynamicLibrary& ) = delete;
	SIRDynamicLibrary& operator=( SIRDynamicLibrary&& ) = delete;
	[[nodiscard]] bool isOpen( void ) const noexcept;
	[[nodiscard]] sirLibraryResult load( const char *pszPath ) noexcept;
	[[nodiscard]] sirLibraryResult unload( void ) noexcept;
	[[nodiscard]] sirLibraryResult findSymbol( const char *pszSymbol, void **ppSymbol ) const noexcept;

private:
	uintptr_t m_nHandle;
};

#endif // __SIR_DYNAMICLIBRARY_H__
