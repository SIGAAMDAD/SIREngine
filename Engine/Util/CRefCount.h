#ifndef __CREFCOUNT_H__
#define __CREFCOUNT_H__

#include <Engine/Core/SIREngine.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <EASTL/atomic.h>

class CRefCount
{
public:
    CRefCount( void );
    CRefCount( CRefCount& other );
    ~CRefCount();

    const CRefCount& operator=( CRefCount& other );

    CRefCount *GetOwner( void );

    void AddRef( void );
    void DecrementRef( void );

    int GetRefCount( void ) const;
private:
    CRefCount( const CRefCount& other ) = delete;
    CRefCount( CRefCount&& other ) = delete;

    const CRefCount& operator=( const CRefCount& other ) = delete;
    const CRefCount& operator=( CRefCount&& other ) = delete;

    eastl::atomic<int> m_nRefCount;
    CRefCount *m_pOwner;
};

SIRENGINE_FORCEINLINE CRefCount::CRefCount( void )
    : m_nRefCount( 1 )
{
}

SIRENGINE_FORCEINLINE CRefCount::CRefCount( CRefCount& other )
    : m_nRefCount( 1 ), m_pOwner( eastl::addressof( other ) )
{
    other.AddRef();
}

SIRENGINE_FORCEINLINE CRefCount::~CRefCount()
{
    DecrementRef();
}

SIRENGINE_FORCEINLINE const CRefCount& CRefCount::operator=( CRefCount& other )
{
    if ( m_pOwner != NULL ) {
        m_pOwner->DecrementRef();
    }

    m_pOwner = eastl::addressof( other );
    m_pOwner->AddRef();
}

SIRENGINE_FORCEINLINE CRefCount *CRefCount::GetOwner( void )
{
    return m_pOwner;
}

SIRENGINE_FORCEINLINE void CRefCount::AddRef( void )
{
    if ( m_pOwner != NULL ) {
        m_pOwner->DecrementRef();
    }
    m_nRefCount.fetch_add( 1 );
}

SIRENGINE_FORCEINLINE void CRefCount::DecrementRef( void )
{
    if ( m_pOwner != NULL ) {
        m_pOwner->DecrementRef();
    }
    m_nRefCount.fetch_sub( 1 );
}

SIRENGINE_FORCEINLINE int CRefCount::GetRefCount( void ) const
{
    return m_nRefCount.load();
}

#endif