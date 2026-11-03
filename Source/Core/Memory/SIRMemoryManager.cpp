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

#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Memory/SIRMemorySegment.h>

#include <Core/Platform/SIRVirtualMemory.h>

#include <cstring>
#include <mutex>
#include <new>

static bool sirIsPowerOfTwo( uint64_t nValue ) noexcept
{
	return nValue != 0 && ( nValue & ( nValue - 1 ) ) == 0;
}

static bool sirAlignUpChecked( uint64_t nValue, uint64_t nAlignment, uint64_t *pResult ) noexcept
{
	if ( pResult == nullptr || !sirIsPowerOfTwo( nAlignment ) || nAlignment > UINTPTR_MAX || nValue > UINTPTR_MAX - ( nAlignment - 1 ) ) {
		return false;
	}

	*pResult = ( nValue + nAlignment - 1 ) & ~( nAlignment - 1 );
	return true;
}

SIRMemoryManager::SIRMemoryManager( void )
    : m_pBase( nullptr ), m_pSegments( nullptr ), m_nPageSize( 0 ), m_nPageShift( 0 ), m_nReservedBytes( 0 ), m_nCommitBudgetBytes( 0 ),
      m_nMaximumCommitBudgetBytes( 0 ), m_nCommitDeniedCount( 0 ), m_nCommitCallCount( 0 ), m_nDecommitCallCount( 0 ),
      m_nDecommittedBytes( 0 ), m_nTraceDroppedCount( 0 ), m_nTraceSequence( 0 ), m_pTrace( nullptr ), m_nTraceCapacity( 0 ),
      m_nTraceCount( 0 ), m_nDomainCommittedBytes{}, m_nDomainMetadataCommittedBytes{}, m_nDomainPeakCommittedBytes{},
      m_nDomainCommitLimits{}, m_nCommittedBytes( 0 ), m_nPeakCommittedBytes( 0 ), m_nMetadataBytes( 0 ), m_nMetadataCommittedBytes( 0 ),
      m_nMetadataOffset( 0 ), m_nRecyclableFloor( 0 ), m_nRecyclableCommittedBytes( 0 ), m_nRecyclableUsedBytes( 0 ),
      m_pRecyclableStates( nullptr ), m_nSegmentCursor( 0 ), m_nMaxSegments( 0 ), m_nSegmentCount( 0 )
{
}

SIRMemoryManager::SIRMemoryManager( const sirMemoryConfig_t &config ) : SIRMemoryManager()
{
	init( config );
}

SIRMemoryManager::SIRMemoryManager( const sirMemoryConfig_t &config, const sirMemoryBudgetProfile_t &budget ) : SIRMemoryManager()
{
	initBudgeted( config, budget );
}

bool SIRMemoryManager::isInitialized( void ) const
{
	std::lock_guard<SIRMutex> lock( m_Mutex );
	return m_pBase != nullptr;
}

SIRMemoryManager::~SIRMemoryManager( void )
{
	shutdown();
}

bool SIRMemoryManager::init( const sirMemoryConfig_t &config )
{
	const sirMemoryBudgetProfile_t budget = { config.nCommitBudgetBytes, config.nCommitBudgetBytes };
	return initBudgeted( config, budget );
}

bool SIRMemoryManager::initBudgeted( const sirMemoryConfig_t &config, const sirMemoryBudgetProfile_t &budget )
{
	std::lock_guard<SIRMutex> lock( m_Mutex );
	if ( m_pBase != nullptr || config.nMaxSegments == 0 ) {
		return false;
	}

	const uint64_t nPageSize = SIRVirtualMemory::getPageSize();
	if ( !sirIsPowerOfTwo( nPageSize ) ) {
		return false;
	}

	uint64_t nReserve = 0;
	uint64_t nMetadata = 0;
	if ( !sirAlignUpChecked( config.nReserveBytes, nPageSize, &nReserve ) ||
	     !sirAlignUpChecked( config.nMetadataBytes, nPageSize, &nMetadata ) ) {
		return false;
	}
	if ( nMetadata == 0 || nReserve <= nMetadata || budget.nInitialCommitBudgetBytes == 0 ||
	     budget.nInitialCommitBudgetBytes > budget.nMaximumCommitBudgetBytes || budget.nMaximumCommitBudgetBytes > nReserve ||
	     ( budget.nInitialCommitBudgetBytes != budget.nMaximumCommitBudgetBytes &&
	       ( ( budget.nInitialCommitBudgetBytes % nPageSize ) != 0 || ( budget.nMaximumCommitBudgetBytes % nPageSize ) != 0 ) ) ||
	     ( ( uint64_t )( config.nMaxSegments ) ) > ( nMetadata / sizeof( SIRMemorySegment ) ) ) {
		return false;
	}

	uint8_t *pBase = ( ( uint8_t * )( SIRVirtualMemory::reserve( nReserve ) ) );
	if ( pBase == nullptr ) {
		return false;
	}

	// Metadata is reserved with the rest of the address space, but individual
	// pages are only committed when allocateMetadataLocked() reaches them.
	m_pBase = pBase;
	m_nPageSize = nPageSize;
	m_nPageShift = 0;
	for ( uint64_t nSize = nPageSize; nSize > 1; nSize >>= 1 ) {
		++m_nPageShift;
	}
	m_nReservedBytes = nReserve;
	m_nCommitBudgetBytes = budget.nInitialCommitBudgetBytes;
	m_nMaximumCommitBudgetBytes = budget.nMaximumCommitBudgetBytes;
	m_nCommitDeniedCount = 0;
	m_nCommitCallCount = 0;
	m_nDecommitCallCount = 0;
	m_nDecommittedBytes = 0;
	m_nTraceDroppedCount = 0;
	m_nTraceSequence = 0;
	m_pTrace = nullptr;
	m_nTraceCapacity = 0;
	m_nTraceCount = 0;
	m_nMetadataBytes = nMetadata;
	m_nMetadataCommittedBytes = 0;
	m_nMetadataOffset = 0;
	m_nRecyclableFloor = nMetadata;
	m_nRecyclableCommittedBytes = 0;
	m_nRecyclableUsedBytes = 0;
	m_pRecyclableStates = nullptr;
	m_nSegmentCursor = nMetadata;
	m_nCommittedBytes = 0;
	m_nPeakCommittedBytes = 0;
	for ( uint32_t i = 0; i < SIR_MEMORY_DOMAIN_COUNT; ++i ) {
		m_nDomainCommittedBytes[i] = 0;
		m_nDomainMetadataCommittedBytes[i] = 0;
		m_nDomainPeakCommittedBytes[i] = 0;
		m_nDomainCommitLimits[i] = 0; // Unrestricted until explicitly configured.
	}
	m_nMaxSegments = config.nMaxSegments;
	m_nSegmentCount = 0;

	const uint64_t nDescriptorBytes = ( ( uint64_t )( config.nMaxSegments ) ) * sizeof( SIRMemorySegment );
	m_pSegments =
	    ( ( SIRMemorySegment * )( allocateMetadataLocked( nDescriptorBytes, alignof( SIRMemorySegment ), SIR_MEMORY_DOMAIN_CORE ) ) );
	m_pRecyclableStates = ( ( uint8_t * )( allocateMetadataLocked( nMetadata / nPageSize, alignof( uint8_t ), SIR_MEMORY_DOMAIN_CORE ) ) );

	if ( m_pSegments == nullptr || m_pRecyclableStates == nullptr ) {
		SIRVirtualMemory::release( m_pBase, m_nReservedBytes );
		m_pBase = nullptr;
		m_pSegments = nullptr;
		m_nSegmentCount = 0;
		for ( uint32_t iDomain = 0; iDomain < SIR_MEMORY_DOMAIN_COUNT; ++iDomain ) {
			m_nDomainCommittedBytes[iDomain] = 0;
			m_nDomainMetadataCommittedBytes[iDomain] = 0;
			m_nDomainPeakCommittedBytes[iDomain] = 0;
			m_nDomainCommitLimits[iDomain] = 0;
		}
		m_nReservedBytes = 0;
		m_nCommittedBytes = 0;
		m_nPeakCommittedBytes = 0;
		m_nSegmentCursor = 0;
		m_nMetadataOffset = 0;
		m_nRecyclableFloor = 0;
		m_nRecyclableCommittedBytes = 0;
		m_nRecyclableUsedBytes = 0;
		m_pRecyclableStates = nullptr;
		m_nMetadataBytes = 0;
		m_nMetadataCommittedBytes = 0;
		m_nCommitBudgetBytes = 0;
		m_nMaximumCommitBudgetBytes = 0;
		m_nPageSize = 0;
		m_nPageShift = 0;
		m_nMaxSegments = 0;
		return false;
	}

	for ( uint32_t i = 0; i < m_nMaxSegments; ++i ) {
		new ( m_pSegments + i ) SIRMemorySegment();
	}
	return true;
}

void SIRMemoryManager::shutdown( void )
{
	std::lock_guard<SIRMutex> lock( m_Mutex );
	if ( m_pBase != nullptr ) {
		SIRVirtualMemory::release( m_pBase, m_nReservedBytes );
	}

	m_pBase = nullptr;
	m_pSegments = nullptr;
	m_nPageSize = 0;
	m_nPageShift = 0;
	m_nReservedBytes = 0;
	m_nCommitBudgetBytes = 0;
	m_nMaximumCommitBudgetBytes = 0;
	m_nCommitDeniedCount = 0;
	m_nCommitCallCount = 0;
	m_nDecommitCallCount = 0;
	m_nDecommittedBytes = 0;
	m_nTraceDroppedCount = 0;
	m_nTraceSequence = 0;
	m_pTrace = nullptr;
	m_nTraceCapacity = 0;
	m_nTraceCount = 0;
	m_nCommittedBytes = 0;
	m_nPeakCommittedBytes = 0;
	for ( uint32_t i = 0; i < SIR_MEMORY_DOMAIN_COUNT; ++i ) {
		m_nDomainCommittedBytes[i] = 0;
		m_nDomainMetadataCommittedBytes[i] = 0;
		m_nDomainPeakCommittedBytes[i] = 0;
		m_nDomainCommitLimits[i] = 0;
	}
	m_nMetadataBytes = 0;
	m_nMetadataCommittedBytes = 0;
	m_nMetadataOffset = 0;
	m_nRecyclableFloor = 0;
	m_nRecyclableCommittedBytes = 0;
	m_nRecyclableUsedBytes = 0;
	m_pRecyclableStates = nullptr;
	m_nSegmentCursor = 0;
	m_nMaxSegments = 0;
	m_nSegmentCount = 0;
}

SIRMemorySegment *SIRMemoryManager::createSegment( const char *pszName, uint64_t nBytes, uint64_t nCommitLimitBytes,
                                                   sirMemoryDomain_t eDomain )
{
	std::lock_guard<SIRMutex> lock( m_Mutex );
	if ( m_pBase == nullptr || pszName == nullptr || eDomain >= SIR_MEMORY_DOMAIN_COUNT || m_nSegmentCount == m_nMaxSegments ) {
		return nullptr;
	}

	uint64_t nCapacity = 0;
	if ( !sirAlignUpChecked( nBytes, m_nPageSize, &nCapacity ) ) {
		return nullptr;
	}
	if ( nCapacity == 0 || nCapacity > m_nReservedBytes - m_nSegmentCursor ) {
		return nullptr;
	}

	uint64_t nLimit = nCapacity;
	if ( nCommitLimitBytes != 0 ) {
		if ( !sirAlignUpChecked( nCommitLimitBytes, m_nPageSize, &nLimit ) ) {
			return nullptr;
		}
		if ( nLimit > nCapacity ) {
			return nullptr;
		}
	}

	const uint64_t nPages = nCapacity / m_nPageSize;
	const uint64_t nWords = ( nPages / 64 ) + ( ( nPages % 64 ) != 0 ? 1 : 0 );
	if ( nWords > UINT64_MAX / sizeof( uint64_t ) ) {
		return nullptr;
	}

	uint64_t *pBits = ( ( uint64_t * )( allocateMetadataLocked( nWords * sizeof( uint64_t ), alignof( uint64_t ), eDomain ) ) );
	if ( pBits == nullptr ) {
		return nullptr;
	}

	SIRMemorySegment *pSegment = m_pSegments + m_nSegmentCount;
	pSegment->m_pManager = this;
	pSegment->m_pBase = m_pBase + m_nSegmentCursor;
	pSegment->m_pCommittedBits = pBits;
	pSegment->m_nReservedBytes = nCapacity;
	pSegment->m_nCommittedBytes = 0;
	pSegment->m_nCommitLimitBytes = nLimit;
	pSegment->m_nPageCount = nPages;
	pSegment->m_eDomain = eDomain;
	uint32_t iName = 0;
	const uint32_t nNameCapacity = ( uint32_t )sizeof( pSegment->m_szName );
	for ( ; iName + 1 < nNameCapacity && pszName[iName] != '\0'; ++iName ) {
		pSegment->m_szName[iName] = pszName[iName];
	}
	pSegment->m_szName[iName] = '\0';

	m_nSegmentCursor += nCapacity;
	++m_nSegmentCount;
	return pSegment;
}

bool SIRMemoryManager::setDomainCommitLimit( sirMemoryDomain_t eDomain, uint64_t nLimitBytes )
{
	std::lock_guard<SIRMutex> lock( m_Mutex );
	if ( m_pBase == nullptr || eDomain >= SIR_MEMORY_DOMAIN_COUNT || nLimitBytes == 0 || ( nLimitBytes % m_nPageSize ) != 0 ) {
		return false;
	}
	const uint64_t nUsed = m_nDomainCommittedBytes[eDomain] + m_nDomainMetadataCommittedBytes[eDomain];
	if ( nLimitBytes < nUsed ) {
		return false;
	}
	m_nDomainCommitLimits[eDomain] = nLimitBytes;
	if ( nUsed > m_nDomainPeakCommittedBytes[eDomain] ) {
		m_nDomainPeakCommittedBytes[eDomain] = nUsed;
	}
	return true;
}

sirMemoryDomainStats_t SIRMemoryManager::getDomainStats( sirMemoryDomain_t eDomain ) const
{
	std::lock_guard<SIRMutex> lock( m_Mutex );
	if ( eDomain >= SIR_MEMORY_DOMAIN_COUNT || m_pBase == nullptr ) {
		return {};
	}
	return { m_nDomainCommittedBytes[eDomain] + m_nDomainMetadataCommittedBytes[eDomain], m_nDomainPeakCommittedBytes[eDomain],
	         m_nDomainCommitLimits[eDomain] };
}

void *SIRMemoryManager::allocateMetadata( uint64_t nBytes, uint64_t nAlignment, sirMemoryDomain_t eDomain )
{
	std::lock_guard<SIRMutex> lock( m_Mutex );
	return allocateMetadataLocked( nBytes, nAlignment, eDomain );
}

void *SIRMemoryManager::allocateRecyclableMetadataPage( sirMemoryDomain_t eDomain )
{
	std::lock_guard<SIRMutex> lock( m_Mutex );
	if ( m_pBase == nullptr || m_pRecyclableStates == nullptr || eDomain >= SIR_MEMORY_DOMAIN_COUNT ) {
		return nullptr;
	}
	const uint64_t nPageCount = m_nMetadataBytes / m_nPageSize;
	uint64_t nIndex = nPageCount;
	for ( uint64_t i = nPageCount; i > m_nRecyclableFloor / m_nPageSize; --i ) {
		if ( m_pRecyclableStates[i - 1] == 1 ) {
			nIndex = i - 1;
			break;
		}
	}
	const bool bNewPage = nIndex == nPageCount;
	if ( bNewPage ) {
		if ( m_nRecyclableFloor < m_nPageSize || m_nRecyclableFloor - m_nPageSize < m_nMetadataOffset ) {
			return nullptr;
		}
		nIndex = ( m_nRecyclableFloor / m_nPageSize ) - 1;
		m_nRecyclableFloor -= m_nPageSize;
	}
	const uint64_t nDomainUsed = m_nDomainCommittedBytes[eDomain] + m_nDomainMetadataCommittedBytes[eDomain];
	if ( m_nPageSize > m_nCommitBudgetBytes - m_nCommittedBytes ||
	     ( m_nDomainCommitLimits[eDomain] != 0 && m_nPageSize > m_nDomainCommitLimits[eDomain] - nDomainUsed ) ) {
		++m_nCommitDeniedCount;
		recordTraceLocked( SIR_MEMORY_TRACE_DENIED, m_nPageSize, UINT32_MAX );
		if ( bNewPage ) {
			m_nRecyclableFloor += m_nPageSize;
		} else {
			m_pRecyclableStates[nIndex] = 1;
		}
		return nullptr;
	}
	void *pPage = m_pBase + nIndex * m_nPageSize;
	if ( !SIRVirtualMemory::commit( pPage, m_nPageSize ) ) {
		if ( bNewPage ) {
			m_nRecyclableFloor += m_nPageSize;
		} else {
			m_pRecyclableStates[nIndex] = 1;
		}
		return nullptr;
	}
	std::memset( pPage, 0, ( ( size_t )( m_nPageSize ) ) );
	m_pRecyclableStates[nIndex] = ( ( uint8_t )( 2 + eDomain ) );
	m_nRecyclableCommittedBytes += m_nPageSize;
	m_nRecyclableUsedBytes += m_nPageSize;
	m_nDomainMetadataCommittedBytes[eDomain] += m_nPageSize;
	m_nCommittedBytes += m_nPageSize;
	if ( m_nCommittedBytes > m_nPeakCommittedBytes ) {
		m_nPeakCommittedBytes = m_nCommittedBytes;
	}
	const uint64_t nDomainPeak = m_nDomainCommittedBytes[eDomain] + m_nDomainMetadataCommittedBytes[eDomain];
	if ( nDomainPeak > m_nDomainPeakCommittedBytes[eDomain] ) {
		m_nDomainPeakCommittedBytes[eDomain] = nDomainPeak;
	}
	++m_nCommitCallCount;
	recordTraceLocked( SIR_MEMORY_TRACE_METADATA, m_nPageSize, UINT32_MAX );
	return pPage;
}

bool SIRMemoryManager::releaseRecyclableMetadataPage( void *pPage, sirMemoryDomain_t eDomain )
{
	std::lock_guard<SIRMutex> lock( m_Mutex );
	if ( m_pBase == nullptr || pPage == nullptr || eDomain >= SIR_MEMORY_DOMAIN_COUNT ) {
		return false;
	}
	const uintptr_t nAddress = ( ( uintptr_t )( pPage ) );
	const uintptr_t nBase = ( ( uintptr_t )( m_pBase ) );
	if ( nAddress < nBase || nAddress - nBase < m_nRecyclableFloor || nAddress - nBase >= m_nMetadataBytes ||
	     ( nAddress - nBase ) % m_nPageSize != 0 ) {
		return false;
	}
	const uint64_t nIndex = ( nAddress - nBase ) / m_nPageSize;
	if ( m_pRecyclableStates[nIndex] != 2 + eDomain || !SIRVirtualMemory::decommit( pPage, m_nPageSize ) ) {
		return false;
	}
	m_pRecyclableStates[nIndex] = 1;
	m_nRecyclableUsedBytes -= m_nPageSize;
	m_nRecyclableCommittedBytes -= m_nPageSize;
	m_nDomainMetadataCommittedBytes[eDomain] -= m_nPageSize;
	m_nCommittedBytes -= m_nPageSize;
	m_nDecommittedBytes += m_nPageSize;
	++m_nDecommitCallCount;
	recordTraceLocked( SIR_MEMORY_TRACE_DECOMMIT, m_nPageSize, UINT32_MAX );
	return true;
}

void *SIRMemoryManager::allocateMetadataLocked( uint64_t nBytes, uint64_t nAlignment, sirMemoryDomain_t eDomain )
{
	if ( m_pBase == nullptr || nBytes == 0 || eDomain >= SIR_MEMORY_DOMAIN_COUNT || !sirIsPowerOfTwo( nAlignment ) ) {
		return nullptr;
	}

	const uintptr_t nCurrent = ( ( uintptr_t )( m_pBase ) ) + m_nMetadataOffset;
	uint64_t nAlignedAddress = 0;
	if ( !sirAlignUpChecked( nCurrent, nAlignment, &nAlignedAddress ) ) {
		return nullptr;
	}
	if ( nAlignedAddress < ( ( uintptr_t )( m_pBase ) ) ) {
		return nullptr;
	}

	const uint64_t nAlignedOffset = nAlignedAddress - ( ( uintptr_t )( m_pBase ) );
	if ( nAlignedOffset > m_nRecyclableFloor || nBytes > m_nRecyclableFloor - nAlignedOffset ) {
		return nullptr;
	}

	const uint64_t nEndOffset = nAlignedOffset + nBytes;
	uint64_t nNewCommittedBytes = 0;
	if ( !sirAlignUpChecked( nEndOffset, m_nPageSize, &nNewCommittedBytes ) ) {
		return nullptr;
	}

	if ( nNewCommittedBytes > m_nMetadataCommittedBytes ) {
		const uint64_t nAdditionalBytes = nNewCommittedBytes - m_nMetadataCommittedBytes;
		if ( nAdditionalBytes > m_nCommitBudgetBytes - m_nCommittedBytes ||
		     ( m_nDomainCommitLimits[eDomain] != 0 &&
		       nAdditionalBytes >
		           m_nDomainCommitLimits[eDomain] - ( m_nDomainCommittedBytes[eDomain] + m_nDomainMetadataCommittedBytes[eDomain] ) ) ) {
			++m_nCommitDeniedCount;
			recordTraceLocked( SIR_MEMORY_TRACE_DENIED, nAdditionalBytes, UINT32_MAX );
			return nullptr;
		}
		if ( !SIRVirtualMemory::commit( m_pBase + m_nMetadataCommittedBytes, nAdditionalBytes ) ) {
			return nullptr;
		}
		++m_nCommitCallCount;
		m_nMetadataCommittedBytes = nNewCommittedBytes;
		m_nDomainMetadataCommittedBytes[eDomain] += nAdditionalBytes;
		const uint64_t nUsed = m_nDomainMetadataCommittedBytes[eDomain] + m_nDomainCommittedBytes[eDomain];
		if ( nUsed > m_nDomainPeakCommittedBytes[eDomain] ) {
			m_nDomainPeakCommittedBytes[eDomain] = nUsed;
		}
		m_nCommittedBytes += nAdditionalBytes;
		if ( m_nCommittedBytes > m_nPeakCommittedBytes ) {
			m_nPeakCommittedBytes = m_nCommittedBytes;
		}
	}

	m_nMetadataOffset = nEndOffset;
	recordTraceLocked( SIR_MEMORY_TRACE_METADATA, nBytes, UINT32_MAX );
	void *pMemory = m_pBase + nAlignedOffset;
	std::memset( pMemory, 0, ( ( size_t )( nBytes ) ) );
	return pMemory;
}

// A portable 64-bit population count. Compilers may replace the arithmetic
// with POPCNT when the selected x86 target supports it, but SSE2-only targets
// do not depend on that instruction being available.
static uint32_t sirCountBits64( uint64_t nBits )
{
	nBits -= ( nBits >> 1 ) & 0x5555555555555555ull;
	nBits = ( nBits & 0x3333333333333333ull ) + ( ( nBits >> 2 ) & 0x3333333333333333ull );
	nBits = ( nBits + ( nBits >> 4 ) ) & 0x0f0f0f0f0f0f0f0full;
	return ( ( uint32_t )( ( nBits * 0x0101010101010101ull ) >> 56 ) );
}

static uint64_t sirLowMask64( uint32_t nBits )
{
	return nBits == 0 ? ~0ull : ( 1ull << nBits ) - 1ull;
}

uint64_t SIRMemoryManager::countCommittedPages( const SIRMemorySegment *pSegment, uint64_t nFirstPage, uint64_t nPageCount ) const
{
	if ( nPageCount == 0 ) {
		return 0;
	}

	const uint64_t nFirstWord = nFirstPage >> 6;
	const uint64_t nEndPage = nFirstPage + nPageCount;
	const uint64_t nLastWord = ( nEndPage - 1 ) >> 6;
	const uint64_t nFirstMask = ~0ull << ( nFirstPage & 63 );
	const uint64_t nLastMask = sirLowMask64( ( ( uint32_t )( nEndPage & 63 ) ) );
	const uint64_t *pBits = pSegment->m_pCommittedBits;

	if ( nFirstWord == nLastWord ) {
		return sirCountBits64( pBits[nFirstWord] & nFirstMask & nLastMask );
	}

	uint64_t nCount = sirCountBits64( pBits[nFirstWord] & nFirstMask );
	for ( uint64_t i = nFirstWord + 1; i < nLastWord; ++i ) {
		nCount += sirCountBits64( pBits[i] );
	}
	return nCount + sirCountBits64( pBits[nLastWord] & nLastMask );
}

static void sirWritePageBits( uint64_t *pWord, uint64_t nMask, bool bCommitted )
{
	if ( bCommitted ) {
		*pWord |= nMask;
	} else {
		*pWord &= ~nMask;
	}
}

void SIRMemoryManager::markCommittedPages( SIRMemorySegment *pSegment, uint64_t nFirstPage, uint64_t nPageCount, bool bCommitted )
{
	if ( nPageCount == 0 ) {
		return;
	}

	const uint64_t nFirstWord = nFirstPage >> 6;
	const uint64_t nEndPage = nFirstPage + nPageCount;
	const uint64_t nLastWord = ( nEndPage - 1 ) >> 6;
	const uint64_t nFirstMask = ~0ull << ( nFirstPage & 63 );
	const uint64_t nLastMask = sirLowMask64( ( ( uint32_t )( nEndPage & 63 ) ) );
	uint64_t *pBits = pSegment->m_pCommittedBits;

	if ( nFirstWord == nLastWord ) {
		sirWritePageBits( &pBits[nFirstWord], nFirstMask & nLastMask, bCommitted );
		return;
	}

	sirWritePageBits( &pBits[nFirstWord], nFirstMask, bCommitted );
	for ( uint64_t i = nFirstWord + 1; i < nLastWord; ++i ) {
		pBits[i] = bCommitted ? ~0ull : 0ull;
	}
	sirWritePageBits( &pBits[nLastWord], nLastMask, bCommitted );
}

bool SIRMemoryManager::commit( SIRMemorySegment *pSegment, uint64_t nOffset, uint64_t nBytes )
{
	std::lock_guard<SIRMutex> lock( m_Mutex );
	if ( m_pBase == nullptr || pSegment == nullptr || nBytes == 0 || ( ( nOffset | nBytes ) & ( m_nPageSize - 1 ) ) != 0 ||
	     nOffset > pSegment->m_nReservedBytes || nBytes > pSegment->m_nReservedBytes - nOffset ) {
		return false;
	}

	const uint64_t nFirst = nOffset >> m_nPageShift;
	const uint64_t nPages = nBytes >> m_nPageShift;
	const uint64_t nPreviouslyCommitted = countCommittedPages( pSegment, nFirst, nPages );
	const uint64_t nAdditionalBytes = ( nPages - nPreviouslyCommitted ) * m_nPageSize;
	if ( nAdditionalBytes == 0 ) {
		return true;
	}

	const uint32_t nDomain = pSegment->m_eDomain;
	const uint64_t nDomainUsage = m_nDomainCommittedBytes[nDomain] + m_nDomainMetadataCommittedBytes[nDomain];
	if ( nAdditionalBytes > m_nCommitBudgetBytes - m_nCommittedBytes ||
	     nAdditionalBytes > pSegment->m_nCommitLimitBytes - pSegment->m_nCommittedBytes ||
	     ( m_nDomainCommitLimits[nDomain] != 0 && nAdditionalBytes > m_nDomainCommitLimits[nDomain] - nDomainUsage ) ) {
		++m_nCommitDeniedCount;
		recordTraceLocked( SIR_MEMORY_TRACE_DENIED, nAdditionalBytes, ( ( uint32_t )( pSegment - m_pSegments ) ) );
		return false;
	}

	if ( !SIRVirtualMemory::commit( pSegment->m_pBase + nOffset, nBytes ) ) {
		return false;
	}

	++m_nCommitCallCount;
	recordTraceLocked( SIR_MEMORY_TRACE_COMMIT, nAdditionalBytes, ( ( uint32_t )( pSegment - m_pSegments ) ) );
	markCommittedPages( pSegment, nFirst, nPages, true );
	pSegment->m_nCommittedBytes += nAdditionalBytes;
	m_nDomainCommittedBytes[nDomain] += nAdditionalBytes;
	const uint64_t nEffectiveDomainUsage = m_nDomainCommittedBytes[nDomain] + m_nDomainMetadataCommittedBytes[nDomain];
	if ( nEffectiveDomainUsage > m_nDomainPeakCommittedBytes[nDomain] ) {
		m_nDomainPeakCommittedBytes[nDomain] = nEffectiveDomainUsage;
	}
	m_nCommittedBytes += nAdditionalBytes;
	if ( m_nCommittedBytes > m_nPeakCommittedBytes ) {
		m_nPeakCommittedBytes = m_nCommittedBytes;
	}
	return true;
}

bool SIRMemoryManager::decommit( SIRMemorySegment *pSegment, uint64_t nOffset, uint64_t nBytes )
{
	std::lock_guard<SIRMutex> lock( m_Mutex );
	if ( m_pBase == nullptr || pSegment == nullptr || nBytes == 0 || ( ( nOffset | nBytes ) & ( m_nPageSize - 1 ) ) != 0 ||
	     nOffset > pSegment->m_nReservedBytes || nBytes > pSegment->m_nReservedBytes - nOffset ) {
		return false;
	}

	const uint64_t nFirst = nOffset >> m_nPageShift;
	const uint64_t nPages = nBytes >> m_nPageShift;
	const uint64_t nCommittedPages = countCommittedPages( pSegment, nFirst, nPages );
	if ( nCommittedPages == 0 ) {
		return true;
	}

	if ( !SIRVirtualMemory::decommit( pSegment->m_pBase + nOffset, nBytes ) ) {
		return false;
	}

	const uint64_t nFreedBytes = nCommittedPages * m_nPageSize;
	++m_nDecommitCallCount;
	m_nDecommittedBytes += nFreedBytes;
	recordTraceLocked( SIR_MEMORY_TRACE_DECOMMIT, nFreedBytes, ( ( uint32_t )( pSegment - m_pSegments ) ) );
	markCommittedPages( pSegment, nFirst, nPages, false );
	pSegment->m_nCommittedBytes -= nFreedBytes;
	m_nDomainCommittedBytes[pSegment->m_eDomain] -= nFreedBytes;
	m_nCommittedBytes -= nFreedBytes;
	return true;
}

uint64_t SIRMemoryManager::getSegmentCommittedBytes( const SIRMemorySegment *pSegment ) const
{
	std::lock_guard<SIRMutex> lock( m_Mutex );
	return m_pBase != nullptr ? pSegment->m_nCommittedBytes : 0;
}

sirMemoryStats_t SIRMemoryManager::getStats( void ) const
{
	std::lock_guard<SIRMutex> lock( m_Mutex );
	return { m_nReservedBytes,
	         m_nCommittedBytes,
	         m_nPeakCommittedBytes,
	         m_nCommitBudgetBytes,
	         m_nMetadataCommittedBytes + m_nRecyclableCommittedBytes,
	         m_nSegmentCount,
	         m_nMaximumCommitBudgetBytes,
	         m_nCommitDeniedCount,
	         m_nCommitCallCount,
	         m_nDecommitCallCount,
	         m_nDecommittedBytes,
	         m_nMetadataOffset + m_nRecyclableUsedBytes,
	         m_nTraceDroppedCount };
}

uint64_t SIRMemoryManager::getPageSize( void ) const
{
	std::lock_guard<SIRMutex> lock( m_Mutex );
	return m_nPageSize;
}

SIRMemorySegment::SIRMemorySegment( void )
    : m_pManager( nullptr ), m_pBase( nullptr ), m_pCommittedBits( nullptr ), m_nReservedBytes( 0 ), m_nCommittedBytes( 0 ),
      m_nCommitLimitBytes( 0 ), m_nPageCount( 0 ), m_eDomain( SIR_MEMORY_DOMAIN_GODOT ), m_szName{}
{
}

void *SIRMemorySegment::getBase( void ) const
{
	return m_pBase;
}

const char *SIRMemorySegment::getName( void ) const
{
	return m_szName;
}

uint64_t SIRMemorySegment::getPageSize( void ) const
{
	return m_pManager != nullptr ? m_pManager->getPageSize() : 0;
}

uint64_t SIRMemorySegment::getReservedBytes( void ) const
{
	return m_nReservedBytes;
}

uint64_t SIRMemorySegment::getCommittedBytes( void ) const
{
	return m_pManager != nullptr ? m_pManager->getSegmentCommittedBytes( this ) : 0;
}

uint64_t SIRMemorySegment::getCommitLimitBytes( void ) const
{
	return m_nCommitLimitBytes;
}

sirMemoryDomain_t SIRMemorySegment::getDomain( void ) const
{
	return m_eDomain;
}

bool SIRMemorySegment::commit( uint64_t nOffset, uint64_t nBytes )
{
	return m_pManager != nullptr && m_pManager->commit( this, nOffset, nBytes );
}

bool SIRMemorySegment::decommit( uint64_t nOffset, uint64_t nBytes )
{
	return m_pManager != nullptr && m_pManager->decommit( this, nOffset, nBytes );
}

SIRMemoryManager *SIRMemorySegment::getManager( void ) const
{
	return m_pManager;
}

bool SIRMemoryManager::setCommitBudgetBytes( uint64_t nNewBudget )
{
	std::lock_guard<SIRMutex> lock( m_Mutex );
	if ( m_pBase == nullptr || ( nNewBudget % m_nPageSize ) != 0 || nNewBudget < m_nCommitBudgetBytes ||
	     nNewBudget > m_nMaximumCommitBudgetBytes ) {
		return false;
	}
	m_nCommitBudgetBytes = nNewBudget;
	return true;
}

void SIRMemoryManager::recordTraceLocked( uint32_t nType, uint64_t nBytes, uint32_t nSegment )
{
	if ( m_pTrace == nullptr ) {
		return;
	}
	// Overwrite the oldest entry while preserving monotonic sequence numbers.
	if ( m_nTraceCount < m_nTraceCapacity ) {
		++m_nTraceCount;
	} else {
		++m_nTraceDroppedCount;
	}
	sirMemoryTraceEvent_t &event = m_pTrace[m_nTraceSequence % m_nTraceCapacity];
	event.nSequence = ++m_nTraceSequence;
	event.nBytes = nBytes;
	event.nType = nType;
	event.nSegment = nSegment;
}

bool SIRMemoryManager::initTraceBuffer( uint32_t nEntries )
{
	if ( nEntries == 0 ) {
		return false;
	}
	std::lock_guard<SIRMutex> lock( m_Mutex );
	if ( m_pTrace != nullptr || m_pBase == nullptr ) {
		return false;
	}
	m_pTrace = ( ( sirMemoryTraceEvent_t * )( allocateMetadataLocked( ( ( uint64_t )( nEntries ) ) * sizeof( sirMemoryTraceEvent_t ),
	                                                                  alignof( sirMemoryTraceEvent_t ), SIR_MEMORY_DOMAIN_CORE ) ) );
	if ( m_pTrace == nullptr ) {
		return false;
	}
	m_nTraceCapacity = nEntries;
	m_nTraceCount = 0;
	return true;
}

uint32_t SIRMemoryManager::copyTrace( sirMemoryTraceEvent_t *pOutput, uint32_t nCapacity ) const
{
	std::lock_guard<SIRMutex> lock( m_Mutex );
	if ( pOutput == nullptr || nCapacity == 0 ) {
		return 0;
	}
	const uint32_t nCount = nCapacity < m_nTraceCount ? nCapacity : m_nTraceCount;
	if ( nCount == 0 ) {
		return 0;
	}
	const uint64_t nFirstSequence = m_nTraceSequence - nCount;
	for ( uint32_t i = 0; i < nCount; ++i ) {
		pOutput[i] = m_pTrace[( nFirstSequence + i ) % m_nTraceCapacity];
	}
	return nCount;
}
