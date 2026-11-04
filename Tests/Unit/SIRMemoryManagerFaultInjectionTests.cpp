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

#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Memory/SIRMemorySegment.h>
#include <Core/Platform/SIRVirtualMemory.h>

#include <atomic>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define SIR_MEMORY_FAULT_CHECK( condition )                                                                                                \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition );                                                              \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( 0 )

static std::atomic<bool> s_bFailReserve{ false };
static std::atomic<bool> s_bInvalidPageSize{ false };
static std::atomic<bool> s_bFailProtection{ false };
static std::atomic<bool> s_bFailDiscard{ false };

extern "C" long __real_sysconf( int nName );
extern "C" void *__real_mmap( void *pAddress, size_t nBytes, int nProtection, int nFlags, int nHandle, off_t nOffset );
extern "C" int __real_mprotect( void *pAddress, size_t nBytes, int nProtection );
extern "C" int __real_madvise( void *pAddress, size_t nBytes, int nAdvice );

extern "C" long __wrap_sysconf( int nName )
{
	if ( nName == _SC_PAGESIZE && s_bInvalidPageSize.exchange( false, std::memory_order_relaxed ) ) {
		return 3;
	}
	return __real_sysconf( nName );
}

extern "C" void *__wrap_mmap( void *pAddress, size_t nBytes, int nProtection, int nFlags, int nHandle, off_t nOffset )
{
	if ( s_bFailReserve.exchange( false, std::memory_order_relaxed ) ) {
		errno = ENOMEM;
		return MAP_FAILED;
	}
	return __real_mmap( pAddress, nBytes, nProtection, nFlags, nHandle, nOffset );
}

extern "C" int __wrap_mprotect( void *pAddress, size_t nBytes, int nProtection )
{
	if ( s_bFailProtection.exchange( false, std::memory_order_relaxed ) ) {
		errno = ENOMEM;
		return -1;
	}
	return __real_mprotect( pAddress, nBytes, nProtection );
}

extern "C" int __wrap_madvise( void *pAddress, size_t nBytes, int nAdvice )
{
	if ( s_bFailDiscard.exchange( false, std::memory_order_relaxed ) ) {
		errno = EIO;
		return -1;
	}
	return __real_madvise( pAddress, nBytes, nAdvice );
}

static void testVirtualBackingFaultRecovery( void )
{
	const uint64_t nPage = SIRVirtualMemory::getPageSize();
	SIR_MEMORY_FAULT_CHECK( nPage != 0 );
	SIRMemoryManager memory;
	const sirMemoryConfig_t config = { nPage * 128, nPage * 32, nPage * 8, 4 };
	s_bFailReserve.store( true, std::memory_order_relaxed );
	SIR_MEMORY_FAULT_CHECK( !memory.init( config ) && !memory.isInitialized() );
	s_bFailProtection.store( true, std::memory_order_relaxed );
	SIR_MEMORY_FAULT_CHECK( !memory.init( config ) && !memory.isInitialized() );
	SIR_MEMORY_FAULT_CHECK( memory.init( config ) );
	SIRMemorySegment *pSegment = memory.createSegment( "fault-backing", nPage * 4, nPage * 4, SIR_MEMORY_DOMAIN_STREAMING );
	SIR_MEMORY_FAULT_CHECK( pSegment != nullptr );
	s_bFailProtection.store( true, std::memory_order_relaxed );
	SIR_MEMORY_FAULT_CHECK( !pSegment->commit( 0, nPage ) );
	SIR_MEMORY_FAULT_CHECK( memory.getPageBackingStats().nFailedCommitCalls == 1 );
	SIR_MEMORY_FAULT_CHECK( pSegment->commit( 0, nPage ) );
	volatile uint8_t *pData = ( volatile uint8_t * )pSegment->getBase();
	pData[0] = 0xA9;
	s_bFailDiscard.store( true, std::memory_order_relaxed );
	SIR_MEMORY_FAULT_CHECK( !pSegment->decommit( 0, nPage ) );
	SIR_MEMORY_FAULT_CHECK( pData[0] == 0xA9 );
	SIR_MEMORY_FAULT_CHECK( memory.getPageBackingStats().nFailedDecommitCalls == 1 );
	SIR_MEMORY_FAULT_CHECK( pSegment->decommit( 0, nPage ) );
	s_bFailProtection.store( true, std::memory_order_relaxed );
	SIR_MEMORY_FAULT_CHECK( memory.allocateMetadata( nPage * 2, 8 ) == nullptr );
	SIR_MEMORY_FAULT_CHECK( memory.allocateMetadata( nPage * 2, 8 ) != nullptr );
	SIR_MEMORY_FAULT_CHECK( memory.allocateMetadata( nPage, 8 ) != nullptr );
	SIR_MEMORY_FAULT_CHECK( memory.allocateMetadata( nPage, 8 ) != nullptr );
	SIR_MEMORY_FAULT_CHECK( memory.allocateMetadata( nPage, 8, SIR_MEMORY_DOMAIN_CORE ) != nullptr );
	SIR_MEMORY_FAULT_CHECK( memory.getDomainStats( SIR_MEMORY_DOMAIN_CORE ).nCommittedBytes >= nPage * 2 );
	SIR_MEMORY_FAULT_CHECK( !memory.setDomainCommitLimit( SIR_MEMORY_DOMAIN_CORE, nPage ) );
	s_bFailProtection.store( true, std::memory_order_relaxed );
	SIR_MEMORY_FAULT_CHECK( memory.allocateRecyclableMetadataPage() == nullptr );
	void *pPage = memory.allocateRecyclableMetadataPage();
	SIR_MEMORY_FAULT_CHECK( pPage != nullptr );
	s_bFailDiscard.store( true, std::memory_order_relaxed );
	SIR_MEMORY_FAULT_CHECK( !memory.releaseRecyclableMetadataPage( pPage ) );
	SIR_MEMORY_FAULT_CHECK( memory.releaseRecyclableMetadataPage( pPage ) );
	s_bFailProtection.store( true, std::memory_order_relaxed );
	SIR_MEMORY_FAULT_CHECK( memory.allocateRecyclableMetadataPage() == nullptr );
	void *pReused = memory.allocateRecyclableMetadataPage();
	SIR_MEMORY_FAULT_CHECK( pReused == pPage );
	SIR_MEMORY_FAULT_CHECK( memory.releaseRecyclableMetadataPage( pReused ) );
	memory.shutdown();
}

static void testInvalidOperatingSystemPageSize( void )
{
	// Page size is cached on its first call, so isolate a deliberately
	// invalid kernel report in a child rather than poisoning the test host.
	const pid_t nChild = fork();
	SIR_MEMORY_FAULT_CHECK( nChild >= 0 );
	if ( nChild == 0 ) {
		s_bInvalidPageSize.store( true, std::memory_order_relaxed );
		SIRMemoryManager memory;
		const sirMemoryConfig_t config = { 4096 * 128, 4096 * 32, 4096 * 8, 4 };
		SIR_MEMORY_FAULT_CHECK( !memory.init( config ) && !memory.isInitialized() );
		exit( 0 );
	}
	int nStatus = 0;
	SIR_MEMORY_FAULT_CHECK( waitpid( nChild, &nStatus, 0 ) == nChild );
	SIR_MEMORY_FAULT_CHECK( WIFEXITED( nStatus ) && WEXITSTATUS( nStatus ) == 0 );
}

int main( void )
{
	testInvalidOperatingSystemPageSize();
	testVirtualBackingFaultRecovery();
	return 0;
}
