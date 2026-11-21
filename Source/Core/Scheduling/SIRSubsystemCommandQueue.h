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
#ifndef __SIRENGINE_SIRSUBSYSTEMCOMMANDQUEUE_H__
#define __SIRENGINE_SIRSUBSYSTEMCOMMANDQUEUE_H__
#include <Core/SIRPCH.h>
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif
#include <Core/Platform/SIRSync.h>
#include <stdint.h>
#include <type_traits>

template <typename T>
struct sirSubsystemCommand_s
{
	T value;
	uint64_t nTargetTick;
	uint64_t nTimestampNs;
	uint64_t nSequence;
};

// Bounded MPSC command ingress. Only the receiving owner drains commands.
// Stable ingress ordering is retained among eligible commands, even if
// producers enqueue commands for future simulation ticks out of order.
// All copying happens under the short metadata mutex; no callbacks run there.
template <typename T, uint32_t Capacity>
class SIRSubsystemCommandQueue
{
public:
	SIRSubsystemCommandQueue( void ) noexcept : m_nCount( 0 ), m_nNextSequence( 1 )
	{
		static_assert( Capacity > 0, "command capacity must be nonzero" );
		static_assert( std::is_trivially_copyable<T>::value, "commands must be trivially copyable" );
		static_assert( std::is_nothrow_default_constructible<T>::value, "commands must be nothrow default constructible" );
		static_assert( std::is_nothrow_copy_assignable<T>::value, "commands must be nothrow copy assignable" );
	}
	SIRSubsystemCommandQueue( const SIRSubsystemCommandQueue& ) = delete;
	SIRSubsystemCommandQueue& operator=( const SIRSubsystemCommandQueue& ) = delete;

	bool tryEnqueue( const T& value, uint64_t nTargetTick, uint64_t nTimestampNs ) noexcept
	{
		m_Mutex.lock();
		if ( m_nCount == Capacity || m_nNextSequence == UINT64_MAX ) {
			m_Mutex.unlock();
			return false;
		}

		sirSubsystemCommand_s<T>& entry = m_szEntries[m_nCount];
		entry.value = value;
		entry.nTargetTick = nTargetTick;
		entry.nTimestampNs = nTimestampNs;
		entry.nSequence = m_nNextSequence;
		++m_nNextSequence;
		++m_nCount;
		m_Mutex.unlock();
		return true;
	}

	// A bounded drain does not lose eligible commands when output fills. Future
	// commands stay queued and cannot prevent a later eligible command draining.
	uint32_t drainReady( uint64_t nTick, sirSubsystemCommand_s<T> *pOutput, uint32_t nOutputCapacity ) noexcept
	{
		if ( pOutput == nullptr || nOutputCapacity == 0 ) {
			return 0;
		}

		m_Mutex.lock();
		uint32_t nOut = 0;
		uint32_t nKeep = 0;
		for ( uint32_t iEntry = 0; iEntry < m_nCount; ++iEntry ) {
			const sirSubsystemCommand_s<T>& entry = m_szEntries[iEntry];
			if ( entry.nTargetTick <= nTick && nOut < nOutputCapacity ) {
				pOutput[nOut] = entry;
				++nOut;
			} else {
				m_szEntries[nKeep] = entry;
				++nKeep;
			}
		}
		m_nCount = nKeep;
		m_Mutex.unlock();
		return nOut;
	}

	uint32_t getPendingCount( void ) noexcept
	{
		m_Mutex.lock();
		const uint32_t nResult = m_nCount;
		m_Mutex.unlock();
		return nResult;
	}

private:
	SIRMutex m_Mutex;
	sirSubsystemCommand_s<T> m_szEntries[Capacity];
	uint32_t m_nCount;
	uint64_t m_nNextSequence;
};
#endif // __SIRENGINE_SIRSUBSYSTEMCOMMANDQUEUE_H__
