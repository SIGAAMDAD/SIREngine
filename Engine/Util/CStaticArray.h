#ifndef __CSTATICARRAY_H__
#define __CSTATICARRAY_H__

#pragma once

#include <Engine/Core/SIREngine.h>
#include "CReverseIterator.h"

template<typename T, uint64_t nItems, uint64_t nAlignment = alignof( T )>
class alignas( nAlignment ) CStaticArray
{
public:
    CStaticArray( void );
    CStaticArray( const std::initializer_list<T>& data )
    {
        size_t i;
        const auto it = data.begin();

        for ( i = 0; i < data.size(); i++, it++ ) {
            m_Storage.szElements[ i ] = it;
        }
    }
    CStaticArray( const CStaticArray& other ) = default;
    CStaticArray( CStaticArray&& other ) = default;
    ~CStaticArray();

    friend bool operator==( const CStaticArray& a, const CStaticArray& b );

    const CStaticArray& operator=( const CStaticArray& other ) = default;
    const CStaticArray& operator=( CStaticArray&& other ) = default;

    bool operator!=( const CStaticArray& other ) const;

    bool IsEmpty( void ) const;

    T& operator[]( uint64_t nIndex );
    const T& operator[]( uint64_t nIndex ) const;

    T& At( uint64_t nIndex );
    const T& At( uint64_t nIndex ) const;

    T *GetBuffer( void );
    const T *GetBuffer( void ) const;

    uint64_t Size( void ) const;

    T *begin( void );
    T *end( void );
    const T *begin( void ) const;
    const T *end( void ) const;

    CReverseIterator<T>& rbegin( void );
    CReverseIterator<T>& rend( void );
    CReverseIterator<const T>& rbegin( void ) const;
    CReverseIterator<const T>& rend( void ) const;
private:
    typedef struct alignas( nAlignment ) ArrayStorageElementAligned {
        ArrayStorageElementAligned( void )
        { }

        template<typename... Args>
        explicit ArrayStorageElementAligned( T, uint64_t, Args&&... args )
            : data( std::forward<Args>( args )... )
        { }

        T data;
    } ArrayStorageElementAligned_t;

    typedef struct ArrayStorage {
        ArrayStorage()
            : szElements()
        { }

        ArrayStorageElementAligned_t szElements[ nItems ];
    } ArrayStorage_t;

    ArrayStorage_t m_Storage;
};

template<typename T, uint64_t nItems, uint64_t nAlignment>
SIRENGINE_FORCEINLINE bool CStaticArray<T, nItems, nAlignment>::operator!=( const CStaticArray& other ) const
{
    for ( uint64_t i = 0; i < nItems; i++ ) {
        if ( !( (*this)[ i ] == other[ i ] ) ) {
            return true;
        }
    }
    return false;
}

template<typename T, uint64_t nItems, uint64_t nAlignment>
SIRENGINE_FORCEINLINE bool CStaticArray<T, nItems, nAlignment>::IsEmpty( void ) const
{
    return nItems == 0;
}

template<typename T, uint64_t nItems, uint64_t nAlignment>
SIRENGINE_FORCEINLINE T& CStaticArray<T, nItems, nAlignment>::operator[]( uint64_t nIndex )
{
    return m_Storage.szElements[ nIndex ];
}

template<typename T, uint64_t nItems, uint64_t nAlignment>
SIRENGINE_FORCEINLINE const T& CStaticArray<T, nItems, nAlignment>::operator[]( uint64_t nIndex ) const
{
    return m_Storage.szElements[ nIndex ];
}

template<typename T, uint64_t nItems, uint64_t nAlignment>
SIRENGINE_FORCEINLINE T& CStaticArray<T, nItems, nAlignment>::At( uint64_t nIndex )
{
    return m_Storage.szElements[ nIndex ];
}

template<typename T, uint64_t nItems, uint64_t nAlignment>
SIRENGINE_FORCEINLINE const T& CStaticArray<T, nItems, nAlignment>::At( uint64_t nIndex ) const
{
    return m_Storage.szElements[ nIndex ];
}

template<typename T, uint64_t nItems, uint64_t nAlignment>
SIRENGINE_FORCEINLINE T *CStaticArray<T, nItems, nAlignment>::GetBuffer( void )
{
    return m_Storage.szElements;
}

template<typename T, uint64_t nItems, uint64_t nAlignment>
SIRENGINE_FORCEINLINE const T *CStaticArray<T, nItems, nAlignment>::GetBuffer( void ) const
{
    return m_Storage.szElements;
}

template<typename T, uint64_t nItems, uint64_t nAlignment>
SIRENGINE_FORCEINLINE uint64_t CStaticArray<T, nItems, nAlignment>::Size( void ) const
{
    return nItems;
}

template<typename T, uint64_t nItems, uint64_t nAlignment>
SIRENGINE_FORCEINLINE T *CStaticArray<T, nItems, nAlignment>::begin( void )
{
    return m_Storage.szElements;
}

template<typename T, uint64_t nItems, uint64_t nAlignment>
SIRENGINE_FORCEINLINE T *CStaticArray<T, nItems, nAlignment>::end( void )
{
    return m_Storage.szElements + nItems;
}

template<typename T, uint64_t nItems, uint64_t nAlignment>
SIRENGINE_FORCEINLINE const T *CStaticArray<T, nItems, nAlignment>::begin( void ) const
{
    return begin();
}

template<typename T, uint64_t nItems, uint64_t nAlignment>
SIRENGINE_FORCEINLINE const T *CStaticArray<T, nItems, nAlignment>::end( void ) const
{
    return end();
}

template<typename T, uint64_t nItems, uint64_t nAlignment>
SIRENGINE_FORCEINLINE CReverseIterator<T>& CStaticArray<T, nItems, nAlignment>::rbegin( void )
{
    return CReverseIterator<T>( end() );
}

template<typename T, uint64_t nItems, uint64_t nAlignment>
SIRENGINE_FORCEINLINE CReverseIterator<T>& CStaticArray<T, nItems, nAlignment>::rend( void )
{
    return CReverseIterator<T>( begin() );
}

template<typename T, uint64_t nItems, uint64_t nAlignment>
SIRENGINE_FORCEINLINE CReverseIterator<const T>& CStaticArray<T, nItems, nAlignment>::rbegin( void ) const
{
    return CReverseIterator<T>( end() );
}

template<typename T, uint64_t nItems, uint64_t nAlignment>
SIRENGINE_FORCEINLINE CReverseIterator<const T>& CStaticArray<T, nItems, nAlignment>::rend( void ) const
{
    return CReverseIterator<T>( begin() );
}

template<typename T, uint64_t nItems, uint64_t nAlignment>
SIRENGINE_FORCEINLINE bool operator==( const CStaticArray<T, nItems, nAlignment>& a, const CStaticArray<T, nItems, nAlignment>& b )
{
    for ( uint64_t i = 0; i < nItems; ++i ) {
        if ( !( a[ i ] == b[ i ] ) ) {
            return false;
        }
    }
    return true;
}

#endif