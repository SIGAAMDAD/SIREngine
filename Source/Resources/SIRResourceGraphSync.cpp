/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/
#include "SIRResourceGraphSync.h"
#include <stdlib.h>

SIRResourceGraphSync::SIRResourceGraphSync( void ) noexcept
    : m_OwnerThread( std::this_thread::get_id() ), m_pReaderEpochs( nullptr ), m_nPublicationEpoch( 1 ), m_nMaxReaders( 0 ),
      m_bWriterActive( false )
{
}

void SIRResourceGraphSync::configure( std::atomic<uint64_t> *pReaderEpochs, uint32_t nMaxReaders ) noexcept
{
	if ( m_pReaderEpochs != nullptr || m_bWriterActive || !isWriterThread() ) {
		abort();
	}

	m_pReaderEpochs = pReaderEpochs;
	m_nMaxReaders = nMaxReaders;
}

bool SIRResourceGraphSync::isWriterThread( void ) const noexcept
{
	return std::this_thread::get_id() == m_OwnerThread;
}

bool SIRResourceGraphSync::beginWrite( void ) noexcept
{
	if ( !isWriterThread() || m_bWriterActive ) {
		return false;
	}

	m_bWriterActive = true;
	return true;
}

void SIRResourceGraphSync::endWrite( void ) noexcept
{
	if ( !isWriterThread() || !m_bWriterActive ) {
		abort();
	}

	m_bWriterActive = false;
}

bool SIRResourceGraphSync::isWriteActive( void ) const noexcept
{
	return isWriterThread() && m_bWriterActive;
}

uint64_t SIRResourceGraphSync::currentEpoch( void ) const noexcept
{
	return m_nPublicationEpoch.load( std::memory_order_seq_cst );
}

void SIRResourceGraphSync::advanceEpoch( uint64_t nPreviousEpoch ) noexcept
{
	if ( !isWriterThread() || !m_bWriterActive || nPreviousEpoch >= UINT64_MAX - 2 ) {
		abort();
	}

	m_nPublicationEpoch.store( nPreviousEpoch + 1, std::memory_order_seq_cst );
}

uint64_t SIRResourceGraphSync::oldestProtectedEpoch( void ) const noexcept
{
	uint64_t nOldest = UINT64_MAX;
	for ( uint32_t iReader = 0; iReader < m_nMaxReaders; ++iReader ) {
		const uint64_t nEpoch = m_pReaderEpochs[iReader].load( std::memory_order_seq_cst );
		if ( nEpoch == UINT64_MAX ) {
			return 0; // Reader entry is in progress; nothing is reclaimable.
		}
		if ( nEpoch != 0 && nEpoch < nOldest ) {
			nOldest = nEpoch;
		}
	}
	return nOldest;
}

uint32_t SIRResourceGraphSync::countActiveReaders( void ) const noexcept
{
	uint32_t nCount = 0;
	for ( uint32_t iReader = 0; iReader < m_nMaxReaders; ++iReader ) {
		const uint64_t nEpoch = m_pReaderEpochs[iReader].load( std::memory_order_seq_cst );
		if ( nEpoch != 0 && nEpoch != UINT64_MAX - 1 ) {
			++nCount;
		}
	}
	return nCount;
}

uint32_t SIRResourceGraphSync::maxReaders( void ) const noexcept
{
	return m_nMaxReaders;
}

bool SIRResourceGraphSync::readersQuiescent( void ) const noexcept
{
	for ( uint32_t iReader = 0; iReader < m_nMaxReaders; ++iReader ) {
		if ( m_pReaderEpochs[iReader].load( std::memory_order_seq_cst ) != 0 ) {
			return false;
		}
	}
	return true;
}

void SIRResourceGraphSync::announceEpoch( uint32_t iReader ) const noexcept
{
	for ( ;; ) {
		const uint64_t nEpoch = currentEpoch();
		m_pReaderEpochs[iReader].store( nEpoch, std::memory_order_seq_cst );
		if ( nEpoch == currentEpoch() ) {
			return;
		}
		m_pReaderEpochs[iReader].store( UINT64_MAX, std::memory_order_seq_cst );
	}
}

uint64_t SIRResourceGraphSync::registerLane( void ) const noexcept
{
	for ( uint32_t iReader = 0; iReader < m_nMaxReaders; ++iReader ) {
		uint64_t nExpected = 0;
		if ( m_pReaderEpochs[iReader].compare_exchange_strong( nExpected, UINT64_MAX - 1, std::memory_order_seq_cst ) ) {
			return ( uint64_t )iReader + 1;
		}
	}
	return 0;
}

void SIRResourceGraphSync::releaseLane( uint64_t nReaderToken ) const noexcept
{
	if ( nReaderToken == 0 || nReaderToken > m_nMaxReaders ) {
		abort();
	}

	const uint32_t iReader = ( uint32_t )( nReaderToken - 1 );
	uint64_t nExpected = UINT64_MAX - 1;
	if ( !m_pReaderEpochs[iReader].compare_exchange_strong( nExpected, 0, std::memory_order_seq_cst ) ) {
		abort();
	}
}

uint64_t SIRResourceGraphSync::enterRegisteredLane( uint64_t nReaderToken ) const noexcept
{
	if ( nReaderToken == 0 || nReaderToken > m_nMaxReaders ) {
		return 0;
	}

	const uint32_t iReader = ( uint32_t )( nReaderToken - 1 );
	uint64_t nExpected = UINT64_MAX - 1;
	if ( !m_pReaderEpochs[iReader].compare_exchange_strong( nExpected, UINT64_MAX, std::memory_order_seq_cst ) ) {
		return 0;
	}

	announceEpoch( iReader );
	return nReaderToken | ( UINT64_C( 1 ) << 63 );
}

uint64_t SIRResourceGraphSync::enterRead( void ) const noexcept
{
	for ( uint32_t iReader = 0; iReader < m_nMaxReaders; ++iReader ) {
		uint64_t nExpected = 0;
		if ( !m_pReaderEpochs[iReader].compare_exchange_strong( nExpected, UINT64_MAX, std::memory_order_seq_cst ) ) {
			continue;
		}

		announceEpoch( iReader );
		return ( uint64_t )iReader + 1;
	}
	return 0;
}

void SIRResourceGraphSync::leaveRead( uint64_t nReaderToken ) const noexcept
{
	const uint64_t nLaneToken = nReaderToken & ~( UINT64_C( 1 ) << 63 );
	if ( nLaneToken == 0 || nLaneToken > m_nMaxReaders ) {
		abort();
	}

	const uint32_t iReader = ( uint32_t )( nLaneToken - 1 );
	const bool bRegistered = ( nReaderToken & ( UINT64_C( 1 ) << 63 ) ) != 0;
	m_pReaderEpochs[iReader].store( bRegistered ? UINT64_MAX - 1 : 0, std::memory_order_seq_cst );
}

SIRResourceGraphPinMutex::SIRResourceGraphPinMutex( void ) noexcept
{
}

void SIRResourceGraphPinMutex::lock( void )
{
	m_Mutex.lock();
}

bool SIRResourceGraphPinMutex::try_lock( void )
{
	return m_Mutex.tryLock();
}

void SIRResourceGraphPinMutex::unlock( void ) noexcept
{
	m_Mutex.unlock();
}

SIRResourceGraphWriteGuard::SIRResourceGraphWriteGuard( SIRResourceGraphSync& rSync ) noexcept
    : m_pSync( rSync.beginWrite() ? &rSync : nullptr )
{
}

SIRResourceGraphWriteGuard::~SIRResourceGraphWriteGuard( void ) noexcept
{
	if ( m_pSync != nullptr ) {
		m_pSync->endWrite();
	}
}

bool SIRResourceGraphWriteGuard::isActive( void ) const noexcept
{
	return m_pSync != nullptr;
}
