#include <Engine/Allocator/SlabAllocator.hpp>

CSlabAllocator::CSlabAllocator( void )
    : IAllocatorBase( "SlabAllocator" )
{
    m_nSize = 0;
    m_nUsed = 0;
}

CSlabAllocator::CSlabAllocator( uint64_t nBytes )
    : IAllocatorBase( "SlabAllocator" )
{
    Init( nBytes );
}

CSlabAllocator::~CSlabAllocator()
{
    Shutdown();
}

void CSlabAllocator::Init( uint64_t nBytes )
{
    // ensure we aren't leaking memory
    Shutdown();

    m_pBase = (uint8_t *)SIRENGINE_MALLOC( nBytes );
    if ( !m_pBase ) {

    }

    m_nSize = nBytes;
    m_nUsed = 0;
}

void CSlabAllocator::Shutdown( void )
{
    if ( m_pBase != NULL ) {
        SIRENGINE_FREE( m_pBase );
        m_nSize = 0;
        m_nUsed = 0;
        m_pBase = NULL;
    }
}

void *CSlabAllocator::Allocate( uint64_t nBytes )
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

void CSlabAllocator::Reset( void )
{
    m_nUsed = 0;
}

void CSlabAllocator::Resize( uint64_t nBytes )
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