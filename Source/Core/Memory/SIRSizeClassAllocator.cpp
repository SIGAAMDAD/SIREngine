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

#include <Core/Memory/Allocators/SIRSizeClassAllocator.h>
#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Memory/SIRMemorySegment.h>
#include <new>

#if defined( _MSC_VER ) && defined( _M_X64 )
#include <intrin.h>
#endif

SIRSizeClassAllocator::SIRSizeClassAllocator( void ) : m_pPools( nullptr ), m_nInitialized( 0 ), m_nClassCount( 0 ), m_bFineGrained( false )
{
}

uint32_t SIRSizeClassAllocator::getClassIndex( uint32_t nBytes )
{
	if ( nBytes == 0 || nBytes > 2048 ) {
		return UINT32_MAX;
	}
	if ( nBytes <= 16 ) {
		return 0;
	}
#if defined( _MSC_VER ) && defined( _M_X64 )
	unsigned long nHighestBit = 0;
	_BitScanReverse( &nHighestBit, nBytes - 1 );
	return ( ( uint32_t )( nHighestBit ) ) - 3;
#elif defined( __GNUC__ ) || defined( __clang__ )
	return 28u - ( ( uint32_t )( __builtin_clz( nBytes - 1 ) ) );
#else
	uint32_t nSize = 16;
	uint32_t nClass = 0;
	while ( nSize < nBytes ) {
		nSize <<= 1;
		++nClass;
	}
	return nClass;
#endif
}

uint32_t SIRSizeClassAllocator::getClassBytes( uint32_t nClass )
{
	return nClass < SIR_SIZE_CLASS_COUNT ? 16u << nClass : 0;
}

uint32_t SIRSizeClassAllocator::getFineClassIndex( uint32_t nBytes )
{
	if ( nBytes == 0 || nBytes > 2048 ) {
		return UINT32_MAX;
	}
	if ( nBytes <= 256 ) {
		return ( nBytes - 1 ) >> 4;
	}
	if ( nBytes <= 512 ) {
		return 16 + ( ( nBytes - 257 ) >> 5 );
	}
	if ( nBytes <= 1024 ) {
		return 24 + ( ( nBytes - 513 ) >> 7 );
	}
	return 28 + ( ( nBytes - 1025 ) >> 8 );
}

uint32_t SIRSizeClassAllocator::getFineClassBytes( uint32_t nClass )
{
	if ( nClass < 16 ) {
		return ( nClass + 1 ) * 16;
	}
	if ( nClass < 24 ) {
		return 256 + ( nClass - 15 ) * 32;
	}
	if ( nClass < 28 ) {
		return 512 + ( nClass - 23 ) * 128;
	}
	return nClass < SIR_FINE_SIZE_CLASS_COUNT ? 1024 + ( nClass - 27 ) * 256 : 0;
}

bool SIRSizeClassAllocator::init( SIRMemorySegment *const *ppSegments )
{
	return initInternal( ppSegments, SIR_SIZE_CLASS_COUNT, false );
}

bool SIRSizeClassAllocator::initFine( SIRMemorySegment *const *ppSegments )
{
	return initInternal( ppSegments, SIR_FINE_SIZE_CLASS_COUNT, true );
}

bool SIRSizeClassAllocator::initInternal( SIRMemorySegment *const *ppSegments, uint32_t nClassCount, bool bFineGrained )
{
	if ( m_pPools != nullptr || ppSegments == nullptr || ppSegments[0] == nullptr ) {
		return false;
	}

	SIRMemoryManager *pManager = ppSegments[0]->getManager();
	if ( pManager == nullptr ) {
		return false;
	}
	// Reject aliased segments before consuming permanent manager metadata.
	for ( uint32_t i = 0; i < nClassCount; ++i ) {
		if ( ppSegments[i] == nullptr || ppSegments[i]->getManager() != pManager ) {
			return false;
		}
		for ( uint32_t j = 0; j < i; ++j ) {
			if ( ppSegments[i] == ppSegments[j] ) {
				return false;
			}
		}
	}

	void *pMemory = pManager->allocateMetadata( ( ( uint64_t )( nClassCount ) ) * sizeof( SIRPoolAllocator ), alignof( SIRPoolAllocator ) );
	if ( pMemory == nullptr ) {
		return false;
	}
	m_pPools = ( ( SIRPoolAllocator * )( pMemory ) );
	m_nClassCount = nClassCount;
	m_bFineGrained = bFineGrained;
	for ( uint32_t i = 0; i < nClassCount; ++i ) {
		new ( m_pPools + i ) SIRPoolAllocator();
	}

	for ( uint32_t i = 0; i < nClassCount; ++i ) {
		const uint32_t nBytes = bFineGrained ? getFineClassBytes( i ) : getClassBytes( i );
		if ( !m_pPools[i].init( ppSegments[i], nBytes, 16 ) ) {
			shutdown();
			return false;
		}
		++m_nInitialized;
	}
	return true;
}

void *SIRSizeClassAllocator::alloc( uint32_t nBytes )
{
	const uint32_t nClass = m_bFineGrained ? getFineClassIndex( nBytes ) : getClassIndex( nBytes );
	return nClass < m_nInitialized ? m_pPools[nClass].alloc() : nullptr;
}

bool SIRSizeClassAllocator::free( void *pMemory, uint32_t nBytes )
{
	const uint32_t nClass = m_bFineGrained ? getFineClassIndex( nBytes ) : getClassIndex( nBytes );
	return nClass < m_nInitialized && m_pPools[nClass].free( pMemory );
}

bool SIRSizeClassAllocator::precommitClass( uint32_t nClass, uint32_t nSlots )
{
	return nClass < m_nInitialized && m_pPools[nClass].precommitSlots( nSlots );
}

bool SIRSizeClassAllocator::shutdown( void )
{
	for ( uint32_t i = 0; i < m_nInitialized; ++i ) {
		if ( m_pPools[i].getLiveSlots() != 0 ) {
			return false;
		}
	}
	for ( uint32_t i = 0; i < m_nInitialized; ++i ) {
		if ( !m_pPools[i].shutdown() ) {
			return false;
		}
	}
	for ( uint32_t i = 0; i < m_nClassCount; ++i ) {
		m_pPools[i].~SIRPoolAllocator();
	}
	m_pPools = nullptr;
	m_nInitialized = 0;
	m_nClassCount = 0;
	m_bFineGrained = false;
	return true;
}

uint32_t SIRSizeClassAllocator::getActiveClassCount( void ) const
{
	return m_nInitialized;
}
