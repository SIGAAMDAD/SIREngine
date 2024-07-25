#ifndef __MEMORY_HPP__
#define __MEMORY_HPP__

#pragma once

#include <Engine/Core/SIREngine.hpp>

#define MEMTAG_RENDERER     0
#define MEMTAG_SOUNDSYSTEM  1
#define MEMTAG_FILESYSTEM   2
#define MEMTAG_CVARS        3
#define MEMTAG_THREADS      4

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
void SIRENGINE_DLL_EXPORT Mem_CreateTag( uint64_t iTag, const char *pszName );
void SIRENGINE_DLL_EXPORT Mem_ClearTagMemory( uint64_t iTag );
void SIRENGINE_DLL_EXPORT Mem_DeleteTag( uint64_t iTag );

#endif