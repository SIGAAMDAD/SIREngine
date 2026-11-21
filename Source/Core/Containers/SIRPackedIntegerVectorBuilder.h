/* SIREngine MPLv2 - transient builder for frozen packed integer vectors. */
#ifndef __SIRENGINE_SIRPACKEDINTEGERVECTORBUILDER_H__
#define __SIRENGINE_SIRPACKEDINTEGERVECTORBUILDER_H__
#include <Core/SIRPCH.h>
#include <Core/Containers/SIRPackedIntegerVector.h>

class SIRPackedIntegerVectorBuilder
{
private:
	SIRVector<uint64_t> m_Values;

public:
	explicit SIRPackedIntegerVectorBuilder( const sirContainerAllocator_t *pAllocator ) noexcept : m_Values( pAllocator )
	{
	}
	sirContainerResult_t tryAppend( uint64_t nValue )
	{
		return m_Values.tryPushBack( nValue );
	}
	uint32_t getSize( void ) const noexcept
	{
		return m_Values.getSize();
	}
	sirContainerResult_t tryBuild( uint8_t nBits, SIRPackedIntegerVector *pOutput )
	{
		if ( pOutput == nullptr ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		sirContainerResult_t eResult = pOutput->tryInitialize( nBits, m_Values.getSize() );
		if ( eResult != SIR_CONTAINER_OK ) {
			return eResult;
		}
		for ( uint32_t i = 0; i < m_Values.getSize(); ++i ) {
			eResult = pOutput->tryAppend( m_Values[i] );
			if ( eResult != SIR_CONTAINER_OK ) {
				return eResult;
			}
		}
		return pOutput->seal();
	}
};
#endif // __SIRENGINE_SIRPACKEDINTEGERVECTORBUILDER_H__
