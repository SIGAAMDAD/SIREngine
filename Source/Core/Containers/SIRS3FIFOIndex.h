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

#ifndef __SIR_S3FIFOINDEX_H__
#define __SIR_S3FIFOINDEX_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRVector.h>
#include <Core/Containers/SIRFlatHashMap.h>

class SIRS3FIFOIndex
{
	static constexpr uint32_t NONE = UINT32_MAX;
	enum class SIRQueue : uint8_t
	{
		SMALL = 0,
		MAIN = 1,
		GHOST = 2
	};

	struct Entry
	{
		uint64_t nKey = 0;
		uint32_t iPrevious = NONE;
		uint32_t iNext = NONE;
		uint32_t nFrequency = 0;
		uint32_t iFree = NONE;
		SIRQueue eQueue = SIRQueue::SMALL;
		bool bLive = false;
	};

	struct List
	{
		uint32_t iHead = NONE;
		uint32_t iTail = NONE;
		uint32_t nCount = 0;
	};

	SIRVector<Entry> m_entries;
	SIRFlatHashMap<uint64_t, uint32_t> m_positions;
	List m_lists[3];
	uint32_t m_iFree = NONE;
	uint32_t m_nResidentCapacity = 0;
	uint32_t m_nGhostCapacity = 0;
	uint32_t m_nResidents = 0;

	void unlink( uint32_t iEntry ) noexcept
	{
		Entry& entry = m_entries[iEntry];
		List& list = m_lists[entry.eQueue];
		if ( entry.iPrevious == NONE ) {
			list.iHead = entry.iNext;
		} else {
			m_entries[entry.iPrevious].iNext = entry.iNext;
		}
		if ( entry.iNext == NONE ) {
			list.iTail = entry.iPrevious;
		} else {
			m_entries[entry.iNext].iPrevious = entry.iPrevious;
		}
		--list.nCount;
	}

	void append( uint32_t iEntry, SIRQueue eQueue ) noexcept
	{
		Entry& entry = m_entries[iEntry];
		List& list = m_lists[eQueue];
		entry.eQueue = eQueue;
		entry.iPrevious = list.iTail;
		entry.iNext = NONE;
		if ( list.iTail != NONE ) {
			m_entries[list.iTail].iNext = iEntry;
		} else {
			list.iHead = iEntry;
		}
		list.iTail = iEntry;
		++list.nCount;
	}

	void dropOldestGhost( void ) noexcept
	{
		const uint32_t iEntry = m_lists[SIRQueue::GHOST].iHead;
		if ( iEntry == NONE ) {
			return;
		}
		unlink( iEntry );
		m_positions.erase( m_entries[iEntry].nKey );
		m_entries[iEntry].bLive = false;
		m_entries[iEntry].iFree = m_iFree;
		m_iFree = iEntry;
	}

	void retireResident( void ) noexcept
	{
		uint32_t iEntry = m_lists[SIRQueue::SMALL].iHead;
		if ( iEntry == NONE ) {
			iEntry = m_lists[SIRQueue::MAIN].iHead;
		}
		while ( iEntry != NONE ) {
			Entry& entry = m_entries[iEntry];
			unlink( iEntry );
			if ( entry.nFrequency != 0 ) {
				--entry.nFrequency;
				append( iEntry, SIRQueue::MAIN );
				iEntry = m_lists[SIRQueue::SMALL].iHead;
				if ( iEntry == NONE ) {
					iEntry = m_lists[SIRQueue::MAIN].iHead;
				}
				continue;
			}
			append( iEntry, SIRQueue::GHOST );
			--m_nResidents;
			return;
		}
	}

public:
	explicit SIRS3FIFOIndex( const sirContainerAllocator_t *pAllocator )
	    : m_entries( pAllocator ),
	      m_positions( pAllocator )
	{
	}

	SIRContainerResult tryInitialize( uint32_t nResidents, uint32_t nGhosts )
	{
		if ( m_nResidentCapacity != 0 || nResidents == 0 || nGhosts == 0 || nResidents > UINT32_MAX - nGhosts ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		SIRContainerResult eResult = m_entries.tryReserve( nResidents + nGhosts );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		eResult = m_positions.tryReserve( nResidents + nGhosts );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		m_nResidentCapacity = nResidents;
		m_nGhostCapacity = nGhosts;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	SIRContainerResult tryAccess( uint64_t nKey, bool *pHit = nullptr )
	{
		if ( m_nResidentCapacity == 0 ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		uint32_t *pIndex = m_positions.find( nKey );
		if ( pIndex != nullptr ) {
			Entry& entry = m_entries[*pIndex];
			const bool bResident = entry.eQueue != SIRQueue::GHOST;
			if ( pHit != nullptr ) {
				*pHit = bResident;
			}
			if ( bResident ) {
				if ( entry.nFrequency < 3 ) {
					++entry.nFrequency;
				}
				return SIRContainerResult::SIR_CONTAINER_OK;
			}
			if ( m_nResidents == m_nResidentCapacity ) {
				retireResident();
			}
			unlink( *pIndex );
			entry.nFrequency = 1;
			append( *pIndex, SIRQueue::MAIN );
			++m_nResidents;
			return SIRContainerResult::SIR_CONTAINER_OK;
		}
		if ( pHit != nullptr ) {
			*pHit = false;
		}
		if ( m_entries.getSize() >= m_nResidentCapacity + m_nGhostCapacity && m_iFree == NONE ) {
			dropOldestGhost();
		}
		uint32_t iEntry = m_iFree;
		bool bNew = false;
		if ( iEntry == NONE ) {
			const SIRContainerResult eResult = m_entries.tryEmplaceBack();
			if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
				return eResult;
			}
			iEntry = m_entries.getSize() - 1;
			bNew = true;
		} else {
			m_iFree = m_entries[iEntry].iFree;
		}
		const SIRContainerResult eResult = m_positions.tryInsert( nKey, iEntry );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			if ( bNew ) {
				m_entries.tryPopBack();
			} else {
				m_entries[iEntry].iFree = m_iFree;
				m_iFree = iEntry;
			}
			return eResult;
		}
		if ( m_nResidents == m_nResidentCapacity ) {
			retireResident();
		}
		Entry& entry = m_entries[iEntry];
		entry.nKey = nKey;
		entry.nFrequency = 0;
		entry.bLive = true;
		append( iEntry, SIRQueue::SMALL );
		++m_nResidents;
		while ( m_lists[SIRQueue::GHOST].nCount > m_nGhostCapacity ) {
			dropOldestGhost();
		}
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	bool containsResident( uint64_t nKey ) const
	{
		const uint32_t *pIndex = m_positions.find( nKey );
		return pIndex != nullptr && m_entries[*pIndex].eQueue != SIRQueue::GHOST;
	}

	uint32_t getResidentCount( void ) const noexcept
	{
		return m_nResidents;
	}

	uint32_t getGhostCount( void ) const noexcept
	{
		return m_lists[SIRQueue::GHOST].nCount;
	}
};
#endif /* __SIR_S3FIFOINDEX_H__ */
