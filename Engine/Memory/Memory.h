#ifndef __MEMORY_HPP__
#define __MEMORY_HPP__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Core/SIREngine.h>

typedef enum {
    MEMTAG_RENDERER,
    MEMTAG_SOUNDSYSTEM,
    MEMTAG_FILESYSTEM,
    MEMTAG_CVARS,
    MEMTAG_THREADS,

    MEMTAG_COUNT
} MemTag_t;

#if defined(SIRENGINE_MEMORY_DEBUG)
#define Mem_Alloc( size ) Mem_AllocDebug( size, __FILE__, __FUNC__, __LINE__ )
#define Mem_Free( ptr ) Mem_FreeDebug( ptr, __FILE__, __FUNC__, __LINE__ )
void SIRENGINE_DLL_EXPORT *Mem_AllocDebug( uint64_t nBytes, const char *pFileName, const char *pFunction, uint64_t nLineNumber );
void SIRENGINE_DLL_EXPORT Mem_FreeDebug( void *pMemory, const char *pFileName, const char *pFunction, uint64_t nLineNumber );
#else
void SIRENGINE_DLL_EXPORT *Mem_Alloc( uint64_t nBytes );
void SIRENGINE_DLL_EXPORT Mem_Free( void *pMemory );
#endif

void SIRENGINE_DLL_EXPORT Mem_Init( void );
void SIRENGINE_DLL_EXPORT Mem_Shutdown( void );
void SIRENGINE_DLL_EXPORT *Mem_Alloc( size_t nBytes, size_t nAlignment );
void SIRENGINE_DLL_EXPORT Mem_Free( void *pMemory );

#include "MemAlloc.h"

#endif