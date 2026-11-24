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
#ifndef __SIR_PUBLISHEDSTATE_H__
#define __SIR_PUBLISHEDSTATE_H__
#include <Core/SIRPCH.h>
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif
#include <atomic>
#include <type_traits>
#include <stdint.h>

template <typename T, uint32_t Capacity>
class SIRPublishedState;

// A reader owns a pin on one immutable revision. No OS mutex, heap allocation,
// or publication-frame rendezvous is required in the reader path.
template <typename T, uint32_t Capacity>
class SIRStateReadLease
{
public:
	SIRStateReadLease( void ) noexcept : m_pOwner( nullptr ), m_pState( nullptr ), m_nSlot( UINT32_MAX ), m_nRevision( 0 )
	{
	}
	~SIRStateReadLease( void )
	{
		release();
	}
	SIRStateReadLease( const SIRStateReadLease& ) = delete;
	SIRStateReadLease& operator=( const SIRStateReadLease& ) = delete;

	bool acquire( SIRPublishedState<T, Capacity>& owner ) noexcept
	{
		return owner.acquire( *this );
	}
	void release( void ) noexcept
	{
		if ( m_pOwner == nullptr ) {
			return;
		}

		m_pOwner->release( m_nSlot );
		m_pOwner = nullptr;
		m_pState = nullptr;
		m_nSlot = UINT32_MAX;
		m_nRevision = 0;
	}
	const T *get( void ) const noexcept
	{
		return m_pState;
	}
	uint64_t getRevision( void ) const noexcept
	{
		return m_nRevision;
	}

private:
	friend class SIRPublishedState<T, Capacity>;
	SIRPublishedState<T, Capacity> *m_pOwner;
	const T *m_pState;
	uint32_t m_nSlot;
	uint64_t m_nRevision;
};

// Single-writer / multiple-reader triple (or larger) buffered publication.
// The writer prepares only closed, unpinned slots. Readers pin a slot with a
// CAS before verifying its generation-tagged publication token. A closed slot
// can never be pinned until the writer has finished rebuilding its contents.
// beginWrite/commitWrite must run on the one designated owner thread.
template <typename T, uint32_t Capacity = 3>
class SIRPublishedState
{
public:
	SIRPublishedState( void ) noexcept : m_nPublished( 0 ), m_nRevision( 0 ), m_nWriteSlot( UINT32_MAX ), m_nCursor( 0 )
	{
		static_assert( Capacity >= 2 && Capacity <= 15, "publication tokens support 2 through 15 state slots" );
		static_assert( std::is_trivially_copyable<T>::value, "published state must be trivially copyable" );
		static_assert( std::is_nothrow_default_constructible<T>::value, "published state must be nothrow default constructible" );
	}
	SIRPublishedState( const SIRPublishedState& ) = delete;
	SIRPublishedState& operator=( const SIRPublishedState& ) = delete;

	bool beginWrite( T **ppState ) noexcept
	{
		if ( ppState == nullptr || m_nWriteSlot != UINT32_MAX || m_nRevision >= UINT64_MAX / 16 - 1 ) {
			return false;
		}
		const uint64_t nPublished = m_nPublished.load( std::memory_order_seq_cst );
		for ( uint32_t iOffset = 0; iOffset < Capacity; ++iOffset ) {
			const uint32_t iSlot = ( m_nCursor + iOffset ) % Capacity;
			if ( nPublished != 0 && iSlot == ( ( nPublished & 15U ) - 1 ) ) {
				continue;
			}
			if ( m_szSlots[iSlot].m_nPins.load( std::memory_order_seq_cst ) != CLOSED ) {
				continue;
			}

			m_nWriteSlot = iSlot;
			m_nCursor = ( iSlot + 1 ) % Capacity;
			*ppState = &m_szSlots[iSlot].m_State;
			return true;
		}
		return false;
	}

	bool commitWrite( void ) noexcept
	{
		if ( m_nWriteSlot == UINT32_MAX ) {
			return false;
		}
		const uint64_t nPrevious = m_nPublished.load( std::memory_order_seq_cst );
		const uint32_t iPublishedSlot = m_nWriteSlot;
		const uint64_t nNextRevision = m_nRevision + 1;
		const uint64_t nNext = ( nNextRevision << 4U ) | static_cast<uint64_t>( iPublishedSlot + 1 );

		// Open the finished immutable slot, then make the new token visible.
		m_szSlots[iPublishedSlot].m_nPins.store( 0, std::memory_order_seq_cst );
		m_nPublished.store( nNext, std::memory_order_seq_cst );
		// An old reader can enter between publication and closure, but must fail
		// the token recheck and release its pin before this slot is reusable.
		if ( nPrevious != 0 ) {
			const uint32_t iOld = static_cast<uint32_t>(  ( nPrevious & 15U ) - 1  );
			m_szSlots[iOld].m_nPins.fetch_or( CLOSED, std::memory_order_seq_cst );
		}
		m_nRevision = nNextRevision;
		m_nWriteSlot = UINT32_MAX;
		return true;
	}

	void cancelWrite( void ) noexcept
	{
		m_nWriteSlot = UINT32_MAX;
	}

	bool acquire( SIRStateReadLease<T, Capacity>& lease ) noexcept
	{
		if ( lease.m_pOwner != nullptr ) {
			return false;
		}
		for ( uint32_t nAttempts = 0; nAttempts < 256; ++nAttempts ) {
			const uint64_t nToken = m_nPublished.load( std::memory_order_seq_cst );
			if ( nToken == 0 ) {
				return false;
			}
			const uint32_t iSlot = static_cast<uint32_t>(  ( nToken & 15U ) - 1  );
			uint32_t nPins = m_szSlots[iSlot].m_nPins.load( std::memory_order_seq_cst );
			if ( ( nPins & CLOSED ) != 0 || nPins == CLOSED - 1 ) {
				continue;
			}
			if ( !m_szSlots[iSlot].m_nPins.compare_exchange_weak( nPins, nPins + 1, std::memory_order_seq_cst ) ) {
				continue;
			}
			if ( m_nPublished.load( std::memory_order_seq_cst ) == nToken ) {
				lease.m_pOwner = this;
				lease.m_pState = &m_szSlots[iSlot].m_State;
				lease.m_nSlot = iSlot;
				lease.m_nRevision = nToken >> 4U;
				return true;
			}
			m_szSlots[iSlot].m_nPins.fetch_sub( 1, std::memory_order_seq_cst );
		}
		return false;
	}

	uint64_t getRevision( void ) const noexcept
	{
		return m_nPublished.load( std::memory_order_acquire ) >> 4U;
	}

private:
	friend class SIRStateReadLease<T, Capacity>;
	static constexpr uint32_t CLOSED = 0x80000000U;

	struct Slot
	{
		Slot( void ) noexcept : m_nPins( CLOSED ), m_State{}
		{
		}
		std::atomic<uint32_t> m_nPins;
		T m_State;
	};

	void release( uint32_t iSlot ) noexcept
	{
		m_szSlots[iSlot].m_nPins.fetch_sub( 1, std::memory_order_seq_cst );
	}

	Slot m_szSlots[Capacity];
	std::atomic<uint64_t> m_nPublished;
	uint64_t m_nRevision;
	uint32_t m_nWriteSlot;
	uint32_t m_nCursor;
};
#endif // __SIR_PUBLISHEDSTATE_H__
