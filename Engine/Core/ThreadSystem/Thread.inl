
SIRENGINE_FORCEINLINE CThreadMutex::CThreadMutex( void )
{
#if defined(SIRENGINE_PLATFORM_WINDOWS)
#elif defined(SIRENGINE_PLATFORM_POSIX)
    if ( pthread_mutexattr_init( &m_hAttribs ) == -1 ) {
        SIRENGINE_ERROR( "pthread_mutexattr_init failed: %s", strerror( errno ) );
    }
    if ( pthread_mutexattr_settype( &m_hAttribs, PTHREAD_MUTEX_RECURSIVE ) == -1 ) {
        SIRENGINE_ERROR( "pthread_mutexattr_settype failed: %s", strerror( errno ) );
    }
    if ( pthread_mutex_init( &m_hLock, &m_hAttribs ) == -1 ) {
        SIRENGINE_ERROR( "pthread_mutex_init failed: %s", strerror( errno ) );
    }
#endif
}

SIRENGINE_FORCEINLINE CThreadMutex::~CThreadMutex()
{
#if defined(SIRENGINE_PLATFORM_WINDOWS)
#elif defined(SIRENGINE_PLATFORM_POSIX)
    if ( pthread_mutexattr_destroy( &m_hAttribs ) == -1 ) {
        SIRENGINE_ERROR( "pthread_mutexattr_destroy failed: %s", strerror( errno ) );
    }
    if ( pthread_mutex_destroy( &m_hLock ) == -1 ) {
        SIRENGINE_ERROR( "pthread_mutex_destroy failed: %s", strerror( errno ) );
    }
#endif
}

SIRENGINE_FORCEINLINE void CThreadMutex::Lock( void )
{
#if defined(SIRENGINE_PLATFORM_WINDOWS)
#elif defined(SIRENGINE_PLATFORM_POSIX)
    if ( pthread_mutex_lock( &m_hLock ) == -1 ) {

    }
#endif
}

SIRENGINE_FORCEINLINE void CThreadMutex::Unlock( void )
{
#if defined(SIRENGINE_PLATFORM_WINDOWS)
#elif defined(SIRENGINE_PLATFORM_POSIX)
    if ( pthread_mutex_unlock( &m_hLock) == -1 ) {

    }
#endif
}

SIRENGINE_FORCEINLINE bool CThreadMutex::TryLock( void )
{
#if defined(SIRENGINE_PLATFORM_WINDOWS)
#elif defined(SIRENGINE_PLATFORM_POSIX)
    return pthread_mutex_trylock( &m_hLock );
#endif
}

SIRENGINE_FORCEINLINE void CThreadMutex::Lock( void ) const
{
#if defined(SIRENGINE_PLATFORM_WINDOWS)
#elif defined(SIRENGINE_PLATFORM_POSIX)
    const_cast<CThreadMutex *>( this )->Lock();
#endif
}

SIRENGINE_FORCEINLINE void CThreadMutex::Unlock( void ) const
{
#if defined(SIRENGINE_PLATFORM_WINDOWS)
#elif defined(SIRENGINE_PLATFORM_POSIX)
    const_cast<CThreadMutex *>( this )->Unlock();
#endif
}

SIRENGINE_FORCEINLINE bool CThreadMutex::TryLock( void ) const
{
#if defined(SIRENGINE_PLATFORM_WINDOWS)
#elif defined(SIRENGINE_PLATFORM_POSIX)
    return const_cast<CThreadMutex *>( this )->TryLock();
#endif
}

SIRENGINE_FORCEINLINE CThreadRWLock::CThreadRWLock( void )
{
#if defined(SIRENGINE_PLATFORM_WINDOWS)
#elif defined(SIRENGINE_PLATFORM_POSIX)
    if ( pthread_rwlockattr_init( &m_hAttribs ) == -1 ) {
    }
    if ( pthread_rwlock_init( &m_hRWLock, &m_hAttribs ) == -1 ) {
    }
#endif
}

SIRENGINE_FORCEINLINE CThreadRWLock::~CThreadRWLock()
{
#if defined(SIRENGINE_PLATFORM_WINDOWS)
#elif defined(SIRENGINE_PLATFORM_POSIX)
    if ( pthread_rwlockattr_destroy( &m_hAttribs ) == -1 ) {
    }
    if ( pthread_rwlock_destroy( &m_hRWLock ) == -1 ) {
    }
#endif
}

SIRENGINE_FORCEINLINE void CThreadRWLock::WriteLock( void )
{
#if defined(SIRENGINE_PLATFORM_WINDOWS)
#elif defined(SIRENGINE_PLATFORM_POSIX)
    if ( pthread_rwlock_wrlock( &m_hRWLock ) == -1 ) {
    }
#endif
}

SIRENGINE_FORCEINLINE void CThreadRWLock::ReadLock( void )
{
#if defined(SIRENGINE_PLATFORM_WINDOWS)
#elif defined(SIRENGINE_PLATFORM_POSIX)
    if ( pthread_rwlock_rdlock( &m_hRWLock ) == -1 ) {
    }
#endif
}

SIRENGINE_FORCEINLINE void CThreadRWLock::Unlock( void )
{
#if defined(SIRENGINE_PLATFORM_WINDOWS)
#elif defined(SIRENGINE_PLATFORM_POSIX)
    if ( pthread_rwlock_unlock( &m_hRWLock ) == -1 ) {
    }
#endif
}

SIRENGINE_FORCEINLINE bool CThreadRWLock::TryWriteLock( void )
{
#if defined(SIRENGINE_PLATFORM_WINDOWS)
#elif defined(SIRENGINE_PLATFORM_POSIX)
    return pthread_rwlock_trywrlock( &m_hRWLock );
#endif
}

SIRENGINE_FORCEINLINE bool CThreadRWLock::TryReadLock( void )
{
#if defined(SIRENGINE_PLATFORM_WINDOWS)
#elif defined(SIRENGINE_PLATFORM_POSIX)
    return pthread_rwlock_tryrdlock( &m_hRWLock );
#endif
}