#ifndef __SIRENGINE_MUTEX_HPP__
#define __SIRENGINE_MUTEX_HPP__

#pragma once

#include <Engine/Core/SIREngine.h>

class CThreadMutex
{
public:
    inline CThreadMutex( void )
    { g_pApplication->MutexInit( (void *)&m_hLock ); }
    inline ~CThreadMutex()
    { g_pApplication->MutexShutdown( (void *)&m_hLock ); }

    inline void Lock( void )
    { g_pApplication->MutexLock( (void *)&m_hLock ); }
    inline void Unlock( void )
    { g_pApplication->MutexUnlock( (void *)&m_hLock ); }
    inline bool TryLock( void )
    { return g_pApplication->MutexTryLock( (void *)&m_hLock ); }

    inline void Lock( void ) const
    { const_cast<CThreadMutex *>( this )->Lock(); }
    inline void Unlock( void ) const
    { const_cast<CThreadMutex *>( this )->Unlock(); }
    inline bool TryLock( void ) const
    { return const_cast<CThreadMutex *>( this )->TryLock(); }
private:
    PlatformTypes::mutex_t m_hLock;
};

class CThreadRWLock
{
public:
    inline CThreadRWLock( void )
    { g_pApplication->MutexRWInit( (void *)&m_hRWLock ); }
    inline ~CThreadRWLock()
    { g_pApplication->MutexRWShutdown( (void *)&m_hRWLock ); }

    inline void WriteLock( void )
    { g_pApplication->MutexWriteLock( (void *)&m_hRWLock ); }
    inline void ReadLock( void )
    { g_pApplication->MutexReadLock( (void *)&m_hRWLock ); }
    inline void Unlock( void )
    { g_pApplication->MutexRWUnlock( (void *)&m_hRWLock ); }
    inline bool TryWriteLock( void )
    { return g_pApplication->MutexRWTryWriteLock( (void *)&m_hRWLock ); }
    inline bool TryReadLock( void )
    { return g_pApplication->MutexRWTryReadLock( (void *)&m_hRWLock ); }
private:
    PlatformTypes::rwlock_t m_hRWLock;
};

#endif