/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#ifndef __SIR_READONLYVECTORVIEW_H__
#define __SIR_READONLYVECTORVIEW_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Containers/SIRContainerResult.h>
#include <assert.h>
#include <stdint.h>

template <typename T>
class SIRReadOnlyVectorView
{
public:
	explicit SIRReadOnlyVectorView( void ) noexcept = default;

	SIRReadOnlyVectorView( const T *pData, uint32_t nSize ) noexcept
	    : m_pData( pData ),
	      m_nSize( pData != nullptr ? nSize : 0 )
	{
		assert( pData != nullptr || nSize == 0 );
	}

	uint32_t getSize( void ) const noexcept
	{
		return m_nSize;
	}

	bool isEmpty( void ) const noexcept
	{
		return m_nSize == 0;
	}

	const T *data( void ) const noexcept
	{
		return m_pData;
	}

	const T *begin( void ) const noexcept
	{
		return m_pData;
	}

	const T *end( void ) const noexcept
	{
		return m_pData != nullptr ? m_pData + m_nSize : nullptr;
	}

	const T *tryGet( uint32_t iIndex ) const noexcept
	{
		return iIndex < m_nSize ? m_pData + iIndex : nullptr;
	}

	const T& operator[]( uint32_t iIndex ) const noexcept
	{
		assert( iIndex < m_nSize );
		return m_pData[iIndex];
	}

	SIRReadOnlyVectorView subview( uint32_t nOffset, uint32_t nCount ) const noexcept
	{
		if ( nOffset > m_nSize || nCount > m_nSize - nOffset ) {
			return {};
		}
		return { m_pData == nullptr ? nullptr : m_pData + nOffset, nCount };
	}

private:
	const T *m_pData = nullptr;
	uint32_t m_nSize = 0;
};

#endif // __SIR_READONLYVECTORVIEW_H__
