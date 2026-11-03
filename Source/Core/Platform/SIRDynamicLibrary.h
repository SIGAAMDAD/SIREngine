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

#ifndef __SIRENGINE_SIRDYNAMICLIBRARY_H__
#define __SIRENGINE_SIRDYNAMICLIBRARY_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <stddef.h>

typedef struct sirDynamicLibrary_s
{
	void *pNativeHandle;
} sirDynamicLibrary_t;

// One handle per successful load; the caller owns unload. Returned symbols expire on unload.
class SIRDynamicLibrary
{
public:
	SIRDynamicLibrary( void ) = delete;
	~SIRDynamicLibrary() = delete;

	static bool load( const char *szPath, sirDynamicLibrary_t *pLibrary ) noexcept;
	static void *findSymbol( const sirDynamicLibrary_t *pLibrary, const char *szSymbol ) noexcept;
	static bool unload( sirDynamicLibrary_t *pLibrary ) noexcept;
	static bool getLastError( char *szOutput, size_t nCapacity ) noexcept;
};

#endif // __SIRENGINE_SIRDYNAMICLIBRARY_H__
