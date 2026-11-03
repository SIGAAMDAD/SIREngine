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
#include <Core/Platform/SIRVirtualMemory.h>
#include <Core/Platform/SIRDynamicLibrary.h>
#include <Core/Platform/SIRPlatformTime.h>
#include <Core/Platform/SIRPlatformProcess.h>
#include <Core/Platform/SIRPlatformMemory.h>
#include <Core/Platform/SIRCPUInfo.h>
#include <Core/Platform/SIRSystemInfo.h>
#include <Core/Platform/SIRThread.h>
#include <Core/Platform/SIRSync.h>

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
	const size_t nPage = SIRPlatformUtils::getPageSize();
	if ( nPage == 0 || SIRPlatformUtils::commitVirtualMemory( nullptr, nPage ) ) {
		return false;
	}
	void *pAddress = SIRPlatformUtils::reserveVirtualMemory( nPage );
	if ( pAddress == nullptr ) {
		return false;
	}
	bool bSuccess = SIRPlatformUtils::commitVirtualMemory( pAddress, nPage );
	if ( bSuccess ) {
		( ( uint8_t * )pAddress )[0] = 0x5a;
		bSuccess = ( ( uint8_t * )pAddress )[0] == 0x5a;
	}
	if ( bSuccess ) {
		bSuccess = SIRPlatformUtils::protectVirtualMemory( pAddress, nPage, SIR_MEMORY_READ_ONLY );
	}
	if ( bSuccess ) {
		bSuccess = SIRPlatformUtils::protectVirtualMemory( pAddress, nPage, SIR_MEMORY_READ_WRITE );
	}
	if ( bSuccess ) {
		bSuccess = SIRPlatformUtils::decommitVirtualMemory( pAddress, nPage );
	}
	const bool bReleased = SIRPlatformUtils::releaseVirtualMemory( pAddress, nPage );
	void *pBootstrap = SIRPlatformUtils::allocatePages( nPage );
	if ( pBootstrap == nullptr ) {
		return false;
	}
	return bReleased && bSuccess && SIRPlatformUtils::freePages( pBootstrap, nPage );
}

static bool testDynamicLibrary( void )
{
	sirDynamicLibrary_t library = { nullptr };
	if ( SIRPlatformUtils::loadDynamicLibrary( nullptr, &library ) ) {
		return false;
	}
#if defined( _WIN32 )
	const char *szName = "kernel32.dll";
	const char *szSymbol = "GetCurrentProcessId";
#else
	const char *szName = "libm.so.6";
	const char *szSymbol = "cos";
#endif
	if ( !SIRPlatformUtils::loadDynamicLibrary( szName, &library ) ) {
		char szError[256];
		SIRPlatformUtils::getDynamicLibraryError( szError, sizeof( szError ) );
		fprintf( stderr, "Library load: %s\n", szError );
		return false;
	}
	const bool bFound = SIRPlatformUtils::findDynamicLibrarySymbol( &library, szSymbol ) != nullptr;
	const bool bMissing = SIRPlatformUtils::findDynamicLibrarySymbol( &library, "sir_no_such_symbol_78283" ) == nullptr;
	const bool bDoubleLoadRejected = !SIRPlatformUtils::loadDynamicLibrary( szName, &library );
	const bool bClosed = SIRPlatformUtils::unloadDynamicLibrary( &library );
	return bFound && bMissing && bDoubleLoadRejected && bClosed && library.pNativeHandle == nullptr;
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
	if ( nPage == 0 || nPage != SIRPlatformUtils::getPageSize() || SIRVirtualMemory::getAllocationGranularity() < nPage ) {
		return false;
	}
	if ( SIRVirtualMemory::reserve( 0 ) != nullptr || SIRVirtualMemory::commit( nullptr, nPage ) ||
	     SIRVirtualMemory::protect( nullptr, nPage, SIR_MEMORY_READ_ONLY ) ) {
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
		bSuccess = !SIRVirtualMemory::protect( pAddress, nPage, ( sirMemoryProtection_t )255 );
	}
	if ( bSuccess ) {
		bSuccess = SIRVirtualMemory::decommit( pAddress, nPage );
	}
	const bool bReleased = SIRVirtualMemory::release( pAddress, nPage * 2 );
	if ( !bSuccess || !bReleased ) {
		return false;
	}

	sirDynamicLibrary_t library = { nullptr };
	if ( SIRDynamicLibrary::load( "sir_absent_module_6b75e4a1.so", &library ) ) {
		return false;
	}
	char szError[256];
	if ( !SIRDynamicLibrary::getLastError( szError, sizeof( szError ) ) || szError[0] == '\0' ) {
		return false;
	}
#if defined( _WIN32 )
	const char *szName = "kernel32.dll";
	const char *szSymbol = "GetCurrentProcessId";
#else
	const char *szName = "libm.so.6";
	const char *szSymbol = "cos";
#endif
	if ( !SIRDynamicLibrary::load( szName, &library ) ) {
		return false;
	}
	const bool bSymbolResolved = SIRDynamicLibrary::findSymbol( &library, szSymbol ) != nullptr;
	const bool bSymbolRejected = SIRDynamicLibrary::findSymbol( &library, "sir_absent_symbol_6b75e4a1" ) == nullptr;
	const bool bClosed = SIRDynamicLibrary::unload( &library );
	const bool bDoubleCloseRejected = !SIRDynamicLibrary::unload( &library );
	if ( !bSymbolResolved || !bSymbolRejected || !bClosed || !bDoubleCloseRejected ) {
		return false;
	}

	sirDynamicLibrary_t module = { nullptr };
	if ( !SIRDynamicLibrary::load( SIR_TEST_MODULE_PATH, &module ) ) {
		char szModuleError[256];
		SIRDynamicLibrary::getLastError( szModuleError, sizeof( szModuleError ) );
		fprintf( stderr, "Test module load failed: %s\n", szModuleError );
		return false;
	}
	typedef int ( *sirTestModuleFunction_t )( void );
	sirTestModuleFunction_t pValue = ( sirTestModuleFunction_t )SIRDynamicLibrary::findSymbol( &module, "sirPlatformTestValue" );
	const bool bModuleCalled = pValue != nullptr && pValue() == 42;
	if ( !SIRDynamicLibrary::unload( &module ) || !bModuleCalled ) {
		return false;
	}

	const uint64_t nBefore = SIRPlatformTime::getMonotonicNanoseconds();
	SIRPlatformProcess::cpuRelax();
	SIRPlatformProcess::yieldThread();
	SIRPlatformProcess::sleepThread( 1 );
	const uint64_t nAfter = SIRPlatformTime::getMonotonicNanoseconds();
	return nBefore > 0 && nAfter >= nBefore && SIRPlatformProcess::getLogicalProcessorCount() > 0;
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
	if ( ( system.nValidFields & SIR_SYSTEM_MEMORY_SWAP ) != 0 && system.nAvailableSwapBytes > system.nTotalSwapBytes ) {
		return false;
	}
	if ( ( system.nValidFields & SIR_SYSTEM_MEMORY_GROUP_LIMIT ) != 0 && system.nGroupMemoryLimitBytes == 0 ) {
		return false;
	}
	if ( ( process.nValidFields & SIR_PROCESS_MEMORY_RESIDENT ) == 0 || process.nResidentBytes == 0 ) {
		return false;
	}
	if ( ( process.nValidFields & SIR_PROCESS_MEMORY_PEAK_RESIDENT ) != 0 && process.nPeakResidentBytes < process.nResidentBytes ) {
		return false;
	}
	if ( ( process.nValidFields & SIR_PROCESS_MEMORY_VIRTUAL_ADDRESS ) != 0 && process.nVirtualAddressBytes < process.nResidentBytes ) {
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
	if ( ( cpu.nFeatures & SIR_CPU_FEATURE_AVX2 ) != 0 && ( cpu.nFeatures & SIR_CPU_FEATURE_AVX ) == 0 ) {
		return false;
	}
	if ( ( cpu.nFeatures & SIR_CPU_FEATURE_FMA ) != 0 && ( cpu.nFeatures & SIR_CPU_FEATURE_AVX ) == 0 ) {
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
	if ( info.szOSName[0] == '\0' || info.szHostName[0] == '\0' || info.eArchitecture == SIR_ARCH_UNKNOWN ) {
		return false;
	}
#if defined( _WIN32 )
	return info.eOS == SIR_OS_WINDOWS;
#else
	return info.eOS == SIR_OS_LINUX && info.szOSRelease[0] != '\0';
#endif
}

int main( void )
{
	if ( !testVirtualMemory() || !testDynamicLibrary() || !testThreading() || !testLocks() || !testSeparatedServices() ||
	     !testOperatingSystemMemory() || !testCPUInformation() || !testSystemInformation() ) {
		fputs( "SIRSystemTests failed\n", stderr );
		return 1;
	}
	if ( SIRPlatformUtils::getMonotonicNanoseconds() == 0 || SIRPlatformUtils::getLogicalProcessorCount() == 0 ) {
		return 1;
	}
	return 0;
}
