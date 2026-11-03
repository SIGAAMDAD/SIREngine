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

#include <Core/Platform/SIRScopedLock.h>
#include <Core/Platform/SIRSync.h>
#include <Core/Platform/SIRAtomic.h>
#include <Core/Memory/Allocators/SIRCompactBucketAllocator.h>
#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Memory/SIRMemoryUtils.h>
#include <Core/Memory/SIRMemorySegment.h>
#include <new>
#include <cstring>

static thread_local uint8_t s_nBucketThreadMarker = 0;
static SIRMutex g_BucketRegistryMutex;
static SIRCompactBucketAllocator *g_pBucketRegisteredHead = nullptr;

// The hot TLS binding is trivial, so findSeat() never enters a dynamic
// thread_local initialization guard. The cold guard is touched only during
// attach(), preserving automatic retirement on thread exit.
typedef struct sirBucketBinding_s
{
	const SIRCompactBucketAllocator *pAllocator;
	uint32_t nSeat;
	uint32_t nGeneration;
} sirBucketBinding_t;

static thread_local sirBucketBinding_t s_BucketBinding = { nullptr, UINT32_MAX, 0 };

typedef struct sirBucketRetirementGuard_s
{
	bool bArmed;
	sirBucketRetirementGuard_s( void ) : bArmed( false )
	{
	}
	~sirBucketRetirementGuard_s( void )
	{
		if ( bArmed && s_BucketBinding.pAllocator != nullptr ) {
			SIRCompactBucketAllocator::retireRegistered( const_cast<SIRCompactBucketAllocator *>( s_BucketBinding.pAllocator ) );
		}
	}
} sirBucketRetirementGuard_t;

static thread_local sirBucketRetirementGuard_t s_BucketRetirementGuard;

static uintptr_t bucketThreadToken( void )
{
	return ( ( uintptr_t )( &s_nBucketThreadMarker ) );
}

sirBucketSlab_s::sirBucketSlab_s( void )
    : nClass( SIR_BUCKET_NONE ), nFresh( 0 ), nAccessGate( 0 ), nSeat( SIR_BUCKET_NONE ), nSeatGeneration( 0 ), nLive( 0 ),
      nFreeHead( SIR_BUCKET_NONE ), nNeutralNext( SIR_BUCKET_NONE ), nLastClass( SIR_BUCKET_NONE ), nPartialNext( SIR_BUCKET_NONE ),
      nPartialPrev( SIR_BUCKET_NONE ), nEpoch( 0 ), pStates( nullptr ), pDirtyStateWords( nullptr ), bInPartial( false ),
      bCommitted( false ), bWarmRetained( false )
{
}

sirBucketDirectory_s::sirBucketDirectory_s( void ) : pDescriptor( nullptr ), nReaders( 0 ), nEpoch( 0 )
{
}

sirBucketSlab_t *SIRCompactBucketAllocator::loadSlab( uint32_t nSlab ) const
{
	return m_pDirectory[nSlab].pDescriptor.load( SIR_MEMORY_ORDER_ACQUIRE );
}

sirBucketInbox_s::sirBucketInbox_s( void ) : pDirtySlabWords( nullptr ), pDirtyGroupWords( nullptr )
{
}

sirBucketSeat_s::sirBucketSeat_s( void )
    : nOwnerToken( 0 ), nGeneration( 0 ), nOrphan( 0 ), nLive( 0 ), nLiveOwner( 0 ), nWarmRetained( 0 ), pActive( nullptr ),
      pPartialHead( nullptr ), pActiveDescriptor( nullptr ), pRemote( nullptr )
{
}

SIRCompactBucketAllocator::SIRCompactBucketAllocator( void )
    : m_pRegistryNext( nullptr ), m_pSegment( nullptr ), m_pBase( nullptr ), m_pDirectory( nullptr ), m_pSeats( nullptr ),
      m_pRemoteInboxes( nullptr ), m_pFreeStateBlocks( nullptr ), m_pStatePages( nullptr ), m_nStateBlockStride( 0 ),
      m_nStateSlotsPerPage( 0 ), m_nStateWordsPerSlab( 0 ), m_nDirtyStateWordsPerSlab( 0 ), m_nDirtySlabWordsPerSeat( 0 ),
      m_nDirtyGroupsPerSeat( 0 ), m_nClassCount( 0 ), m_nMapCount( 0 ), m_nMaxBytes( 0 ), m_nAlignment( 0 ), m_nSlabBytes( 0 ),
      m_nSlabShift( 0 ), m_nSlabCount( 0 ), m_nMaximumSlots( 0 ), m_nSeatCount( 0 ), m_nWarmPerSeat( 0 ), m_nNextDormant( 0 ),
      m_nOrphanCount( 0 ), m_nNeutralHead( SIR_BUCKET_NONE )
{
	std::memset( m_nClassStride, 0, sizeof( m_nClassStride ) );
	std::memset( m_nClassCapacity, 0, sizeof( m_nClassCapacity ) );
	std::memset( m_nClassReciprocal, 0, sizeof( m_nClassReciprocal ) );
	m_nAlignmentShift = 0;
	std::memset( m_nClassMap, 0, sizeof( m_nClassMap ) );
	SIRScopedLock<SIRMutex> guard( g_BucketRegistryMutex );
	m_pRegistryNext = g_pBucketRegisteredHead;
	g_pBucketRegisteredHead = this;
}

SIRCompactBucketAllocator::~SIRCompactBucketAllocator( void )
{
	SIRScopedLock<SIRMutex> guard( g_BucketRegistryMutex );
	SIRCompactBucketAllocator **ppNext = &g_pBucketRegisteredHead;
	while ( *ppNext != nullptr && *ppNext != this ) {
		ppNext = &( *ppNext )->m_pRegistryNext;
	}
	if ( *ppNext == this ) {
		*ppNext = m_pRegistryNext;
	}
	if ( s_BucketBinding.pAllocator == this ) {
		s_BucketBinding.pAllocator = nullptr;
	}
}

bool SIRCompactBucketAllocator::retireRegistered( SIRCompactBucketAllocator *pAllocator )
{
	SIRScopedLock<SIRMutex> guard( g_BucketRegistryMutex );
	for ( SIRCompactBucketAllocator *p = g_pBucketRegisteredHead; p != nullptr; p = p->m_pRegistryNext ) {
		if ( p == pAllocator ) {
			return p->retireThread();
		}
	}
	return false;
}

uint64_t SIRCompactBucketAllocator::getRequiredMetadataBytes( uint64_t nSegmentBytes, const sirCompactBucketConfig_t &config )
{
	if ( !SIR_IS_POWER_OF_TWO( config.nSlabBytes ) || ( config.nAlignment != 8 && config.nAlignment != 16 ) ||
	     config.nMaximumSmallBytes == 0 || config.nMaximumSmallBytes > 2048 || config.nWorkerSeats == 0 || config.nClassProfile > 3 ||
	     nSegmentBytes == 0 || nSegmentBytes % config.nSlabBytes != 0 || nSegmentBytes / config.nSlabBytes > UINT32_MAX ||
	     config.nSlabBytes < 2048 ) {
		return 0;
	}
	const uint64_t nSlabs = nSegmentBytes / config.nSlabBytes;
	const uint64_t nSlots = config.nSlabBytes / config.nAlignment;
	const uint64_t nSeats = config.nWorkerSeats;
	const uint64_t nStateWords = ( nSlots + 31 ) / 32;
	const uint64_t nPayload =
	    ( ( nStateWords * sizeof( SIRAtomic<uint64_t> ) + 63 ) & ~63ull ) + ( ( nStateWords + 63 ) / 64 ) * sizeof( SIRAtomic<uint64_t> );
	const uint64_t nBlockStride = ( nPayload + sizeof( sirBucketStatePage_t * ) + 63 ) & ~63ull;
	const uint64_t nBlocksPerPage = 4096 / nBlockStride;
	const uint64_t nStatePages = nBlocksPerPage != 0 ? ( nSlabs + nBlocksPerPage - 1 ) / nBlocksPerPage : 0;
	const uint64_t nStates = nBlocksPerPage != 0 ? nStatePages * ( 4096 + sizeof( sirBucketStatePage_t ) ) : nSlabs * nBlockStride;
	const uint64_t nDesc = nSlabs * ( sizeof( sirBucketDirectory_t ) + sizeof( sirBucketSlab_t ) );
	const uint64_t nClasses = config.nClassProfile == 0 ? 40 : ( config.nClassProfile == 1 ? 8 : ( config.nClassProfile == 2 ? 24 : 32 ) );
	const uint64_t nHeadBytes = ( nClasses * ( 2 * sizeof( uint32_t ) + sizeof( sirBucketSlab_t * ) ) + 63 ) & ~63ull;
	const uint64_t nSeatBytes = nSeats * ( sizeof( sirBucketSeat_t ) + sizeof( sirBucketInbox_t ) + nHeadBytes );
	if ( nStates > UINT32_MAX * 64ull || nDesc > UINT64_MAX - nStates || nSeatBytes > UINT64_MAX - nStates - nDesc - 128 ) {
		return 0;
	}
	const uint64_t nDirtySlabWords = ( nSlabs + 63 ) / 64;
	const uint64_t nDirtyGroups = ( nDirtySlabWords + 63 ) / 64;
	const uint64_t nQueueBytes = nSeats * ( nDirtySlabWords + nDirtyGroups ) * sizeof( SIRAtomic<uint64_t> );
	if ( nQueueBytes > UINT64_MAX - nStates - nDesc - nSeatBytes - 128 ) {
		return 0;
	}
	return nStates + nDesc + nSeatBytes + nQueueBytes + 128;
}

bool SIRCompactBucketAllocator::init( SIRMemorySegment *pSegment, const sirCompactBucketConfig_t &config )
{
	if ( m_pSegment != nullptr || pSegment == nullptr || config.nSlabBytes < pSegment->getPageSize() ||
	     config.nSlabBytes % pSegment->getPageSize() != 0 || config.nSlabBytes < config.nMaximumSmallBytes ||
	     getRequiredMetadataBytes( pSegment->getReservedBytes(), config ) == 0 ) {
		return false;
	}
	SIRMemoryManager *pManager = pSegment->getManager();
	const uint32_t nSlabs = ( ( uint32_t )( pSegment->getReservedBytes() / config.nSlabBytes ) );
	const uint32_t nMaximumSlots = config.nSlabBytes / config.nAlignment;
	const uint32_t nClassCount =
	    config.nClassProfile == 0 ? 40 : ( config.nClassProfile == 1 ? 8 : ( config.nClassProfile == 2 ? 24 : 32 ) );
	const uint32_t nHeadStride = ( nClassCount * ( 2 * sizeof( uint32_t ) + sizeof( sirBucketSlab_t * ) ) + 63 ) & ~63u;
	sirBucketDirectory_t *pDirectory = ( ( sirBucketDirectory_t * )( pManager->allocateMetadata(
	    sizeof( sirBucketDirectory_t ) * nSlabs, alignof( sirBucketDirectory_t ), pSegment->getDomain() ) ) );
	sirBucketSeat_t *pSeats = ( ( sirBucketSeat_t * )( pManager->allocateMetadata( sizeof( sirBucketSeat_t ) * config.nWorkerSeats,
	                                                                               alignof( sirBucketSeat_t ), pSegment->getDomain() ) ) );
	sirBucketInbox_t *pRemoteInboxes = ( ( sirBucketInbox_t * )( pManager->allocateMetadata(
	    sizeof( sirBucketInbox_t ) * config.nWorkerSeats, alignof( sirBucketInbox_t ), pSegment->getDomain() ) ) );
	uint32_t *pClassHeads = ( (
	    uint32_t * )( pManager->allocateMetadata( ( ( uint64_t )( nHeadStride ) ) * config.nWorkerSeats, 64, pSegment->getDomain() ) ) );
	const uint32_t nDirtySlabWords = ( nSlabs + 63 ) / 64;
	const uint32_t nDirtyGroups = ( nDirtySlabWords + 63 ) / 64;
	SIRAtomic<uint64_t> *pDirtySlabWords = ( ( SIRAtomic<uint64_t> * )( pManager->allocateMetadata(
	    ( ( uint64_t )( config.nWorkerSeats ) ) * nDirtySlabWords * 8, alignof( SIRAtomic<uint64_t> ), pSegment->getDomain() ) ) );
	SIRAtomic<uint64_t> *pDirtyGroupWords = ( ( SIRAtomic<uint64_t> * )( pManager->allocateMetadata(
	    ( ( uint64_t )( config.nWorkerSeats ) ) * nDirtyGroups * 8, alignof( SIRAtomic<uint64_t> ), pSegment->getDomain() ) ) );
	if ( pDirectory == nullptr || pSeats == nullptr || pRemoteInboxes == nullptr || pClassHeads == nullptr || pDirtySlabWords == nullptr ||
	     pDirtyGroupWords == nullptr ) {
		return false;
	}
	for ( uint32_t i = 0; i < nSlabs; ++i ) {
		new ( pDirectory + i ) sirBucketDirectory_t();
	}
	for ( uint32_t i = 0; i < config.nWorkerSeats; ++i ) {
		new ( pSeats + i ) sirBucketSeat_t();
		pSeats[i].pActive = ( ( uint32_t * )( ( ( uint8_t * )( pClassHeads ) ) + ( ( uint64_t )( i ) ) * nHeadStride ) );
		pSeats[i].pPartialHead = pSeats[i].pActive + nClassCount;
		pSeats[i].pActiveDescriptor = ( ( sirBucketSlab_t ** )( pSeats[i].pPartialHead + nClassCount ) );
		for ( uint32_t c = 0; c < nClassCount; ++c ) {
			pSeats[i].pActive[c] = SIR_BUCKET_NONE;
			pSeats[i].pPartialHead[c] = SIR_BUCKET_NONE;
			pSeats[i].pActiveDescriptor[c] = nullptr;
		}
		new ( pRemoteInboxes + i ) sirBucketInbox_t();
		pRemoteInboxes[i].pDirtySlabWords = pDirtySlabWords + ( ( uint64_t )( i ) ) * nDirtySlabWords;
		pRemoteInboxes[i].pDirtyGroupWords = pDirtyGroupWords + ( ( uint64_t )( i ) ) * nDirtyGroups;
		for ( uint32_t w = 0; w < nDirtySlabWords; ++w ) {
			new ( pRemoteInboxes[i].pDirtySlabWords + w ) SIRAtomic<uint64_t>( 0 );
		}
		for ( uint32_t w = 0; w < nDirtyGroups; ++w ) {
			new ( pRemoteInboxes[i].pDirtyGroupWords + w ) SIRAtomic<uint64_t>( 0 );
		}
		pSeats[i].pRemote = pRemoteInboxes + i;
	}
	uint32_t nCount = 0;
	if ( config.nAlignment == 8 && config.nClassProfile == 0 ) {
		for ( uint32_t i = 1; i <= 16; ++i ) {
			m_nClassStride[nCount++] = i * 8;
		}
		for ( uint32_t i = 1; i <= 8; ++i ) {
			m_nClassStride[nCount++] = 128 + i * 16;
		}
		for ( uint32_t i = 1; i <= 8; ++i ) {
			m_nClassStride[nCount++] = 256 + i * 32;
		}
		for ( uint32_t i = 1; i <= 8; ++i ) {
			m_nClassStride[nCount++] = 512 + i * 192;
		}
	} else if ( config.nClassProfile == 1 ) {
		for ( uint32_t n = 16; n <= 2048; n *= 2 ) {
			m_nClassStride[nCount++] = n;
		}
	} else {
		for ( uint32_t i = 1; i <= 16; ++i ) {
			m_nClassStride[nCount++] = i * 16;
		}
		if ( config.nClassProfile == 2 ) {
			const uint32_t extra[] = { 512, 768, 1024, 1152, 1280, 1536, 1792, 2048 };
			for ( uint32_t n : extra ) {
				m_nClassStride[nCount++] = n;
			}
		} else {
			for ( uint32_t i = 1; i <= 8; ++i ) {
				m_nClassStride[nCount++] = 256 + i * 32;
			}
			if ( config.nClassProfile == 3 ) {
				const uint32_t extra[] = { 640, 768, 896, 1024, 1280, 1536, 1792, 2048 };
				for ( uint32_t n : extra ) {
					m_nClassStride[nCount++] = n;
				}
			} else {
				for ( uint32_t i = 1; i <= 8; ++i ) {
					m_nClassStride[nCount++] = 512 + i * 64;
				}
				for ( uint32_t i = 1; i <= 8; ++i ) {
					m_nClassStride[nCount++] = 1024 + i * 128;
				}
			}
		}
	}
	if ( nCount != nClassCount ) {
		return false;
	}
	for ( uint32_t nClass = 0; nClass < nCount; ++nClass ) {
		m_nClassCapacity[nClass] = config.nSlabBytes / m_nClassStride[nClass];
		// The inner offset never exceeds one slab. Ceiling reciprocal
		// division yields the exact quotient or an overestimate of one.
		m_nClassReciprocal[nClass] = ( ( 1ull << 32 ) + m_nClassStride[nClass] - 1 ) / m_nClassStride[nClass];
	}
	m_nAlignmentShift = config.nAlignment == 8 ? 3u : 4u;
	const uint32_t nMapCount = SIR_ROUND_UP_MULTIPLE( config.nMaximumSmallBytes, config.nAlignment ) / config.nAlignment;
	for ( uint32_t i = 0; i < nMapCount; ++i ) {
		uint32_t nClass = 0;
		const uint32_t nBytes = ( i + 1 ) * config.nAlignment;
		while ( nClass + 1 < nCount && m_nClassStride[nClass] < nBytes ) {
			++nClass;
		}
		m_nClassMap[i] = ( ( uint8_t )( nClass ) );
	}
	m_pDirectory = pDirectory;
	m_pSeats = pSeats;
	m_pRemoteInboxes = pRemoteInboxes;
	m_pFreeStateBlocks = nullptr;
	m_pStatePages = nullptr;
	m_nStateWordsPerSlab = ( nMaximumSlots + 31 ) / 32;
	m_nDirtyStateWordsPerSlab = ( m_nStateWordsPerSlab + 63 ) / 64;
	const uint64_t nStateBytes = ( ( uint64_t )( m_nStateWordsPerSlab ) ) * sizeof( SIRAtomic<uint64_t> );
	m_nStateBlockStride = ( ( uint32_t )( ( ( nStateBytes + 63 ) & ~63ull ) + m_nDirtyStateWordsPerSlab * sizeof( SIRAtomic<uint64_t> ) +
	                                      sizeof( sirBucketStatePage_t * ) + 63 ) ) &
	                      ~63u;
	m_nStateSlotsPerPage = ( ( uint32_t )( pManager->getPageSize() / m_nStateBlockStride ) );
	if ( m_nStateSlotsPerPage > 64 ) {
		m_nStateSlotsPerPage = 64;
	}
	m_nDirtySlabWordsPerSeat = nDirtySlabWords;
	m_nDirtyGroupsPerSeat = nDirtyGroups;
	m_pBase = ( ( uint8_t * )( pSegment->getBase() ) );
	m_nClassCount = nCount;
	m_nMapCount = nMapCount;
	m_nMaxBytes = config.nMaximumSmallBytes;
	m_nAlignment = config.nAlignment;
	m_nSlabBytes = config.nSlabBytes;
	m_nSlabCount = nSlabs;
	m_nMaximumSlots = nMaximumSlots;
	m_nSeatCount = config.nWorkerSeats;
	m_nWarmPerSeat = config.nWarmSlabsPerSeat;
	uint32_t nShift = 0;
	while ( ( 1u << nShift ) < m_nSlabBytes ) {
		++nShift;
	}
	m_nSlabShift = nShift;
	m_pSegment = pSegment;
	return true;
}

bool SIRCompactBucketAllocator::findSeat( uint32_t *pSeat ) const
{
	if ( m_pSegment == nullptr || s_BucketBinding.pAllocator != this || s_BucketBinding.nSeat >= m_nSeatCount ) {
		return false;
	}
	const sirBucketSeat_t &seat = m_pSeats[s_BucketBinding.nSeat];
	if ( seat.nOwnerToken.load( SIR_MEMORY_ORDER_ACQUIRE ) != bucketThreadToken() ||
	     seat.nGeneration.load( SIR_MEMORY_ORDER_ACQUIRE ) != s_BucketBinding.nGeneration ) {
		return false;
	}
	*pSeat = s_BucketBinding.nSeat;
	return true;
}

bool SIRCompactBucketAllocator::attach( uint32_t *pSeat )
{
	if ( m_pSegment == nullptr ) {
		return false;
	}
	uint32_t nSeat = 0;
	if ( findSeat( &nSeat ) ) {
		if ( pSeat != nullptr ) {
			*pSeat = nSeat;
		}
		return true;
	}
	if ( s_BucketBinding.pAllocator != nullptr && s_BucketBinding.pAllocator != this ) {
		return false;
	}
	for ( uint32_t i = 0; i < m_nSeatCount; ++i ) {
		if ( m_pSeats[i].nOrphan.load( SIR_MEMORY_ORDER_ACQUIRE ) != 0 ) {
			continue;
		}
		uintptr_t nExpected = 0;
		if ( !m_pSeats[i].nOwnerToken.compareExchangeStrong( nExpected, bucketThreadToken(), SIR_MEMORY_ORDER_ACQUIRE_RELEASE ) ) {
			continue;
		}
		const uint32_t nGeneration = m_pSeats[i].nGeneration.fetchAdd( 1, SIR_MEMORY_ORDER_ACQUIRE_RELEASE ) + 1;
		m_pSeats[i].nLiveOwner = 0;
		m_pSeats[i].nWarmRetained = 0;
		for ( uint32_t c = 0; c < m_nClassCount; ++c ) {
			m_pSeats[i].pActiveDescriptor[c] = nullptr;
		}
		s_BucketBinding.pAllocator = this;
		s_BucketBinding.nSeat = i;
		s_BucketBinding.nGeneration = nGeneration;
		// This is the only hot-binding registration site. Its destructor
		// remains independent of the trivially initialized TLS binding.
		s_BucketRetirementGuard.bArmed = true;
		if ( pSeat != nullptr ) {
			*pSeat = i;
		}
		return true;
	}
	return false;
}

uint32_t SIRCompactBucketAllocator::getAttachedSeat( void ) const
{
	uint32_t nSeat = 0;
	return findSeat( &nSeat ) ? nSeat : SIR_BUCKET_NONE;
}

uint32_t SIRCompactBucketAllocator::getSlotCapacity( uint32_t nClass ) const
{
	return m_nClassCapacity[nClass];
}

uint32_t SIRCompactBucketAllocator::loadState( uint32_t nSlab, uint32_t nSlot ) const
{
	const SIRAtomic<uint64_t> *pWords = ( *loadSlab( nSlab ) ).pStates;
	if ( pWords == nullptr ) {
		return 0;
	}
	const uint32_t nShift = ( nSlot & 31 ) * 2;
	return ( ( uint32_t )( pWords[nSlot >> 5].load( SIR_MEMORY_ORDER_ACQUIRE ) >> nShift ) ) & 3;
}

bool SIRCompactBucketAllocator::claimState( uint32_t nSlab, uint32_t nSlot, uint32_t nExpected, uint32_t nDesired )
{
	SIRAtomic<uint64_t> *pWords = ( *loadSlab( nSlab ) ).pStates;
	if ( pWords == nullptr ) {
		return false;
	}
	const uint32_t nShift = ( nSlot & 31 ) * 2;
	const uint64_t nMask = 3ull << nShift;
	SIRAtomic<uint64_t> &word = pWords[nSlot >> 5];
	uint64_t nOld = word.load( SIR_MEMORY_ORDER_ACQUIRE );
	for ( ;; ) {
		if ( ( ( nOld & nMask ) >> nShift ) != nExpected ) {
			return false;
		}
		const uint64_t nNew = ( nOld & ~nMask ) | ( ( ( uint64_t )( nDesired ) ) << nShift );
		if ( word.compareExchangeWeak( nOld, nNew, SIR_MEMORY_ORDER_ACQUIRE_RELEASE, SIR_MEMORY_ORDER_ACQUIRE ) ) {
			return true;
		}
	}
}

SIRAtomic<uint64_t> *SIRCompactBucketAllocator::allocateStateBlock( void )
{
	const uint64_t nStateBytes = ( ( uint64_t )( m_nStateWordsPerSlab ) ) * sizeof( SIRAtomic<uint64_t> );
	const uint64_t nDirtyOffset = ( nStateBytes + 63 ) & ~63ull;
	SIRAtomic<uint64_t> *pBlock = nullptr;
	if ( m_nStateSlotsPerPage == 0 ) {
		// Unusual large slabs use the original permanent metadata path.
		pBlock = m_pFreeStateBlocks;
		if ( pBlock != nullptr ) {
			m_pFreeStateBlocks = ( ( SIRAtomic<uint64_t> * )( ( ( uintptr_t )( pBlock[0].load( SIR_MEMORY_ORDER_RELAXED ) ) ) ) );
		} else {
			pBlock = ( ( SIRAtomic<uint64_t> * )( m_pSegment->getManager()->allocateMetadata( nDirtyOffset + m_nDirtyStateWordsPerSlab * 8,
			                                                                                  64, m_pSegment->getDomain() ) ) );
		}
	} else {
		sirBucketStatePage_t *pPage = m_pStatePages;
		while ( pPage != nullptr && ( pPage->pBase == nullptr || pPage->nFreeBits == 0 ) ) {
			pPage = pPage->pNext;
		}
		if ( pPage == nullptr ) {
			for ( pPage = m_pStatePages; pPage != nullptr; pPage = pPage->pNext ) {
				if ( pPage->pBase == nullptr ) {
					break;
				}
			}
			if ( pPage == nullptr ) {
				pPage = ( ( sirBucketStatePage_t * )( m_pSegment->getManager()->allocateMetadata(
				    sizeof( sirBucketStatePage_t ), alignof( sirBucketStatePage_t ), m_pSegment->getDomain() ) ) );
				if ( pPage == nullptr ) {
					return nullptr;
				}
				new ( pPage ) sirBucketStatePage_t();
				pPage->pNext = m_pStatePages;
				m_pStatePages = pPage;
			}
			pPage->pBase = ( ( uint8_t * )( m_pSegment->getManager()->allocateRecyclableMetadataPage( m_pSegment->getDomain() ) ) );
			if ( pPage->pBase == nullptr ) {
				return nullptr;
			}
			pPage->nSlots = m_nStateSlotsPerPage;
			pPage->nUsed = 0;
			pPage->nFreeBits = ( 1ull << pPage->nSlots ) - 1ull;
		}
#if defined( _MSC_VER )
		unsigned long nSlot = 0;
		_BitScanForward64( &nSlot, pPage->nFreeBits );
#else
		const uint32_t nSlot = ( ( uint32_t )( __builtin_ctzll( pPage->nFreeBits ) ) );
#endif
		pPage->nFreeBits &= ~( 1ull << nSlot );
		++pPage->nUsed;
		pBlock = ( ( SIRAtomic<uint64_t> * )( pPage->pBase + ( ( uint64_t )( nSlot ) ) * m_nStateBlockStride ) );
		sirBucketStatePage_t **ppPage =
		    ( ( sirBucketStatePage_t ** )( ( ( uint8_t * )( pBlock ) ) + m_nStateBlockStride - sizeof( sirBucketStatePage_t * ) ) );
		*ppPage = pPage;
	}
	if ( pBlock == nullptr ) {
		return nullptr;
	}
	for ( uint32_t i = 0; i < m_nStateWordsPerSlab; ++i ) {
		new ( pBlock + i ) SIRAtomic<uint64_t>( 0 );
	}
	SIRAtomic<uint64_t> *pDirty = ( ( SIRAtomic<uint64_t> * )( ( ( uint8_t * )( pBlock ) ) + nDirtyOffset ) );
	for ( uint32_t i = 0; i < m_nDirtyStateWordsPerSlab; ++i ) {
		new ( pDirty + i ) SIRAtomic<uint64_t>( 0 );
	}
	return pBlock;
}

void SIRCompactBucketAllocator::releaseStateBlock( SIRAtomic<uint64_t> *pBlock )
{
	if ( pBlock == nullptr ) {
		return;
	}
	if ( m_nStateSlotsPerPage == 0 ) {
		pBlock[0].store( ( ( uint64_t )( ( uintptr_t )( m_pFreeStateBlocks ) ) ), SIR_MEMORY_ORDER_RELAXED );
		m_pFreeStateBlocks = pBlock;
		return;
	}
	sirBucketStatePage_t *pPage =
	    *( ( sirBucketStatePage_t ** )( ( ( uint8_t * )( pBlock ) ) + m_nStateBlockStride - sizeof( sirBucketStatePage_t * ) ) );
	const uint32_t nSlot = ( ( uint32_t )( ( ( uint8_t * )( pBlock )-pPage->pBase ) / m_nStateBlockStride ) );
	pPage->nFreeBits |= 1ull << nSlot;
	--pPage->nUsed;
	if ( pPage->nUsed == 0 && m_pSegment->getManager()->releaseRecyclableMetadataPage( pPage->pBase, m_pSegment->getDomain() ) ) {
		pPage->pBase = nullptr;
		pPage->nFreeBits = 0;
	}
}

void SIRCompactBucketAllocator::linkPartial( uint32_t nSeat, uint32_t nClass, uint32_t nSlab )
{
	sirBucketSlab_t &slab = ( *loadSlab( nSlab ) );
	if ( slab.bInPartial ) {
		return;
	}
	sirBucketSeat_t &seat = m_pSeats[nSeat];
	slab.nPartialPrev = SIR_BUCKET_NONE;
	slab.nPartialNext = seat.pPartialHead[nClass];
	if ( slab.nPartialNext != SIR_BUCKET_NONE ) {
		( *loadSlab( slab.nPartialNext ) ).nPartialPrev = nSlab;
	}
	seat.pPartialHead[nClass] = nSlab;
	slab.bInPartial = true;
}

void SIRCompactBucketAllocator::unlinkPartial( uint32_t nSeat, uint32_t nClass, uint32_t nSlab )
{
	sirBucketSlab_t &slab = ( *loadSlab( nSlab ) );
	if ( !slab.bInPartial ) {
		return;
	}
	sirBucketSeat_t &seat = m_pSeats[nSeat];
	if ( slab.nPartialPrev == SIR_BUCKET_NONE ) {
		seat.pPartialHead[nClass] = slab.nPartialNext;
	} else {
		( *loadSlab( slab.nPartialPrev ) ).nPartialNext = slab.nPartialNext;
	}
	if ( slab.nPartialNext != SIR_BUCKET_NONE ) {
		( *loadSlab( slab.nPartialNext ) ).nPartialPrev = slab.nPartialPrev;
	}
	slab.nPartialPrev = SIR_BUCKET_NONE;
	slab.nPartialNext = SIR_BUCKET_NONE;
	slab.bInPartial = false;
}

// The top bit closes the slab to remote producers before state storage is
// recycled. An entering producer holds a reference until its queue publication
// is visible. The epoch check rejects a token resolved before slab reuse.
bool SIRCompactBucketAllocator::enterProducer( uint32_t nSlab, uint32_t nEpoch )
{
	sirBucketSlab_t &slab = ( *loadSlab( nSlab ) );
	uint32_t nOld = slab.nAccessGate.load( SIR_MEMORY_ORDER_ACQUIRE );
	while ( ( nOld & SIR_BUCKET_RETIRED ) == 0 ) {
		if ( nOld == SIR_BUCKET_RETIRED - 1 ) {
			return false;
		}
		if ( slab.nAccessGate.compareExchangeWeak( nOld, nOld + 1, SIR_MEMORY_ORDER_ACQUIRE_RELEASE, SIR_MEMORY_ORDER_ACQUIRE ) ) {
			if ( slab.nEpoch.load( SIR_MEMORY_ORDER_ACQUIRE ) != nEpoch || slab.nClass.load( SIR_MEMORY_ORDER_ACQUIRE ) >= m_nClassCount ) {
				exitProducer( nSlab );
				return false;
			}
			return true;
		}
	}
	return false;
}

void SIRCompactBucketAllocator::exitProducer( uint32_t nSlab )
{
	( *loadSlab( nSlab ) ).nAccessGate.fetchSub( 1, SIR_MEMORY_ORDER_RELEASE );
}

bool SIRCompactBucketAllocator::fillActive( uint32_t nSeat, uint32_t nClass )
{
	sirBucketSeat_t &seat = m_pSeats[nSeat];
	// Remote returns are already claimed and published. Reclaim them before
	// committing an additional slab when the active class just ran out.
	sirBucketInbox_t &inbox = *seat.pRemote;
	bool bHasRemote = false;
	for ( uint32_t i = 0; i < m_nDirtyGroupsPerSeat; ++i ) {
		bHasRemote |= inbox.pDirtyGroupWords[i].load( SIR_MEMORY_ORDER_ACQUIRE ) != 0;
	}
	if ( bHasRemote ) {
		drainSeat( nSeat, UINT32_MAX );
	}
	const uint32_t nActive = seat.pActive[nClass];
	if ( nActive != SIR_BUCKET_NONE ) {
		const sirBucketSlab_t &active = *loadSlab( nActive );
		if ( active.nFreeHead != SIR_BUCKET_NONE || active.nFresh.load( SIR_MEMORY_ORDER_RELAXED ) < getSlotCapacity( nClass ) ) {
			return true;
		}
	}
	if ( seat.pPartialHead[nClass] != SIR_BUCKET_NONE ) {
		const uint32_t nReused = seat.pPartialHead[nClass];
		unlinkPartial( nSeat, nClass, nReused );
		seat.pActive[nClass] = nReused;
		seat.pActiveDescriptor[nClass] = loadSlab( nReused );
		return true;
	}
	SIRScopedLock<SIRMutex> guard( m_ColdMutex );
	uint32_t nSlab = SIR_BUCKET_NONE;
	if ( m_nNeutralHead != SIR_BUCKET_NONE ) {
		// Prefer a previously class-affine warm slab, looking through at
		// most eight neutral candidates so this remains bounded cold work.
		uint32_t nPrevious = SIR_BUCKET_NONE;
		uint32_t nCandidate = m_nNeutralHead;
		for ( uint32_t i = 0; i < 8 && nCandidate != SIR_BUCKET_NONE; ++i ) {
			if ( loadSlab( nCandidate )->nLastClass == nClass ) {
				break;
			}
			nPrevious = nCandidate;
			nCandidate = loadSlab( nCandidate )->nNeutralNext;
		}
		if ( nCandidate == SIR_BUCKET_NONE || loadSlab( nCandidate )->nLastClass != nClass ) {
			nCandidate = m_nNeutralHead;
			nPrevious = SIR_BUCKET_NONE;
		}
		nSlab = nCandidate;
		if ( nPrevious == SIR_BUCKET_NONE ) {
			m_nNeutralHead = loadSlab( nSlab )->nNeutralNext;
		} else {
			loadSlab( nPrevious )->nNeutralNext = loadSlab( nSlab )->nNeutralNext;
		}
	} else if ( m_nNextDormant.load( SIR_MEMORY_ORDER_RELAXED ) < m_nSlabCount ) {
		nSlab = m_nNextDormant.fetchAdd( 1, SIR_MEMORY_ORDER_RELEASE );
	}
	if ( nSlab == SIR_BUCKET_NONE ) {
		return false;
	}
	sirBucketSlab_t *pDescriptor = loadSlab( nSlab );
	if ( pDescriptor == nullptr ) {
		void *pStorage =
		    m_pSegment->getManager()->allocateMetadata( sizeof( sirBucketSlab_t ), alignof( sirBucketSlab_t ), m_pSegment->getDomain() );
		if ( pStorage == nullptr ) {
			// A newly selected dormant ID has not been exposed yet.
			m_nNextDormant.fetchSub( 1, SIR_MEMORY_ORDER_RELAXED );
			return false;
		}
		pDescriptor = new ( pStorage ) sirBucketSlab_t();
		m_pDirectory[nSlab].pDescriptor.store( pDescriptor, SIR_MEMORY_ORDER_RELEASE );
	}
	sirBucketSlab_t &slab = *pDescriptor;
	SIRAtomic<uint64_t> *pStateBlock = allocateStateBlock();
	if ( pStateBlock == nullptr ) {
		slab.nNeutralNext = m_nNeutralHead;
		m_nNeutralHead = nSlab;
		return false;
	}
	if ( !slab.bCommitted && !m_pSegment->commit( ( ( uint64_t )( nSlab ) ) * m_nSlabBytes, m_nSlabBytes ) ) {
		releaseStateBlock( pStateBlock );
		slab.nNeutralNext = m_nNeutralHead;
		m_nNeutralHead = nSlab;
		return false;
	}
	slab.nAccessGate.store( SIR_BUCKET_RETIRED, SIR_MEMORY_ORDER_RELEASE );
	slab.pStates = pStateBlock;
	slab.pDirtyStateWords = ( ( SIRAtomic<uint64_t> * )( ( ( uint8_t * )( pStateBlock ) ) +
	                                                     ( ( m_nStateWordsPerSlab * sizeof( SIRAtomic<uint64_t> ) + 63 ) & ~63ull ) ) );
	slab.bCommitted = true;
	slab.nSeat.store( nSeat, SIR_MEMORY_ORDER_RELAXED );
	slab.nSeatGeneration.store( seat.nGeneration.load( SIR_MEMORY_ORDER_RELAXED ), SIR_MEMORY_ORDER_RELAXED );
	slab.nLive = 0;
	slab.bWarmRetained = false;
	slab.nFresh.store( 0, SIR_MEMORY_ORDER_RELAXED );
	slab.nFreeHead = SIR_BUCKET_NONE;
	slab.nNeutralNext = SIR_BUCKET_NONE;
	slab.nPartialNext = SIR_BUCKET_NONE;
	slab.nPartialPrev = SIR_BUCKET_NONE;
	slab.bInPartial = false;
	slab.nEpoch.fetchAdd( 1, SIR_MEMORY_ORDER_RELEASE );
	slab.nClass.store( nClass, SIR_MEMORY_ORDER_RELEASE );
	slab.nAccessGate.store( 0, SIR_MEMORY_ORDER_RELEASE );
	seat.pActive[nClass] = nSlab;
	seat.pActiveDescriptor[nClass] = pDescriptor;
	return true;
}

void *SIRCompactBucketAllocator::alloc( uint32_t nBytes, uint32_t nAlignment )
{
	uint32_t nSeat = 0;
	if ( !findSeat( &nSeat ) || nBytes == 0 || nBytes > m_nMaxBytes || nAlignment == 0 || !SIR_IS_POWER_OF_TWO( nAlignment ) ||
	     nAlignment > m_nAlignment ) {
		return nullptr;
	}
	const uint32_t nClass = m_nClassMap[( nBytes - 1 ) >> m_nAlignmentShift];
	sirBucketSeat_t &owner = m_pSeats[nSeat];
	uint32_t nSlab = owner.pActive[nClass];
	// Only the owning seat writes the active descriptor cache. Descriptor
	// storage is stable until shutdown; neutralization invalidates this slot.
	sirBucketSlab_t *pDescriptor = owner.pActiveDescriptor[nClass];
	if ( pDescriptor == nullptr || ( pDescriptor->nFreeHead == SIR_BUCKET_NONE &&
	                                 pDescriptor->nFresh.load( SIR_MEMORY_ORDER_RELAXED ) >= getSlotCapacity( nClass ) ) ) {
		if ( !fillActive( nSeat, nClass ) ) {
			return nullptr;
		}
		nSlab = owner.pActive[nClass];
		pDescriptor = owner.pActiveDescriptor[nClass];
	}
	sirBucketSlab_t &slab = *pDescriptor;
	uint32_t nSlot = slab.nFreeHead;
	uint8_t *pSlab = m_pBase + ( ( uint64_t )( nSlab ) ) * m_nSlabBytes;
	if ( nSlot != SIR_BUCKET_NONE ) {
		std::memcpy( &slab.nFreeHead, pSlab + nSlot * m_nClassStride[nClass], sizeof( uint32_t ) );
		// Recycled slots can be observed by remote producers. Keep the
		// checked compare-exchange transition for this path.
		// Recycled slots are exclusively popped by their owning seat. A
		// single atomic bit-set preserves neighboring remote transitions in
		// the packed word without the extra CAS retry and load.
		SIRAtomic<uint64_t> &word = slab.pStates[nSlot >> 5];
		const uint32_t nShift = ( nSlot & 31 ) * 2;
		const uint64_t nPrevious = word.fetchOr( 1ull << nShift, SIR_MEMORY_ORDER_ACQUIRE_RELEASE );
		if ( ( ( nPrevious >> nShift ) & 3ull ) != 0 ) {
			// This indicates a corrupted freelist/state invariant. Restore
			// the owner freelist head and undo only our newly set low bit;
			// leave a producer-owned high bit intact for later reclamation.
			word.fetchAnd( ~( 1ull << nShift ), SIR_MEMORY_ORDER_ACQUIRE_RELEASE );
			slab.nFreeHead = nSlot;
			return nullptr;
		}
	} else {
		// A fresh slot was never published to another thread. Only the
		// owning seat advances nFresh; the packed word still requires atomic
		// RMW because adjacent, older slots can be remotely freed.
		// The fresh cursor has exactly one writer (the owning seat).
		// Readers on other threads observe the release-store in resolve().
		nSlot = slab.nFresh.load( SIR_MEMORY_ORDER_RELAXED );
		slab.nFresh.store( nSlot + 1, SIR_MEMORY_ORDER_RELEASE );
		SIRAtomic<uint64_t> &word = slab.pStates[nSlot >> 5];
		const uint32_t nShift = ( nSlot & 31 ) * 2;
		const uint64_t nMask = 1ull << nShift;
		const uint64_t nPrevious = word.fetchOr( nMask, SIR_MEMORY_ORDER_ACQUIRE_RELEASE );
		// Freshness is guaranteed by initialization and monotonic nFresh.
		// Should the invariant fail, roll back the single bit without ever
		// admitting a duplicate live allocation.
		if ( ( ( nPrevious >> nShift ) & 3ull ) != 0 ) {
			word.fetchAnd( ~nMask, SIR_MEMORY_ORDER_ACQUIRE_RELEASE );
			return nullptr;
		}
	}
	sirBucketSeat_t &seat = m_pSeats[nSeat];
	if ( slab.bWarmRetained ) {
		// Only the owning seat can allocate or drain this slab.
		slab.bWarmRetained = false;
		--seat.nWarmRetained;
	}
	++slab.nLive;
	seat.nLive.store( ++seat.nLiveOwner, SIR_MEMORY_ORDER_RELAXED );
	return pSlab + nSlot * m_nClassStride[nClass];
}

bool SIRCompactBucketAllocator::owns( const void *pMemory ) const
{
	if ( m_pSegment == nullptr || pMemory == nullptr ) {
		return false;
	}
	const uintptr_t nPointer = ( ( uintptr_t )( pMemory ) );
	const uintptr_t nBase = ( ( uintptr_t )( m_pBase ) );
	return nPointer >= nBase && nPointer - nBase < m_pSegment->getReservedBytes();
}

bool SIRCompactBucketAllocator::resolve( const void *pMemory, sirSlotLocation_t *pLocation ) const
{
	if ( !owns( pMemory ) || pLocation == nullptr ) {
		return false;
	}
	const uint64_t nOffset = ( ( uintptr_t )( pMemory ) ) - ( ( uintptr_t )( m_pBase ) );
	const uint32_t nSlab = ( ( uint32_t )( nOffset >> m_nSlabShift ) );
	const sirBucketSlab_t *pSlab = loadSlab( nSlab );
	if ( pSlab == nullptr ) {
		return false;
	}
	const uint32_t nClass = pSlab->nClass.load( SIR_MEMORY_ORDER_ACQUIRE );
	if ( nClass >= m_nClassCount ) {
		return false;
	}
	const uint32_t nInner = ( ( uint32_t )( nOffset ) ) & ( m_nSlabBytes - 1 );
	const uint32_t nStride = m_nClassStride[nClass];
	uint32_t nSlot = ( ( uint32_t )( ( ( uint64_t )( nInner )*m_nClassReciprocal[nClass] ) >> 32 ) );
	if ( nSlot * nStride > nInner ) {
		--nSlot;
	}
	if ( nSlot >= pSlab->nFresh.load( SIR_MEMORY_ORDER_ACQUIRE ) || nSlot * nStride != nInner ) {
		return false;
	}
	*pLocation = { nSlab, nSlot, nClass, pSlab->nEpoch.load( SIR_MEMORY_ORDER_ACQUIRE ) };
	return true;
}

uint32_t SIRCompactBucketAllocator::getMaximumBytes( void ) const
{
	return m_nMaxBytes;
}

uint32_t SIRCompactBucketAllocator::getMinimumAlignment( void ) const
{
	return m_nAlignment;
}

uint32_t SIRCompactBucketAllocator::getUsableSize( const void *pMemory ) const
{
	sirSlotLocation_t loc = {};
	return resolve( pMemory, &loc ) && loadState( loc.nSlab, loc.nSlot ) == 1 ? m_nClassStride[loc.nClass] : 0;
}

void SIRCompactBucketAllocator::makeNeutral( uint32_t nSlab )
{
	sirBucketSlab_t &slab = ( *loadSlab( nSlab ) );
	if ( slab.nLive != 0 ) {
		return;
	}
	// Own the retirement bit before releasing the packed state block. Every
	// concurrent producer must either complete its publication or fail entry.
	uint32_t nExpected = 0;
	if ( !slab.nAccessGate.compareExchangeStrong( nExpected, SIR_BUCKET_RETIRED, SIR_MEMORY_ORDER_ACQUIRE_RELEASE,
	                                              SIR_MEMORY_ORDER_ACQUIRE ) ) {
		return;
	}
	if ( slab.nLive != 0 ) {
		slab.nAccessGate.store( 0, SIR_MEMORY_ORDER_RELEASE );
		return;
	}
	const uint32_t nSeat = slab.nSeat.load( SIR_MEMORY_ORDER_ACQUIRE );
	const uint32_t nClass = slab.nClass.load( SIR_MEMORY_ORDER_RELAXED );
	if ( nSeat < m_nSeatCount && nClass < m_nClassCount && m_pSeats[nSeat].pActive[nClass] == nSlab ) {
		m_pSeats[nSeat].pActive[nClass] = SIR_BUCKET_NONE;
		m_pSeats[nSeat].pActiveDescriptor[nClass] = nullptr;
	}
	if ( nSeat < m_nSeatCount && nClass < m_nClassCount ) {
		unlinkPartial( nSeat, nClass, nSlab );
	}
	SIRScopedLock<SIRMutex> guard( m_ColdMutex );
	slab.nLastClass = nClass;
	if ( slab.bWarmRetained ) {
		if ( nSeat < m_nSeatCount ) {
			--m_pSeats[nSeat].nWarmRetained;
		}
		slab.bWarmRetained = false;
	}
	slab.nClass.store( SIR_BUCKET_NONE, SIR_MEMORY_ORDER_RELEASE );
	releaseStateBlock( slab.pStates );
	slab.pStates = nullptr;

	slab.pDirtyStateWords = nullptr;
	slab.nSeat.store( SIR_BUCKET_NONE, SIR_MEMORY_ORDER_RELEASE );
	slab.nNeutralNext = m_nNeutralHead;
	m_nNeutralHead = nSlab;
}

void SIRCompactBucketAllocator::releaseSlot( uint32_t nSlab, uint32_t nSlot )
{
	sirBucketSlab_t &slab = ( *loadSlab( nSlab ) );
	const uint32_t nClass = slab.nClass.load( SIR_MEMORY_ORDER_RELAXED );
	uint8_t *pSlot = m_pBase + ( ( uint64_t )( nSlab ) ) * m_nSlabBytes + nSlot * m_nClassStride[nClass];
	std::memcpy( pSlot, &slab.nFreeHead, sizeof( uint32_t ) );
	slab.nFreeHead = nSlot;
	--slab.nLive;
	const uint32_t nOwnerSeat = slab.nSeat.load( SIR_MEMORY_ORDER_RELAXED );
	if ( nOwnerSeat < m_nSeatCount ) {
		sirBucketSeat_t &seat = m_pSeats[nOwnerSeat];
		if ( seat.nOrphan.load( SIR_MEMORY_ORDER_ACQUIRE ) != 0 ) {
			seat.nLive.fetchSub( 1, SIR_MEMORY_ORDER_RELAXED );
		} else {
			seat.nLive.store( --seat.nLiveOwner, SIR_MEMORY_ORDER_RELAXED );
		}
	}
	if ( slab.nLive != 0 && slab.nFreeHead != SIR_BUCKET_NONE ) {
		const uint32_t nSeat = slab.nSeat.load( SIR_MEMORY_ORDER_RELAXED );
		const uint32_t nClass = slab.nClass.load( SIR_MEMORY_ORDER_RELAXED );
		if ( nSeat < m_nSeatCount && nClass < m_nClassCount && m_pSeats[nSeat].pActive[nClass] != nSlab &&
		     m_pSeats[nSeat].nOrphan.load( SIR_MEMORY_ORDER_RELAXED ) == 0 ) {
			linkPartial( nSeat, nClass, nSlab );
		}
	}
	if ( slab.nLive == 0 ) {
		const uint32_t nSeat = slab.nSeat.load( SIR_MEMORY_ORDER_RELAXED );
		const uint32_t nClass = slab.nClass.load( SIR_MEMORY_ORDER_RELAXED );
		// Retain a bounded number of recently emptied active slabs in the
		// owner's cache. The slot-state map remains valid and all slots are
		// marked free; a producer cannot have a pending free when nLive=0.
		if ( nSeat < m_nSeatCount && nClass < m_nClassCount && m_pSeats[nSeat].nOrphan.load( SIR_MEMORY_ORDER_ACQUIRE ) == 0 &&
		     m_pSeats[nSeat].pActive[nClass] == nSlab && m_pSeats[nSeat].nWarmRetained < m_nWarmPerSeat ) {
			slab.bWarmRetained = true;
			++m_pSeats[nSeat].nWarmRetained;
			return;
		}
		makeNeutral( nSlab );
	}
}

bool SIRCompactBucketAllocator::free( void *pMemory )
{
	if ( pMemory == nullptr ) {
		return true;
	}
	sirSlotLocation_t loc = {};
	if ( !resolve( pMemory, &loc ) ) {
		return false;
	}
	sirBucketSlab_t &slab = ( *loadSlab( loc.nSlab ) );
	const uint32_t nSeat = getAttachedSeat();
	if ( nSeat == slab.nSeat.load( SIR_MEMORY_ORDER_ACQUIRE ) &&
	     m_pSeats[nSeat].nGeneration.load( SIR_MEMORY_ORDER_ACQUIRE ) == slab.nSeatGeneration.load( SIR_MEMORY_ORDER_ACQUIRE ) ) {
		if ( !claimState( loc.nSlab, loc.nSlot, 1, 0 ) ) {
			return false;
		}
		releaseSlot( loc.nSlab, loc.nSlot );
		return true;
	}
	// Enter before accessing state. The access gate prevents state-block
	// retirement during publication, while owner-only nLive counts every
	// outstanding allocation until the remote return is drained.
	if ( !enterProducer( loc.nSlab, loc.nEpoch ) ) {
		return false;
	}
	if ( !claimState( loc.nSlab, loc.nSlot, 1, 2 ) ) {
		exitProducer( loc.nSlab );
		return false;
	}
	const uint32_t nOwner = slab.nSeat.load( SIR_MEMORY_ORDER_ACQUIRE );
	if ( nOwner >= m_nSeatCount ) {
		// The slab must remain discoverable until an orphan worker reclaims it.
		exitProducer( loc.nSlab );
		return true;
	}
	// The producer publishes the affected packed-state word, then the slab.
	// A consumer exchange racing either publication cannot lose the event:
	// the later slab publication forces another inspection.
	const uint64_t nWordMask = 1ull << ( ( loc.nSlot >> 5 ) & 63 );
	const uint64_t nPreviouslyDirty = slab.pDirtyStateWords[( loc.nSlot >> 5 ) >> 6].fetchOr( nWordMask, SIR_MEMORY_ORDER_RELEASE );
	// An already-published dirty word covers all newly queued slots in
	// that word. On a concurrent exchange, fetchOr observes zero and must
	// re-publish the slab. This saves two shared RMWs per subsequent free.
	if ( ( nPreviouslyDirty & nWordMask ) == 0 ) {
		publishDirtySlab( nOwner, loc.nSlab );
	}
	exitProducer( loc.nSlab );
	return true;
}

void SIRCompactBucketAllocator::publishDirtySlab( uint32_t nSeat, uint32_t nSlab )
{
	sirBucketInbox_t &inbox = *m_pSeats[nSeat].pRemote;
	const uint32_t nWord = nSlab >> 6;
	inbox.pDirtySlabWords[nWord].fetchOr( 1ull << ( nSlab & 63 ), SIR_MEMORY_ORDER_RELEASE );
	inbox.pDirtyGroupWords[nWord >> 6].fetchOr( 1ull << ( nWord & 63 ), SIR_MEMORY_ORDER_RELEASE );
}

uint32_t SIRCompactBucketAllocator::drainDirtySlab( uint32_t nSeat, uint32_t nSlab, uint32_t nMaximum )
{
	sirBucketSlab_t &slab = ( *loadSlab( nSlab ) );
	if ( slab.nSeat.load( SIR_MEMORY_ORDER_ACQUIRE ) != nSeat || slab.nClass.load( SIR_MEMORY_ORDER_ACQUIRE ) >= m_nClassCount ||
	     slab.pDirtyStateWords == nullptr ) {
		return 0;
	}
	uint32_t nDrained = 0;
	for ( uint32_t i = 0; i < m_nDirtyStateWordsPerSlab; ++i ) {
		if ( slab.nClass.load( SIR_MEMORY_ORDER_ACQUIRE ) >= m_nClassCount || slab.pDirtyStateWords == nullptr ) {
			break;
		}
		uint64_t nDirty = slab.pDirtyStateWords[i].exchange( 0, SIR_MEMORY_ORDER_ACQUIRE_RELEASE );
		while ( nDirty != 0 ) {
#if defined( _MSC_VER )
			unsigned long nBit = 0;
			_BitScanForward64( &nBit, nDirty );
#else
			const uint32_t nBit = ( ( uint32_t )( __builtin_ctzll( nDirty ) ) );
#endif
			const uint32_t nStateWord = i * 64 + ( ( uint32_t )( nBit ) );
			const uint64_t nMask = 1ull << nBit;
			nDirty &= ~nMask;
			if ( nStateWord >= m_nStateWordsPerSlab || slab.nClass.load( SIR_MEMORY_ORDER_ACQUIRE ) >= m_nClassCount ) {
				continue;
			}
			const uint64_t nStates = slab.pStates[nStateWord].load( SIR_MEMORY_ORDER_ACQUIRE );
			// State 2 (binary 10) appears as a bit in every even bit position.
			uint64_t nQueued = ( ~nStates & ( nStates >> 1 ) ) & 0x5555555555555555ull;
			while ( nQueued != 0 ) {
#if defined( _MSC_VER )
				unsigned long nQueuedBit = 0;
				_BitScanForward64( &nQueuedBit, nQueued );
#else
				const uint32_t nQueuedBit = ( ( uint32_t )( __builtin_ctzll( nQueued ) ) );
#endif
				nQueued &= nQueued - 1;
				const uint32_t nSlot = nStateWord * 32 + ( ( uint32_t )( nQueuedBit ) >> 1 );
				if ( nSlot < slab.nFresh.load( SIR_MEMORY_ORDER_ACQUIRE ) && claimState( nSlab, nSlot, 2, 0 ) ) {
					releaseSlot( nSlab, nSlot );
					++nDrained;
					if ( nDrained == nMaximum ) {
						// Preserve unconsumed state words. Revisit this word
						// because other queued slots may still be present.
						if ( slab.nClass.load( SIR_MEMORY_ORDER_ACQUIRE ) < m_nClassCount && slab.pDirtyStateWords != nullptr ) {
							slab.pDirtyStateWords[i].fetchOr( nDirty | nMask, SIR_MEMORY_ORDER_RELEASE );
							publishDirtySlab( nSeat, nSlab );
						}
						return nDrained;
					}
				}
			}
		}
	}
	return nDrained;
}

uint32_t SIRCompactBucketAllocator::drainSeat( uint32_t nSeat, uint32_t nMaximum )
{
	sirBucketInbox_t &inbox = *m_pSeats[nSeat].pRemote;
	uint32_t nDrained = 0;
	for ( uint32_t group = 0; group < m_nDirtyGroupsPerSeat && nDrained < nMaximum; ++group ) {
		uint64_t nGroups = inbox.pDirtyGroupWords[group].exchange( 0, SIR_MEMORY_ORDER_ACQUIRE_RELEASE );
		while ( nGroups != 0 ) {
#if defined( _MSC_VER )
			unsigned long nBit = 0;
			_BitScanForward64( &nBit, nGroups );
#else
			const uint32_t nBit = ( ( uint32_t )( __builtin_ctzll( nGroups ) ) );
#endif
			const uint64_t nMask = 1ull << nBit;
			nGroups &= ~nMask;
			const uint32_t nWord = group * 64 + ( ( uint32_t )( nBit ) );
			if ( nWord >= m_nDirtySlabWordsPerSeat ) {
				continue;
			}
			uint64_t nSlabs = inbox.pDirtySlabWords[nWord].exchange( 0, SIR_MEMORY_ORDER_ACQUIRE_RELEASE );
			while ( nSlabs != 0 ) {
#if defined( _MSC_VER )
				unsigned long nSlabBit = 0;
				_BitScanForward64( &nSlabBit, nSlabs );
#else
				const uint32_t nSlabBit = ( ( uint32_t )( __builtin_ctzll( nSlabs ) ) );
#endif
				const uint64_t nSlabMask = 1ull << nSlabBit;
				nSlabs &= ~nSlabMask;
				const uint32_t nSlab = nWord * 64 + ( ( uint32_t )( nSlabBit ) );
				if ( nSlab < m_nSlabCount ) {
					nDrained += drainDirtySlab( nSeat, nSlab, nMaximum - nDrained );
				}
				if ( nDrained >= nMaximum ) {
					if ( nSlabs != 0 ) {
						inbox.pDirtySlabWords[nWord].fetchOr( nSlabs, SIR_MEMORY_ORDER_RELEASE );
						inbox.pDirtyGroupWords[group].fetchOr( nMask, SIR_MEMORY_ORDER_RELEASE );
					}
					if ( nGroups != 0 ) {
						inbox.pDirtyGroupWords[group].fetchOr( nGroups, SIR_MEMORY_ORDER_RELEASE );
					}
					return nDrained;
				}
			}
		}
	}
	return nDrained;
}

uint32_t SIRCompactBucketAllocator::servicePending( uint32_t nMaximum )
{
	uint32_t nSeat = 0;
	return nMaximum != 0 && findSeat( &nSeat ) ? drainSeat( nSeat, nMaximum ) : 0;
}

uint32_t SIRCompactBucketAllocator::drainRemote( void )
{
	return servicePending( UINT32_MAX );
}

bool SIRCompactBucketAllocator::detach( void )
{
	uint32_t nSeat = 0;
	if ( !findSeat( &nSeat ) ) {
		return false;
	}
	drainSeat( nSeat );
	if ( m_pSeats[nSeat].nLive.load( SIR_MEMORY_ORDER_ACQUIRE ) != 0 ) {
		return false;
	}
	for ( uint32_t c = 0; c < m_nClassCount; ++c ) {
		const uint32_t nSlab = m_pSeats[nSeat].pActive[c];
		if ( nSlab != SIR_BUCKET_NONE && ( *loadSlab( nSlab ) ).bWarmRetained ) {
			makeNeutral( nSlab );
		}
		m_pSeats[nSeat].pActive[c] = SIR_BUCKET_NONE;
		m_pSeats[nSeat].pActiveDescriptor[c] = nullptr;
	}
	m_pSeats[nSeat].nOwnerToken.store( 0, SIR_MEMORY_ORDER_RELEASE );
	s_BucketBinding = { nullptr, UINT32_MAX, 0 };
	return true;
}

bool SIRCompactBucketAllocator::retireThread( void )
{
	uint32_t nSeat = 0;
	if ( !findSeat( &nSeat ) ) {
		return false;
	}
	drainSeat( nSeat );
	// nLive tracks all slots owned by a seat, including remotely queued
	// objects. It provides an exact retirement predicate without scanning
	// every activated slab under m_ColdMutex.
	const bool bHasLive = m_pSeats[nSeat].nLive.load( SIR_MEMORY_ORDER_ACQUIRE ) != 0;
	if ( bHasLive ) {
		m_pSeats[nSeat].nOrphan.store( 1, SIR_MEMORY_ORDER_RELEASE );
		m_nOrphanCount.fetchAdd( 1, SIR_MEMORY_ORDER_RELEASE );
	}
	// Release empty active slabs before letting another worker claim this
	// seat. No other worker is allowed to mutate this seat while attached.
	for ( uint32_t c = 0; c < m_nClassCount; ++c ) {
		const uint32_t nSlab = m_pSeats[nSeat].pActive[c];
		if ( nSlab != SIR_BUCKET_NONE && ( *loadSlab( nSlab ) ).nLive == 0 &&
		     ( *loadSlab( nSlab ) ).nAccessGate.load( SIR_MEMORY_ORDER_ACQUIRE ) == 0 ) {
			makeNeutral( nSlab );
		}
	}
	for ( uint32_t c = 0; c < m_nClassCount; ++c ) {
		m_pSeats[nSeat].pActive[c] = SIR_BUCKET_NONE;
		m_pSeats[nSeat].pActiveDescriptor[c] = nullptr;
	}
	m_pSeats[nSeat].nOwnerToken.store( 0, SIR_MEMORY_ORDER_RELEASE );
	s_BucketBinding = { nullptr, UINT32_MAX, 0 };
	return true;
}

uint32_t SIRCompactBucketAllocator::serviceOrphans( uint32_t nMaximum )
{
	if ( m_pSegment == nullptr || nMaximum == 0 ) {
		return 0;
	}
	SIRScopedLock<SIRMutex> orphanGuard( m_OrphanMutex );
	uint32_t nDrained = 0;
	for ( uint32_t nSeat = 0; nSeat < m_nSeatCount && nDrained < nMaximum; ++nSeat ) {
		if ( m_pSeats[nSeat].nOrphan.load( SIR_MEMORY_ORDER_ACQUIRE ) == 0 ) {
			continue;
		}
		nDrained += drainSeat( nSeat, nMaximum - nDrained );
		if ( m_pSeats[nSeat].nLive.load( SIR_MEMORY_ORDER_ACQUIRE ) == 0 ) {
			m_pSeats[nSeat].nOrphan.store( 0, SIR_MEMORY_ORDER_RELEASE );
			m_nOrphanCount.fetchSub( 1, SIR_MEMORY_ORDER_RELEASE );
		}
	}
	return nDrained;
}

bool SIRCompactBucketAllocator::hasPendingOrphans( void ) const
{
	return m_nOrphanCount.load( SIR_MEMORY_ORDER_ACQUIRE ) != 0;
}

uint64_t SIRCompactBucketAllocator::trimEmptySlabs( uint32_t nMaximumWarm )
{
	if ( getAttachedSeat() == SIR_BUCKET_NONE ) {
		return 0;
	}
	const uint32_t nSeat = getAttachedSeat();
	// Cold trim must be able to reclaim active warm slabs as well as neutral
	// slabs. Iterate before taking the cold mutex because makeNeutral locks it.
	if ( nMaximumWarm == 0 && nSeat < m_nSeatCount ) {
		for ( uint32_t c = 0; c < m_nClassCount; ++c ) {
			const uint32_t nSlab = m_pSeats[nSeat].pActive[c];
			if ( nSlab != SIR_BUCKET_NONE && ( *loadSlab( nSlab ) ).bWarmRetained ) {
				makeNeutral( nSlab );
			}
		}
	}
	SIRScopedLock<SIRMutex> guard( m_ColdMutex );
	uint32_t nWarm = 0;
	uint64_t nReleased = 0;
	for ( uint32_t i = m_nNeutralHead; i != SIR_BUCKET_NONE; i = ( *loadSlab( i ) ).nNeutralNext ) {
		sirBucketSlab_t &slab = ( *loadSlab( i ) );
		if ( !slab.bCommitted ) {
			continue;
		}
		if ( ++nWarm <= nMaximumWarm ) {
			continue;
		}
		if ( m_pSegment->decommit( ( ( uint64_t )( i ) ) * m_nSlabBytes, m_nSlabBytes ) ) {
			slab.bCommitted = false;
			nReleased += m_nSlabBytes;
		}
	}
	return nReleased;
}

uint64_t SIRCompactBucketAllocator::trimIdle( void )
{
	const uint64_t nWarm = ( ( uint64_t )( m_nWarmPerSeat ) ) * m_nSeatCount;
	return trimEmptySlabs( nWarm > UINT32_MAX ? UINT32_MAX : ( ( uint32_t )( nWarm ) ) );
}

uint64_t SIRCompactBucketAllocator::getLiveCount( void ) const
{
	uint64_t nTotal = 0;
	for ( uint32_t i = 0; i < m_nSeatCount; ++i ) {
		nTotal += m_pSeats[i].nLive.load( SIR_MEMORY_ORDER_ACQUIRE );
	}
	return nTotal;
}

bool SIRCompactBucketAllocator::shutdown( void )
{
	if ( m_pSegment == nullptr ) {
		return true;
	}
	if ( getLiveCount() != 0 ) {
		return false;
	}
	for ( uint32_t i = 0; i < m_nSeatCount; ++i ) {
		if ( m_pSeats[i].nOwnerToken.load( SIR_MEMORY_ORDER_ACQUIRE ) != 0 ) {
			return false;
		}
	}
	m_pSegment = nullptr;
	m_pBase = nullptr;
	return true;
}
