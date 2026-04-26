#include "LinearAllocator.h"

using namespace SIREngine::Core::Memory::Allocators;

LinearAllocator::LinearAllocator( size_t nSize, size_t nAlignment )
	: m_nSize( SIRENGINE_PAD( nSize, nAlignment ) ), m_nUsed( 0 )
{
	m_pData = calloc( m_nSize, 1 );
}

LinearAllocator::~LinearAllocator()
{
	free( m_pData );
}

void *LinearAllocator::Alloc( size_t nSize, size_t nAlignment )
{
	const size_t nRealSize = SIRENGINE_PAD( nSize, nAlignment );
	void *pAllocated = (char *)m_pData + m_nUsed;
	memset( pAllocated, 0, nRealSize );
	m_nUsed += nRealSize;

	return pAllocated;
}

void LinearAllocator::Clear( void )
{
	memset( m_pData, 0, m_nUsed );
	m_nUsed = 0;
}