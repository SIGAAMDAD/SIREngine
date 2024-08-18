#ifndef __MEMORY_HPP__
#define __MEMORY_HPP__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Core/Compiler.h>
#include <Engine/Core/Config.h>
#include <Engine/Core/Logging/Logger.h>

typedef enum {
    MEMTAG_RENDERER,
    MEMTAG_SOUNDSYSTEM,
    MEMTAG_FILESYSTEM,
    MEMTAG_CVARS,
    MEMTAG_THREADS,

    MEMTAG_COUNT
} MemTag_t;

#if defined(SIRENGINE_DEBUG)
#define Mem_Alloc( ... ) Mem_AllocDebug( __VA_ARGS__, __FILE__, __FUNC__, __LINE__ )
#define Mem_ClearedAlloc( size ) Mem_ClearedAllocDebug( size, __FILE__, __FUNC__, __LINE__ )
#define Mem_Free( ptr ) Mem_FreeDebug( ptr, __FILE__, __FUNC__, __LINE__ )
void SIRENGINE_DLL_EXPORT *Mem_AllocDebug( size_t nBytes, size_t nAlignment, const char *pFileName, const char *pFunction, uint64_t nLineNumber );
void SIRENGINE_DLL_EXPORT *Mem_AllocDebug( size_t nBytes, const char *pFileName, const char *pFunction, uint64_t nLineNumber );
void SIRENGINE_DLL_EXPORT *Mem_ClearedAllocDebug( size_t nBytes, const char *pFileName, const char *pFunction, uint64_t nLineNumber );
void SIRENGINE_DLL_EXPORT *Mem_RellocDebug( void *pOriginalPointer, size_t nBytes, const char *pFileName, const char *pFunction, uint64_t nLineNumber );
void SIRENGINE_DLL_EXPORT Mem_FreeDebug( void *pMemory, const char *pFileName, const char *pFunction, uint64_t nLineNumber );
#else
void SIRENGINE_DLL_EXPORT *Mem_Alloc( size_t nBytes );
void SIRENGINE_DLL_EXPORT *Mem_Alloc( size_t nBytes, size_t nAlignment );
void SIRENGINE_DLL_EXPORT *Mem_ClearedAlloc( size_t nBytes );
void SIRENGINE_DLL_EXPORT *Mem_Realloc( void *pOriginalPointer, size_t nBytes );
void SIRENGINE_DLL_EXPORT Mem_Free( void *pMemory );
#endif

void SIRENGINE_DLL_EXPORT Mem_Init( void );
void SIRENGINE_DLL_EXPORT Mem_Shutdown( void );

SIRENGINE_DECLARE_LOG_CATEGORY( Memory, ELogLevel::Warning );
#include "MemAlloc.h"

#endif