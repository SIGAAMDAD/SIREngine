#ifndef __MEMORY_BUFFER_H__
#define __MEMORY_BUFFER_H__

#pragma once

#include <Engine/Core/SIREngine.h>
#include <Engine/Util/CVector.h>

template<typename T, typename AllocatorType = UtlMallocAllocator>
class IMemoryBuffer
{
public:
    IMemoryBuffer( void )
    { }
    IMemoryBuffer( const IMemoryBuffer<T, AllocatorType>& other )
        : m_Data( other.m_Data )
    { }
    IMemoryBuffer( IMemoryBuffer<T, AllocatorType>&& other )
        : m_Data( other.m_Data )
    { }
    virtual ~IMemoryBuffer()
    { m_Data.clear(); }

    virtual const IMemoryBuffer<T, AllocatorType>& operator=( const IMemoryBuffer<T, AllocatorType>& other );
    virtual const IMemoryBuffer<T, AllocatorType>& operator=( IMemoryBuffer<T, AllocatorType>&& other );

    virtual T *ReleaseOwnership( uint64_t& nSize, uint64_t& nAllocated );

    virtual void Set( const T *pNewData, uint64_t nItems );
    virtual void Set( const IMemoryBuffer<T, AllocatorType>& other );

    virtual T *GetBuffer( void );
    virtual const T *GetBuffer( void ) const;
    virtual CVector<T, AllocatorType>& GetObjectBuffer( void );
    virtual const CVector<T, AllocatorType>& GetObjectBuffer( void ) const;

    virtual uint64_t GetSize( void ) const;
protected:
    CVector<T, AllocatorType> m_Data;
};


template<typename T, typename AllocatorType>
inline const IMemoryBuffer<T, AllocatorType>& IMemoryBuffer<T, AllocatorType>::operator=( const IMemoryBuffer<T, AllocatorType>& other )
{
    m_Data = other.m_Data;
    return *this;
}

template<typename T, typename AllocatorType>
inline const IMemoryBuffer<T, AllocatorType>& IMemoryBuffer<T, AllocatorType>::operator=( IMemoryBuffer<T, AllocatorType>&& other )
{
    m_Data = other.m_Data;
    return *this;
}

template<typename T, typename AllocatorType>
inline T *IMemoryBuffer<T, AllocatorType>::ReleaseOwnership( uint64_t& nSize, uint64_t& nAllocated )
{
    return NULL;
//    return m_Data.ReleaseOwnership( nSize, nAllocated );
}

template<typename T, typename AllocatorType>
inline void IMemoryBuffer<T, AllocatorType>::Set( const T *pNewData, uint64_t nItems )
{
    m_Data.clear();
    m_Data.insert( m_Data.end(), pNewData, pNewData + nItems );
}

template<typename T, typename AllocatorType>
inline void IMemoryBuffer<T, AllocatorType>::Set( const IMemoryBuffer<T, AllocatorType>& other )
{
    m_Data = other.m_Data;
}

template<typename T, typename AllocatorType>
inline T *IMemoryBuffer<T, AllocatorType>::GetBuffer( void )
{
    return m_Data.data();
}

template<typename T, typename AllocatorType>
inline const T *IMemoryBuffer<T, AllocatorType>::GetBuffer( void ) const
{
    return m_Data.data();
}

template<typename T, typename AllocatorType>
inline CVector<T, AllocatorType>& IMemoryBuffer<T, AllocatorType>::GetObjectBuffer( void )
{
    return m_Data;
}

template<typename T, typename AllocatorType>
inline const CVector<T, AllocatorType>& IMemoryBuffer<T, AllocatorType>::GetObjectBuffer( void ) const
{
    return m_Data;
}

template<typename T, typename AllocatorType>
inline uint64_t IMemoryBuffer<T, AllocatorType>::GetSize( void ) const
{
    return m_Data.size();
}

template<typename T>
class CImmutableMemoryBuffer : public IMemoryBuffer<T>
{};

template<typename T>
class CExternalMemoryBuffer : public IMemoryBuffer<T>
{};

template<typename T>
class CSharedMemoryBuffer : public IMemoryBuffer<T>
{};

#endif