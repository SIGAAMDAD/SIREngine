/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#ifndef __SIRENGINE_SIRWRITEONLYVECTORVIEW_H__
#define __SIRENGINE_SIRWRITEONLYVECTORVIEW_H__

#include <Core/SIRPCH.h>

#include <Core/Containers/SIRContainerResult.h>
#include <stdint.h>
#include <type_traits>
#include <utility>

template <typename T>
class SIRWriteOnlyVectorView
{
private:
	T *m_pData = nullptr;
	uint32_t m_nSize = 0;

public:
	SIRWriteOnlyVectorView( void ) noexcept = default;
	SIRWriteOnlyVectorView( T *pData, uint32_t nSize ) noexcept : m_pData( pData ), m_nSize( pData != nullptr ? nSize : 0 )
	{
	}
	uint32_t getSize( void ) const noexcept
	{
		return m_nSize;
	}
	sirContainerResult_t tryWrite( uint32_t iIndex, const T& rValue ) noexcept
	{
		static_assert( std::is_nothrow_copy_assignable<T>::value, "Write view requires nothrow copy assignment" );
		if ( iIndex >= m_nSize ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		m_pData[iIndex] = rValue;
		return SIR_CONTAINER_OK;
	}
	sirContainerResult_t tryWriteRange( uint32_t iOffset, const T *pValues, uint32_t nCount ) noexcept
	{
		static_assert( std::is_nothrow_copy_assignable<T>::value, "Write view requires nothrow copy assignment" );
		if ( iOffset > m_nSize || nCount > m_nSize - iOffset || ( nCount != 0 && pValues == nullptr ) ) {
			return SIR_CONTAINER_INVALID_ARGUMENT;
		}
		for ( uint32_t i = 0; i < nCount; ++i ) {
			m_pData[iOffset + i] = pValues[i];
		}
		return SIR_CONTAINER_OK;
	}
};

#endif // __SIRENGINE_SIRWRITEONLYVECTORVIEW_H__
