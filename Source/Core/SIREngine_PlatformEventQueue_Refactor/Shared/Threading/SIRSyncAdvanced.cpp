/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Core/Platform/Shared/Threading/SIRSyncAdvanced.h>
#include <Core/Platform/Shared/Memory/SIRVirtualMemory.h>
#include <Core/Platform/Shared/Diagnostics/SIRPlatformTime.h>
#include <Core/Platform/Shared/Diagnostics/SIRCPUInfo.h>
#include <Core/Platform/Shared/Threading/SIRThread.h>

#include <assert.h>

SIRTicketLock::SIRTicketLock( void ) noexcept : m_nNextTicket( 0 ), m_nServing( 0 )
{
}

void SIRTicketLock::lock( void ) noexcept
{
	const uint32_t nTicket = m_nNextTicket.fetch_add( 1, std::memory_order_relaxed );
	uint32_t nSpins = 0;

	while ( m_nServing.load( std::memory_order_acquire ) != nTicket ) {
		SIRThread::cpuRelax();
		++nSpins;
		if ( nSpins == 1024 ) {
			SIRThread::yieldThread();
			nSpins = 0;
		}
	}
}

bool SIRTicketLock::tryLock( void ) noexcept
{
	const uint32_t nServing = m_nServing.load( std::memory_order_acquire );
	uint32_t nExpected = nServing;
	return m_nNextTicket.compare_exchange_strong( nExpected, nServing + 1, std::memory_order_acquire, std::memory_order_relaxed );
}

void SIRTicketLock::unlock( void ) noexcept
{
	m_nServing.fetch_add( 1, std::memory_order_release );
}

SIRMCSLock::SIRMCSLock( void ) noexcept : m_pTail( nullptr )
{
}

void SIRMCSLock::lock( sirMCSNode_t& node ) noexcept
{
	node.pNext.store( nullptr, std::memory_order_relaxed );
	node.bWaiting.store( true, std::memory_order_relaxed );

	sirMCSNode_t *pPredecessor = m_pTail.exchange( &node, std::memory_order_acq_rel );
	if ( pPredecessor == nullptr ) {
		return;
	}

	pPredecessor->pNext.store( &node, std::memory_order_release );
	uint32_t nSpins = 0;
	while ( node.bWaiting.load( std::memory_order_acquire ) ) {
		SIRThread::cpuRelax();
		++nSpins;
		if ( nSpins == 1024 ) {
			SIRThread::yieldThread();
			nSpins = 0;
		}
	}
}

bool SIRMCSLock::tryLock( sirMCSNode_t& node ) noexcept
{
	node.pNext.store( nullptr, std::memory_order_relaxed );
	node.bWaiting.store( false, std::memory_order_relaxed );

	sirMCSNode_t *pExpected = nullptr;
	return m_pTail.compare_exchange_strong( pExpected, &node, std::memory_order_acquire, std::memory_order_relaxed );
}

void SIRMCSLock::unlock( sirMCSNode_t& node ) noexcept
{
	sirMCSNode_t *pSuccessor = node.pNext.load( std::memory_order_acquire );
	if ( pSuccessor == nullptr ) {
		sirMCSNode_t *pExpected = &node;
		if ( m_pTail.compare_exchange_strong( pExpected, nullptr, std::memory_order_release, std::memory_order_relaxed ) ) {
			return;
		}

		while ( ( pSuccessor = node.pNext.load( std::memory_order_acquire ) ) == nullptr ) {
			SIRThread::cpuRelax();
		}
	}

	pSuccessor->bWaiting.store( false, std::memory_order_release );
}

SIRAdaptiveMutex::SIRAdaptiveMutex( void ) noexcept : m_nLocked( 0 ), m_nWaiters( 0 )
{
}

bool SIRAdaptiveMutex::tryLock( void ) noexcept
{
	uint32_t nExpected = 0;
	return m_nLocked.compare_exchange_strong( nExpected, 1, std::memory_order_seq_cst, std::memory_order_seq_cst );
}

void SIRAdaptiveMutex::lock( void ) noexcept
{
	if ( tryLock() ) {
		return;
	}

	constexpr uint32_t SPIN_LIMIT = 128;
	for ( uint32_t iSpin = 0; iSpin < SPIN_LIMIT; ++iSpin ) {
		SIRThread::cpuRelax();
		if ( m_nLocked.load( std::memory_order_relaxed ) == 0 && tryLock() ) {
			return;
		}
	}

	// Announce the waiter before checking again under the wait mutex.
	// This closes the unlock/park lost-wakeup window.
	m_nWaiters.fetch_add( 1, std::memory_order_seq_cst );
	m_WaitMutex.lock();
	while ( !tryLock() ) {
		m_WaitCondition.wait( m_WaitMutex );
	}
	m_WaitMutex.unlock();
	m_nWaiters.fetch_sub( 1, std::memory_order_seq_cst );
}

void SIRAdaptiveMutex::unlock( void ) noexcept
{
	m_nLocked.store( 0, std::memory_order_seq_cst );
	if ( m_nWaiters.load( std::memory_order_seq_cst ) == 0 ) {
		return;
	}

	// Holding the wait mutex while signaling prevents a waiter from
	// crossing between its state check and condition-variable sleep.
	m_WaitMutex.lock();
	m_WaitCondition.notifyOne();
	m_WaitMutex.unlock();
}

SIRShortMetadataMutex::SIRShortMetadataMutex( void ) noexcept
{
}

void SIRShortMetadataMutex::lock( void ) noexcept
{
	if ( m_Mutex.tryLock() ) {
		return;
	}

	constexpr uint32_t SPIN_ATTEMPTS = 32;
	for ( uint32_t iAttempt = 0; iAttempt < SPIN_ATTEMPTS; ++iAttempt ) {
		SIRThread::cpuRelax();
		if ( m_Mutex.tryLock() ) {
			return;
		}
	}

	m_Mutex.lock();
}

bool SIRShortMetadataMutex::tryLock( void ) noexcept
{
	return m_Mutex.tryLock();
}

void SIRShortMetadataMutex::unlock( void ) noexcept
{
	m_Mutex.unlock();
}

SIRSingleWriterReadMostlyMutex::SIRSingleWriterReadMostlyMutex( void ) noexcept : m_nState( 0 )
{
}

bool SIRSingleWriterReadMostlyMutex::tryLockRead( void ) noexcept
{
	uint32_t nState = m_nState.load( std::memory_order_relaxed );
	while ( ( nState & WRITER_PENDING ) == 0 ) {
		if ( ( nState & READER_MASK ) == READER_MASK ) {
			return false;
		}
		if ( m_nState.compare_exchange_weak( nState, nState + 1, std::memory_order_acquire, std::memory_order_relaxed ) ) {
			return true;
		}
	}
	return false;
}

void SIRSingleWriterReadMostlyMutex::lockRead( void ) noexcept
{
	if ( tryLockRead() ) {
		return;
	}
	m_WaitMutex.lock();
	while ( !tryLockRead() ) {
		m_ReadersReady.wait( m_WaitMutex );
	}
	m_WaitMutex.unlock();
}

void SIRSingleWriterReadMostlyMutex::unlockRead( void ) noexcept
{
	const uint32_t nPrevious = m_nState.fetch_sub( 1, std::memory_order_release );
	assert( ( nPrevious & READER_MASK ) != 0 );
	if ( ( nPrevious & READER_MASK ) != 1 || ( nPrevious & WRITER_PENDING ) == 0 ) {
		return;
	}
	m_WaitMutex.lock();
	if ( m_nState.load( std::memory_order_acquire ) == WRITER_PENDING ) {
		m_WritersReady.notifyOne();
	}
	m_WaitMutex.unlock();
}

bool SIRSingleWriterReadMostlyMutex::tryLock( void ) noexcept
{
	uint32_t nExpected = 0;
	return m_nState.compare_exchange_strong( nExpected, WRITER_PENDING, std::memory_order_acquire, std::memory_order_relaxed );
}

void SIRSingleWriterReadMostlyMutex::lock( void ) noexcept
{
	if ( tryLock() ) {
		return;
	}
	m_WaitMutex.lock();
	m_nState.fetch_or( WRITER_PENDING, std::memory_order_acq_rel );
	while ( m_nState.load( std::memory_order_acquire ) != WRITER_PENDING ) {
		m_WritersReady.wait( m_WaitMutex );
	}
	m_WaitMutex.unlock();
}

void SIRSingleWriterReadMostlyMutex::unlock( void ) noexcept
{
	m_WaitMutex.lock();
	assert( m_nState.load( std::memory_order_relaxed ) == WRITER_PENDING );
	m_nState.store( 0, std::memory_order_release );
	m_ReadersReady.notifyAll();
	m_WaitMutex.unlock();
}

SIRReadWriteMutex::SIRReadWriteMutex( void ) noexcept : m_nReaders( 0 ), m_nWaitingWriters( 0 ), m_bWriter( false )
{
}

void SIRReadWriteMutex::lockRead( void ) noexcept
{
	m_Mutex.lock();
	while ( m_bWriter || m_nWaitingWriters != 0 ) {
		m_ReadersReady.wait( m_Mutex );
	}
	++m_nReaders;
	m_Mutex.unlock();
}

bool SIRReadWriteMutex::tryLockRead( void ) noexcept
{
	m_Mutex.lock();
	const bool bAvailable = !m_bWriter && m_nWaitingWriters == 0;
	if ( bAvailable ) {
		++m_nReaders;
	}
	m_Mutex.unlock();
	return bAvailable;
}

void SIRReadWriteMutex::unlockRead( void ) noexcept
{
	m_Mutex.lock();
	assert( m_nReaders != 0 );
	--m_nReaders;
	if ( m_nReaders == 0 && m_nWaitingWriters != 0 ) {
		m_WritersReady.notifyOne();
	}
	m_Mutex.unlock();
}

void SIRReadWriteMutex::lock( void ) noexcept
{
	m_Mutex.lock();
	++m_nWaitingWriters;
	while ( m_bWriter || m_nReaders != 0 ) {
		m_WritersReady.wait( m_Mutex );
	}
	--m_nWaitingWriters;
	m_bWriter = true;
	m_Mutex.unlock();
}

bool SIRReadWriteMutex::tryLock( void ) noexcept
{
	m_Mutex.lock();
	const bool bAvailable = !m_bWriter && m_nReaders == 0 && m_nWaitingWriters == 0;
	if ( bAvailable ) {
		m_bWriter = true;
	}
	m_Mutex.unlock();
	return bAvailable;
}

void SIRReadWriteMutex::unlock( void ) noexcept
{
	m_Mutex.lock();
	assert( m_bWriter );
	m_bWriter = false;
	if ( m_nWaitingWriters != 0 ) {
		m_WritersReady.notifyOne();
	} else {
		m_ReadersReady.notifyAll();
	}
	m_Mutex.unlock();
}

SIRReadMostlyMutex::SIRReadMostlyMutex( void ) noexcept : m_nState( 0 ), m_nWaitingWriters( 0 )
{
}

bool SIRReadMostlyMutex::tryLockRead( void ) noexcept
{
	uint32_t nState = m_nState.load( std::memory_order_relaxed );
	while ( ( nState & ( WRITER_ACTIVE | WRITER_PENDING ) ) == 0 ) {
		assert( ( nState & READER_MASK ) != READER_MASK );
		if ( m_nState.compare_exchange_weak( nState, nState + 1, std::memory_order_acquire, std::memory_order_relaxed ) ) {
			return true;
		}
	}
	return false;
}

void SIRReadMostlyMutex::lockRead( void ) noexcept
{
	if ( tryLockRead() ) {
		return;
	}

	m_Mutex.lock();
	while ( !tryLockRead() ) {
		m_ReadersReady.wait( m_Mutex );
	}
	m_Mutex.unlock();
}

void SIRReadMostlyMutex::unlockRead( void ) noexcept
{
	const uint32_t nPrevious = m_nState.fetch_sub( 1, std::memory_order_release );
	assert( ( nPrevious & READER_MASK ) != 0 );
	if ( ( nPrevious & READER_MASK ) != 1 || ( nPrevious & WRITER_PENDING ) == 0 ) {
		return;
	}

	// A queued writer must observe the last reader's release before waking.
	m_Mutex.lock();
	if ( m_nWaitingWriters != 0 && ( m_nState.load( std::memory_order_acquire ) & READER_MASK ) == 0 ) {
		m_WritersReady.notifyOne();
	}
	m_Mutex.unlock();
}

bool SIRReadMostlyMutex::tryLock( void ) noexcept
{
	uint32_t nExpected = 0;
	return m_nState.compare_exchange_strong( nExpected, WRITER_ACTIVE, std::memory_order_acquire, std::memory_order_relaxed );
}

void SIRReadMostlyMutex::lock( void ) noexcept
{
	if ( tryLock() ) {
		return;
	}

	m_Mutex.lock();
	++m_nWaitingWriters;
	m_nState.fetch_or( WRITER_PENDING, std::memory_order_acq_rel );

	while ( ( m_nState.load( std::memory_order_acquire ) & ( READER_MASK | WRITER_ACTIVE ) ) != 0 ) {
		m_WritersReady.wait( m_Mutex );
	}

	--m_nWaitingWriters;
	m_nState.store( WRITER_ACTIVE | WRITER_PENDING, std::memory_order_release );
	m_Mutex.unlock();
}

void SIRReadMostlyMutex::unlock( void ) noexcept
{
	m_Mutex.lock();
	assert( ( m_nState.load( std::memory_order_relaxed ) & WRITER_ACTIVE ) != 0 );
	if ( m_nWaitingWriters != 0 ) {
		m_nState.store( WRITER_PENDING, std::memory_order_release );
		m_WritersReady.notifyOne();
	} else {
		m_nState.store( 0, std::memory_order_release );
		m_ReadersReady.notifyAll();
	}
	m_Mutex.unlock();
}

SIRReadScopedLock::SIRReadScopedLock( SIRReadWriteMutex& mutex ) noexcept : m_Mutex( mutex )
{
	m_Mutex.lockRead();
}

SIRReadScopedLock::~SIRReadScopedLock( void ) noexcept
{
	m_Mutex.unlockRead();
}

SIRReadMostlyScopedLock::SIRReadMostlyScopedLock( SIRReadMostlyMutex& mutex ) noexcept : m_Mutex( mutex )
{
	m_Mutex.lockRead();
}

SIRReadMostlyScopedLock::~SIRReadMostlyScopedLock( void ) noexcept
{
	m_Mutex.unlockRead();
}

SIRCountDownEvent::SIRCountDownEvent( uint32_t nCount ) noexcept : m_nRemaining( nCount )
{
}

void SIRCountDownEvent::countDown( uint32_t nCount ) noexcept
{
	if ( nCount == 0 ) {
		return;
	}

	// The acquire-release CAS chain publishes every producer's writes to
	// waiters observing completion with an acquire load.
	uint32_t nRemaining = m_nRemaining.load( std::memory_order_relaxed );
	while ( nRemaining != 0 ) {
		const uint32_t nNext = nCount >= nRemaining ? 0 : nRemaining - nCount;
		if ( m_nRemaining.compare_exchange_weak( nRemaining, nNext, std::memory_order_acq_rel, std::memory_order_relaxed ) ) {
			if ( nNext == 0 ) {
				// Serialize with the waiter's predicate check and sleep.
				m_Mutex.lock();
				m_Completed.notifyAll();
				m_Mutex.unlock();
			}
			return;
		}
	}
}

void SIRCountDownEvent::wait( void ) noexcept
{
	if ( m_nRemaining.load( std::memory_order_acquire ) == 0 ) {
		return;
	}

	m_Mutex.lock();
	while ( m_nRemaining.load( std::memory_order_acquire ) != 0 ) {
		m_Completed.wait( m_Mutex );
	}
	m_Mutex.unlock();
}

bool SIRCountDownEvent::isComplete( void ) noexcept
{
	return m_nRemaining.load( std::memory_order_acquire ) == 0;
}

SIRBarrier::SIRBarrier( uint32_t nParticipants ) noexcept
    : m_nGeneration( 0 ), m_nParticipants( nParticipants == 0 ? 1 : nParticipants ), m_nArrived( 0 )
{
	assert( nParticipants != 0 );
}

bool SIRBarrier::arriveAndWait( void ) noexcept
{
	if ( m_nParticipants == 1 ) {
		return true;
	}

	m_Mutex.lock();
	const uint64_t nGeneration = m_nGeneration;
	++m_nArrived;
	if ( m_nArrived == m_nParticipants ) {
		m_nArrived = 0;
		++m_nGeneration;
		m_Advanced.notifyAll();
		m_Mutex.unlock();
		return true;
	}

	while ( m_nGeneration == nGeneration ) {
		m_Advanced.wait( m_Mutex );
	}
	m_Mutex.unlock();
	return false;
}
