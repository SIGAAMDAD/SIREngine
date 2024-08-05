#ifndef __SIRENGINE_THREAD_H__
#define __SIRENGINE_THREAD_H__

#include <Engine/Core/SIREngine.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif


#include <Engine/Core/SIREngine.h>
#include <Engine/Core/Application/GenericPlatform/GenericApplication.h>
#include <Engine/Core/Logging/Logger.h>

#if defined(SIRENGINE_PLATFORM_WINDOWS)
#else
    #include <Engine/Core/Application/Posix/PosixTypes.h>
    #include <errno.h>
    #include <pthread.h>
#endif

#include <EASTL/atomic.h>

template<typename T>
using CThreadAtomic = eastl::atomic<T>;

class CThreadSpinLock
{
public:
};

class CThreadMutex
{
public:
    CThreadMutex( void );
    ~CThreadMutex();

    void Lock( void );
    void Unlock( void );
    bool TryLock( void );

    void Lock( void ) const;
    void Unlock( void ) const;
    bool TryLock( void ) const;
private:
    PlatformTypes::mutex_t m_hLock;
#if defined(SIRENGINE_PLATFORM_POSIX)
    pthread_mutexattr_t m_hAttribs;
#endif
};

class CThreadRWLock
{
public:
    CThreadRWLock( void );
    ~CThreadRWLock();

    void WriteLock( void );
    void ReadLock( void );
    void Unlock( void );
    bool TryWriteLock( void );
    bool TryReadLock( void );
private:
    PlatformTypes::rwlock_t m_hRWLock;
#if defined(SIRENGINE_PLATFORM_POSIX)
    pthread_rwlockattr_t m_hAttribs;
#endif
};

template<typename Mutex>
class CThreadAutoLock
{
public:
    SIRENGINE_FORCEINLINE CThreadAutoLock( Mutex& mutex )
        : m_pLock( eastl::addressof( mutex ) )
    { Lock(); }
    SIRENGINE_FORCEINLINE ~CThreadAutoLock()
    { Unlock(); }

    SIRENGINE_FORCEINLINE void Lock( void )
    { m_pLock->Lock(); }
    SIRENGINE_FORCEINLINE void Unlock( void )
    { m_pLock->Unlock(); }
private:
    Mutex *m_pLock;
};

class CThreadAutoWriteLock
{
public:
    SIRENGINE_FORCEINLINE CThreadAutoWriteLock( CThreadRWLock& mutex )
        : m_pLock( eastl::addressof( mutex ) )
    { Lock(); }
    SIRENGINE_FORCEINLINE ~CThreadAutoWriteLock()
    { Unlock(); }

    SIRENGINE_FORCEINLINE void Lock( void )
    { m_pLock->WriteLock(); }
    SIRENGINE_FORCEINLINE void Unlock( void )
    { m_pLock->Unlock(); }
private:
    CThreadRWLock *m_pLock;
};

class CThreadAutoReadLock
{
public:
    SIRENGINE_FORCEINLINE CThreadAutoReadLock( CThreadRWLock& mutex )
        : m_pLock( eastl::addressof( mutex ) )
    { Lock(); }
    SIRENGINE_FORCEINLINE ~CThreadAutoReadLock()
    { Unlock(); }

    SIRENGINE_FORCEINLINE void Lock( void )
    { m_pLock->ReadLock(); }
    SIRENGINE_FORCEINLINE void Unlock( void )
    { m_pLock->Unlock(); }
private:
    CThreadRWLock *m_pLock;
};

class CThread
{
public:
    CThread( const CString& name )
        : m_Name( name ), m_hThreadID( 0 )
    {
#ifdef USE_SMMALLOC
        _sm_allocator_thread_cache_create( g_pMemAlloc, sm::CacheWarmupOptions::CACHE_HOT, { 256, 256, 256, 256 } );
#endif
    }
    ~CThread()
    {
#ifdef USE_SMMALLOC
        _sm_allocator_thread_cache_destroy( g_pMemAlloc );
#endif
    }

    SIRENGINE_FORCEINLINE const CString& GetName( void ) const
    { return m_Name; }

    template<typename Fn, typename... Args>
    inline void Start( Fn&& fn, Args&&... args )
    {
        m_RunFunc = [&]( void ) -> void { fn( std::forward<Args>( args )... ); };
        g_pApplication->ThreadStart( (void *)&m_hThreadID, this, &CThread::RunThread );
    }
    inline void Join( uint64_t nTimeout = SIRENGINE_UINT64_MAX )
    { g_pApplication->ThreadJoin( (void *)&m_hThreadID, this, nTimeout ); }

    // should only ever be called by IGenericApplication
    inline void RunThread( void )
    { m_RunFunc(); }
private:
    CString m_Name;
    PlatformTypes::thread_t m_hThreadID;
    eastl::function<void()> m_RunFunc;
};

#include "Thread.inl"

#endif