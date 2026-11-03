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

#include <Core/Platform/SIRPlatformUtils.h>

size_t SIRPlatformUtils::getPageSize( void ) noexcept
{
	return SIRVirtualMemory::getPageSize();
}
size_t SIRPlatformUtils::getAllocationGranularity( void ) noexcept
{
	return SIRVirtualMemory::getAllocationGranularity();
}
void *SIRPlatformUtils::reserveVirtualMemory( size_t nBytes ) noexcept
{
	return SIRVirtualMemory::reserve( nBytes );
}
bool SIRPlatformUtils::commitVirtualMemory( void *pAddress, size_t nBytes ) noexcept
{
	return SIRVirtualMemory::commit( pAddress, nBytes );
}
bool SIRPlatformUtils::decommitVirtualMemory( void *pAddress, size_t nBytes ) noexcept
{
	return SIRVirtualMemory::decommit( pAddress, nBytes );
}
bool SIRPlatformUtils::releaseVirtualMemory( void *pAddress, size_t nBytes ) noexcept
{
	return SIRVirtualMemory::release( pAddress, nBytes );
}
bool SIRPlatformUtils::protectVirtualMemory( void *pAddress, size_t nBytes, sirMemoryProtection_t eProtection ) noexcept
{
	return SIRVirtualMemory::protect( pAddress, nBytes, eProtection );
}
void *SIRPlatformUtils::allocatePages( size_t nBytes ) noexcept
{
	return SIRVirtualMemory::allocatePages( nBytes );
}
bool SIRPlatformUtils::freePages( void *pAddress, size_t nBytes ) noexcept
{
	return SIRVirtualMemory::freePages( pAddress, nBytes );
}

bool SIRPlatformUtils::loadDynamicLibrary( const char *szPath, sirDynamicLibrary_t *pLibrary ) noexcept
{
	return SIRDynamicLibrary::load( szPath, pLibrary );
}
void *SIRPlatformUtils::findDynamicLibrarySymbol( const sirDynamicLibrary_t *pLibrary, const char *szSymbol ) noexcept
{
	return SIRDynamicLibrary::findSymbol( pLibrary, szSymbol );
}
bool SIRPlatformUtils::unloadDynamicLibrary( sirDynamicLibrary_t *pLibrary ) noexcept
{
	return SIRDynamicLibrary::unload( pLibrary );
}
bool SIRPlatformUtils::getDynamicLibraryError( char *szOutput, size_t nCapacity ) noexcept
{
	return SIRDynamicLibrary::getLastError( szOutput, nCapacity );
}

uint64_t SIRPlatformUtils::getMonotonicNanoseconds( void ) noexcept
{
	return SIRPlatformTime::getMonotonicNanoseconds();
}
uint32_t SIRPlatformUtils::getLogicalProcessorCount( void ) noexcept
{
	return SIRPlatformProcess::getLogicalProcessorCount();
}
void SIRPlatformUtils::cpuRelax( void ) noexcept
{
	SIRPlatformProcess::cpuRelax();
}
void SIRPlatformUtils::yieldThread( void ) noexcept
{
	SIRPlatformProcess::yieldThread();
}
void SIRPlatformUtils::sleepThread( uint32_t nMilliseconds ) noexcept
{
	SIRPlatformProcess::sleepThread( nMilliseconds );
}
