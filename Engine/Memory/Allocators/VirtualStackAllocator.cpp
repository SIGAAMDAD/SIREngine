#include "VirtualStackAllocator.h"

using namespace SIREngine;

template<typename T>
static inline T *OffsetPointer( T *pStart, size_t nOffset )
{ return (T *)( (intptr_t)( pStart ) + nOffset ); }

template<typename T, typename U>
static inline ptrdiff_t PointerDifference(T *pEnd, U *pStart )
{ return (ptrdiff_t)( (intptr_t)( pEnd ) - (intptr_t)( pStart ) ); }

CVirtualStackAllocator::CVirtualStackAllocator( const char *pAllocatorName, size_t nRequestedSize,EVirtualStackAllocatorDecommitMode decommitMode )
    : IAllocatorBase( pAllocatorName ), m_DecommitMode( decommitMode )
{
    m_nTotalReservationSize = SIRENGINE_PAD( nRequestedSize, Application::Get()->GetOSPageSize() );

    if ( m_nTotalReservationSize > 0 ) {
        m_pMemory = Application::Get()->VirtualAlloc( &m_nTotalReservationSize, 64 );
        m_pNextUncommittedPage = m_pMemory;
        m_pNextAllocationStart = m_pNextUncommittedPage;
        m_pRecentHighWaterMark = m_pNextUncommittedPage;
    }
}

CVirtualStackAllocator::~CVirtualStackAllocator()
{
    if ( m_pNextUncommittedPage != NULL ) {
        Application::Get()->VirtualFree( m_pMemory, m_nVirtualMemorySize );
    }
}

void *CVirtualStackAllocator::Allocate( uint64_t nBytes, uint64_t nAlignment )
{
    void *pAllocationStart = (void *)SIRENGINE_PAD( (uintptr_t)m_pNextAllocationStart, nAlignment );

    if ( nBytes > 0 ) {
        void *pAllocationEnd = (void *)OffsetPointer( pAllocationStart, nBytes );
        void *pUsableMemoryEnd = (void *)OffsetPointer( m_pMemory, m_nTotalReservationSize );

        if ( pAllocationEnd > pUsableMemoryEnd ) {
            Application::Get()->OnOutOfMemory( nBytes, nAlignment );
        }

        // after the high water mark is established, needing to commit pages should be rare
        if ( pAllocationEnd > m_pNextUncommittedPage ) {
            // we need to commit some more pages. Let's see how many
			const uintptr_t nRequiredAdditionalCommit = PointerDifference( pAllocationEnd, m_pNextUncommittedPage );

			const size_t nSizeToCommit = SIRENGINE_PAD( nRequiredAdditionalCommit, Application::Get()->GetOSPageSize() );
			Application::Get()->CommitByAddress( m_pNextUncommittedPage, nSizeToCommit );

			m_pNextUncommittedPage = (void *)SIRENGINE_PAD( (uintptr_t)pAllocationEnd, Application::Get()->GetOSPageSize() );
        }

        if ( (byte *)pAllocationEnd > (byte *)m_pRecentHighWaterMark ) {
            m_pRecentHighWaterMark = (void *)SIRENGINE_PAD( (uintptr_t)pAllocationEnd, Application::Get()->GetOSPageSize() );
        }

        m_pNextAllocationStart = pAllocationEnd;
    }

    return pAllocationStart;
}

void CVirtualStackAllocator::DecommitUnusedPages( void )
{
    if ( m_DecommitMode == EVirtualStackAllocatorDecommitMode::AllOnStackEmpty ) {
        Application::Get()->DecommitMemory( m_pMemory, 0, m_nVirtualMemorySize );
        m_pNextUncommittedPage = m_pMemory;
    }
    else if ( m_DecommitMode == EVirtualStackAllocatorDecommitMode::ExcessOnStackEmpty ) {
        // in this mode, each time we get down to zero memory in use we consider decommitting some of the memory above the most recent high water mark
		const ptrdiff_t nAmountToFree = (uintptr_t)m_pNextUncommittedPage - (uintptr_t)m_pNextUncommittedPage;

		// we will only decommit memory if it would free up at least 25% of the current commit. This helps prevent us from thrashing pages if our
		// memory usage is consistant but not exactly constant and ensures we only pay to decommit if it will actually result in a significant savings
		const ptrdiff_t nMinimumToDecommit = PointerDifference( m_pNextUncommittedPage, m_pMemory ) / 4;
		if ( nAmountToFree > nMinimumToDecommit ) {
			// we have used less memory this time than the last time, decommit the excess
			Application::Get()->DecommitByAddress( m_pRecentHighWaterMark, nAmountToFree );
			m_pNextUncommittedPage = m_pRecentHighWaterMark;
		}
    }
}