#ifndef __CREVERSE_ITERATOR_H__
#define __CREVERSE_ITERATOR_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Core/SIREngine.h>

template<typename T>
class CReverseIterator
{
public:
    explicit CReverseIterator( T *pBase )
        : m_pIterator( pBase )
    { }
    ~CReverseIterator()
    { }

    T& operator*( void ) const;
    CReverseIterator& operator++( void );
    CReverseIterator& operator--( void );
    bool operator!=( const CReverseIterator& other ) const;
    bool operator==( const CReverseIterator& other ) const;
private:
    T *m_pIterator;
};

template<typename T>
SIRENGINE_FORCEINLINE T& CReverseIterator<T>::operator*( void ) const
{
    return *( m_pIterator - 1 );
}


template<typename T>
SIRENGINE_FORCEINLINE CReverseIterator<T>& CReverseIterator<T>::operator++( void )
{
    m_pIterator--;
    return *this;
}

template<typename T>
SIRENGINE_FORCEINLINE CReverseIterator<T>& CReverseIterator<T>::operator--( void )
{
    --m_pIterator;
    return *this;
}

template<typename T>
SIRENGINE_FORCEINLINE bool CReverseIterator<T>::operator!=( const CReverseIterator& other ) const
{
    return *m_pIterator != *other.m_pIterator;
}

template<typename T>
SIRENGINE_FORCEINLINE bool CReverseIterator<T>::operator==( const CReverseIterator& other ) const
{
    return *m_pIterator == *other.m_pIterator;
}

#endif