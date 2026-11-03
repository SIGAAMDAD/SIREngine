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

#include <Core/Memory/SIRMemoryColdPageStager.h>
#include <Core/Memory/SIRMemoryManager.h>
#include <Core/Memory/SIRMemorySegment.h>

SIRMemoryColdPageStager::SIRMemoryColdPageStager( void ) : m_pRanges( nullptr ), m_nCapacity( 0 ), m_nCount( 0 ), m_pManager( nullptr )
{
}

bool SIRMemoryColdPageStager::init( SIRMemoryManager *pManager, uint32_t nCapacity )
{
	if ( m_pRanges != nullptr || pManager == nullptr || nCapacity == 0 ) {
		return false;
	}
	const uint64_t nBytes = ( ( uint64_t )( nCapacity ) ) * sizeof( sirStageRange_t );
	sirStageRange_t *pRanges = ( ( sirStageRange_t * )( pManager->allocateMetadata( nBytes, alignof( sirStageRange_t ) ) ) );
	if ( pRanges == nullptr ) {
		return false;
	}
	m_pRanges = pRanges;
	m_nCapacity = nCapacity;
	m_pManager = pManager;
	return true;
}

bool SIRMemoryColdPageStager::add( SIRMemorySegment *pSegment, uint64_t nOffset, uint64_t nBytes )
{
	if ( m_pRanges == nullptr || pSegment == nullptr ) {
		return false;
	}
	if ( pSegment->getManager() != m_pManager ) {
		return false;
	}
	if ( m_nCount == m_nCapacity || nBytes == 0 || nOffset > pSegment->getReservedBytes() ||
	     nBytes > pSegment->getReservedBytes() - nOffset || ( ( nOffset | nBytes ) & ( pSegment->getPageSize() - 1 ) ) != 0 ) {
		return false;
	}
	if ( m_nCount != 0 ) {
		const sirStageRange_t &prev = m_pRanges[m_nCount - 1];
		if ( prev.pSegment == pSegment && nOffset < prev.nOffset ) {
			return false;
		}
	}
	m_pRanges[m_nCount++] = { pSegment, nOffset, nBytes };
	return true;
}

sirColdStageReport_t SIRMemoryColdPageStager::commitStaged( void )
{
	sirColdStageReport_t result = { 0, 0, 0, UINT32_MAX };
	uint32_t i = 0;
	while ( i < m_nCount ) {
		SIRMemorySegment *pSegment = m_pRanges[i].pSegment;
		uint64_t nFirst = m_pRanges[i].nOffset;
		uint64_t nEnd = nFirst + m_pRanges[i].nBytes;
		++i;
		while ( i < m_nCount && m_pRanges[i].pSegment == pSegment && m_pRanges[i].nOffset <= nEnd ) {
			const uint64_t nNextEnd = m_pRanges[i].nOffset + m_pRanges[i].nBytes;
			if ( nNextEnd > nEnd ) {
				nEnd = nNextEnd;
			}
			++i;
		}
		const uint64_t nBefore = pSegment->getCommittedBytes();
		result.nRequestedBytes += nEnd - nFirst;
		if ( !pSegment->commit( nFirst, nEnd - nFirst ) ) {
			result.nFailedRange = i - 1;
			break;
		}
		const uint64_t nAfter = pSegment->getCommittedBytes();
		result.nNewCommittedBytes += nAfter - nBefore;
		++result.nCompletedRanges;
	}
	m_nCount = 0;
	return result;
}

void SIRMemoryColdPageStager::clear( void )
{
	m_nCount = 0;
}
