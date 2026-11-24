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

#include <Core/Memory/Pointers/SIRUniquePtr.h>
#include <atomic>
#include <Core/Memory/Pointers/SIRObserverPtr.h>
#include <Core/Memory/Pointers/SIRNotNullPtr.h>
#include <Core/Memory/Pointers/SIROffsetPtr.h>
#include <Core/Memory/Pointers/SIRSelfRelativePtr.h>
#include <string.h>
#include <Core/Memory/Pointers/SIRSharedPtr.h>
#include <Core/Memory/Pointers/SIRHazardPtr.h>
#include <Core/Memory/Pointers/SIRRCUPtr.h>
#include <Core/Memory/Pointers/SIRDeferredPtr.h>
#include <Core/Memory/Pointers/SIRFenceRetiredPtr.h>
#include <Core/Memory/Pointers/SIRArenaPtr.h>
#include <Core/Platform/SIRThread.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <type_traits>
#include <utility>
#if defined( _WIN32 )
#include <malloc.h>
#endif

#define SIR_OPT_CHECK( bCondition )                                                                                                        \
	do {                                                                                                                                   \
		if ( !( bCondition ) ) {                                                                                                           \
			fprintf( stderr, "pointer optimization test %s:%d: %s\n", __FILE__, __LINE__, #bCondition );                                   \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( 0 )

static uint32_t s_nEmptyDeletions = 0;

struct sirEmptyDeleteTest_s
{
	void operator()( int *pObject ) const noexcept
	{
		++s_nEmptyDeletions;
		delete pObject;
	}
};

struct sirFinalEmptyDeleteTest_s final
{
	void operator()( int *pObject ) const noexcept
	{
		delete pObject;
	}
};

struct sirMoveOnlyDeleteTest_s
{
	uint32_t *pDeletions;
	explicit sirMoveOnlyDeleteTest_s( uint32_t *pCount ) noexcept : pDeletions( pCount )
	{
	}
	sirMoveOnlyDeleteTest_s( const sirMoveOnlyDeleteTest_s& ) = delete;
	sirMoveOnlyDeleteTest_s& operator=( const sirMoveOnlyDeleteTest_s& ) = delete;
	sirMoveOnlyDeleteTest_s( sirMoveOnlyDeleteTest_s&& rOther ) noexcept : pDeletions( rOther.pDeletions )
	{
		rOther.pDeletions = nullptr;
	}
	sirMoveOnlyDeleteTest_s& operator=( sirMoveOnlyDeleteTest_s&& rOther ) noexcept
	{
		if ( this != &rOther ) {
			pDeletions = rOther.pDeletions;
			rOther.pDeletions = nullptr;
		}
		return *this;
	}
	void operator()( int *pObject ) noexcept
	{
		SIR_OPT_CHECK( pDeletions != nullptr );
		++*pDeletions;
		delete pObject;
	}
};

static_assert( sizeof( SIRUniquePtr<int, sirEmptyDeleteTest_s> ) == sizeof( int * ), "empty deleter should be compressed" );
static_assert( sizeof( SIRUniquePtr<int, sirFinalEmptyDeleteTest_s> ) >= sizeof( int * ), "final deleter layout" );
static_assert( std::is_move_constructible<SIRUniquePtr<int, sirMoveOnlyDeleteTest_s>>::value, "move-only deleter" );
static_assert( !std::is_copy_constructible<SIRUniquePtr<int, sirMoveOnlyDeleteTest_s>>::value, "unique ownership" );
static_assert( sizeof( SIRArenaPtr<int> ) == 3 * sizeof( void * ), "arena compact layout" );

static void testUniqueOwnership( void )
{
	s_nEmptyDeletions = 0;
	{
		SIRUniquePtr<int, sirEmptyDeleteTest_s> first( new int( 3 ), {} );
		SIR_OPT_CHECK( first.isValid() );
		SIR_OPT_CHECK( *first == 3 );
		SIRUniquePtr<int, sirEmptyDeleteTest_s> second( std::move( first ) );
		SIR_OPT_CHECK( !first.isValid() );
		SIR_OPT_CHECK( *second == 3 );
		second.reset( second.get() );
		SIR_OPT_CHECK( s_nEmptyDeletions == 0 );
		second.reset( new int( 4 ) );
		SIR_OPT_CHECK( s_nEmptyDeletions == 1 );
		SIRUniquePtr<int, sirEmptyDeleteTest_s> *pSelf = &second;
		second = std::move( *pSelf );
		SIR_OPT_CHECK( *second == 4 );
		int *pReleased = second.release();
		SIR_OPT_CHECK( !second.isValid() );
		SIR_OPT_CHECK( *pReleased == 4 );
		delete pReleased;
	}
	SIR_OPT_CHECK( s_nEmptyDeletions == 1 );
	uint32_t nDeletions = 0;
	{
		SIRUniquePtr<int, sirMoveOnlyDeleteTest_s> first( new int( 5 ), sirMoveOnlyDeleteTest_s( &nDeletions ) );
		SIRUniquePtr<int, sirMoveOnlyDeleteTest_s> second( new int( 6 ), sirMoveOnlyDeleteTest_s( &nDeletions ) );
		second = std::move( first );
		SIR_OPT_CHECK( nDeletions == 1 );
		SIR_OPT_CHECK( !first.isValid() );
		SIR_OPT_CHECK( *second == 5 );
	}
	SIR_OPT_CHECK( nDeletions == 2 );
	{
		SIRUniquePtr<int, sirFinalEmptyDeleteTest_s> finalOwner( new int( 8 ), {} );
		SIR_OPT_CHECK( *finalOwner == 8 );
	}
}

typedef struct sirOptimizationAllocationState_s
{
	uint32_t nAllocate;
	uint32_t nDeallocate;
} sirOptimizationAllocationState_t;

static void *testAllocate( void *pContext, size_t nBytes, size_t nAlignment ) noexcept
{
	sirOptimizationAllocationState_t *pState = ( sirOptimizationAllocationState_t * )pContext;
	++pState->nAllocate;
	if ( nAlignment < sizeof( void * ) ) {
		nAlignment = sizeof( void * );
	}
#if defined( _WIN32 )
	return _aligned_malloc( nBytes, nAlignment );
#else
	void *pMemory = nullptr;
	return posix_memalign( &pMemory, nAlignment, nBytes ) == 0 ? pMemory : nullptr;
#endif
}

static void testDeallocate( void *pContext, void *pMemory ) noexcept
{
	sirOptimizationAllocationState_t *pState = ( sirOptimizationAllocationState_t * )pContext;
	++pState->nDeallocate;
#if defined( _WIN32 )
	_aligned_free( pMemory );
#else
	free( pMemory );
#endif
}

typedef struct sirOptimizationObject_s
{
	uint32_t *pDestroyed;
	explicit sirOptimizationObject_s( uint32_t *pCount ) noexcept : pDestroyed( pCount )
	{
	}
	~sirOptimizationObject_s( void ) noexcept
	{
		++*pDestroyed;
	}
} sirOptimizationObject_t;

static void testSharedLifetime( void )
{
	sirOptimizationAllocationState_t state = {};
	const sirSharedAllocationCallbacks_t callbacks = { &state, testAllocate, testDeallocate };
	uint32_t nDestroyed = 0;
	SIRWeakPtr<sirOptimizationObject_t> weak;
	{
		auto first = SIRMakeShared<sirOptimizationObject_t>( callbacks, &nDestroyed );
		SIR_OPT_CHECK( first && first.useCount() == 1 );
		weak = first;
		SIR_OPT_CHECK( weak.useCount() == 1 );
		{
			auto second = first;
			SIR_OPT_CHECK( second.useCount() == 2 );
			SIR_OPT_CHECK( weak.lock().useCount() == 3 );
		}
		SIR_OPT_CHECK( first.useCount() == 1 );
	}
	SIR_OPT_CHECK( weak.expired() );
	SIR_OPT_CHECK( !weak.lock() );
	SIR_OPT_CHECK( nDestroyed == 1 );
	SIR_OPT_CHECK( state.nAllocate == 1 && state.nDeallocate == 0 );
	weak.reset();
	SIR_OPT_CHECK( state.nDeallocate == 1 );
}

typedef struct sirCallbackState_s
{
	uint32_t nCalls;
	SIRDeferredRetirementQueue *pDeferred;
	int *pReentryValue;
} sirCallbackState_t;

static void testDestroyCallback( void *pContext, void * ) noexcept
{
	sirCallbackState_t *pState = ( sirCallbackState_t * )pContext;
	++pState->nCalls;
	if ( pState->nCalls == 1 && pState->pDeferred != nullptr ) {
		sirDeferredRetirementEntry_t added = { pState->pReentryValue, pState, testDestroyCallback };
		SIR_OPT_CHECK( pState->pDeferred->enqueue( added ) );
	}
}

static void testHazardReuseAndExhaustion( void )
{
	SIRHazardDomain empty( nullptr, 0 );
	std::atomic<int *> source( nullptr );
	SIRHazardPtr<int> guard;
	SIR_OPT_CHECK( !guard.protect( empty, source ) );

	int nValue = 17;
	source.store( &nValue );
	SIRHazardRecord records[4];
	SIRHazardDomain domain( records, 4 );
	SIRHazardPtr<int> guards[5];
	for ( uint32_t iGuard = 0; iGuard < 4; ++iGuard ) {
		SIR_OPT_CHECK( guards[iGuard].protect( domain, source ) );
		SIR_OPT_CHECK( *guards[iGuard].get() == 17 );
	}
	SIR_OPT_CHECK( !guards[4].protect( domain, source ) );
	guards[1].reset();
	SIR_OPT_CHECK( guards[4].protect( domain, source ) );
	SIRHazardPtr<int> moved( std::move( guards[4] ) );
	SIR_OPT_CHECK( guards[4].get() == nullptr );
	SIR_OPT_CHECK( moved.get() == &nValue );
	moved.reset();
	for ( uint32_t iGuard = 0; iGuard < 4; ++iGuard ) {
		guards[iGuard].reset();
	}
	SIR_OPT_CHECK( !domain.isProtected( &nValue ) );
	SIR_OPT_CHECK( !domain.isProtected( nullptr ) );

	sirCallbackState_t state = {};
	sirHazardRetiredEntry_t entries[1] = { { &nValue, &state, testDestroyCallback } };
	SIRHazardRetireList list( domain, entries, 1 );
	SIR_OPT_CHECK( guard.protect( domain, source ) );
	SIR_OPT_CHECK( list.retire( entries[0] ) );
	SIR_OPT_CHECK( list.collect() == 0 );
	guard.reset();
	SIR_OPT_CHECK( list.collect() == 1 );
	SIR_OPT_CHECK( state.nCalls == 1 );
}

static void testRCUReuseAndExhaustion( void )
{
	SIRRCUDomain empty( nullptr, 0 );
	std::atomic<int *> source( nullptr );
	SIRRCUPtr<int> guard;
	SIR_OPT_CHECK( !guard.read( empty, source ) );

	int nFirst = 11;
	int nSecond = 22;
	source.store( &nFirst );
	SIRRCUReaderRecord records[4];
	SIRRCUDomain domain( records, 4 );
	SIRRCUPtr<int> readers[5];
	for ( uint32_t iReader = 0; iReader < 4; ++iReader ) {
		SIR_OPT_CHECK( readers[iReader].read( domain, source ) );
		SIR_OPT_CHECK( *readers[iReader].get() == 11 );
	}
	SIR_OPT_CHECK( !readers[4].read( domain, source ) );
	const sirRCURetiredEntry_t previous = domain.publish( source, &nSecond );
	SIR_OPT_CHECK( previous.pObject == &nFirst );
	SIR_OPT_CHECK( !domain.canReclaim( previous.nEpoch ) );
	readers[1].reset();
	SIR_OPT_CHECK( readers[4].read( domain, source ) );
	SIR_OPT_CHECK( *readers[4].get() == 22 );
	for ( uint32_t iReader = 0; iReader < 5; ++iReader ) {
		readers[iReader].reset();
	}
	SIR_OPT_CHECK( domain.canReclaim( previous.nEpoch ) );
}

static void testDeferredBatchAndReentry( void )
{
	int szValues[96] = {};
	sirDeferredRetirementEntry_t szEntries[96] = {};
	SIRDeferredRetirementQueue queue( szEntries, 96 );
	sirCallbackState_t state = {};
	SIR_OPT_CHECK( !queue.enqueue( { nullptr, &state, testDestroyCallback } ) );
	SIR_OPT_CHECK( !queue.enqueue( { &szValues[0], &state, nullptr } ) );
	for ( uint32_t iValue = 0; iValue < 96; ++iValue ) {
		SIR_OPT_CHECK( queue.enqueue( { &szValues[iValue], &state, testDestroyCallback } ) );
	}
	SIR_OPT_CHECK( !queue.enqueue( { &szValues[0], &state, testDestroyCallback } ) );
	SIR_OPT_CHECK( queue.drain( 0 ) == 0 );
	SIR_OPT_CHECK( queue.drain( 5 ) == 5 );
	SIR_OPT_CHECK( queue.pending() == 91 );
	SIR_OPT_CHECK( queue.drain( 1000 ) == 91 );
	SIR_OPT_CHECK( queue.pending() == 0 );
	SIR_OPT_CHECK( state.nCalls == 96 );

	sirDeferredRetirementEntry_t szReentry[2] = {};
	SIRDeferredRetirementQueue reentrant( szReentry, 2 );
	sirCallbackState_t reentryState = { 0, &reentrant, &szValues[1] };
	SIR_OPT_CHECK( reentrant.enqueue( { &szValues[0], &reentryState, testDestroyCallback } ) );
	SIR_OPT_CHECK( reentrant.drain( 2 ) == 2 );
	SIR_OPT_CHECK( reentryState.nCalls == 2 );
}

static void testFenceBatchAndOutOfOrder( void )
{
	int szValues[80] = {};
	sirFenceRetiredEntry_t szEntries[80] = {};
	SIRFenceRetirementQueue queue( szEntries, 80 );
	sirCallbackState_t state = {};
	const sirProviderCompletionSet_t ready = { 2, 4, 1, 1, { 100, 0, 0, 0 } };
	const sirProviderCompletionSet_t early = { 2, 4, 1, 1, { 10, 0, 0, 0 } };
	const sirProviderCompletionSet_t blocked = { 2, 4, 1, 1, { 200, 0, 0, 0 } };
	const sirProviderCompletionSet_t wrongGeneration = { 2, 5, 1, 1, { 300, 0, 0, 0 } };
	for ( uint32_t iValue = 0; iValue < 80; ++iValue ) {
		const sirProviderCompletionSet_t& rRequired = iValue % 2 == 0 ? early : blocked;
		SIR_OPT_CHECK( queue.enqueue( { &szValues[iValue], &state, testDestroyCallback, rRequired } ) );
	}
	SIR_OPT_CHECK( !queue.enqueue( { &szValues[0], &state, testDestroyCallback, early } ) );
	SIR_OPT_CHECK( queue.collect( wrongGeneration, 80 ) == 0 );
	SIR_OPT_CHECK( queue.collect( ready, 0 ) == 0 );
	SIR_OPT_CHECK( queue.collect( ready, 7 ) == 7 );
	SIR_OPT_CHECK( queue.collect( ready, 80 ) == 33 );
	SIR_OPT_CHECK( queue.pending() == 40 );
	SIR_OPT_CHECK( queue.collect( { 2, 4, 1, 1, { 200, 0, 0, 0 } }, 80 ) == 40 );
	SIR_OPT_CHECK( state.nCalls == 80 );
}

typedef struct sirHazardStressState_s
{
	SIRHazardDomain *pDomain;
	std::atomic<int *> *pSource;
	std::atomic<uint32_t> *pFailureCount;
} sirHazardStressState_t;

static void hazardStressWorker( void *pUserData )
{
	sirHazardStressState_t *pState = ( sirHazardStressState_t * )pUserData;
	for ( uint32_t iIteration = 0; iIteration < 8000; ++iIteration ) {
		SIRHazardPtr<int> ptr;
		bool bProtected = false;
		for ( uint32_t nAttempt = 0; nAttempt < 8 && !bProtected; ++nAttempt ) {
			bProtected = ptr.protect( *pState->pDomain, *pState->pSource );
		}
		if ( !bProtected || ptr.get() == nullptr || *ptr.get() != 77 ) {
			pState->pFailureCount->fetch_add( 1 );
		}
	}
}

static void testHazardConcurrentClaim( void )
{
	int nValue = 77;
	std::atomic<int *> source( &nValue );
	std::atomic<uint32_t> failures( 0 );
	SIRHazardRecord records[8];
	SIRHazardDomain domain( records, 8 );
	sirHazardStressState_t state = { &domain, &source, &failures };
	SIRThread workers[4];
	for ( uint32_t iThread = 0; iThread < 4; ++iThread ) {
		SIR_OPT_CHECK( workers[iThread].start( hazardStressWorker, &state ) );
	}
	for ( uint32_t iThread = 0; iThread < 4; ++iThread ) {
		SIR_OPT_CHECK( workers[iThread].join() );
	}
	SIR_OPT_CHECK( failures.load() == 0 );
}

// Deterministic model-based ownership exercise: multiple independent control
// blocks, repeated aliases, same-owner assignments, weak rebinds and moves.
static void testSharedAssignmentTransitions( void )
{
	sirOptimizationAllocationState_t state = {};
	const sirSharedAllocationCallbacks_t callbacks = { &state, testAllocate, testDeallocate };
	uint32_t nDestroyed = 0;
	SIRWeakPtr<sirOptimizationObject_t> delayed;
	{
		auto first = SIRMakeShared<sirOptimizationObject_t>( callbacks, &nDestroyed );
		auto second = SIRMakeShared<sirOptimizationObject_t>( callbacks, &nDestroyed );
		SIR_OPT_CHECK( first && second );

		SIRSharedPtr<sirOptimizationObject_t> szStrong[16];
		SIRWeakPtr<sirOptimizationObject_t> szWeak[16];
		uint32_t nSeed = 0x517acde1U;
		for ( uint32_t iStep = 0; iStep < 120000; ++iStep ) {
			nSeed = nSeed * 1664525U + 1013904223U;
			const uint32_t iTarget = ( nSeed >> 5 ) & 15U;
			const uint32_t iOther = ( ( nSeed >> 13 ) + 1U ) & 15U;
			switch ( ( nSeed >> 21 ) % 12U ) {
			case 0:
				szStrong[iTarget] = first;
				break;
			case 1:
				szStrong[iTarget] = second;
				break;
			case 2:
				szStrong[iTarget] = szStrong[iTarget];
				break;
			case 3:
				szStrong[iTarget] = szStrong[iOther];
				break;
			case 4:
				szStrong[iTarget].reset();
				break;
			case 5:
				szStrong[iTarget].swap( szStrong[iOther] );
				break;
			case 6: {
				SIRSharedPtr<sirOptimizationObject_t> moved( std::move( szStrong[iTarget] ) );
				szStrong[iTarget] = std::move( moved );
				break;
			}
			case 7:
				szWeak[iTarget] = szStrong[iOther];
				break;
			case 8:
				szWeak[iTarget] = szWeak[iOther];
				break;
			case 9:
				szWeak[iTarget] = szWeak[iTarget];
				break;
			case 10:
				szWeak[iTarget].reset();
				break;
			case 11: {
				auto acquired = szWeak[iTarget].lock();
				SIR_OPT_CHECK( !acquired || acquired.get() == first.get() || acquired.get() == second.get() );
				break;
			}
			}

			uint32_t nFirstOwners = 1;
			uint32_t nSecondOwners = 1;
			for ( uint32_t iOwner = 0; iOwner < 16; ++iOwner ) {
				if ( szStrong[iOwner].get() == first.get() ) {
					++nFirstOwners;
				}
				if ( szStrong[iOwner].get() == second.get() ) {
					++nSecondOwners;
				}
			}
			SIR_OPT_CHECK( first.useCount() == nFirstOwners );
			SIR_OPT_CHECK( second.useCount() == nSecondOwners );
		}
		delayed = first;
		for ( uint32_t iOwner = 0; iOwner < 16; ++iOwner ) {
			szStrong[iOwner].reset();
		}
		first.reset();
		second.reset();
		SIR_OPT_CHECK( nDestroyed == 2 );
		SIR_OPT_CHECK( delayed.expired() && !delayed.lock() );
	}
	SIR_OPT_CHECK( state.nAllocate == 2 );
	SIR_OPT_CHECK( state.nDeallocate == 1 );
	delayed.reset();
	SIR_OPT_CHECK( state.nDeallocate == 2 );
}

static void testLocalOwnershipTransitions( void )
{
	sirOptimizationAllocationState_t state = {};
	const sirSharedAllocationCallbacks_t callbacks = { &state, testAllocate, testDeallocate };
	uint32_t nDestroyed = 0;
	SIRLocalWeakPtr<sirOptimizationObject_t> weak;
	{
		auto first = SIRMakeLocalShared<sirOptimizationObject_t>( callbacks, &nDestroyed );
		auto second = first;
		weak = first;
		for ( uint32_t iStep = 0; iStep < 25000; ++iStep ) {
			second = first;
			SIR_OPT_CHECK( first.useCount() == 2 );
			auto promoted = weak.lock();
			SIR_OPT_CHECK( promoted.useCount() == 3 );
			promoted.reset();
			SIR_OPT_CHECK( first.useCount() == 2 );
		}
		second.reset();
		SIR_OPT_CHECK( weak.useCount() == 1 );
	}
	SIR_OPT_CHECK( nDestroyed == 1 && weak.expired() );
	SIR_OPT_CHECK( state.nDeallocate == 0 );
	weak.reset();
	SIR_OPT_CHECK( state.nDeallocate == 1 );
}

class SIRPointerLeftBase
{
public:
	virtual ~SIRPointerLeftBase( void ) = default;
	uint32_t nLeft = 3;
};

class SIRPointerRightBase
{
public:
	virtual ~SIRPointerRightBase( void ) = default;
	uint32_t nRight = 7;
};

class SIRPointerMultipleDerived : public SIRPointerLeftBase, public SIRPointerRightBase
{
public:
	explicit SIRPointerMultipleDerived( uint32_t *pDestroyed ) noexcept : m_pDestroyed( pDestroyed )
	{
	}
	~SIRPointerMultipleDerived( void ) override
	{
		++*m_pDestroyed;
	}

private:
	uint32_t *m_pDestroyed;
};

static void testMultipleInheritanceAndWeakConversion( void )
{
	sirOptimizationAllocationState_t state = {};
	const sirSharedAllocationCallbacks_t callbacks = { &state, testAllocate, testDeallocate };
	uint32_t nDestroyed = 0;
	SIRWeakPtr<SIRPointerRightBase> weak;
	{
		auto owner = SIRMakeShared<SIRPointerMultipleDerived>( callbacks, &nDestroyed );
		SIRSharedPtr<SIRPointerRightBase> right( owner );
		SIRSharedPtr<SIRPointerLeftBase> left( owner );
		weak = SIRWeakPtr<SIRPointerRightBase>( owner );
		SIR_OPT_CHECK( right.get() == static_cast<SIRPointerRightBase *>( owner.get() ) );
		SIR_OPT_CHECK( left.get() == static_cast<SIRPointerLeftBase *>( owner.get() ) );
		SIR_OPT_CHECK( ( void * )right.get() != ( void * )left.get() );
		SIR_OPT_CHECK( right->nRight == 7 && left->nLeft == 3 );
		SIR_OPT_CHECK( weak.lock()->nRight == 7 );
		SIR_OPT_CHECK( owner.useCount() == 3 );
	}
	SIR_OPT_CHECK( nDestroyed == 1 && weak.expired() );
	weak.reset();
	SIR_OPT_CHECK( state.nAllocate == 1 && state.nDeallocate == 1 );
}

static void testCompactPointerBoundaries( void )
{
	alignas( 16 ) uint8_t szBytes[32] = {};
	uint32_t *pValue = ::new ( szBytes + 16 ) uint32_t( 104U );
	SIROffsetPtr<uint32_t> offset( 16 );
	SIR_OPT_CHECK( offset.resolve( szBytes, 32 ) == pValue );
	SIR_OPT_CHECK( offset.resolve( szBytes, 19 ) == nullptr );
	SIR_OPT_CHECK( offset.resolve( nullptr, 32 ) == nullptr );
	SIROffsetPtr<uint32_t> misaligned( 15 );
	SIR_OPT_CHECK( misaligned.resolve( szBytes, 32 ) == nullptr );
	SIROffsetPtr<uint32_t> missing;
	SIR_OPT_CHECK( missing.resolve( szBytes, 32 ) == nullptr );
	offset.reset();
	SIR_OPT_CHECK( !offset.isValid() );

	typedef struct sirRelocationFixture_s
	{
		SIRSelfRelativePtr<uint32_t> relative;
		uint32_t nValue;
	} sirRelocationFixture_t;
	sirRelocationFixture_t first = {};
	first.nValue = 93;
	SIR_OPT_CHECK( first.relative.set( &first.nValue ) );
	SIR_OPT_CHECK( *first.relative == 93 );
	sirRelocationFixture_t second = {};
	memcpy( &second, &first, sizeof( second ) );
	SIR_OPT_CHECK( second.relative.get() == &second.nValue );
	SIR_OPT_CHECK( second.relative.set( nullptr ) );
	SIR_OPT_CHECK( !second.relative );

	SIRObserverPtr<const uint32_t> observer( &first.nValue );
	SIRNotNullPtr<const uint32_t> notNull( &first.nValue );
	SIR_OPT_CHECK( *observer == 93 && *notNull == 93 );
	observer.reset();
	SIR_OPT_CHECK( !observer.isValid() );
}

int main( void )
{
	testUniqueOwnership();
	testSharedAssignmentTransitions();
	testLocalOwnershipTransitions();
	testMultipleInheritanceAndWeakConversion();
	testCompactPointerBoundaries();
	testSharedLifetime();
	testHazardReuseAndExhaustion();
	testRCUReuseAndExhaustion();
	testDeferredBatchAndReentry();
	testFenceBatchAndOutOfOrder();
	testHazardConcurrentClaim();
	return 0;
}
