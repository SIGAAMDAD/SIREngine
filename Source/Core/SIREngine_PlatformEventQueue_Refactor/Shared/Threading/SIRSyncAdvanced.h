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

#ifndef __SIR_SYNCADVANCED_H__
#define __SIR_SYNCADVANCED_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <atomic>
#include <Core/Platform/Shared/Threading/SIRSync.h>

// FIFO spinlock for short, nonblocking critical sections. A ticket owner
// must keep running; do not use for I/O or scheduler-suspending jobs.
// Requires fewer than 2^31 simultaneously outstanding tickets.
class SIRTicketLock final
{
public:
	explicit SIRTicketLock( void ) noexcept;
	explicit SIRTicketLock( const SIRTicketLock& ) = delete;
	explicit SIRTicketLock( SIRTicketLock&& ) = delete;

	SIRTicketLock& operator=( const SIRTicketLock& ) = delete;
	SIRTicketLock& operator=( SIRTicketLock&& ) = delete;
	void lock( void ) noexcept;
	bool tryLock( void ) noexcept;
	void unlock( void ) noexcept;

private:
	std::atomic<uint32_t> m_nNextTicket;
	std::atomic<uint32_t> m_nServing;
};

// Caller-owned queue node: one distinct node per concurrent acquisition.
// A node must remain alive and must not be reused until its unlock returns.
// Queue nodes must not be moved while enqueued or holding the lock.
typedef struct sirMCSNode_s
{
	std::atomic<sirMCSNode_s *> pNext;
	std::atomic<bool> bWaiting;
} sirMCSNode_t;

// Scalable queued spinlock: each waiter spins on its own node, and the
// releasing owner explicitly transfers ownership to the next waiter.
class SIRMCSLock final
{
public:
	explicit SIRMCSLock( void ) noexcept;
	explicit SIRMCSLock( const SIRMCSLock& ) = delete;
	explicit SIRMCSLock( SIRMCSLock&& ) = delete;

	SIRMCSLock& operator=( const SIRMCSLock& ) = delete;
	SIRMCSLock& operator=( SIRMCSLock&& ) = delete;
	void lock( sirMCSNode_t& node ) noexcept;
	bool tryLock( sirMCSNode_t& node ) noexcept;
	void unlock( sirMCSNode_t& node ) noexcept;

private:
	std::atomic<sirMCSNode_t *> m_pTail;
};

// Bounded spin followed by a condition-variable wait. The fast path has
// no kernel wait and no engine heap allocations. Not a fiber-aware mutex.
class SIRAdaptiveMutex final
{
public:
	explicit SIRAdaptiveMutex( void ) noexcept;
	explicit SIRAdaptiveMutex( const SIRAdaptiveMutex& ) = delete;
	explicit SIRAdaptiveMutex( SIRAdaptiveMutex&& ) = delete;

	SIRAdaptiveMutex& operator=( const SIRAdaptiveMutex& ) = delete;
	SIRAdaptiveMutex& operator=( SIRAdaptiveMutex&& ) = delete;
	void lock( void ) noexcept;
	bool tryLock( void ) noexcept;
	void unlock( void ) noexcept;

private:
	std::atomic<uint32_t> m_nLocked;
	std::atomic<uint32_t> m_nWaiters;
	SIRMutex m_WaitMutex;
	SIRConditionVariable m_WaitCondition;
};

// Native mutex with a short userspace try-lock window. Intended only for
// very short metadata updates where the owner remains runnable; on extended
// contention it blocks on the native mutex instead of spinning indefinitely.
// No allocation, no auxiliary wait queue, and the native mutex owns fairness.
class SIRShortMetadataMutex final
{
public:
	explicit SIRShortMetadataMutex( void ) noexcept;
	explicit SIRShortMetadataMutex( const SIRShortMetadataMutex& ) = delete;
	explicit SIRShortMetadataMutex( SIRShortMetadataMutex&& ) = delete;

	SIRShortMetadataMutex& operator=( const SIRShortMetadataMutex& ) = delete;
	SIRShortMetadataMutex& operator=( SIRShortMetadataMutex&& ) = delete;
	void lock( void ) noexcept;
	bool tryLock( void ) noexcept;
	void unlock( void ) noexcept;

private:
	SIRMutex m_Mutex;
};

// One designated writer, many readers. A single atomic state word blocks
// new readers as soon as the writer announces intent; writers park while
// existing readers drain. Exclusive operations MUST be confined to one
// designated thread and must not overlap or re-enter. This is for mutable
// read-mostly metadata, not for GRDG immutable record lifetime protection.
class SIRSingleWriterReadMostlyMutex final
{
public:
	explicit SIRSingleWriterReadMostlyMutex( void ) noexcept;
	explicit SIRSingleWriterReadMostlyMutex( const SIRSingleWriterReadMostlyMutex& ) = delete;
	explicit SIRSingleWriterReadMostlyMutex( SIRSingleWriterReadMostlyMutex&& ) = delete;

	SIRSingleWriterReadMostlyMutex& operator=( const SIRSingleWriterReadMostlyMutex& ) = delete;
	SIRSingleWriterReadMostlyMutex& operator=( SIRSingleWriterReadMostlyMutex&& ) = delete;
	void lock( void ) noexcept;
	bool tryLock( void ) noexcept;
	void unlock( void ) noexcept;
	void lockRead( void ) noexcept;
	bool tryLockRead( void ) noexcept;
	void unlockRead( void ) noexcept;

private:
	static constexpr uint32_t WRITER_PENDING = UINT32_C( 0x80000000 );
	static constexpr uint32_t READER_MASK = UINT32_C( 0x7fffffff );
	std::atomic<uint32_t> m_nState;
	SIRMutex m_WaitMutex;
	SIRConditionVariable m_ReadersReady;
	SIRConditionVariable m_WritersReady;
};

// Writer-preferred shared/exclusive lock. Reader admission stops when a
// writer queues, preventing sustained new readers from starving a writer.
// Not recursive; readers must not upgrade while holding a shared lock.
class SIRReadWriteMutex final
{
public:
	explicit SIRReadWriteMutex( void ) noexcept;
	explicit SIRReadWriteMutex( const SIRReadWriteMutex& ) = delete;
	explicit SIRReadWriteMutex( SIRReadWriteMutex&& ) = delete;

	SIRReadWriteMutex& operator=( const SIRReadWriteMutex& ) = delete;
	SIRReadWriteMutex& operator=( SIRReadWriteMutex&& ) = delete;
	void lock( void ) noexcept;
	bool tryLock( void ) noexcept;
	void unlock( void ) noexcept;
	void lockRead( void ) noexcept;
	bool tryLockRead( void ) noexcept;
	void unlockRead( void ) noexcept;

private:
	SIRMutex m_Mutex;
	SIRConditionVariable m_ReadersReady;
	SIRConditionVariable m_WritersReady;
	uint32_t m_nReaders;
	uint32_t m_nWaitingWriters;
	bool m_bWriter;
};

// Reader-optimized writer-preferred lock. Readers acquire using an atomic
// state CAS, while queued writers and blocking readers use native waiting.
// Use for read-heavy metadata; writer-heavy workloads may prefer
// SIRReadWriteMutex due to lower contended write-side bookkeeping.
class SIRReadMostlyMutex final
{
public:
	explicit SIRReadMostlyMutex( void ) noexcept;
	explicit SIRReadMostlyMutex( const SIRReadMostlyMutex& ) = delete;
	explicit SIRReadMostlyMutex( SIRReadMostlyMutex&& ) = delete;

	SIRReadMostlyMutex& operator=( const SIRReadMostlyMutex& ) = delete;
	SIRReadMostlyMutex& operator=( SIRReadMostlyMutex&& ) = delete;
	void lock( void ) noexcept;
	bool tryLock( void ) noexcept;
	void unlock( void ) noexcept;
	void lockRead( void ) noexcept;
	bool tryLockRead( void ) noexcept;
	void unlockRead( void ) noexcept;

private:
	static constexpr uint32_t WRITER_ACTIVE = 0x80000000u;
	static constexpr uint32_t WRITER_PENDING = 0x40000000u;
	static constexpr uint32_t READER_MASK = 0x3fffffffu;

	// Readers use one atomic fast path. The wait mutex serializes admission
	// of pending writers and the sleep / wakeup protocol.
	std::atomic<uint32_t> m_nState;
	SIRMutex m_Mutex;
	SIRConditionVariable m_ReadersReady;
	SIRConditionVariable m_WritersReady;
	uint32_t m_nWaitingWriters;
};

// RAII reader-side ownership; the lock must outlive this object.
class SIRReadScopedLock final
{
public:
	explicit SIRReadScopedLock( SIRReadWriteMutex& mutex ) noexcept;
	explicit SIRReadScopedLock( const SIRReadScopedLock& ) = delete;
	explicit SIRReadScopedLock( SIRReadScopedLock&& ) = delete;
	~SIRReadScopedLock() noexcept;

	SIRReadScopedLock& operator=( const SIRReadScopedLock& ) = delete;
	SIRReadScopedLock& operator=( SIRReadScopedLock&& ) = delete;

private:
	SIRReadWriteMutex& m_Mutex;
};

// Reader-side RAII for SIRReadMostlyMutex.
class SIRReadMostlyScopedLock final
{
public:
	explicit SIRReadMostlyScopedLock( SIRReadMostlyMutex& mutex ) noexcept;
	explicit SIRReadMostlyScopedLock( const SIRReadMostlyScopedLock& ) = delete;
	explicit SIRReadMostlyScopedLock( SIRReadMostlyScopedLock&& ) = delete;
	~SIRReadMostlyScopedLock() noexcept;

	SIRReadMostlyScopedLock& operator=( const SIRReadMostlyScopedLock& ) = delete;
	SIRReadMostlyScopedLock& operator=( SIRReadMostlyScopedLock&& ) = delete;

private:
	SIRReadMostlyMutex& m_Mutex;
};

// One-shot atomic countdown; native blocking is only used for pending waits
// and the final wakeup. Join all producers and waiters before destruction.
class SIRCountDownEvent final
{
public:
	explicit SIRCountDownEvent( uint32_t nCount ) noexcept;
	explicit SIRCountDownEvent( const SIRCountDownEvent& ) = delete;
	explicit SIRCountDownEvent( SIRCountDownEvent&& ) = delete;

	SIRCountDownEvent& operator=( const SIRCountDownEvent& ) = delete;
	SIRCountDownEvent& operator=( SIRCountDownEvent&& ) = delete;
	void countDown( uint32_t nCount = 1 ) noexcept;
	void wait( void ) noexcept;
	bool isComplete( void ) noexcept;

private:
	SIRMutex m_Mutex;
	SIRConditionVariable m_Completed;
	std::atomic<uint32_t> m_nRemaining;
};

// Reusable rendezvous for dedicated threads. Exactly nParticipants threads
// must arrive in each generation; blocking inside a fixed-size job pool
// can deadlock if the pool cannot schedule every participant.
class SIRBarrier final
{
public:
	explicit SIRBarrier( uint32_t nParticipants ) noexcept;
	explicit SIRBarrier( const SIRBarrier& ) = delete;
	explicit SIRBarrier( SIRBarrier&& ) = delete;

	SIRBarrier& operator=( const SIRBarrier& ) = delete;
	SIRBarrier& operator=( SIRBarrier&& ) = delete;
	// Returns true for the final arrival of each generation.
	bool arriveAndWait( void ) noexcept;

private:
	SIRMutex m_Mutex;
	SIRConditionVariable m_Advanced;
	uint64_t m_nGeneration;
	uint32_t m_nParticipants;
	uint32_t m_nArrived;
};

#endif // __SIR_SYNCADVANCED_H__
