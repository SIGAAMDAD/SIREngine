#ifndef __CSTATICARRAY_H__
#define __CSTATICARRAY_H__

#pragma once

#include <Engine/Core/SIREngine.h>
#include "CReverseIterator.h"

template<typename T, size_t N, uint64_t nAlignment = alignof( T )>
class alignas( nAlignment ) CStaticArray
{
public:
    typedef CStaticArray<T, N, nAlignment> this_type;
    typedef T value_type;
    typedef value_type& reference;
    typedef const value_type& const_reference;
    typedef value_type* iterator;
    typedef const value_type* const_iterator;
    typedef CReverseIterator<value_type> reverse_iterator;
    typedef CReverseIterator<const value_type> const_reverse_iterator;
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
public:
    void fill( const value_type& value );
    void swap( this_type& other );

    SIRENGINE_CONSTEXPR iterator begin( void );
    SIRENGINE_CONSTEXPR const_iterator begin( void ) const;
    SIRENGINE_CONSTEXPR const_iterator cbegin( void ) const;

    SIRENGINE_CONSTEXPR iterator end( void );
    SIRENGINE_CONSTEXPR const_iterator end( void ) const;
    SIRENGINE_CONSTEXPR const_iterator cend( void ) const;

    SIRENGINE_CONSTEXPR reverse_iterator rbegin( void );
    SIRENGINE_CONSTEXPR const_reverse_iterator rbegin( void ) const;
    SIRENGINE_CONSTEXPR const_reverse_iterator crbegin( void ) const;

    SIRENGINE_CONSTEXPR reverse_iterator rend( void );
    SIRENGINE_CONSTEXPR const_reverse_iterator rend( void ) const;
    SIRENGINE_CONSTEXPR const_reverse_iterator crend( void ) const;

    SIRENGINE_CONSTEXPR bool empty( void ) const;
    SIRENGINE_CONSTEXPR size_type size( void ) const;
    
    SIRENGINE_CONSTEXPR T *data( void );
    SIRENGINE_CONSTEXPR const T *data( void ) const;

    SIRENGINE_CONSTEXPR reference operator[]( size_type nIndex );
    SIRENGINE_CONSTEXPR const_reference operator[]( size_type nIndex ) const;
    SIRENGINE_CONSTEXPR reference at( size_type nIndex );
    SIRENGINE_CONSTEXPR const_reference at( size_type nIndex ) const;

    SIRENGINE_CONSTEXPR reference front( void );
    SIRENGINE_CONSTEXPR const_reference front( void ) const;

    SIRENGINE_CONSTEXPR reference back( void );
    SIRENGINE_CONSTEXPR const_reference back( void ) const;
public:
    // intentionally public so that we can use
    // aggregate initialization
    value_type m_Data[ N ? N : 1 ];
};

template<typename T, size_t N, uint64_t nAlignment>
inline void CStaticArray<T, N, nAlignment>::fill( const value_type& value )
{
    eastl::fill_n( &m_Data[0], N, value );
}

template<typename T, size_t N, uint64_t nAlignment>
inline void CStaticArray<T, N, nAlignment>::swap( this_type& other )
{
    eastl::swap_ranges( &m_Data[0], &m_Data[N], &other.m_Data[0] );
}

template<typename T, size_t N, uint64_t nAlignment>
SIRENGINE_CONSTEXPR inline typename CStaticArray<T, N, nAlignment>::iterator
CStaticArray<T, N, nAlignment>::begin( void )
{
    return &m_Data[0];
}

template<typename T, size_t N, uint64_t nAlignment>
SIRENGINE_CONSTEXPR inline typename CStaticArray<T, N, nAlignment>::const_iterator
CStaticArray<T, N, nAlignment>::begin( void ) const
{
    return &m_Data[0];
}

template<typename T, size_t N, uint64_t nAlignment>
SIRENGINE_CONSTEXPR inline typename CStaticArray<T, N, nAlignment>::const_iterator
CStaticArray<T, N, nAlignment>::cbegin( void ) const
{
    return &m_Data[0];
}

template<typename T, size_t N, uint64_t nAlignment>
SIRENGINE_CONSTEXPR inline typename CStaticArray<T, N, nAlignment>::iterator
CStaticArray<T, N, nAlignment>::end( void )
{
    return &m_Data[N];
}

template<typename T, size_t N, uint64_t nAlignment>
SIRENGINE_CONSTEXPR inline typename CStaticArray<T, N, nAlignment>::const_iterator
CStaticArray<T, N, nAlignment>::end( void ) const
{
    return &m_Data[N];
}

template<typename T, size_t N, uint64_t nAlignment>
SIRENGINE_CONSTEXPR inline typename CStaticArray<T, N, nAlignment>::const_iterator
CStaticArray<T, N, nAlignment>::cend( void ) const
{
    return &m_Data[N];
}

template<typename T, size_t N, uint64_t nAlignment>
SIRENGINE_CONSTEXPR inline typename CStaticArray<T, N, nAlignment>::reverse_iterator
CStaticArray<T, N, nAlignment>::rbegin( void )
{
    return reverse_iterator( &m_Data[N] );
}

template<typename T, size_t N, uint64_t nAlignment>
SIRENGINE_CONSTEXPR inline typename CStaticArray<T, N, nAlignment>::const_reverse_iterator
CStaticArray<T, N, nAlignment>::rbegin( void ) const
{
    return const_reverse_iterator( &m_Data[N] );
}

template<typename T, size_t N, uint64_t nAlignment>
SIRENGINE_CONSTEXPR inline typename CStaticArray<T, N, nAlignment>::const_reverse_iterator
CStaticArray<T, N, nAlignment>::crbegin( void ) const
{
    return const_reverse_iterator( &m_Data[N] );
}

template<typename T, size_t N, uint64_t nAlignment>
SIRENGINE_CONSTEXPR inline typename CStaticArray<T, N, nAlignment>::reverse_iterator
CStaticArray<T, N, nAlignment>::rend( void )
{
    return reverse_iterator( &m_Data[0] );
}

template<typename T, size_t N, uint64_t nAlignment>
SIRENGINE_CONSTEXPR inline typename CStaticArray<T, N, nAlignment>::const_reverse_iterator
CStaticArray<T, N, nAlignment>::rend( void ) const
{
    return const_reverse_iterator( &m_Data[0] );
}

template<typename T, size_t N, uint64_t nAlignment>
SIRENGINE_CONSTEXPR inline typename CStaticArray<T, N, nAlignment>::const_reverse_iterator
CStaticArray<T, N, nAlignment>::crend( void ) const
{
    return const_reverse_iterator( &m_Data[0] );
}

template<typename T, size_t N, uint64_t nAlignment>
SIRENGINE_CONSTEXPR inline typename CStaticArray<T, N, nAlignment>::size_type
CStaticArray<T, N, nAlignment>::size( void ) const
{
    return (size_type)N;
}

template<typename T, size_t N, uint64_t nAlignment>
SIRENGINE_CONSTEXPR inline bool CStaticArray<T, N, nAlignment>::empty( void ) const
{
    return ( N == 0 );
}

template<typename T, size_t N, uint64_t nAlignment>
SIRENGINE_CONSTEXPR inline typename CStaticArray<T, N, nAlignment>::reference
CStaticArray<T, N, nAlignment>::operator[]( size_type nIndex )
{
    return m_Data[ nIndex ];
}

template<typename T, size_t N, uint64_t nAlignment>
SIRENGINE_CONSTEXPR inline typename CStaticArray<T, N, nAlignment>::const_reference
CStaticArray<T, N, nAlignment>::operator[]( size_type nIndex ) const
{
    return m_Data[ nIndex ];
}

template<typename T, size_t N, uint64_t nAlignment>
SIRENGINE_CONSTEXPR inline typename CStaticArray<T, N, nAlignment>::reference
CStaticArray<T, N, nAlignment>::at( size_type nIndex )
{
    return m_Data[ nIndex ];
}

template<typename T, size_t N, uint64_t nAlignment>
SIRENGINE_CONSTEXPR inline typename CStaticArray<T, N, nAlignment>::const_reference
CStaticArray<T, N, nAlignment>::at( size_type nIndex ) const
{
    return m_Data[ nIndex ];
}

template<typename T, size_t N, uint64_t nAlignment>
SIRENGINE_CONSTEXPR inline typename CStaticArray<T, N, nAlignment>::reference
CStaticArray<T, N, nAlignment>::front( void )
{
    return m_Data[ 0 ];
}

template<typename T, size_t N, uint64_t nAlignment>
SIRENGINE_CONSTEXPR inline typename CStaticArray<T, N, nAlignment>::const_reference
CStaticArray<T, N, nAlignment>::front( void ) const
{
    return m_Data[ 0 ];
}

template<typename T, size_t N, uint64_t nAlignment>
SIRENGINE_CONSTEXPR inline typename CStaticArray<T, N, nAlignment>::reference
CStaticArray<T, N, nAlignment>::back( void )
{
    return m_Data[ N - 1 ];
}

template<typename T, size_t N, uint64_t nAlignment>
SIRENGINE_CONSTEXPR inline typename CStaticArray<T, N, nAlignment>::const_reference
CStaticArray<T, N, nAlignment>::back( void ) const
{
    return m_Data[ N - 1 ];
}

template<typename T, size_t N, uint64_t nAlignment>
SIRENGINE_CONSTEXPR inline T *CStaticArray<T, N, nAlignment>::data( void )
{
    return m_Data;
}

template<typename T, size_t N, uint64_t nAlignment>
SIRENGINE_CONSTEXPR inline const T *CStaticArray<T, N, nAlignment>::data( void ) const
{
    return m_Data;
}

template<typename T, size_t N, uint64_t nAlignment>
SIRENGINE_CONSTEXPR inline bool operator==( const CStaticArray<T, N, nAlignment>& a, const CStaticArray<T, N, nAlignment>& b )
{
	return eastl::equal( &a.m_Data[0], &a.m_Data[N], &b.m_Data[0] );
}

template<typename T, size_t N, uint64_t nAlignment>
SIRENGINE_CONSTEXPR inline bool operator<( const CStaticArray<T, N, nAlignment>& a, const CStaticArray<T, N, nAlignment>& b )
{
	return eastl::lexicographical_compare( &a.m_Data[0], &a.m_Data[N], &b.m_Data[0], &b.m_Data[N] );
}

template<typename T, size_t N, uint64_t nAlignment>
SIRENGINE_CONSTEXPR inline bool operator!=( const CStaticArray<T, N, nAlignment>& a, const CStaticArray<T, N, nAlignment>& b )
{
	return !eastl::equal( &a.m_Data[0], &a.m_Data[N], &b.m_Data[0] );
}

template<typename T, size_t N, uint64_t nAlignment>
SIRENGINE_CONSTEXPR inline bool operator>( const CStaticArray<T, N, nAlignment>& a, const CStaticArray<T, N, nAlignment>& b )
{
	return eastl::lexicographical_compare( &b.m_Data[0], &b.m_Data[N], &a.m_Data[0], &a.m_Data[N] );
}

template<typename T, size_t N, uint64_t nAlignment>
SIRENGINE_CONSTEXPR inline bool operator<=( const CStaticArray<T, N, nAlignment>& a, const CStaticArray<T, N, nAlignment>& b )
{
	return !eastl::lexicographical_compare( &b.m_Data[0], &b.m_Data[N], &a.m_Data[0], &a.m_Data[N] );
}

template<typename T, size_t N, uint64_t nAlignment>
SIRENGINE_CONSTEXPR inline bool operator>=( const CStaticArray<T, N, nAlignment>& a, const CStaticArray<T, N, nAlignment>& b )
{
	return !eastl::lexicographical_compare( &a.m_Data[0], &a.m_Data[N], &b.m_Data[0], &b.m_Data[N] );
}

template<typename T, size_t N, uint64_t nAlignment>
inline void swap( CStaticArray<T, N, nAlignment>& a, CStaticArray<T, N, nAlignment>& b )
{
	eastl::swap_ranges( &a.m_Data[0], &a.m_Data[N], &b.m_Data[0] );
}

#endif