/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIR_SPAN_H__
#define __SIR_SPAN_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <type_traits>

template <typename T>
class SIRSpan
{
public:
	constexpr SIRSpan( void ) noexcept = default;
	constexpr SIRSpan( const SIRSpan& ) noexcept = default;
	constexpr SIRSpan( SIRSpan&& ) noexcept = default;

	constexpr explicit SIRSpan( T *pData, size_t nSize ) noexcept;

	template <typename U, typename = std::enable_if_t<std::is_convertible_v<U ( * )[], T ( * )[]>>>
	SIR_FORCEINLINE constexpr SIRSpan( const SIRSpan<U>& span ) noexcept
	    : m_pData( span.m_pData ),
	      m_nSize( span.m_nSize )
	{
	}

	~SIRSpan() noexcept = default;

	SIRSpan& operator=( const SIRSpan& ) noexcept = default;
	SIRSpan& operator=( SIRSpan&& ) noexcept = default;

	constexpr T& operator[]( size_t nIndex ) const noexcept;
	constexpr T *getData( void ) const noexcept;

	constexpr size_t getSize( void ) const noexcept;
	constexpr size_t getSizeBytes( void ) const noexcept;
	constexpr bool isEmpty( void ) const noexcept;

	constexpr SIRSpan subSpan( size_t nOffset, size_t nCount ) const noexcept;

private:
	T *m_pData;
	size_t m_nSize;
};

template <typename T>
SIR_FORCEINLINE constexpr SIRSpan<T>::SIRSpan( T *pData, size_t nSize ) noexcept
    : m_pData( pData ),
      m_nSize( nSize )
{
}

template <typename T>
SIR_FORCEINLINE constexpr T& SIRSpan<T>::operator[]( size_t nIndex ) const noexcept
{
	return m_pData[nIndex];
}

template <typename T>
SIR_FORCEINLINE constexpr T *SIRSpan<T>::getData( void ) const noexcept
{
	return m_pData;
}

template <typename T>
SIR_FORCEINLINE constexpr size_t SIRSpan<T>::getSize( void ) const noexcept
{
	return m_nSize;
}

template <typename T>
SIR_FORCEINLINE constexpr bool SIRSpan<T>::isEmpty( void ) const noexcept
{
	return m_nSize == 0;
}

template <typename T>
constexpr SIRSpan<T> SIRSpan<T>::subSpan( size_t nOffset, size_t nCount ) const noexcept
{
	if ( nOffset >= m_nSize ) {
		return SIRSpan<T>();
	}

	const size_t nRemaining = m_nSize - nOffset;
	const size_t nNewSize = nCount > nRemaining ? nRemaining : nCount;

	return SIRSpan<T>( m_pData + nOffset, nNewSize );
}

#endif // __SIR_SPAN_H__
