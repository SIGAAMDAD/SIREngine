#include <Engine/Memory/Allocators/SlabAllocator.h>

using namespace SIREngine;

template<typename AllocatorType>
void CSlabAllocator<AllocatorType>::Init( uint64_t nBytes )
{
    // ensure we aren't leaking memory
    Shutdown();

    m_pBase = (uint8_t *)m_Allocator.allocate( nBytes );
    if ( !m_pBase ) {

    }

    m_nSize = nBytes;
    m_nUsed = 0;
}

template<typename AllocatorType>
void CSlabAllocator<AllocatorType>::Shutdown( void )
{
    if ( m_pBase != NULL ) {
        m_Allocator.deallocate( m_pBase );
        m_nSize = 0;
        m_nUsed = 0;
        m_pBase = NULL;
    }
}

template<typename AllocatorType>
void *CSlabAllocator<AllocatorType>::Allocate( uint64_t nBytes, uint64_t nAlignment )
{
    void *pBuffer;

    nBytes = SIRENGINE_PAD( nBytes, sizeof( uintptr_t ) );
    if ( m_nUsed + nBytes >= m_nSize ) {
        assert( 0 );
        return NULL;
    }

    pBuffer = (void *)( m_pBase + m_nUsed );
    m_nUsed += nBytes;

    return memset( pBuffer, 0, nBytes );
}

template<typename AllocatorType>
void CSlabAllocator<AllocatorType>::Reset( void )
{
    m_nUsed = 0;
}

template<typename AllocatorType>
void CSlabAllocator<AllocatorType>::Resize( uint64_t nBytes )
{
    if ( !m_pBase ) {
        assert( 0 );
        return;
    }

    uint8_t *newBuffer = (uint8_t *)SIRENGINE_MALLOC( nBytes );
    memcpy( newBuffer, m_pBase, m_nUsed ); // only copy what we have used so far
    SIRENGINE_FREE( m_pBase );

    m_pBase = newBuffer;
    m_nSize = nBytes;
}