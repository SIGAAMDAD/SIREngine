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

#include <Core/Platform/SIRSyncAdvanced.h>
#include <atomic>
#include <Core/Platform/SIRPlatformUtils.h>
#include <Core/Platform/SIRScopedLock.h>
#include <Core/Platform/SIRThread.h>

#include <stdio.h>
#include <stdlib.h>

#define SIR_TEST( condition )                                                                                                              \
	do {                                                                                                                                   \
		if ( !( condition ) ) {                                                                                                            \
			fprintf( stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition );                                                         \
			abort();                                                                                                                       \
		}                                                                                                                                  \
	} while ( false )

typedef struct sirTicketTest_s
{
	SIRTicketLock *pLock;
	uint32_t *pCount;
	uint32_t nIterations;
} sirTicketTest_t;

static void ticketWorker( void *pUserData )
{
	sirTicketTest_t *pTest = ( sirTicketTest_t * )pUserData;
	for ( uint32_t iIteration = 0; iIteration < pTest->nIterations; ++iIteration ) {
		SIRScopedLock<SIRTicketLock> guard( *pTest->pLock );
		++*pTest->pCount;
	}
}

static void testTicketLock( void )
{
	constexpr uint32_t N_THREADS = 6;
	constexpr uint32_t N_ITERATIONS = 5000;
	SIRTicketLock lock;
	SIRThread workers[N_THREADS];
	uint32_t nCount = 0;
	sirTicketTest_t context = { &lock, &nCount, N_ITERATIONS };

	SIR_TEST( lock.tryLock() );
	SIR_TEST( !lock.tryLock() );
	lock.unlock();

	for ( uint32_t iThread = 0; iThread < N_THREADS; ++iThread ) {
		SIR_TEST( workers[iThread].start( ticketWorker, &context ) );
	}
	for ( uint32_t iThread = 0; iThread < N_THREADS; ++iThread ) {
		SIR_TEST( workers[iThread].join() );
	}
	SIR_TEST( nCount == N_THREADS * N_ITERATIONS );
}

typedef struct sirMCSTest_s
{
	SIRMCSLock *pLock;
	uint32_t *pCount;
	uint32_t nIterations;
} sirMCSTest_t;

static void mcsWorker( void *pUserData )
{
	sirMCSTest_t *pTest = ( sirMCSTest_t * )pUserData;
	sirMCSNode_t node;
	for ( uint32_t iIteration = 0; iIteration < pTest->nIterations; ++iIteration ) {
		pTest->pLock->lock( node );
		++*pTest->pCount;
		pTest->pLock->unlock( node );
	}
}

static void testMCSLock( void )
{
	constexpr uint32_t N_THREADS = 6;
	constexpr uint32_t N_ITERATIONS = 5000;
	SIRMCSLock lock;
	SIRThread workers[N_THREADS];
	sirMCSNode_t owner;
	sirMCSNode_t contender;
	uint32_t nCount = 0;
	sirMCSTest_t context = { &lock, &nCount, N_ITERATIONS };

	SIR_TEST( lock.tryLock( owner ) );
	SIR_TEST( !lock.tryLock( contender ) );
	lock.unlock( owner );

	for ( uint32_t iThread = 0; iThread < N_THREADS; ++iThread ) {
		SIR_TEST( workers[iThread].start( mcsWorker, &context ) );
	}
	for ( uint32_t iThread = 0; iThread < N_THREADS; ++iThread ) {
		SIR_TEST( workers[iThread].join() );
	}
	SIR_TEST( nCount == N_THREADS * N_ITERATIONS );
	SIR_TEST( lock.tryLock( contender ) );
	lock.unlock( contender );
}

typedef struct sirMCSOrderTest_s
{
	SIRMCSLock *pLock;
	sirMCSNode_t node;
	std::atomic<uint32_t> *pNextOutput;
	uint32_t *pOutput;
	uint32_t nIdentity;
} sirMCSOrderTest_t;

static void orderedMCSWorker( void *pUserData )
{
	sirMCSOrderTest_t *pTest = ( sirMCSOrderTest_t * )pUserData;
	pTest->pLock->lock( pTest->node );
	const uint32_t iOutput = pTest->pNextOutput->fetch_add( 1 );
	pTest->pOutput[iOutput] = pTest->nIdentity;
	pTest->pLock->unlock( pTest->node );
}

static void testMCSOrderedHandoff( void )
{
	SIRMCSLock lock;
	sirMCSNode_t owner;
	std::atomic<uint32_t> nextOutput( 0 );
	uint32_t output[2] = {};
	sirMCSOrderTest_t first;
	sirMCSOrderTest_t second;
	SIRThread firstThread;
	SIRThread secondThread;

	first.pLock = &lock;
	first.pNextOutput = &nextOutput;
	first.pOutput = output;
	first.nIdentity = 1;
	second.pLock = &lock;
	second.pNextOutput = &nextOutput;
	second.pOutput = output;
	second.nIdentity = 2;

	lock.lock( owner );
	SIR_TEST( firstThread.start( orderedMCSWorker, &first ) );
	while ( owner.pNext.load( std::memory_order_acquire ) == nullptr ) {
		SIRPlatformUtils::yieldThread();
	}
	SIR_TEST( secondThread.start( orderedMCSWorker, &second ) );
	while ( first.node.pNext.load( std::memory_order_acquire ) == nullptr ) {
		SIRPlatformUtils::yieldThread();
	}
	lock.unlock( owner );

	SIR_TEST( firstThread.join() );
	SIR_TEST( secondThread.join() );
	SIR_TEST( nextOutput.load() == 2 );
	SIR_TEST( output[0] == 1 );
	SIR_TEST( output[1] == 2 );
}

typedef struct sirAdaptiveTest_s
{
	SIRAdaptiveMutex *pMutex;
	std::atomic<uint32_t> *pStarted;
	uint32_t *pCount;
	uint32_t nIterations;
} sirAdaptiveTest_t;

static void adaptiveWorker( void *pUserData )
{
	sirAdaptiveTest_t *pTest = ( sirAdaptiveTest_t * )pUserData;
	pTest->pStarted->fetch_add( 1, std::memory_order_release );

	for ( uint32_t iIteration = 0; iIteration < pTest->nIterations; ++iIteration ) {
		SIRScopedLock<SIRAdaptiveMutex> guard( *pTest->pMutex );
		++*pTest->pCount;
	}
}

static void testAdaptiveMutex( void )
{
	constexpr uint32_t N_THREADS = 6;
	constexpr uint32_t N_ITERATIONS = 5000;
	SIRAdaptiveMutex mutex;
	std::atomic<uint32_t> started( 0 );
	SIRThread workers[N_THREADS];
	uint32_t nCount = 0;
	sirAdaptiveTest_t context = { &mutex, &started, &nCount, N_ITERATIONS };

	mutex.lock();
	SIR_TEST( !mutex.tryLock() );
	for ( uint32_t iThread = 0; iThread < N_THREADS; ++iThread ) {
		SIR_TEST( workers[iThread].start( adaptiveWorker, &context ) );
	}
	while ( started.load( std::memory_order_acquire ) != N_THREADS ) {
		SIRPlatformUtils::yieldThread();
	}
	SIRPlatformUtils::sleepThread( 3 );
	mutex.unlock();

	for ( uint32_t iThread = 0; iThread < N_THREADS; ++iThread ) {
		SIR_TEST( workers[iThread].join() );
	}
	SIR_TEST( nCount == N_THREADS * N_ITERATIONS );
	SIR_TEST( mutex.tryLock() );
	mutex.unlock();
}

typedef struct sirReadWriteTest_s
{
	SIRReadWriteMutex lock;
	std::atomic<uint32_t> activeReaders;
	std::atomic<uint32_t> activeWriters;
	std::atomic<uint32_t> errors;
	uint32_t nValue = 0;
} sirReadWriteTest_t;

static void readerWorker( void *pUserData )
{
	sirReadWriteTest_t *pTest = ( sirReadWriteTest_t * )pUserData;
	for ( uint32_t iIteration = 0; iIteration < 4000; ++iIteration ) {
		SIRReadScopedLock guard( pTest->lock );
		pTest->activeReaders.fetch_add( 1, std::memory_order_seq_cst );
		if ( pTest->activeWriters.load() != 0 || pTest->nValue > 8000 ) {
			pTest->errors.fetch_add( 1 );
		}
		pTest->activeReaders.fetch_sub( 1, std::memory_order_seq_cst );
	}
}

static void writerWorker( void *pUserData )
{
	sirReadWriteTest_t *pTest = ( sirReadWriteTest_t * )pUserData;
	for ( uint32_t iIteration = 0; iIteration < 4000; ++iIteration ) {
		SIRScopedLock<SIRReadWriteMutex> guard( pTest->lock );
		pTest->activeWriters.fetch_add( 1, std::memory_order_seq_cst );
		if ( pTest->activeReaders.load() != 0 || pTest->activeWriters.load() != 1 ) {
			pTest->errors.fetch_add( 1 );
		}
		++pTest->nValue;
		pTest->activeWriters.fetch_sub( 1, std::memory_order_seq_cst );
	}
}

static void testReadWriteMutex( void )
{
	constexpr uint32_t N_READERS = 4;
	constexpr uint32_t N_WRITERS = 2;
	sirReadWriteTest_t context;
	SIRThread readers[N_READERS];
	SIRThread writers[N_WRITERS];

	SIR_TEST( context.lock.tryLockRead() );
	SIR_TEST( context.lock.tryLockRead() );
	SIR_TEST( !context.lock.tryLock() );
	context.lock.unlockRead();
	context.lock.unlockRead();
	SIR_TEST( context.lock.tryLock() );
	SIR_TEST( !context.lock.tryLockRead() );
	context.lock.unlock();

	for ( uint32_t iReader = 0; iReader < N_READERS; ++iReader ) {
		SIR_TEST( readers[iReader].start( readerWorker, &context ) );
	}
	for ( uint32_t iWriter = 0; iWriter < N_WRITERS; ++iWriter ) {
		SIR_TEST( writers[iWriter].start( writerWorker, &context ) );
	}
	for ( uint32_t iReader = 0; iReader < N_READERS; ++iReader ) {
		SIR_TEST( readers[iReader].join() );
	}
	for ( uint32_t iWriter = 0; iWriter < N_WRITERS; ++iWriter ) {
		SIR_TEST( writers[iWriter].join() );
	}
	SIR_TEST( context.errors.load() == 0 );
	SIR_TEST( context.nValue == 8000 );
}

// Regression: a writer must wake readers already asleep in lockRead().
// A previous fast-unlock experiment omitted this wakeup and deadlocked.
typedef struct sirBlockedReaderTest_s
{
	SIRReadWriteMutex *pLock;
	std::atomic<uint32_t> *pStarted;
	std::atomic<uint32_t> *pCompleted;
} sirBlockedReaderTest_t;

static void blockedReaderWorker( void *pUserData )
{
	sirBlockedReaderTest_t *pTest = ( sirBlockedReaderTest_t * )pUserData;
	pTest->pStarted->fetch_add( 1, std::memory_order_release );
	pTest->pLock->lockRead();
	pTest->pCompleted->fetch_add( 1, std::memory_order_release );
	pTest->pLock->unlockRead();
}

static void testReadWriteSleepingReaders( void )
{
	constexpr uint32_t N_ROUNDS = 20;
	constexpr uint32_t N_READERS = 3;

	for ( uint32_t iRound = 0; iRound < N_ROUNDS; ++iRound ) {
		SIRReadWriteMutex lock;
		std::atomic<uint32_t> started( 0 );
		std::atomic<uint32_t> completed( 0 );
		SIRThread readers[N_READERS];
		sirBlockedReaderTest_t test = { &lock, &started, &completed };

		lock.lock();
		for ( uint32_t iReader = 0; iReader < N_READERS; ++iReader ) {
			SIR_TEST( readers[iReader].start( blockedReaderWorker, &test ) );
		}
		while ( started.load( std::memory_order_acquire ) != N_READERS ) {
			SIRPlatformUtils::yieldThread();
		}
		SIRPlatformUtils::sleepThread( 1 );
		SIR_TEST( completed.load() == 0 );
		lock.unlock();

		for ( uint32_t iReader = 0; iReader < N_READERS; ++iReader ) {
			SIR_TEST( readers[iReader].join() );
		}
		SIR_TEST( completed.load() == N_READERS );
	}
}

typedef struct sirWriterPreferenceTest_s
{
	SIRReadWriteMutex *pLock;
	std::atomic<bool> *pStarted;
	std::atomic<bool> *pAcquired;
} sirWriterPreferenceTest_t;

static void writerPreferenceWorker( void *pUserData )
{
	sirWriterPreferenceTest_t *pTest = ( sirWriterPreferenceTest_t * )pUserData;
	pTest->pStarted->store( true, std::memory_order_release );
	pTest->pLock->lock();
	pTest->pAcquired->store( true, std::memory_order_release );
	pTest->pLock->unlock();
}

static void testReadWriteWriterPreference( void )
{
	SIRReadWriteMutex lock;
	std::atomic<bool> started( false );
	std::atomic<bool> acquired( false );
	SIRThread writer;
	sirWriterPreferenceTest_t context = { &lock, &started, &acquired };
	lock.lockRead();
	SIR_TEST( writer.start( writerPreferenceWorker, &context ) );
	while ( !started.load( std::memory_order_acquire ) ) {
		SIRPlatformUtils::yieldThread();
	}

	// Hold the first reader while the writer queues. New readers must then
	// be rejected, so the queued writer cannot be starved indefinitely.
	const uint64_t nDeadline = SIRPlatformUtils::getMonotonicNanoseconds() + 3000000000ULL;
	bool bRejected = false;
	while ( SIRPlatformUtils::getMonotonicNanoseconds() < nDeadline ) {
		if ( !lock.tryLockRead() ) {
			bRejected = true;
			break;
		}
		lock.unlockRead();
		SIRPlatformUtils::yieldThread();
	}

	lock.unlockRead();
	SIR_TEST( writer.join() );
	SIR_TEST( bRejected );
	SIR_TEST( acquired.load( std::memory_order_acquire ) );
}

typedef struct sirReadMostlyTest_s
{
	SIRReadMostlyMutex lock;
	std::atomic<uint32_t> activeReaders;
	std::atomic<uint32_t> activeWriters;
	std::atomic<uint32_t> errors;
	uint32_t nValue = 0;
} sirReadMostlyTest_t;

static void readMostlyReaderWorker( void *pUserData )
{
	sirReadMostlyTest_t *pTest = ( sirReadMostlyTest_t * )pUserData;
	for ( uint32_t iIteration = 0; iIteration < 5000; ++iIteration ) {
		SIRReadMostlyScopedLock guard( pTest->lock );
		pTest->activeReaders.fetch_add( 1 );
		if ( pTest->activeWriters.load() != 0 || pTest->nValue > 10000 ) {
			pTest->errors.fetch_add( 1 );
		}
		pTest->activeReaders.fetch_sub( 1 );
	}
}

static void readMostlyWriterWorker( void *pUserData )
{
	sirReadMostlyTest_t *pTest = ( sirReadMostlyTest_t * )pUserData;
	for ( uint32_t iIteration = 0; iIteration < 5000; ++iIteration ) {
		SIRScopedLock<SIRReadMostlyMutex> guard( pTest->lock );
		pTest->activeWriters.fetch_add( 1 );
		if ( pTest->activeReaders.load() != 0 || pTest->activeWriters.load() != 1 ) {
			pTest->errors.fetch_add( 1 );
		}
		++pTest->nValue;
		pTest->activeWriters.fetch_sub( 1 );
	}
}

static void testReadMostlyMutex( void )
{
	constexpr uint32_t N_READERS = 4;
	constexpr uint32_t N_WRITERS = 2;
	sirReadMostlyTest_t context;
	SIRThread readers[N_READERS];
	SIRThread writers[N_WRITERS];

	SIR_TEST( context.lock.tryLockRead() );
	SIR_TEST( context.lock.tryLockRead() );
	SIR_TEST( !context.lock.tryLock() );
	context.lock.unlockRead();
	context.lock.unlockRead();
	SIR_TEST( context.lock.tryLock() );
	SIR_TEST( !context.lock.tryLockRead() );
	context.lock.unlock();

	for ( uint32_t iReader = 0; iReader < N_READERS; ++iReader ) {
		SIR_TEST( readers[iReader].start( readMostlyReaderWorker, &context ) );
	}
	for ( uint32_t iWriter = 0; iWriter < N_WRITERS; ++iWriter ) {
		SIR_TEST( writers[iWriter].start( readMostlyWriterWorker, &context ) );
	}
	for ( uint32_t iReader = 0; iReader < N_READERS; ++iReader ) {
		SIR_TEST( readers[iReader].join() );
	}
	for ( uint32_t iWriter = 0; iWriter < N_WRITERS; ++iWriter ) {
		SIR_TEST( writers[iWriter].join() );
	}
	SIR_TEST( context.errors.load() == 0 );
	SIR_TEST( context.nValue == 10000 );
}

typedef struct sirReadMostlyBlockTest_s
{
	SIRReadMostlyMutex *pLock;
	std::atomic<uint32_t> *pStarted;
	std::atomic<uint32_t> *pCompleted;
} sirReadMostlyBlockTest_t;

static void readMostlyBlockedWorker( void *pUserData )
{
	sirReadMostlyBlockTest_t *pTest = ( sirReadMostlyBlockTest_t * )pUserData;
	pTest->pStarted->fetch_add( 1, std::memory_order_release );
	pTest->pLock->lockRead();
	pTest->pCompleted->fetch_add( 1 );
	pTest->pLock->unlockRead();
}

static void testReadMostlyBlockedWakeup( void )
{
	constexpr uint32_t N_ROUNDS = 15;
	constexpr uint32_t N_READERS = 3;

	for ( uint32_t iRound = 0; iRound < N_ROUNDS; ++iRound ) {
		SIRReadMostlyMutex lock;
		std::atomic<uint32_t> started( 0 );
		std::atomic<uint32_t> completed( 0 );
		SIRThread readers[N_READERS];
		sirReadMostlyBlockTest_t context = { &lock, &started, &completed };

		lock.lock();
		for ( uint32_t iReader = 0; iReader < N_READERS; ++iReader ) {
			SIR_TEST( readers[iReader].start( readMostlyBlockedWorker, &context ) );
		}
		while ( started.load( std::memory_order_acquire ) != N_READERS ) {
			SIRPlatformUtils::yieldThread();
		}
		SIRPlatformUtils::sleepThread( 1 );
		SIR_TEST( completed.load() == 0 );
		lock.unlock();

		for ( uint32_t iReader = 0; iReader < N_READERS; ++iReader ) {
			SIR_TEST( readers[iReader].join() );
		}
		SIR_TEST( completed.load() == N_READERS );
	}
}

typedef struct sirReadMostlyWriterPreference_s
{
	SIRReadMostlyMutex *pLock;
	std::atomic<bool> *pStarted;
	std::atomic<bool> *pAcquired;
} sirReadMostlyWriterPreference_t;

static void readMostlyPreferenceWorker( void *pUserData )
{
	sirReadMostlyWriterPreference_t *pTest = ( sirReadMostlyWriterPreference_t * )pUserData;
	pTest->pStarted->store( true, std::memory_order_release );
	pTest->pLock->lock();
	pTest->pAcquired->store( true, std::memory_order_release );
	pTest->pLock->unlock();
}

static void testReadMostlyWriterPreference( void )
{
	SIRReadMostlyMutex lock;
	std::atomic<bool> started( false );
	std::atomic<bool> acquired( false );
	SIRThread writer;
	sirReadMostlyWriterPreference_t context = { &lock, &started, &acquired };
	lock.lockRead();
	SIR_TEST( writer.start( readMostlyPreferenceWorker, &context ) );
	while ( !started.load( std::memory_order_acquire ) ) {
		SIRPlatformUtils::yieldThread();
	}

	const uint64_t nDeadline = SIRPlatformUtils::getMonotonicNanoseconds() + 3000000000ULL;
	bool bRejected = false;
	while ( SIRPlatformUtils::getMonotonicNanoseconds() < nDeadline ) {
		if ( !lock.tryLockRead() ) {
			bRejected = true;
			break;
		}
		lock.unlockRead();
		SIRPlatformUtils::yieldThread();
	}
	lock.unlockRead();
	SIR_TEST( writer.join() );
	SIR_TEST( bRejected );
	SIR_TEST( acquired.load( std::memory_order_acquire ) );
}

typedef struct sirEventTest_s
{
	SIRCountDownEvent *pEvent;
	std::atomic<bool> *pStart;
	std::atomic<uint32_t> *pAwakened;
} sirEventTest_t;

static void eventProducer( void *pUserData )
{
	sirEventTest_t *pTest = ( sirEventTest_t * )pUserData;
	while ( !pTest->pStart->load( std::memory_order_acquire ) ) {
		SIRPlatformUtils::cpuRelax();
	}
	pTest->pEvent->countDown();
}

static void eventWaiter( void *pUserData )
{
	sirEventTest_t *pTest = ( sirEventTest_t * )pUserData;
	pTest->pEvent->wait();
	pTest->pAwakened->fetch_add( 1 );
}

static void testCountDownEvent( void )
{
	constexpr uint32_t N_PRODUCERS = 6;
	SIRCountDownEvent event( N_PRODUCERS );
	std::atomic<bool> start( false );
	std::atomic<uint32_t> awakened( 0 );
	SIRThread producers[N_PRODUCERS];
	SIRThread waiters[3];
	sirEventTest_t context = { &event, &start, &awakened };

	SIR_TEST( !event.isComplete() );
	for ( uint32_t iWaiter = 0; iWaiter < 3; ++iWaiter ) {
		SIR_TEST( waiters[iWaiter].start( eventWaiter, &context ) );
	}
	for ( uint32_t iProducer = 0; iProducer < N_PRODUCERS; ++iProducer ) {
		SIR_TEST( producers[iProducer].start( eventProducer, &context ) );
	}
	SIR_TEST( awakened.load() == 0 );
	start.store( true, std::memory_order_release );
	event.wait();
	for ( uint32_t iProducer = 0; iProducer < N_PRODUCERS; ++iProducer ) {
		SIR_TEST( producers[iProducer].join() );
	}
	for ( uint32_t iWaiter = 0; iWaiter < 3; ++iWaiter ) {
		SIR_TEST( waiters[iWaiter].join() );
	}
	SIR_TEST( event.isComplete() );
	SIR_TEST( awakened.load() == 3 );
	event.countDown( UINT32_MAX );
	SIR_TEST( event.isComplete() );
	SIRCountDownEvent complete( 0 );
	complete.wait();
	SIR_TEST( complete.isComplete() );
}

// Multiple producers must publish their writes before any waiter observes
// completion. Repeated rounds exercise final-decrement / sleep races.
typedef struct sirEventPublicationTest_s
{
	SIRCountDownEvent *pEvent;
	uint32_t *pValues;
	std::atomic<uint32_t> *pErrors;
	uint32_t iProducer;
} sirEventPublicationTest_t;

static void eventPublicationProducer( void *pUserData )
{
	sirEventPublicationTest_t *pTest = ( sirEventPublicationTest_t * )pUserData;
	pTest->pValues[pTest->iProducer] = pTest->iProducer + 100;
	pTest->pEvent->countDown();
}

static void eventPublicationWaiter( void *pUserData )
{
	sirEventPublicationTest_t *pTest = ( sirEventPublicationTest_t * )pUserData;
	pTest->pEvent->wait();
	if ( !pTest->pEvent->isComplete() ) {
		pTest->pErrors->fetch_add( 1 );
	}
	for ( uint32_t iProducer = 0; iProducer < 4; ++iProducer ) {
		if ( pTest->pValues[iProducer] != iProducer + 100 ) {
			pTest->pErrors->fetch_add( 1 );
		}
	}
}

static void testCountDownEventPublication( void )
{
	constexpr uint32_t N_ROUNDS = 64;
	constexpr uint32_t N_PRODUCERS = 4;
	constexpr uint32_t N_WAITERS = 3;

	for ( uint32_t iRound = 0; iRound < N_ROUNDS; ++iRound ) {
		SIRCountDownEvent event( N_PRODUCERS );
		std::atomic<uint32_t> errors( 0 );
		uint32_t values[N_PRODUCERS] = {};
		SIRThread producers[N_PRODUCERS];
		SIRThread waiters[N_WAITERS];
		sirEventPublicationTest_t contexts[N_PRODUCERS];
		for ( uint32_t iProducer = 0; iProducer < N_PRODUCERS; ++iProducer ) {
			contexts[iProducer] = { &event, values, &errors, iProducer };
		}
		for ( uint32_t iWaiter = 0; iWaiter < N_WAITERS; ++iWaiter ) {
			SIR_TEST( waiters[iWaiter].start( eventPublicationWaiter, &contexts[0] ) );
		}
		for ( uint32_t iProducer = 0; iProducer < N_PRODUCERS; ++iProducer ) {
			SIR_TEST( producers[iProducer].start( eventPublicationProducer, &contexts[iProducer] ) );
		}
		for ( uint32_t iProducer = 0; iProducer < N_PRODUCERS; ++iProducer ) {
			SIR_TEST( producers[iProducer].join() );
		}
		for ( uint32_t iWaiter = 0; iWaiter < N_WAITERS; ++iWaiter ) {
			SIR_TEST( waiters[iWaiter].join() );
		}
		SIR_TEST( errors.load() == 0 );
		SIR_TEST( event.isComplete() );
		event.countDown( UINT32_MAX );
		SIR_TEST( event.isComplete() );
	}

	SIRCountDownEvent saturated( 5 );
	saturated.countDown( 0 );
	SIR_TEST( !saturated.isComplete() );
	saturated.countDown( 2 );
	SIR_TEST( !saturated.isComplete() );
	saturated.countDown( UINT32_MAX );
	SIR_TEST( saturated.isComplete() );
	saturated.wait();
	saturated.countDown();
}

typedef struct sirBarrierTest_s
{
	SIRBarrier *pBarrier;
	std::atomic<uint32_t> *pCounts;
	std::atomic<uint32_t> *pLeaders;
	std::atomic<uint32_t> *pErrors;
	uint32_t nThreads;
	uint32_t nRounds;
} sirBarrierTest_t;

static void barrierWorker( void *pUserData )
{
	sirBarrierTest_t *pTest = ( sirBarrierTest_t * )pUserData;
	for ( uint32_t iRound = 0; iRound < pTest->nRounds; ++iRound ) {
		pTest->pCounts[iRound].fetch_add( 1, std::memory_order_seq_cst );
		if ( pTest->pBarrier->arriveAndWait() ) {
			pTest->pLeaders->fetch_add( 1 );
		}
		if ( pTest->pCounts[iRound].load( std::memory_order_acquire ) != pTest->nThreads ) {
			pTest->pErrors->fetch_add( 1 );
		}
		if ( pTest->pBarrier->arriveAndWait() ) {
			pTest->pLeaders->fetch_add( 1 );
		}
	}
}

static void testBarrier( void )
{
	constexpr uint32_t N_THREADS = 5;
	constexpr uint32_t N_ROUNDS = 200;
	SIRBarrier barrier( N_THREADS );
	std::atomic<uint32_t> counts[N_ROUNDS];
	std::atomic<uint32_t> leaders( 0 );
	std::atomic<uint32_t> errors( 0 );
	SIRThread workers[N_THREADS];
	sirBarrierTest_t context = { &barrier, counts, &leaders, &errors, N_THREADS, N_ROUNDS };

	for ( uint32_t iThread = 0; iThread < N_THREADS; ++iThread ) {
		SIR_TEST( workers[iThread].start( barrierWorker, &context ) );
	}
	for ( uint32_t iThread = 0; iThread < N_THREADS; ++iThread ) {
		SIR_TEST( workers[iThread].join() );
	}
	SIR_TEST( errors.load() == 0 );
	SIR_TEST( leaders.load() == 2 * N_ROUNDS );
	for ( uint32_t iRound = 0; iRound < N_ROUNDS; ++iRound ) {
		SIR_TEST( counts[iRound].load() == N_THREADS );
	}
	SIRBarrier single( 1 );
	SIR_TEST( single.arriveAndWait() );
	SIR_TEST( single.arriveAndWait() );
}

typedef struct sirShortMetadataTest_s
{
	SIRShortMetadataMutex *pMutex;
	uint32_t *pValue;
	uint32_t nIterations;
} sirShortMetadataTest_t;

static void shortMetadataWorker( void *pUserData )
{
	sirShortMetadataTest_t *pTest = ( sirShortMetadataTest_t * )pUserData;
	for ( uint32_t iIteration = 0; iIteration < pTest->nIterations; ++iIteration ) {
		SIRScopedLock<SIRShortMetadataMutex> guard( *pTest->pMutex );
		++*pTest->pValue;
	}
}

static void testShortMetadataMutex( void )
{
	constexpr uint32_t N_THREADS = 4;
	constexpr uint32_t N_ITERATIONS = 7000;
	SIRShortMetadataMutex mutex;
	SIRThread workers[N_THREADS];
	uint32_t nValue = 0;
	sirShortMetadataTest_t context = { &mutex, &nValue, N_ITERATIONS };
	SIR_TEST( mutex.tryLock() );
	SIR_TEST( !mutex.tryLock() );
	mutex.unlock();
	for ( uint32_t iThread = 0; iThread < N_THREADS; ++iThread ) {
		SIR_TEST( workers[iThread].start( shortMetadataWorker, &context ) );
	}
	for ( uint32_t iThread = 0; iThread < N_THREADS; ++iThread ) {
		SIR_TEST( workers[iThread].join() );
	}
	SIR_TEST( nValue == N_THREADS * N_ITERATIONS );
}

typedef struct sirSingleWriterTest_s
{
	SIRSingleWriterReadMostlyMutex *pMutex;
	std::atomic<uint32_t> *pActiveReaders;
	std::atomic<uint32_t> *pWriterActive;
	std::atomic<uint32_t> *pErrors;
	uint32_t nIterations;
} sirSingleWriterTest_t;

static void singleWriterReadWorker( void *pUserData )
{
	sirSingleWriterTest_t *pTest = ( sirSingleWriterTest_t * )pUserData;
	for ( uint32_t iIteration = 0; iIteration < pTest->nIterations; ++iIteration ) {
		pTest->pMutex->lockRead();
		pTest->pActiveReaders->fetch_add( 1, std::memory_order_seq_cst );
		if ( pTest->pWriterActive->load( std::memory_order_seq_cst ) != 0 ) {
			pTest->pErrors->fetch_add( 1 );
		}
		SIRPlatformUtils::cpuRelax();
		pTest->pActiveReaders->fetch_sub( 1, std::memory_order_seq_cst );
		pTest->pMutex->unlockRead();
	}
}

static void singleWriterWriteWorker( void *pUserData )
{
	sirSingleWriterTest_t *pTest = ( sirSingleWriterTest_t * )pUserData;
	for ( uint32_t iIteration = 0; iIteration < pTest->nIterations; ++iIteration ) {
		pTest->pMutex->lock();
		pTest->pWriterActive->store( 1, std::memory_order_seq_cst );
		if ( pTest->pActiveReaders->load( std::memory_order_seq_cst ) != 0 ) {
			pTest->pErrors->fetch_add( 1 );
		}
		SIRPlatformUtils::cpuRelax();
		pTest->pWriterActive->store( 0, std::memory_order_seq_cst );
		pTest->pMutex->unlock();
	}
}

static void testSingleWriterReadMostlyMutex( void )
{
	constexpr uint32_t N_READERS = 4;
	constexpr uint32_t N_ROUNDS = 6000;
	SIRSingleWriterReadMostlyMutex mutex;
	std::atomic<uint32_t> nReaders( 0 );
	std::atomic<uint32_t> nWriterActive( 0 );
	std::atomic<uint32_t> nErrors( 0 );
	sirSingleWriterTest_t context = { &mutex, &nReaders, &nWriterActive, &nErrors, N_ROUNDS };
	SIRThread readers[N_READERS];
	SIRThread writer;

	SIR_TEST( mutex.tryLockRead() );
	SIR_TEST( mutex.tryLockRead() );
	SIR_TEST( !mutex.tryLock() );
	mutex.unlockRead();
	mutex.unlockRead();
	SIR_TEST( mutex.tryLock() );
	SIR_TEST( !mutex.tryLockRead() );
	mutex.unlock();
	SIR_TEST( mutex.tryLockRead() );
	mutex.unlockRead();

	for ( uint32_t iReader = 0; iReader < N_READERS; ++iReader ) {
		SIR_TEST( readers[iReader].start( singleWriterReadWorker, &context ) );
	}
	SIR_TEST( writer.start( singleWriterWriteWorker, &context ) );
	for ( uint32_t iReader = 0; iReader < N_READERS; ++iReader ) {
		SIR_TEST( readers[iReader].join() );
	}
	SIR_TEST( writer.join() );
	SIR_TEST( nErrors.load() == 0 );
	SIR_TEST( nReaders.load() == 0 );
	SIR_TEST( nWriterActive.load() == 0 );
}

int main( void )
{
	testSingleWriterReadMostlyMutex();
	testShortMetadataMutex();
	testTicketLock();
	testMCSLock();
	testMCSOrderedHandoff();
	testAdaptiveMutex();
	testReadWriteMutex();
	testReadWriteSleepingReaders();
	testReadWriteWriterPreference();
	testReadMostlyMutex();
	testReadMostlyBlockedWakeup();
	testReadMostlyWriterPreference();
	testCountDownEvent();
	testCountDownEventPublication();
	testBarrier();
	return 0;
}
