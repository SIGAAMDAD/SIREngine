#ifndef __SIRENGINE_THREAD_H__
#define __SIRENGINE_THREAD_H__

#include <Engine/Core/SIREngine.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

class CThread
{
public:
    CThread( void )
        : m_hThreadID( 0 ), m_hFunction( NULL )
    { }
    ~CThread()
    { }

    inline  void SetFunction( ThreadFunc_t pThreadFunc )
    { m_hFunction = pThreadFunc; }
    inline void Start( void )
    { g_pApplication->ThreadStart( (void *)&m_hThreadID, m_hFunction ); }
    inline void Join( uint64_t nTimeout = SIRENGINE_UINT64_MAX )
    { g_pApplication->ThreadJoin( (void *)&m_hThreadID, nTimeout ); }
private:
    PlatformTypes::thread_t m_hThreadID;
    ThreadFunc_t m_hFunction;
};

#endif