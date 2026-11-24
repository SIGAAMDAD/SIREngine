/* SIREngine MPLv2 - transient builder for frozen packed integer vectors. */
#ifndef __SIR_PACKEDINTEGERVECTORBUILDER_H__
#define __SIR_PACKEDINTEGERVECTORBUILDER_H__
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
	SIRContainerResult tryAppend( uint64_t nValue )
	{
		return m_Values.tryPushBack( nValue );
	}
	uint32_t getSize( void ) const noexcept
	{
		return m_Values.getSize();
	}
	SIRContainerResult tryBuild( uint8_t nBits, SIRPackedIntegerVector *pOutput )
	{
		if ( pOutput == nullptr ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		SIRContainerResult eResult = pOutput->tryInitialize( nBits, m_Values.getSize() );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		for ( uint32_t i = 0; i < m_Values.getSize(); ++i ) {
			eResult = pOutput->tryAppend( m_Values[i] );
			if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
				return eResult;
			}
		}
		return pOutput->seal();
	}
};
#endif // __SIR_PACKEDINTEGERVECTORBUILDER_H__
