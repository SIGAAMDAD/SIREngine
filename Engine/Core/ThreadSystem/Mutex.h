#ifndef __SIRENGINE_MUTEX_HPP__
#define __SIRENGINE_MUTEX_HPP__

#pragma once

#include <Engine/Core/SIREngine.h>

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
};

SIRENGINE_FORCEINLINE CThreadMutex::CThreadMutex( void )
{
}

#endif