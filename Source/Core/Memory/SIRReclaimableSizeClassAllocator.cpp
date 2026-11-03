/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
Ported from The Nomad Framework, Copyright (C) 2025-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Core/Memory/Allocators/SIRReclaimableSizeClassAllocator.h>
#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Memory/SIRMemorySegment.h>
#include <Core/Memory/SIRMemoryUtils.h>
#include <new>

SIRReclaimableSizeClassAllocator::SIRReclaimableSizeClassAllocator( void )
    : m_pSlabs( nullptr ), m_ppSegments( nullptr ), m_pClassState( nullptr ), m_nClassCount( 0 ), m_nInitialized( 0 ), m_nAlignment( 16 ),
      m_nLiveRequestedBytes( 0 ), m_bFine( false ), m_bAdaptive( false )
{
}

bool SIRReclaimableSizeClassAllocator::init( SIRMemorySegment *const *ppSegments, bool bFine, uint64_t nSlabBytes, uint32_t nAlignment )
{
	if ( m_pSlabs != nullptr || ppSegments == nullptr || !SIR_IS_POWER_OF_TWO( nAlignment ) || nAlignment > 32 ) {
		return false;
	}
	const uint32_t nCount = bFine ? SIRSizeClassAllocator::SIR_FINE_SIZE_CLASS_COUNT : SIRSizeClassAllocator::SIR_SIZE_CLASS_COUNT;
	SIRMemoryManager *pManager = ppSegments[0] != nullptr ? ppSegments[0]->getManager() : nullptr;
	if ( pManager == nullptr ) {
		return false;
	}
	for ( uint32_t i = 0; i < nCount; ++i ) {
		const uint32_t nClassBytes = bFine ? SIRSizeClassAllocator::getFineClassBytes( i ) : SIRSizeClassAllocator::getClassBytes( i );
		if ( ppSegments[i] == nullptr || ppSegments[i]->getManager() != pManager ||
		     SIRSlabAllocator::getRequiredMetadataBytes( ppSegments[i]->getReservedBytes(), nSlabBytes, nClassBytes, nAlignment ) == 0 ) {
			return false;
		}
		for ( uint32_t j = 0; j < i; ++j ) {
			if ( ppSegments[i] == ppSegments[j] ) {
				return false;
			}
		}
	}
	// Three arrays, all permanent manager metadata. Provision once, discard the
	// unpublished manager if an error occurs after this point.
	m_pSlabs = ( ( SIRSlabAllocator * )( pManager->allocateMetadata( ( ( uint64_t )( nCount ) ) * sizeof( SIRSlabAllocator ),
	                                                                 alignof( SIRSlabAllocator ) ) ) );
	m_ppSegments = ( ( SIRMemorySegment ** )( pManager->allocateMetadata( ( ( uint64_t )( nCount ) ) * sizeof( SIRMemorySegment * ),
	                                                                      alignof( SIRMemorySegment * ) ) ) );
	m_pClassState = ( ( sirClassState_t * )( pManager->allocateMetadata( ( ( uint64_t )( nCount ) ) * sizeof( sirClassState_t ),
	                                                                     alignof( sirClassState_t ) ) ) );
	if ( m_pSlabs == nullptr || m_ppSegments == nullptr || m_pClassState == nullptr ) {
		return false;
	}
	m_nClassCount = nCount;
	m_nAlignment = nAlignment;
	m_bFine = bFine;
	for ( uint32_t i = 0; i < nCount; ++i ) {
		new ( m_pSlabs + i ) SIRSlabAllocator();
		m_ppSegments[i] = ppSegments[i];
		m_pClassState[i] = { 0, 0, 0 };
		const uint32_t nClassBytes = bFine ? SIRSizeClassAllocator::getFineClassBytes( i ) : SIRSizeClassAllocator::getClassBytes( i );
		if ( !m_pSlabs[i].init( ppSegments[i], nClassBytes, nAlignment, nSlabBytes ) ) {
			return false;
		}
		++m_nInitialized;
	}
	return true;
}

void *SIRReclaimableSizeClassAllocator::alloc( uint32_t nBytes, uint32_t nAlignment )
{
	if ( nAlignment > m_nAlignment || !SIR_IS_POWER_OF_TWO( nAlignment ) ) {
		return nullptr;
	}
	const uint32_t nClass = m_bFine ? SIRSizeClassAllocator::getFineClassIndex( nBytes ) : SIRSizeClassAllocator::getClassIndex( nBytes );
	if ( nClass >= m_nInitialized ) {
		return nullptr;
	}
	void *pData = m_pSlabs[nClass].alloc();
	if ( pData != nullptr ) {
		m_nLiveRequestedBytes += nBytes;
	}
	return pData;
}

bool SIRReclaimableSizeClassAllocator::free( void *pMemory, uint32_t nBytes )
{
	const uint32_t nClass = m_bFine ? SIRSizeClassAllocator::getFineClassIndex( nBytes ) : SIRSizeClassAllocator::getClassIndex( nBytes );
	if ( nClass >= m_nInitialized || m_nLiveRequestedBytes < nBytes || !m_pSlabs[nClass].free( pMemory ) ) {
		return false;
	}
	m_nLiveRequestedBytes -= nBytes;
	return true;
}

bool SIRReclaimableSizeClassAllocator::precommitClass( uint32_t nClass, uint32_t nSlabs )
{
	return nClass < m_nInitialized && m_pSlabs[nClass].precommitSlabs( nSlabs );
}

bool SIRReclaimableSizeClassAllocator::setWarmSlabs( uint32_t nClass, uint32_t nWarm )
{
	if ( nClass >= m_nInitialized || nWarm > m_pSlabs[nClass].getSlabCount() ) {
		return false;
	}
	m_pClassState[nClass].nWarmSlabs = nWarm;
	return true;
}

bool SIRReclaimableSizeClassAllocator::setAdaptiveRetention( bool bEnabled )
{
	if ( m_pSlabs == nullptr ) {
		return false;
	}
	m_bAdaptive = bEnabled;
	return true;
}

bool SIRReclaimableSizeClassAllocator::advanceRetentionEpoch( void )
{
	if ( m_pSlabs == nullptr ) {
		return false;
	}
	if ( !m_bAdaptive ) {
		return true;
	}
	for ( uint32_t i = 0; i < m_nInitialized; ++i ) {
		sirClassState_t &state = m_pClassState[i];
		const uint64_t nCurrent = m_pSlabs[i].getColdActivations();
		const uint64_t nRefills = nCurrent - state.nLastActivations;
		state.nLastActivations = nCurrent;
		if ( nRefills >= 3 ) {
			state.nWarmSlabs = 2;
			state.nQuietEpochs = 0;
		} else if ( nRefills == 0 ) {
			if ( ++state.nQuietEpochs >= 3 ) {
				state.nWarmSlabs = 0;
			}
		} else {
			state.nWarmSlabs = 1;
			state.nQuietEpochs = 0;
		}
	}
	return true;
}

bool SIRReclaimableSizeClassAllocator::trimIdle( void )
{
	if ( m_pSlabs == nullptr ) {
		return false;
	}
	for ( uint32_t i = 0; i < m_nInitialized; ++i ) {
		if ( !m_pSlabs[i].trimEmptySlabsTo( m_pClassState[i].nWarmSlabs ) ) {
			return false;
		}
	}
	return true;
}

bool SIRReclaimableSizeClassAllocator::trimTo( uint32_t nMaximumWarmSlabs )
{
	if ( m_pSlabs == nullptr ) {
		return false;
	}
	for ( uint32_t i = 0; i < m_nInitialized; ++i ) {
		const uint32_t nConfiguredWarm = m_pClassState[i].nWarmSlabs;
		const uint32_t nKeep = nConfiguredWarm < nMaximumWarmSlabs ? nConfiguredWarm : nMaximumWarmSlabs;
		if ( !m_pSlabs[i].trimEmptySlabsTo( nKeep ) ) {
			return false;
		}
	}
	return true;
}

bool SIRReclaimableSizeClassAllocator::shutdown( void )
{
	if ( m_pSlabs == nullptr || m_nLiveRequestedBytes != 0 ) {
		return false;
	}
	for ( uint32_t i = 0; i < m_nInitialized; ++i ) {
		if ( !m_pSlabs[i].shutdown() ) {
			return false;
		}
		m_pSlabs[i].~SIRSlabAllocator();
	}
	m_pSlabs = nullptr;
	m_ppSegments = nullptr;
	m_pClassState = nullptr;
	m_nInitialized = 0;
	m_nClassCount = 0;
	return true;
}

uint32_t SIRReclaimableSizeClassAllocator::getClassCount( void ) const
{
	return m_nClassCount;
}

uint64_t SIRReclaimableSizeClassAllocator::getLiveRequestedBytes( void ) const
{
	return m_nLiveRequestedBytes;
}

uint64_t SIRReclaimableSizeClassAllocator::getClassCommittedBytes( uint32_t nClass ) const
{
	return nClass < m_nInitialized ? m_ppSegments[nClass]->getCommittedBytes() : 0;
}
