/* SIREngine MPLv2 - transactional constructed-tail append for inline vectors. */
#ifndef __SIR_INLINEVECTORAPPENDWRITER_H__
#define __SIR_INLINEVECTORAPPENDWRITER_H__
#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on
#include <Core/Containers/SIRInlineVector.h>
#include <new>
#include <utility>

template <typename T, uint32_t N>
class SIRInlineVectorAppendWriter
{
public:
	explicit SIRInlineVectorAppendWriter( SIRInlineVector<T, N>& rVector ) noexcept
	    : m_pVector( &rVector )
	{
	}

	~SIRInlineVectorAppendWriter() noexcept
	{
		rollback();
	}

	SIRInlineVectorAppendWriter( const SIRInlineVectorAppendWriter& ) = delete;
	SIRInlineVectorAppendWriter& operator=( const SIRInlineVectorAppendWriter& ) = delete;

	SIRContainerResult tryBeginAppend( uint32_t nCount )
	{
		if ( m_bActive ) {
			return SIRContainerResult::SIR_CONTAINER_BUSY;
		}
		m_nBase = m_pVector->m_nSize;
		if ( nCount > UINT32_MAX - m_nBase ) {
			return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
		}
		const SIRContainerResult eResult = m_pVector->tryReserve( m_nBase + nCount );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		m_nReserved = nCount;
		m_nConstructed = 0;
		m_bActive = true;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	template <typename... Args>
	SIRContainerResult tryEmplaceNext( Args&&...args )
	{
		if ( !m_bActive || m_nConstructed >= m_nReserved ) {
			return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
		}
		new ( m_pVector->m_pData + m_nBase + m_nConstructed ) T( std::forward<Args>( args )... );
		++m_nConstructed;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	SIRContainerResult commit( void ) noexcept
	{
		if ( !m_bActive || m_nConstructed != m_nReserved ) {
			return SIRContainerResult::SIR_CONTAINER_BUSY;
		}
		m_pVector->m_nSize = m_nBase + m_nConstructed;
		m_bActive = false;
		return SIRContainerResult::SIR_CONTAINER_OK;
	}

	void rollback( void ) noexcept
	{
		if ( m_bActive ) {
			SIRLifetimeOps<T>::destroy( m_pVector->m_pData + m_nBase, m_nConstructed );
			m_bActive = false;
			m_nConstructed = 0;
		}
	}

private:
	SIRInlineVector<T, N> *m_pVector = nullptr;
	uint32_t m_nBase = 0;
	uint32_t m_nReserved = 0;
	uint32_t m_nConstructed = 0;
	bool m_bActive = false;
};

#endif // __SIR_INLINEVECTORAPPENDWRITER_H__
