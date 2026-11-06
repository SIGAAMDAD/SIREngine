/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIRENGINE_SIRGLBUFFERS_H__
#define __SIRENGINE_SIRGLBUFFERS_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/Drivers/OpenGL/SIRGLFunctionLoader.h>
#include <Renderer/SIRGraphicsDriverABI.h>

typedef struct sirGLBufferDispatch_s
{
	void ( *pCreateBuffers )( int32_t nCount, uint32_t *pNames );
	void ( *pNamedBufferStorage )( uint32_t nName, intptr_t nBytes, const void *pData, uint32_t nFlags );
	void ( *pDeleteBuffers )( int32_t nCount, const uint32_t *pNames );
	uint32_t ( *pGetError )( void );
} sirGLBufferDispatch_t;

sirGraphicsResult_t sirGLLoadBufferDispatch( sirGLProcedureLookup_t pLookup, void *pContext, sirGLBufferDispatch_t *pAPI ) noexcept;
sirGraphicsResult_t sirGLCreateNativeBuffer( const sirGLBufferDispatch_t *pAPI, const sirGraphicsBufferDesc_t *pDesc,
                                             uint32_t *pName ) noexcept;
sirGraphicsResult_t sirGLDeleteNativeBuffer( const sirGLBufferDispatch_t *pAPI, uint32_t nName ) noexcept;

#endif // __SIRENGINE_SIRGLBUFFERS_H__
