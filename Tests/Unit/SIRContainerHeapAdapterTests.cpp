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

#include <Core/Containers/SIRContainerHeapAdapter.h>
#include <Core/Containers/SIRVector.h>
#include <Core/Containers/SIRInlineVector.h>
#include <Core/Containers/SIRFlatHashMap.h>
#include <Core/Containers/SIRString.h>
#include <Core/Containers/SIRPagedArray.h>
#include <Core/Containers/SIRBitVector.h>
#include <Core/Containers/SIRRingBuffer.h>
#include <Core/Memory/Allocators/SIRCompactBucketAllocator.h>
#include <Core/Memory/Allocators/SIRFreeListAllocator.h>
#include <Core/Memory/Allocators/SIRBuddyAllocator.h>
#include <Core/Memory/SIRMemorySegment.h>
#include <Core/Platform/SIRVirtualMemory.h>

#include <stdio.h>
#include <stdint.h>

#define SIR_TEST_CHECK( expression )                                                                                                       \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			fprintf( stderr, "Container heap adapter test failed at line %d: %s\n", __LINE__, #expression );                               \
			return false;                                                                                                                  \
		}                                                                                                                                  \
	} while ( 0 )

typedef struct alignas( 128 ) sirTestAlignedValue_s
{
	uint64_t nValues[16];
} sirTestAlignedValue_t;

class SIRContainerTestHeap
{
public:
	SIRMemoryManager m_Memory;
	SIRCompactBucketAllocator m_Small;
	SIRFreeListAllocator m_Medium;
	SIRBuddyAllocator m_Large;
	SIRGeneralHeap m_Heap;
	SIRContainerHeapAdapter m_Adapter;

	bool init( void )
	{
		const uint64_t nPage = SIRVirtualMemory::getPageSize();
		if ( nPage == 0 ) {
			return false;
		}
		const sirMemoryConfig_t config = { nPage * 4096, nPage * 3072, nPage * 384, 12 };
		if ( !m_Memory.init( config ) ) {
			return false;
		}
		SIRMemorySegment *pSmall = m_Memory.createSegment( "container.small", nPage * 256, 0, SIR_MEMORY_DOMAIN_GAMEPLAY );
		SIRMemorySegment *pMedium = m_Memory.createSegment( "container.medium", nPage * 256, 0, SIR_MEMORY_DOMAIN_GAMEPLAY );
		SIRMemorySegment *pLarge = m_Memory.createSegment( "container.large", nPage * 1024, 0, SIR_MEMORY_DOMAIN_GAMEPLAY );
		if ( pSmall == nullptr || pMedium == nullptr || pLarge == nullptr ) {
			return false;
		}
		const sirCompactBucketConfig_t bucketConfig = { 256, 16, 16384, 2, 0, 0 };
		if ( !m_Small.init( pSmall, bucketConfig ) || !m_Medium.init( pMedium ) || !m_Large.init( pLarge ) ) {
			return false;
		}
		const sirGeneralHeapConfig_t heapConfig = { &m_Small, &m_Medium, nullptr, &m_Large, pSmall, pMedium, pLarge, 256, 4096, false };
		return m_Heap.init( heapConfig ) && m_Heap.attachThread() && m_Adapter.init( &m_Heap, SIR_MEMORY_DOMAIN_GAMEPLAY, 0x434F4E54u );
	}

	bool shutdown( void )
	{
		return m_Heap.detachThread() && m_Heap.shutdown() && m_Small.shutdown() && m_Medium.shutdown();
	}
};

static bool testBindingAndAllocations( SIRContainerTestHeap *pFixture )
{
	SIRContainerHeapAdapter wrongDomain;
	SIRContainerHeapAdapter unbound;
	SIR_TEST_CHECK( unbound.getAllocator() == nullptr );
	SIR_TEST_CHECK( !unbound.init( nullptr, SIR_MEMORY_DOMAIN_GAMEPLAY, 1 ) );
	SIR_TEST_CHECK( !wrongDomain.init( &pFixture->m_Heap, SIR_MEMORY_DOMAIN_AUDIO, 1 ) );
	SIR_TEST_CHECK( pFixture->m_Heap.isConfiguredForDomain( SIR_MEMORY_DOMAIN_GAMEPLAY ) );
	SIR_TEST_CHECK( !pFixture->m_Heap.isConfiguredForDomain( SIR_MEMORY_DOMAIN_CORE ) );

	const sirContainerAllocator_t *pAllocator = pFixture->m_Adapter.getAllocator();
	SIR_TEST_CHECK( pAllocator != nullptr && pAllocator->nDomainId == SIR_MEMORY_DOMAIN_GAMEPLAY );
	SIR_TEST_CHECK( pAllocator->nOwnerTag == 0x434F4E54u );

	const uint64_t nCoreBefore = pFixture->m_Memory.getDomainStats( SIR_MEMORY_DOMAIN_CORE ).nCommittedBytes;
	const uint64_t nGameplayBefore = pFixture->m_Memory.getDomainStats( SIR_MEMORY_DOMAIN_GAMEPLAY ).nCommittedBytes;
	{
		SIRVector<uint8_t> small( pAllocator );
		SIRVector<uint8_t> medium( pAllocator );
		SIRVector<uint8_t> large( pAllocator );
		SIR_TEST_CHECK( small.tryResize( 64 ) == SIR_CONTAINER_OK );
		SIR_TEST_CHECK( medium.tryResize( 1536 ) == SIR_CONTAINER_OK );
		SIR_TEST_CHECK( large.tryResize( 32768 ) == SIR_CONTAINER_OK );
		SIR_TEST_CHECK( pFixture->m_Heap.owns( small.data() ) && pFixture->m_Heap.owns( medium.data() ) &&
		                pFixture->m_Heap.owns( large.data() ) );
		SIR_TEST_CHECK( small.getSize() == 64 && medium.getSize() == 1536 && large.getSize() == 32768 );
		SIR_TEST_CHECK( small.getFootprint().nLiveDomainBytes >= 64 && large.getFootprint().nLiveDomainBytes >= 32768 );

		SIRVector<sirTestAlignedValue_t> aligned( pAllocator );
		SIR_TEST_CHECK( aligned.tryResize( 16 ) == SIR_CONTAINER_OK );
		SIR_TEST_CHECK( ( ( uintptr_t )aligned.data() % alignof( sirTestAlignedValue_t ) ) == 0 );

		SIRInlineVector<uint32_t, 8> inlineValues( pAllocator );
		for ( uint32_t i = 0; i < 200; ++i ) {
			SIR_TEST_CHECK( inlineValues.tryPushBack( i ) == SIR_CONTAINER_OK );
		}
		SIR_TEST_CHECK( inlineValues.getSize() == 200 );

		SIRFlatHashMap<uint32_t, uint32_t> hash( pAllocator );
		SIR_TEST_CHECK( hash.tryReserve( 512 ) == SIR_CONTAINER_OK );
		for ( uint32_t i = 0; i < 256; ++i ) {
			SIR_TEST_CHECK( hash.tryInsert( i * 7, i ) == SIR_CONTAINER_OK );
		}
		for ( uint32_t i = 0; i < 256; ++i ) {
			const uint32_t *pFound = hash.find( i * 7 );
			SIR_TEST_CHECK( pFound != nullptr && *pFound == i );
		}

		SIRString text( pAllocator );
		const char szPhrase[] = "A tracked container in the gameplay domain.";
		SIR_TEST_CHECK( text.tryAppendBytes( szPhrase, sizeof( szPhrase ) - 1 ) == SIR_CONTAINER_OK );
		SIR_TEST_CHECK( text.getByteLength() == sizeof( szPhrase ) - 1 );

		SIRPagedArray<uint32_t, 64> pages( pAllocator );
		for ( uint32_t i = 0; i < 1024; ++i ) {
			SIR_TEST_CHECK( pages.tryEmplaceBack( i ) == SIR_CONTAINER_OK );
		}
		SIR_TEST_CHECK( pages.tryGet( 1023 ) != nullptr && *pages.tryGet( 1023 ) == 1023 );

		SIRBitVector bits( pAllocator );
		SIR_TEST_CHECK( bits.tryResize( 16384 ) == SIR_CONTAINER_OK );
		SIR_TEST_CHECK( bits.trySet( 16383, true ) == SIR_CONTAINER_OK );

		SIRRingBuffer<uint32_t> ring( pAllocator );
		SIR_TEST_CHECK( ring.tryInitialize( 128 ) == SIR_CONTAINER_OK );
		for ( uint32_t i = 0; i < 128; ++i ) {
			SIR_TEST_CHECK( ring.tryPushBack( i ) == SIR_CONTAINER_OK );
		}
		SIR_TEST_CHECK( ring.tryPopFront() == SIR_CONTAINER_OK );
	}
	const uint64_t nGameplayAfter = pFixture->m_Memory.getDomainStats( SIR_MEMORY_DOMAIN_GAMEPLAY ).nCommittedBytes;
	SIR_TEST_CHECK( nGameplayAfter >= nGameplayBefore );
	SIR_TEST_CHECK( pFixture->m_Memory.getDomainStats( SIR_MEMORY_DOMAIN_CORE ).nCommittedBytes == nCoreBefore );
	SIR_TEST_CHECK( pFixture->m_Small.getLiveCount() == 0 );
	SIR_TEST_CHECK( pFixture->m_Medium.getAllocationCount() == 0 );
	SIR_TEST_CHECK( pFixture->m_Large.getAllocationCount() == 0 );
	return true;
}

static bool testDomainBudgetFailure( SIRContainerTestHeap *pFixture )
{
	const uint64_t nPage = SIRVirtualMemory::getPageSize();
	SIR_TEST_CHECK( pFixture->m_Large.trimUnusedPages() );
	const uint64_t nUsed = pFixture->m_Memory.getDomainStats( SIR_MEMORY_DOMAIN_GAMEPLAY ).nCommittedBytes;
	const uint64_t nLimit = nUsed == 0 ? nPage : nUsed;
	SIR_TEST_CHECK( pFixture->m_Memory.setDomainCommitLimit( SIR_MEMORY_DOMAIN_GAMEPLAY, nLimit ) );
	const sirContainerAllocator_t *pAllocator = pFixture->m_Adapter.getAllocator();
	SIRVector<uint32_t> values( pAllocator );
	SIR_TEST_CHECK( values.tryResize( ( uint32_t )( nPage * 128 / sizeof( uint32_t ) ) ) == SIR_CONTAINER_OUT_OF_MEMORY );
	SIR_TEST_CHECK( values.getSize() == 0 && values.data() == nullptr );
	return true;
}

int main( void )
{
	SIRContainerTestHeap fixture;
	if ( !fixture.init() ) {
		fputs( "Unable to initialize real SIREngine container heap fixture\n", stderr );
		return 1;
	}
	if ( !testBindingAndAllocations( &fixture ) || !testDomainBudgetFailure( &fixture ) || !fixture.shutdown() ) {
		fputs( "Container allocator integration tests failed\n", stderr );
		return 1;
	}
	return 0;
}
