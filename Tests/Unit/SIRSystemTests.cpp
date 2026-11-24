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

#include <Core/Platform/Shared/Memory/SIRVirtualMemory.h>
#include <Core/Platform/Shared/Diagnostics/SIRPlatformTime.h>
#include <Core/Platform/Shared/Diagnostics/SIRCPUInfo.h>
#include <Core/Platform/Shared/Threading/SIRThread.h>
#include <Core/Platform/Shared/Memory/SIRVirtualMemory.h>
#include <Core/Platform/Shared/Library/SIRDynamicLibrary.h>
#include <Core/Platform/Shared/Diagnostics/SIRPlatformTime.h>
#include <Core/Platform/Shared/Memory/SIRVirtualMemory.h>
#include <Core/Platform/Shared/Diagnostics/SIRPlatformTime.h>
#include <Core/Platform/Shared/Diagnostics/SIRCPUInfo.h>
#include <Core/Platform/Shared/Threading/SIRThread.h>
#include <Core/Platform/Shared/Memory/SIRPlatformMemory.h>
#include <Core/Platform/Shared/Diagnostics/SIRCPUInfo.h>
#include <Core/Platform/Shared/Diagnostics/SIRSystemInfo.h>
#include <Core/Platform/Shared/Threading/SIRThread.h>
#include <Core/Platform/Shared/Threading/SIRSync.h>

#include <atomic>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct sirTestWorker_s
{
	SIRMutex mutex;
	SIRConditionVariable ready;
	bool bRelease = false;
	bool bStarted = false;
	std::atomic<uint32_t> nRuns{ 0 };
} sirTestWorker_t;

static void runWorker( void *pUserData )
{
	sirTestWorker_t *pWorker = ( sirTestWorker_t * )pUserData;
	pWorker->mutex.lock();
	pWorker->bStarted = true;
	pWorker->ready.notifyOne();
	while ( !pWorker->bRelease ) {
		pWorker->ready.wait( pWorker->mutex );
	}
	pWorker->mutex.unlock();
	pWorker->nRuns.fetch_add( 1, std::memory_order_release );
}

static bool testVirtualMemory( void )
{
	const size_t nPage = SIRVirtualMemory::getPageSize();
	if ( nPage == 0 || SIRVirtualMemory::commit( nullptr, nPage ) ) {
		return false;
	}
	void *pAddress = SIRVirtualMemory::reserve( nPage );
	if ( pAddress == nullptr ) {
		return false;
	}
	bool bSuccess = SIRVirtualMemory::commit( pAddress, nPage );
	if ( bSuccess ) {
		( ( uint8_t * )pAddress )[0] = 0x5a;
		bSuccess = ( ( uint8_t * )pAddress )[0] == 0x5a;
	}
	if ( bSuccess ) {
		bSuccess = SIRVirtualMemory::protect( pAddress, nPage, sirMemoryProtection::SIR_MEMORY_READ_ONLY );
	}
	if ( bSuccess ) {
		bSuccess = SIRVirtualMemory::protect( pAddress, nPage, sirMemoryProtection::SIR_MEMORY_READ_WRITE );
	}
	if ( bSuccess ) {
		bSuccess = SIRVirtualMemory::decommit( pAddress, nPage );
	}
	const bool bReleased = SIRVirtualMemory::release( pAddress, nPage );
	void *pBootstrap = SIRVirtualMemory::allocatePages( nPage );
	if ( pBootstrap == nullptr ) {
		return false;
	}
	return bReleased && bSuccess && SIRVirtualMemory::freePages( pBootstrap, nPage );
}

static bool testDynamicLibrary( void )
{
 SIRDynamicLibrary library;
 if ( library.load( nullptr ) != sirLibraryResult::SIR_LIBRARY_INVALID_ARGUMENT ) { return false; }
#if defined( _WIN32 )
 const char *szName = "kernel32.dll";
 const char *szSymbol = "GetCurrentProcessId";
#else
 const char *szName = "libm.so.6";
 const char *szSymbol = "cos";
#endif
 if ( library.load( szName ) != sirLibraryResult::SIR_LIBRARY_OK ) { return false; }
 void *pSymbol = nullptr;
 if ( library.findSymbol( szSymbol, &pSymbol ) != sirLibraryResult::SIR_LIBRARY_OK || pSymbol == nullptr ) { return false; }
 if ( library.findSymbol( "sir_no_such_symbol_78283", &pSymbol ) != sirLibraryResult::SIR_LIBRARY_SYMBOL_NOT_FOUND ) { return false; }
 if ( library.load( szName ) != sirLibraryResult::SIR_LIBRARY_ALREADY_OPEN ) { return false; }
 SIRDynamicLibrary moved( static_cast<SIRDynamicLibrary&&>( library ) );
 return !library.isOpen() && moved.isOpen() && moved.unload() == sirLibraryResult::SIR_LIBRARY_OK && !moved.isOpen();
}

static bool testThreading( void )
{
	sirTestWorker_t worker;
	SIRThread thread;
	if ( thread.start( nullptr, nullptr ) || !thread.start( &runWorker, &worker ) ) {
		return false;
	}
	if ( thread.start( &runWorker, &worker ) ) {
		return false;
	}
	worker.mutex.lock();
	while ( !worker.bStarted ) {
		worker.ready.wait( worker.mutex );
	}
	worker.bRelease = true;
	worker.ready.notifyOne();
	worker.mutex.unlock();
	if ( !thread.join() || thread.isJoinable() || thread.isAlive() || worker.nRuns.load( std::memory_order_acquire ) != 1 ) {
		return false;
	}
	if ( !thread.start( &runWorker, &worker ) || !thread.join() ) {
		return false;
	}
	return worker.nRuns.load( std::memory_order_acquire ) == 2;
}

static bool testLocks( void )
{
	SIRMutex mutex;
	if ( !mutex.tryLock() || mutex.tryLock() ) {
		return false;
	}
	mutex.unlock();
	SIRRecursiveMutex recursive;
	if ( !recursive.tryLock() || !recursive.tryLock() ) {
		return false;
	}
	recursive.unlock();
	recursive.unlock();
	SIRSpinLock spin;
	if ( !spin.tryLock() || spin.tryLock() ) {
		return false;
	}
	spin.unlock();
	const bool bReacquired = spin.tryLock();
	if ( bReacquired ) {
		spin.unlock();
	}
	return bReacquired;
}

static bool testSeparatedServices( void )
{
	const size_t nPage = SIRVirtualMemory::getPageSize();
	if ( nPage == 0 || nPage != SIRVirtualMemory::getPageSize() || SIRVirtualMemory::getAllocationGranularity() < nPage ) {
		return false;
	}
	if ( SIRVirtualMemory::reserve( 0 ) != nullptr || SIRVirtualMemory::commit( nullptr, nPage ) ||
	     SIRVirtualMemory::protect( nullptr, nPage, sirMemoryProtection::SIR_MEMORY_READ_ONLY ) ) {
		return false;
	}
	void *pAddress = SIRVirtualMemory::reserve( nPage * 2 );
	if ( pAddress == nullptr ) {
		return false;
	}
	bool bSuccess = SIRVirtualMemory::commit( pAddress, nPage );
	if ( bSuccess ) {
		( ( uint8_t * )pAddress )[0] = 0x48;
		bSuccess = ( ( uint8_t * )pAddress )[0] == 0x48;
	}
	if ( bSuccess ) {
		bSuccess = !SIRVirtualMemory::protect( pAddress, nPage, ( sirMemoryProtection )255 );
	}
	if ( bSuccess ) {
		bSuccess = SIRVirtualMemory::decommit( pAddress, nPage );
	}
	const bool bReleased = SIRVirtualMemory::release( pAddress, nPage * 2 );
	if ( !bSuccess || !bReleased ) {
		return false;
	}

	SIRDynamicLibrary module;
	if ( module.load( SIR_TEST_MODULE_PATH ) != sirLibraryResult::SIR_LIBRARY_OK ) { return false; }
	void *pModuleAddress = nullptr;
	if ( module.findSymbol( "sirPlatformTestValue", &pModuleAddress ) != sirLibraryResult::SIR_LIBRARY_OK ) { return false; }
	typedef int ( *sirTestModuleFunction_t )( void );
	const sirTestModuleFunction_t pValue = reinterpret_cast<sirTestModuleFunction_t>( pModuleAddress );
	if ( pValue == nullptr || pValue() != 42 || module.unload() != sirLibraryResult::SIR_LIBRARY_OK ) { return false; }

	const uint64_t nBefore = SIRPlatformTime::getMonotonicNanoseconds();
	SIRThread::cpuRelax();
	SIRThread::yieldThread();
	SIRThread::sleepThread( 1 );
	const uint64_t nAfter = SIRPlatformTime::getMonotonicNanoseconds();
	return nBefore > 0 && nAfter >= nBefore && SIRCPUInfo::getLogicalProcessorCount() > 0;
}

static bool testOperatingSystemMemory( void )
{
	if ( SIRPlatformMemory::querySystem( nullptr ) || SIRPlatformMemory::queryProcess( nullptr ) ) {
		return false;
	}
	sirSystemMemoryStats_t system = {};
	sirProcessMemoryStats_t process = {};
	if ( !SIRPlatformMemory::querySystem( &system ) || !SIRPlatformMemory::queryProcess( &process ) ) {
		return false;
	}
	if ( system.nTotalPhysicalBytes == 0 || system.nAvailablePhysicalBytes > system.nTotalPhysicalBytes ) {
		return false;
	}
	if ( ( system.nValidFields & static_cast<uint64_t>( sirSystemMemoryField::SIR_SYSTEM_MEMORY_SWAP ) ) != 0 && system.nAvailableSwapBytes > system.nTotalSwapBytes ) {
		return false;
	}
	if ( ( system.nValidFields & static_cast<uint64_t>( sirSystemMemoryField::SIR_SYSTEM_MEMORY_GROUP_LIMIT ) ) != 0 && system.nGroupMemoryLimitBytes == 0 ) {
		return false;
	}
	if ( ( process.nValidFields & static_cast<uint64_t>( sirProcessMemoryField::SIR_PROCESS_MEMORY_RESIDENT ) ) == 0 || process.nResidentBytes == 0 ) {
		return false;
	}
	if ( ( process.nValidFields & static_cast<uint64_t>( sirProcessMemoryField::SIR_PROCESS_MEMORY_PEAK_RESIDENT ) ) != 0 && process.nPeakResidentBytes < process.nResidentBytes ) {
		return false;
	}
	if ( ( process.nValidFields & static_cast<uint64_t>( sirProcessMemoryField::SIR_PROCESS_MEMORY_VIRTUAL_ADDRESS ) ) != 0 && process.nVirtualAddressBytes < process.nResidentBytes ) {
		return false;
	}
	// A live process can grow between snapshots; do not assert an allocation-sized RSS delta.
	const size_t nPage = SIRVirtualMemory::getPageSize();
	void *pMemory = SIRVirtualMemory::allocatePages( nPage );
	if ( pMemory == nullptr ) {
		return false;
	}
	( ( volatile uint8_t * )pMemory )[0] = 0x42;
	sirProcessMemoryStats_t later = {};
	const bool bObserved = SIRPlatformMemory::queryProcess( &later );
	const bool bReleased = SIRVirtualMemory::freePages( pMemory, nPage );
	return bObserved && bReleased && later.nResidentBytes > 0;
}

static bool testCPUInformation( void )
{
	if ( SIRCPUInfo::query( nullptr ) ) {
		return false;
	}
	sirCPUInfo_t cpu = {};
	if ( !SIRCPUInfo::query( &cpu ) || cpu.nLogicalProcessorCount == 0 ) {
		return false;
	}
	if ( cpu.nPhysicalCoreCount > cpu.nLogicalProcessorCount ) {
		return false;
	}
	if ( ( cpu.nFeatures & static_cast<uint64_t>( sirCPUFeature::SIR_CPU_FEATURE_AVX2 ) ) != 0 && ( cpu.nFeatures & static_cast<uint64_t>( sirCPUFeature::SIR_CPU_FEATURE_AVX ) ) == 0 ) {
		return false;
	}
	if ( ( cpu.nFeatures & static_cast<uint64_t>( sirCPUFeature::SIR_CPU_FEATURE_FMA ) ) != 0 && ( cpu.nFeatures & static_cast<uint64_t>( sirCPUFeature::SIR_CPU_FEATURE_AVX ) ) == 0 ) {
		return false;
	}
#if defined( __x86_64__ ) || defined( _M_X64 )
	if ( cpu.szVendor[0] == '\0' || cpu.szBrand[0] == '\0' ) {
		return false;
	}
#endif
	return true;
}

static bool testSystemInformation( void )
{
	if ( SIRSystemInfo::query( nullptr ) ) {
		return false;
	}
	sirSystemInfo_t info = {};
	if ( !SIRSystemInfo::query( &info ) ) {
		return false;
	}
	if ( info.nProcessId == 0 || info.nPageSize != SIRVirtualMemory::getPageSize() || info.nAllocationGranularity < info.nPageSize ) {
		return false;
	}
	if ( info.szOSName[0] == '\0' || info.szHostName[0] == '\0' || info.eArchitecture == sirCPUArchitecture::SIR_ARCH_UNKNOWN ) {
		return false;
	}
#if defined( _WIN32 )
	return info.eOS == sirOperatingSystem::SIR_OS_WINDOWS;
#else
	return info.eOS == sirOperatingSystem::SIR_OS_LINUX && info.szOSRelease[0] != '\0';
#endif
}

int main( void )
{
	if ( !testVirtualMemory() || !testDynamicLibrary() || !testThreading() || !testLocks() || !testSeparatedServices() ||
	     !testOperatingSystemMemory() || !testCPUInformation() || !testSystemInformation() ) {
		fputs( "SIRSystemTests failed\n", stderr );
		return 1;
	}
	if ( SIRPlatformTime::getMonotonicNanoseconds() == 0 || SIRCPUInfo::getLogicalProcessorCount() == 0 ) {
		return 1;
	}
	return 0;
}
