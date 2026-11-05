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

#ifndef __SIRENGINE_SIRGLFRAMEBUFFERS_H__
#define __SIRENGINE_SIRGLFRAMEBUFFERS_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/Drivers/OpenGL/SIRGLFunctionLoader.h>
#include <Renderer/SIRGraphicsDriverABI.h>

typedef struct sirGLFramebufferDispatch_s
{
	void ( *pCreateFramebuffers )( int32_t nCount, uint32_t *pNames );
	void ( *pDeleteFramebuffers )( int32_t nCount, const uint32_t *pNames );
	void ( *pNamedFramebufferTexture )( uint32_t nFramebuffer, uint32_t nAttachment, uint32_t nTexture, int32_t nLevel );
	uint32_t ( *pCheckNamedFramebufferStatus )( uint32_t nFramebuffer, uint32_t nTarget );
	void ( *pNamedFramebufferDrawBuffer )( uint32_t nFramebuffer, uint32_t nMode );
	void ( *pNamedFramebufferReadBuffer )( uint32_t nFramebuffer, uint32_t nMode );
	uint32_t ( *pGetError )( void );
} sirGLFramebufferDispatch_t;

typedef struct sirGLFramebufferDesc_s
{
	uint32_t nColorName;
	uint32_t nColorMip;
	uint32_t nDepthName;
	uint32_t nDepthMip;
	sirGraphicsFormat_t eColorFormat;
	sirGraphicsFormat_t eDepthFormat;
} sirGLFramebufferDesc_t;

sirGraphicsResult_t sirGLLoadFramebufferDispatch( sirGLProcedureLookup_t pLookup, void *pContext,
                                                  sirGLFramebufferDispatch_t *pAPI ) noexcept;
sirGraphicsResult_t sirGLCreateNativeFramebuffer( const sirGLFramebufferDispatch_t *pAPI, const sirGLFramebufferDesc_t *pDesc,
                                                  uint32_t *pName ) noexcept;
sirGraphicsResult_t sirGLDeleteNativeFramebuffer( const sirGLFramebufferDispatch_t *pAPI, uint32_t nName ) noexcept;

#endif // __SIRENGINE_SIRGLFRAMEBUFFERS_H__
