#ifndef __CVECTOR_H__
#define __CVECTOR_H__

#pragma once

#include <Engine/Core/SIREngine.h>
#include <Engine/Util/UtlMemory.h>
#include <initializer_list>

template<typename T, typename AllocatorType = UtlMemory<T>, typename IndexType = uint64_t>
class CVector
{
public:
    typedef CVector<T> this_type;
    typedef T value_type;
    typedef value_type& reference;
    typedef const value_type& const_reference;
    typedef value_type* pointer;
    typedef const value_type* const_pointer;
    typedef pointer iterator;
    typedef const_pointer const_iterator;
    typedef IndexType size_type;
    typedef ptrdiff_t difference_type;
public:
    constexpr CVector( void );
    CVector( IndexType nItems );
    CVector( const CVector<T, AllocatorType, IndexType>& other );
    CVector( CVector<T, AllocatorType, IndexType>&& other );
    CVector( const std::initializer_list<T>& data );
    ~CVector();

    const CVector<T, AllocatorType, IndexType>& operator=( const CVector<T, AllocatorType, IndexType>& other );
    const CVector<T, AllocatorType, IndexType>& operator+=( const CVector<T, AllocatorType, IndexType>& other );

    bool operator==( const CVector<T, AllocatorType, IndexType>& other ) const;

    bool IsEmpty( void ) const;

    void Push( const T& value );
    void Pop( void );

    void Emplace( void );
    void Add( const T& value );
    template<typename... Args>
    void Add( Args&&... args );

    void RemoveAt( IndexType nIndex );
    void Remove( IndexType nStart, IndexType nEnd = SIRENGINE_UINT64_MAX );
    void PopLast( void );
    void Clear( void );
    void Copy( const CVector<T, AllocatorType, IndexType>& other );
    void Move( CVector<T, AllocatorType, IndexType>&& other );
    void Compress( void );

    void Append( const CVector<T, AllocatorType, IndexType>& other );
    void Append( const T *data, uint64_t nItems );
    void Append( const T *begin, const T *end );
    void Append( const std::initializer_list<T>& other );

    void Insert( const T *position, const *T begin, const T *end );

    CVector<T, AllocatorType, IndexType> GetSubArray( IndexType nStart, IndexType nEnd = SIRENGINE_UINT64_MAX );

    void Reserve( IndexType nItems );
    void Resize( IndexType nItems );

    T& At( void );
    const T& At( void ) const;
    T& operator[]( uint64_t nIndex );
    const T& operator[]( uint64_t nIndex ) const;

    T *ReleaseOwnership( uint64_t& nSize, uint64_t& nAllocated );

    IndexType Size( void ) const;
    IndexType Allocated( void ) const;

    T *GetBuffer( void );
    const T *GetBuffer( void ) const;

    T& First( void );
    const T& First( void ) const;
    T& Last( void );
    const T& Last( void ) const;

    iterator Find( const T& value );
    const_iterator Find( const T& value ) const;

    iterator begin( void );
    iterator end( void );
    const_iterator begin( void ) const;
    const_iterator end( void ) const;
    const_iterator cbegin( void ) const;
    const_iterator cend( void ) const;
private:
    T *m_pBuffer;
    IndexType m_nAllocated;
    IndexType m_nSize;
    AllocatorType m_Allocator;

    void EnsureAllocated( IndexType nItems );
};

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE constexpr CVector<T, AllocatorType, IndexType>::CVector( void )
    : m_pBuffer( NULL ), m_nAllocated( 0 ), m_nSize( 0 )
{
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE CVector<T, AllocatorType, IndexType>::CVector( IndexType nItems )
    : m_pBuffer( NULL ), m_nAllocated( 0 ), m_nSize( 0 )
{
    EnsureAllocated( nItems );
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE CVector<T, AllocatorType, IndexType>::CVector( const CVector& other )
{
    Copy( other );
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE CVector<T, AllocatorType, IndexType>::CVector( CVector&& other )
{
    Move( other );
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE CVector<T, AllocatorType, IndexType>::CVector( const std::initializer_list<T>& data )
{
    IndexType i;

    Resize( data.size() );

    const auto it = data.begin();
    for ( i = 0; i < data.size(); i++ ) {
        m_pBuffer[ i ] = it;
    }
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE CVector<T, AllocatorType, IndexType>::~CVector()
{
    Clear();
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE const CVector<T, AllocatorType, IndexType>& CVector<T, AllocatorType, IndexType>::operator=( const CVector<T, AllocatorType, IndexType>& other )
{
    Copy( other );
    return *this;
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE const CVector<T, AllocatorType, IndexType>& CVector<T, AllocatorType, IndexType>::operator+=( const CVector<T, AllocatorType, IndexType>& other )
{
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE bool CVector<T, AllocatorType, IndexType>::operator==( const CVector<T, AllocatorType, IndexType>& other ) const
{
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE bool CVector<T, AllocatorType, IndexType>::IsEmpty( void )
{
    return m_nLength == 0;
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE void CVector<T, AllocatorType, IndexType>::Push( const T& value )
{
    Add( value );
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE void CVector<T, AllocatorType, IndexType>::Pop( void )
{
    PopLast();
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE void CVector<T, AllocatorType, IndexType>::Emplace( void )
{
    Add();
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE void CVector<T, AllocatorType, IndexType>::Add( const T& value )
{
    T *pElement;

    EnsureAllocated( 1 );

    m_nSize++;
    pElement = &m_pBuffer[ m_nSize ];
    if constexpr ( std::is_constructible<T>() ) {
        ::new ( pElement ) T( value );
    } else {
        *pElement = value;
    }
}

template<typename T, typename AllocatorType, typename IndexType>
template<typename... Args>
SIRENGINE_FORCEINLINE void CVector<T, AllocatorType, IndexType>::Add( Args&&... args )
{
    T *pElement;

    EnsureAllocated( 1 );

    m_nSize++;
    pElement = &m_pBuffer[ m_nSize ];
    ::new ( pElement ) T( std::forward<Args>( args )... );
}

template<typename T, typename AllocatorType, typename IndexType>
inline void CVector<T, AllocatorType, IndexType>::RemoveAt( IndexType nIndex )
{
    if ( nIndex >= m_nSize || nIndex < 0 ) {
        assert( false );
        return;
    }

    if ( nIndex == m_nSize - 1 ) {
        PopLast();
        return;
    }

    if constexpr ( std::is_destructible<T>() ) {
        m_pBuffer[ nIndex ].~T();
    }
    // shuffle backwards
    memmove( m_pBuffer + nIndex, m_pBuffer + ( nIndex + 1 ), m_nSize - nIndex * sizeof( T ) );
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE void CVector<T, AllocatorType, IndexType>::Remove( IndexType nStart, IndexType nEnd = SIRENGINE_UINT64_MAX )
{
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE void CVector<T, AllocatorType, IndexType>::Clear( void )
{
    if ( m_pBuffer != NULL ) {
        m_Allocator.deallocate( m_pBuffer );
        m_nSize = 0;
        m_nAllocated = 0;
        m_pBuffer = NULL;
    }
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE void CVector<T, AllocatorType, IndexType>::Move( CVector<T, AllocatorType, IndexType>&& other )
{
    m_pBuffer = other.m_pBuffer;
    m_nSize = other.m_nSize;
    m_nAllocated = other.m_nAllocated;

    other.m_pBuffer = NULL;
    other.m_nSize = 0;
    other.m_nAllocated = 0;
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE void CVector<T, AllocatorType, IndexType>::Copy( const CVector<T, AllocatorType, IndexType>& other )
{
    Clear();

    m_pBuffer = (T *)m_Allocator.allocate( other.m_nSize );
    m_nSize = other.m_nSize;
    m_nAllocated = other.m_nAllocated;

    memcpy( m_pBuffer, other.m_pBuffer, m_nSize * sizeof( T ) );
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE void CVector<T, AllocatorType, IndexType>::PopLast( void )
{
    if ( m_nSize == 0 ) {
        assert( m_nSize > 0 );
        return;
    }
    if constexpr ( std::is_destructible<T>() ) {
        m_pBuffer[ m_nSize ].~T();
    }
    m_nSize--;
}

template<typename T, typename AllocatorType, typename IndexType>
inline void CVector<T, AllocatorType, IndexType>::Reserve( IndexType nItems )
{
    EnsureAllocated( nItems );
}

template<typename T, typename AllocatorType, typename IndexType>
inline void CVector<T, AllocatorType, IndexType>::Resize( IndexType nItems )
{
    void *newBuffer;

    if ( m_nSize == nItems && m_nAllocated == nItems ) {
        return;
    }

    m_nAllocated = nItems;
    newBuffer = m_Allocator.allocate( sizeof( T ) * m_nAllocated );
    if ( m_pBuffer ) {
        memcpy( newBuffer, m_pBuffer, m_nSize );
        m_Allocator.deallocate( m_pBuffer );
    }
    m_pBuffer = (T *)newBuffer;
    m_nSize = nItems;
}

template<typename T, typename AllocatorType, typename IndexType>
inline void CVector<T, AllocatorType, IndexType>::Compress( void )
{
    if ( m_nAllocated == 0 || m_nSize == 0 ) {
        return;
    }
    if ( m_nAllocated > m_nSize ) {
        m_nAllocated = m_nSize;

        void *newBuffer = m_Allocator.allocate( sizeof( T ) * m_nAllocated );
        memcpy( newBuffer, m_pBuffer, m_nSize );
        m_Allocator.deallocate( m_pBuffer );
        m_pBuffer = newBuffer;
    }
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE typename CVector<T, AllocatorType, IndexType>::size_type CVector<T, AllocatorType, IndexType>::Size( void ) const
{
    return m_nSize;
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE typename CVector<T, AllocatorType, IndexType>::size_type CVector<T, AllocatorType, IndexType>::Allocated( void ) const
{
    return m_nAllocated;
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE T& CVector<T, AllocatorType, IndexType>::At( void )
{
    assert( nIndex < m_nSize );
    return m_pBuffer[ nIndex ];
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE const T& CVector<T, AllocatorType, IndexType>::At( void ) const
{
    assert( nIndex < m_nSize );
    return m_pBuffer[ nIndex ];
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE T& CVector<T, AllocatorType, IndexType>::operator[]( uint64_t nIndex )
{
    assert( nIndex < m_nSize );
    return m_pBuffer[ nIndex ];
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE const T& CVector<T, AllocatorType, IndexType>::operator[]( uint64_t nIndex ) const
{
    assert( nIndex < m_nSize );
    return m_pBuffer[ nIndex ];
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE T *CVector<T, AllocatorType, IndexType>::ReleaseOwnership( uint64_t& nSize, uint64_t& nAllocated )
{
    T *tmp = m_pBuffer;
    m_pBuffer = NULL;
    nSize = m_nSize;
    nAllocated = m_nAllocated;
    return tmp;
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE T *CVector<T, AllocatorType, IndexType>::GetBuffer( void )
{
    return m_pBuffer;
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE const T *CVector<T, AllocatorType, IndexType>::GetBuffer( void ) const
{
    return m_pBuffer;
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE T& CVector<T, AllocatorType, IndexType>::First( void )
{
    return *m_pBuffer;
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE const T& CVector<T, AllocatorType, IndexType>::First( void ) const
{
    return *m_pBuffer;
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE T& CVector<T, AllocatorType, IndexType>::Last( void )
{
    return *( m_pBuffer + ( m_nSize - 1 ) );
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE const T& CVector<T, AllocatorType, IndexType>::Last( void ) const
{
    return *( m_pBuffer + ( m_nSize - 1 ) );
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE typename CVector<T, AllocatorType, IndexType>::iterator CVector<T, AllocatorType, IndexType>::Find( const T& value )
{
    for ( auto it = begin(); it != end(); it++ ) {
        if ( *it == value ) {
            return it;
        }
    }
    return end();
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE typename CVector<T, AllocatorType, IndexType>::const_iterator CVector<T, AllocatorType, IndexType>::Find( const T& value ) const
{
    for ( const auto it = begin(); it != end(); it++ ) {
        if ( *it == value ) {
            return it;
        }
    }
    return end();
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE typename CVector<T, AllocatorType, IndexType>::iterator CVector<T, AllocatorType, IndexType>::begin( void )
{
    return m_pBuffer;
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE typename CVector<T, AllocatorType, IndexType>::iterator CVector<T, AllocatorType, IndexType>::end( void )
{
    return m_pBuffer + m_nSize;
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE typename CVector<T, AllocatorType, IndexType>::const_iterator CVector<T, AllocatorType, IndexType>::cbegin( void ) const
{
    return begin();
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE typename CVector<T, AllocatorType, IndexType>::const_iterator CVector<T, AllocatorType, IndexType>::cend( void ) const
{
    return end();
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE typename CVector<T, AllocatorType, IndexType>::const_iterator CVector<T, AllocatorType, IndexType>::begin( void ) const
{
    return begin();
}

template<typename T, typename AllocatorType, typename IndexType>
SIRENGINE_FORCEINLINE typename CVector<T, AllocatorType, IndexType>::const_iterator CVector<T, AllocatorType, IndexType>::end( void ) const
{
    return end();
}

template<typename T, typename AllocatorType, typename IndexType>
void CVector<T, AllocatorType, IndexType>::EnsureAllocated( IndexType nItems )
{
    const IndexType newSize = m_nSize + nItems;

    if ( newSize >= m_nAllocated ) {
        m_nAllocated *= 2;
        void *newBuffer = m_Allocator.allocate( sizeof( T ) * m_nAllocated );
        if ( m_pBuffer ) {
            memcpy( newBuffer, m_pBuffer, m_nSize * sizeof( T ) );
            m_Allocate.deallocate( m_pBuffer );
        }
        m_pBuffer = (T *)newBuffer;
    }
}

#endif