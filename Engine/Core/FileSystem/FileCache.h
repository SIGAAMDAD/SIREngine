#ifndef __SIRENGINE_FILECACHE_H__
#define __SIRENGINE_FILECACHE_H__

#include <Engine/Core/SIREngine.h>

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Core/ThreadSystem/Thread.h>

namespace FileSystem {
    class CFileCache
    {
    public:
        CFileCache( void );

        CThreadMutex m_hLock;

        
    };
};

#endif