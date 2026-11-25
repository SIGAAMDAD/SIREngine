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

#ifndef __SIR_TINYLFUADMISSIONINDEX_H__
#define __SIR_TINYLFUADMISSIONINDEX_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRVector.h>
#include <stdint.h>

class SIRTinyLFUAdmissionIndex
{
	SIRVector<uint8_t> m_counters;
	SIRVector<uint64_t> m_doorkeeper;
	uint32_t m_nBuckets = 0;
	uint32_t m_nSamples = 0;
	uint32_t m_nDecayInterval = 0;

	static uint64_t mix( uint64_t nValue ) noexcept
	{
		nValue ^= nValue >> 33;
		nValue *= 0xff51afd7ed558ccdull;
		nValue ^= nValue >> 33;
		return nValue * 0xc4ceb9fe1a85ec53ull ^ ( nValue >> 33 );
	}

	uint32_t position( uint64_t nKey, uint32_t iRow ) const noexcept
	{
		return iRow * m_nBuckets + static_cast<uint32_t>( mix( nKey + iRow * 0x9e3779b97f4a7c15ull ) ) % m_nBuckets;
	}

	void decay( void ) noexcept
	{
		for ( uint32_t iIndex = 0; iIndex < m_counters.getSize(); ++iIndex ) {
			m_counters[iIndex] >>= 1;
		}
		for ( uint32_t iIndex = 0; iIndex < m_doorkeeper.getSize(); ++iIndex ) {
			m_doorkeeper[iIndex] = 0;
		}
		m_nSamples = 0;
	}

public:
	explicit SIRTinyLFUAdmissionIndex( const sirContainerAllocator_t *pAllocator )
	    : m_counters( pAllocator ),
	      m_doorkeeper( pAllocator ),
	      m_pAllocatorStore( pAllocator )
	{
	}

	SIRContainerResult tryInitialize( uint32_t nBuckets, uint32_t nDecayInterval )
	{
		if ( nBuckets == 0 || nBuckets > UINT32_MAX / 4 || nDecayInterval == 0 ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		SIRVector<uint8_t> counters( m_pAllocatorStore );
		SIRVector<uint64_t> door( m_pAllocatorStore );
		SIRContainerResult eResult = counters.tryResize( nBuckets * 4 );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		eResult = door.tryResize( ( nBuckets + 63 ) / 64 );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		m_counters = std::move( counters );
		m_doorkeeper = std::move( door );
		m_nBuckets = nBuckets;
		m_nDecayInterval = nDecayInterval;
		m_nSamples = 0;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	void record( uint64_t nKey ) noexcept
	{
		if ( m_nBuckets == 0 ) {
			return;
		}
		const uint32_t iDoor = static_cast<uint32_t>( mix( nKey ) ) % m_nBuckets;
		const uint64_t nMask = uint64_t( 1 ) << ( iDoor % 64 );
		const bool bSeen = ( m_doorkeeper[iDoor / 64] & nMask ) != 0;
		m_doorkeeper[iDoor / 64] |= nMask;
		if ( bSeen ) {
			for ( uint32_t iRow = 0; iRow < 4; ++iRow ) {
				uint8_t& nCount = m_counters[position( nKey, iRow )];
				if ( nCount != 15 ) {
					++nCount;
				}
			}
		}
		if ( ++m_nSamples >= m_nDecayInterval ) {
			decay();
		}
	}

	uint32_t estimate( uint64_t nKey ) const noexcept
	{
		if ( m_nBuckets == 0 ) {
			return 0;
		}
		uint32_t nMin = 15;
		for ( uint32_t iRow = 0; iRow < 4; ++iRow ) {
			const uint32_t nCount = m_counters[position( nKey, iRow )];
			if ( nCount < nMin ) {
				nMin = nCount;
			}
		}
		const uint32_t iDoor = static_cast<uint32_t>( mix( nKey ) ) % m_nBuckets;
		return nMin + ( ( m_doorkeeper[iDoor / 64] >> ( iDoor % 64 ) ) & 1 );
	}

	bool prefer( uint64_t nCandidate, uint64_t nVictim ) const noexcept
	{
		return estimate( nCandidate ) >= estimate( nVictim );
	}

private:
	const sirContainerAllocator_t *m_pAllocatorStore = nullptr;
};
#endif /* __SIR_TINYLFUADMISSIONINDEX_H__ */
