#ifndef __CCONST_LIST_H__
#define __CCONST_LIST_H__

#include <Engine/Core/SIREngine.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

template<typename T>
class CConstList
{
public:
    typedef T value_type;
    typedef const T& reference;
    typedef const T& const_reference;
    typedef const T* iterator;
    typedef const T* const_iterator;
    typedef size_t size_type;
private:
    const_iterator m_pArray;
    size_type m_nSize;

    CConstList( const_iterator pArray, size_type nSize )
        : m_pArray( pArray ), m_nSize( nSize )
    { }
public:
    CConstList( void )
        : m_pArray( NULL ), m_nSize( 0 )
    { }

    SIRENGINE_CONSTEXPR SIRENGINE_FORCEINLINE size_type size( void ) const
    { return m_nSize; }
    SIRENGINE_CONSTEXPR SIRENGINE_FORCEINLINE const_iterator begin( void ) const
    { return m_pArray; }
    SIRENGINE_CONSTEXPR SIRENGINE_FORCEINLINE const_iterator end( void ) const
    { return m_pArray + m_nSize; }
};

#endif