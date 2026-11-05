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

#ifndef __SIRENGINE_SIRPLATFORMUTILS_H__
#define __SIRENGINE_SIRPLATFORMUTILS_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/SIRVirtualMemory.h>
#include <Core/Platform/SIRDynamicLibrary.h>
#include <Core/Platform/SIRPlatformTime.h>
#include <Core/Platform/SIRPlatformProcess.h>

// Transitional facade for existing SIR callers. New code should include the owning service header.
class SIRPlatformUtils
{
public:
	SIRPlatformUtils( void ) = delete;
	~SIRPlatformUtils() = delete;

	static size_t getPageSize( void ) noexcept;
	static size_t getAllocationGranularity( void ) noexcept;
	static void *reserveVirtualMemory( size_t nBytes ) noexcept;
	static bool commitVirtualMemory( void *pAddress, size_t nBytes ) noexcept;
	static bool decommitVirtualMemory( void *pAddress, size_t nBytes ) noexcept;
	static bool releaseVirtualMemory( void *pAddress, size_t nBytes ) noexcept;
	static bool protectVirtualMemory( void *pAddress, size_t nBytes, sirMemoryProtection_t eProtection ) noexcept;
	static void *allocatePages( size_t nBytes ) noexcept;
	static bool freePages( void *pAddress, size_t nBytes ) noexcept;

	static bool loadDynamicLibrary( const char *szPath, sirDynamicLibrary_t *pLibrary ) noexcept;
	static void *findDynamicLibrarySymbol( const sirDynamicLibrary_t *pLibrary, const char *szSymbol ) noexcept;
	static bool unloadDynamicLibrary( sirDynamicLibrary_t *pLibrary ) noexcept;
	static bool getDynamicLibraryError( char *szOutput, size_t nCapacity ) noexcept;

	static uint64_t getMonotonicNanoseconds( void ) noexcept;
	static uint32_t getLogicalProcessorCount( void ) noexcept;
	static void cpuRelax( void ) noexcept;
	static void yieldThread( void ) noexcept;
	static void sleepThread( uint32_t nMilliseconds ) noexcept;
};

#endif // __SIRENGINE_SIRPLATFORMUTILS_H__
