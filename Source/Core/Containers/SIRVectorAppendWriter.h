/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#ifndef __SIRENGINE_SIRVECTORAPPENDWRITER_H__
#define __SIRENGINE_SIRVECTORAPPENDWRITER_H__

#include <Core/SIRPCH.h>

#include <Core/Containers/SIRVector.h>
#include <new>
#include <utility>

template <typename T>
class SIRVectorAppendWriter
{
private:
	SIRVector<T> *m_pVector = nullptr;
	uint32_t m_nBase = 0;
	uint32_t m_nReserved = 0;
	uint32_t m_nConstructed = 0;
	bool m_bActive = false;

public:
	explicit SIRVectorAppendWriter( SIRVector<T>& rVector ) noexcept : m_pVector( &rVector )
	{
	}
	~SIRVectorAppendWriter( void ) noexcept
	{
		rollback();
	}
	SIRVectorAppendWriter( const SIRVectorAppendWriter& ) = delete;
	SIRVectorAppendWriter& operator=( const SIRVectorAppendWriter& ) = delete;

	sirContainerResult_t tryBeginAppend( uint32_t nCount )
	{
		if ( m_bActive ) {
			return SIR_CONTAINER_BUSY;
		}
		m_nBase = m_pVector->m_nSize;
		if ( nCount > UINT32_MAX - m_nBase ) {
			return SIR_CONTAINER_SIZE_OVERFLOW;
		}
		const sirContainerResult_t eResult = m_pVector->tryReserve( m_nBase + nCount );
		if ( eResult != SIR_CONTAINER_OK ) {
			return eResult;
		}
		m_nReserved = nCount;
		m_nConstructed = 0;
		m_bActive = true;
		return SIR_CONTAINER_OK;
	}
	template <typename... Args>
	sirContainerResult_t tryEmplaceNext( Args&&...args )
	{
		if ( !m_bActive || m_nConstructed >= m_nReserved ) {
			return SIR_CONTAINER_CAPACITY_EXCEEDED;
		}
		new ( m_pVector->m_pData + m_nBase + m_nConstructed ) T( std::forward<Args>( args )... );
		++m_nConstructed;
		return SIR_CONTAINER_OK;
	}
	sirContainerResult_t commit( void ) noexcept
	{
		if ( !m_bActive || m_nConstructed != m_nReserved ) {
			return SIR_CONTAINER_BUSY;
		}
		m_pVector->m_nSize = m_nBase + m_nConstructed;
		m_bActive = false;
		return SIR_CONTAINER_OK;
	}
	void rollback( void ) noexcept
	{
		if ( m_bActive ) {
			SIRLifetimeOps<T>::destroy( m_pVector->m_pData + m_nBase, m_nConstructed );
			m_bActive = false;
			m_nConstructed = 0;
		}
	}
};

#endif // __SIRENGINE_SIRVECTORAPPENDWRITER_H__
