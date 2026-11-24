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

#include <Core/Memory/Pointers/SIRSharedPtr.h>
#include <atomic>
#include <Core/Memory/Pointers/SIRWeakPtr.h>
#include <Core/Memory/SIRSharedGeneralHeapAdapter.h>
#include <Core/Memory/Pointers/SIRLocalSharedPtr.h>
#include <Core/Memory/Pointers/SIRNotNullPtr.h>
#include <Core/Memory/Pointers/SIRComPtr.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <type_traits>
#if defined( __linux__ )
#include <pthread.h>
#endif
#if defined( _MSC_VER )
#include <malloc.h>
#endif

#define SIR_CHECK( bCondition )                                                                                                            \
	do {                                                                                                                                   \
		if ( !( bCondition ) ) {                                                                                                           \
			fprintf( stderr, "Smart pointer test failed at %d: %s\n", __LINE__, #bCondition );                                             \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( 0 )

typedef struct sirPointerTestAllocator_s
{
	uint32_t nAllocations;
	uint32_t nFrees;
	bool bFailNext;
} sirPointerTestAllocator_t;

static void *sirPointerTestAllocate( void *pContext, size_t nBytes, size_t nAlignment ) noexcept
{
	sirPointerTestAllocator_t *pState = ( sirPointerTestAllocator_t * )pContext;
	++pState->nAllocations;
	if ( pState->bFailNext ) {
		pState->bFailNext = false;
		return nullptr;
	}
	if ( nAlignment < sizeof( void * ) ) {
		nAlignment = sizeof( void * );
	}
	void *pResult = nullptr;
#if defined( _MSC_VER )
	pResult = _aligned_malloc( nBytes, nAlignment );
#else
	if ( posix_memalign( &pResult, nAlignment, nBytes ) != 0 ) {
		pResult = nullptr;
	}
#endif
	return pResult;
}

static void sirPointerTestDeallocate( void *pContext, void *pMemory ) noexcept
{
	sirPointerTestAllocator_t *pState = ( sirPointerTestAllocator_t * )pContext;
	++pState->nFrees;
#if defined( _MSC_VER )
	_aligned_free( pMemory );
#else
	free( pMemory );
#endif
}

typedef struct sirPointerTestObject_s
{
	uint32_t *pDestroyed;
	uint32_t nPayload;

	explicit sirPointerTestObject_s( uint32_t *pCounter, uint32_t nInitial ) noexcept : pDestroyed( pCounter ), nPayload( nInitial )
	{
	}
	~sirPointerTestObject_s( void ) noexcept
	{
		++*pDestroyed;
	}
} sirPointerTestObject_t;

class SIRPointerTestBase
{
public:
	virtual ~SIRPointerTestBase( void ) = default;
	virtual int value( void ) const noexcept = 0;
};

class SIRPointerTestDerived : public SIRPointerTestBase
{
public:
	explicit SIRPointerTestDerived( uint32_t *pDestroyed ) noexcept : m_pDestroyed( pDestroyed )
	{
	}
	~SIRPointerTestDerived( void ) override
	{
		++*m_pDestroyed;
	}
	int value( void ) const noexcept override
	{
		return 42;
	}

private:
	uint32_t *m_pDestroyed;
};

struct alignas( 64 ) sirPointerAlignedObject_t
{
	uint64_t nData;
	explicit sirPointerAlignedObject_t( uint64_t nValue ) noexcept : nData( nValue )
	{
	}
};

class SIRPointerTestComInterface
{
public:
	explicit SIRPointerTestComInterface( uint32_t *pDestroyed ) noexcept : m_pDestroyed( pDestroyed ), m_nRefs( 1 )
	{
	}
	uint32_t AddRef( void ) noexcept
	{
		return ++m_nRefs;
	}
	uint32_t Release( void ) noexcept
	{
		const uint32_t nCount = --m_nRefs;
		if ( nCount == 0 ) {
			++*m_pDestroyed;
			delete this;
		}
		return nCount;
	}
	uint32_t count( void ) const noexcept
	{
		return m_nRefs;
	}

private:
	uint32_t *m_pDestroyed;
	uint32_t m_nRefs;
};

static void sirPointerTestMakeCom( SIRPointerTestComInterface **pInterface, uint32_t *pDestroyed )
{
	*pInterface = new SIRPointerTestComInterface( pDestroyed );
}

static void sirTestSharedPointer( const sirSharedAllocationCallbacks_t& rCallbacks, sirPointerTestAllocator_t *pAllocator )
{
	uint32_t nDestroyed = 0;
	const uint32_t nBeginAllocations = pAllocator->nAllocations;
	SIRWeakPtr<sirPointerTestObject_t> weak;
	{
		auto owner = SIRMakeShared<sirPointerTestObject_t>( rCallbacks, &nDestroyed, 17 );
		SIR_CHECK( owner && owner->nPayload == 17 );
		SIR_CHECK( owner.useCount() == 1 );
		SIR_CHECK( pAllocator->nAllocations == nBeginAllocations + 1 );
		weak = owner;
		{
			auto copy = owner;
			SIR_CHECK( copy.useCount() == 2 );
			auto promoted = weak.lock();
			SIR_CHECK( promoted && promoted.useCount() == 3 );
			auto moved = std::move( copy );
			SIR_CHECK( !copy && moved.useCount() == 3 );
			auto& rMovedAlias = moved;
			moved = rMovedAlias;
			auto& rOwnerAlias = owner;
			owner = rOwnerAlias;
		}
		SIR_CHECK( owner.useCount() == 1 );
		SIR_CHECK( !weak.expired() );
	}
	SIR_CHECK( nDestroyed == 1 );
	SIR_CHECK( weak.expired() && !weak.lock() );
	SIR_CHECK( pAllocator->nFrees + 1 == pAllocator->nAllocations );
	weak.reset();
	SIR_CHECK( pAllocator->nFrees == pAllocator->nAllocations );

	pAllocator->bFailNext = true;
	auto failed = SIRMakeShared<sirPointerTestObject_t>( rCallbacks, &nDestroyed, 18 );
	SIR_CHECK( !failed && nDestroyed == 1 );

	uint32_t nDerivedDestroyed = 0;
	{
		auto derived = SIRMakeShared<SIRPointerTestDerived>( rCallbacks, &nDerivedDestroyed );
		SIRSharedPtr<SIRPointerTestBase> base = derived;
		SIRWeakPtr<SIRPointerTestDerived> derivedWeak( derived );
		SIRWeakPtr<SIRPointerTestBase> baseWeak( derivedWeak );
		SIR_CHECK( base->value() == 42 && base.useCount() == 2 );
		derived.reset();
		SIR_CHECK( baseWeak.lock()->value() == 42 );
	}
	SIR_CHECK( nDerivedDestroyed == 1 );

#if defined( __cpp_exceptions ) || defined( _CPPUNWIND )
	const uint32_t nBeforeThrowAllocations = pAllocator->nAllocations;
	const uint32_t nBeforeThrowFrees = pAllocator->nFrees;
	bool bCaught = false;
	try {
		class SIRThrowingObject
		{
		public:
			SIRThrowingObject( void )
			{
				throw 19;
			}
		};
		auto throwing = SIRMakeShared<SIRThrowingObject>( rCallbacks );
		( void )throwing;
	} catch ( int nException ) {
		bCaught = nException == 19;
	}
	SIR_CHECK( bCaught );
	SIR_CHECK( pAllocator->nAllocations == nBeforeThrowAllocations + 1 );
	SIR_CHECK( pAllocator->nFrees == nBeforeThrowFrees + 1 );
#endif

	auto aligned = SIRMakeShared<sirPointerAlignedObject_t>( rCallbacks, 123 );
	SIR_CHECK( aligned && ( ( uintptr_t )aligned.get() & 63 ) == 0 );
	SIR_CHECK( aligned->nData == 123 );
	aligned.reset();
}

static void sirTestLocalPointer( const sirSharedAllocationCallbacks_t& rCallbacks )
{
	uint32_t nDestroyed = 0;
	SIRLocalWeakPtr<sirPointerTestObject_t> weak;
	{
		auto first = SIRMakeLocalShared<sirPointerTestObject_t>( rCallbacks, &nDestroyed, 22 );
		auto second = first;
		weak = SIRLocalWeakPtr<sirPointerTestObject_t>( first );
		SIR_CHECK( first.useCount() == 2 );
		second.reset();
		SIR_CHECK( weak.lock()->nPayload == 22 );
	}
	SIR_CHECK( nDestroyed == 1 && weak.expired() );
	weak.reset();
}

static void sirTestNotNull( void )
{
	int nValue = 100;
	SIRNotNullPtr<int> ptr( &nValue );
	SIR_CHECK( *ptr == 100 );
	*ptr = 101;
	SIR_CHECK( nValue == 101 && ptr.get() == &nValue );
	static_assert( !std::is_default_constructible<SIRNotNullPtr<int>>::value, "non-null pointers cannot default construct" );
	static_assert( !std::is_constructible<SIRNotNullPtr<int>, std::nullptr_t>::value, "non-null pointers cannot take nullptr" );
}

static void sirTestComPointer( void )
{
	uint32_t nDestroyed = 0;
	{
		SIRComPtr<SIRPointerTestComInterface> first;
		first.attach( new SIRPointerTestComInterface( &nDestroyed ) );
		SIR_CHECK( first->count() == 1 );
		{
			auto second = first;
			SIR_CHECK( first->count() == 2 );
			SIRComPtr<SIRPointerTestComInterface> third( std::move( second ) );
			SIR_CHECK( !second && third->count() == 2 );
			third.copyFrom( third.get() );
			SIR_CHECK( third->count() == 2 );
		}
		SIR_CHECK( first->count() == 1 );
		sirPointerTestMakeCom( first.put(), &nDestroyed );
		SIR_CHECK( nDestroyed == 1 && first->count() == 1 );
		SIRPointerTestComInterface *pDetached = first.detach();
		SIR_CHECK( !first && pDetached->count() == 1 );
		first.attach( pDetached );
		pDetached->AddRef();
		first.attach( pDetached );
	}
	SIR_CHECK( nDestroyed == 2 );
}

#if defined( __linux__ )
typedef struct sirPointerThreadArgs_s
{
	SIRWeakPtr<sirPointerTestObject_t> *pWeak;
	std::atomic<uint32_t> *pStarted;
} sirPointerThreadArgs_t;

static void *sirPointerThreadWorker( void *pData )
{
	sirPointerThreadArgs_t *pArgs = ( sirPointerThreadArgs_t * )pData;
	pArgs->pStarted->fetch_add( 1, std::memory_order_release );
	for ( uint32_t iIteration = 0; iIteration < 20000; ++iIteration ) {
		auto owner = pArgs->pWeak->lock();
		if ( owner ) {
			SIR_CHECK( owner->nPayload == 77 );
			auto extra = owner;
			SIR_CHECK( extra->nPayload == 77 );
		}
	}
	return nullptr;
}

static void sirTestConcurrentWeakLock( const sirSharedAllocationCallbacks_t& rCallbacks )
{
	uint32_t nDestroyed = 0;
	auto owner = SIRMakeShared<sirPointerTestObject_t>( rCallbacks, &nDestroyed, 77 );
	SIRWeakPtr<sirPointerTestObject_t> weak( owner );
	std::atomic<uint32_t> nStarted( 0 );
	pthread_t szThreads[4];
	sirPointerThreadArgs_t args{ &weak, &nStarted };
	for ( uint32_t iIndex = 0; iIndex < 4; ++iIndex ) {
		SIR_CHECK( pthread_create( &szThreads[iIndex], nullptr, sirPointerThreadWorker, &args ) == 0 );
	}
	while ( nStarted.load( std::memory_order_acquire ) != 4 ) {
	}
	owner.reset();
	for ( uint32_t iIndex = 0; iIndex < 4; ++iIndex ) {
		SIR_CHECK( pthread_join( szThreads[iIndex], nullptr ) == 0 );
	}
	SIR_CHECK( weak.expired() && !weak.lock() && nDestroyed == 1 );
}
#endif

int main( void )
{
	sirPointerTestAllocator_t allocator{};
	const sirSharedAllocationCallbacks_t callbacks{ &allocator, sirPointerTestAllocate, sirPointerTestDeallocate };

	sirTestSharedPointer( callbacks, &allocator );
	sirTestLocalPointer( callbacks );
	sirTestNotNull();
	sirTestComPointer();
#if defined( __linux__ )
	sirTestConcurrentWeakLock( callbacks );
#endif
	SIR_CHECK( allocator.nAllocations == allocator.nFrees + 1 ); // One intentional allocation failure.
	printf( "SIR ownership pointer tests passed (%u allocations, %u frees)\n", allocator.nAllocations, allocator.nFrees );
	return 0;
}
