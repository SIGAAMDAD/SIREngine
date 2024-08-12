#ifndef __SIRENGINE_POSIX_TYPES_H__
#define __SIRENGINE_POSIX_TYPES_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

namespace SIREngine::PlatformTypes {
    typedef pthread_mutex_t mutex_t;
    typedef pthread_rwlock_t rwlock_t;
    typedef pthread_cond_t condition_variable_t;
    typedef pthread_t thread_t;
    typedef int file_t;
};

#endif