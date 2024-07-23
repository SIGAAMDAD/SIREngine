#ifndef __MEMORY_HPP__
#define __MEMORY_HPP__

#pragma once

#include <Engine/Core/SIREngine.hpp>

void SIRENGINE_DLL_EXPORT Mem_Init( void );
void SIRENGINE_DLL_EXPORT Mem_Shutdown( void );
void SIRENGINE_DLL_EXPORT *Mem_Alloc( uint64_t nBytes );
void SIRENGINE_DLL_EXPORT Mem_Free( void *pMemory );
void SIRENGINE_DLL_EXPORT Mem_CreateTag( int iTag, const char *pszName );
void SIRENGINE_DLL_EXPORT Mem_ClearTagMemory( int iTag );
void SIRENGINE_DLL_EXPORT Mem_DeleteTag( int iTag );

#endif