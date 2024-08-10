#ifndef __CVECTOR_H__
#define __CVECTOR_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Util/UtlMemory.h>
#include <initializer_list>
#include <EASTL/vector.h>

//template<typename T, typename AllocatorType = UtlMallocAllocator>
//using CVector = eastl::vector<T, AllocatorType>;

template<typename T, typename AllocatorType = UtlMallocAllocator>
class CVector : public eastl::vector<T, AllocatorType>
{
public:
    typedef eastl::vector<T, AllocatorType> base_type;
    /*
public:
    typedef CVector<T, AllocatorType> this_type;
    typedef T value_type;
    typedef value_type& reference;
    typedef const value_type& const_reference;
    typedef value_type* pointer;
    typedef const value_type* const_pointer;
    typedef pointer iterator;
    typedef const_pointer const_iterator;
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
public:
    constexpr CVector( void );
    CVector( size_type nItems );
    CVector( const this_type& other );
    CVector( this_type&& other );
    CVector( const std::initializer_list<T>& data ) = default;
    CVector( const eastl::vector<T, AllocatorType>& other );
    ~CVector();

    void clear( void );
    void reserve( size_type nItems );
    void resize( size_type nItems );

    reference operator[]( size_type nIndex );
    const_reference operator[]( size_type nIndex ) const;
    reference at( size_type nIndex );
    const_reference at( size_type nIndex ) const;

    size_type size( void ) const;
    size_type allocated( void ) const;
    size_type usedBytes( void ) const;

    T *data( void );
    const T *data( void ) const;

    iterator find( const value_type& value );
    const_iterator find( const value_type& value ) const;

    iterator begin( void );
    const_iterator begin( void ) const;
    const_iterator cbegin( void ) const;

    iterator end( void );
    const_iterator end( void ) const;
    const_iterator cend( void ) const;

    iterator rbegin( void );
    const_iterator rbegin( void ) const;
    const_iterator rcbegin( void ) const;

    iterator rend( void );
    const_iterator rend( void ) const;
    const_iterator rcend( void ) const;
private:
    T *m_pBuffer;
    size_type m_nAllocated;
    size_type m_nSize;
    AllocatorType m_Allocator;

    void EnsureAllocated( size_type nItems );
    */
};

#endif