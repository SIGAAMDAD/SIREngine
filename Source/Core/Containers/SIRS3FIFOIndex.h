/* SIREngine - MPL-2.0. Three-queue scan-resistant FIFO cache admission index. */
#ifndef SIR_S3_FIFO_INDEX_H
#define SIR_S3_FIFO_INDEX_H
#include <Core/Containers/SIRVector.h>
#include <Core/Containers/SIRFlatHashMap.h>

class SIRS3FIFOIndex
{
	static constexpr uint32_t NONE = UINT32_MAX;
	enum Queue : uint8_t
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
		Queue eQueue = SMALL;
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
	void append( uint32_t iEntry, Queue eQueue ) noexcept
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
		const uint32_t iEntry = m_lists[GHOST].iHead;
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
		uint32_t iEntry = m_lists[SMALL].iHead;
		if ( iEntry == NONE ) {
			iEntry = m_lists[MAIN].iHead;
		}
		while ( iEntry != NONE ) {
			Entry& entry = m_entries[iEntry];
			unlink( iEntry );
			if ( entry.nFrequency != 0 ) {
				--entry.nFrequency;
				append( iEntry, MAIN );
				iEntry = m_lists[SMALL].iHead;
				if ( iEntry == NONE ) {
					iEntry = m_lists[MAIN].iHead;
				}
				continue;
			}
			append( iEntry, GHOST );
			--m_nResidents;
			return;
		}
	}

public:
	explicit SIRS3FIFOIndex( const sirContainerAllocator_t *pAllocator ) : m_entries( pAllocator ), m_positions( pAllocator )
	{
	}
	sirContainerResult_t tryInitialize( uint32_t nResidents, uint32_t nGhosts )
	{
		if ( m_nResidentCapacity != 0 || nResidents == 0 || nGhosts == 0 || nResidents > UINT32_MAX - nGhosts ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		sirContainerResult_t eResult = m_entries.tryReserve( nResidents + nGhosts );
		if ( eResult != SIR_CONTAINER_OK ) {
			return eResult;
		}
		eResult = m_positions.tryReserve( nResidents + nGhosts );
		if ( eResult != SIR_CONTAINER_OK ) {
			return eResult;
		}
		m_nResidentCapacity = nResidents;
		m_nGhostCapacity = nGhosts;
		return SIR_CONTAINER_OK;
	}
	sirContainerResult_t tryAccess( uint64_t nKey, bool *pHit = nullptr )
	{
		if ( m_nResidentCapacity == 0 ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		uint32_t *pIndex = m_positions.find( nKey );
		if ( pIndex != nullptr ) {
			Entry& entry = m_entries[*pIndex];
			const bool bResident = entry.eQueue != GHOST;
			if ( pHit != nullptr ) {
				*pHit = bResident;
			}
			if ( bResident ) {
				if ( entry.nFrequency < 3 ) {
					++entry.nFrequency;
				}
				return SIR_CONTAINER_OK;
			}
			if ( m_nResidents == m_nResidentCapacity ) {
				retireResident();
			}
			unlink( *pIndex );
			entry.nFrequency = 1;
			append( *pIndex, MAIN );
			++m_nResidents;
			return SIR_CONTAINER_OK;
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
			const sirContainerResult_t eResult = m_entries.tryEmplaceBack();
			if ( eResult != SIR_CONTAINER_OK ) {
				return eResult;
			}
			iEntry = m_entries.getSize() - 1;
			bNew = true;
		} else {
			m_iFree = m_entries[iEntry].iFree;
		}
		const sirContainerResult_t eResult = m_positions.tryInsert( nKey, iEntry );
		if ( eResult != SIR_CONTAINER_OK ) {
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
		append( iEntry, SMALL );
		++m_nResidents;
		while ( m_lists[GHOST].nCount > m_nGhostCapacity ) {
			dropOldestGhost();
		}
		return SIR_CONTAINER_OK;
	}
	bool containsResident( uint64_t nKey ) const
	{
		const uint32_t *pIndex = m_positions.find( nKey );
		return pIndex != nullptr && m_entries[*pIndex].eQueue != GHOST;
	}
	uint32_t getResidentCount( void ) const noexcept
	{
		return m_nResidents;
	}
	uint32_t getGhostCount( void ) const noexcept
	{
		return m_lists[GHOST].nCount;
	}
};
#endif
