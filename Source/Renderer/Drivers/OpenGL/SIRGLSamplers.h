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

#ifndef __SIR_GLSAMPLERS_H__
#define __SIR_GLSAMPLERS_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/Drivers/OpenGL/SIRGLFunctionLoader.h>
#include <Renderer/SIRGraphicsDriverABI.h>

typedef struct sirGLSamplerDispatch_s
{
	void ( *pCreateSamplers )( int32_t nCount, uint32_t *pNames );
	void ( *pDeleteSamplers )( int32_t nCount, const uint32_t *pNames );
	void ( *pSamplerParameteri )( uint32_t nName, uint32_t nParameter, int32_t nValue );
	void ( *pSamplerParameterf )( uint32_t nName, uint32_t nParameter, float fValue );
	uint32_t ( *pGetError )( void );
} sirGLSamplerDispatch_t;

SIRGraphicsResult sirGLLoadSamplerDispatch( sirGLProcedureLookup_t pLookup, void *pContext, sirGLSamplerDispatch_t *pAPI ) noexcept;
SIRGraphicsResult sirGLCreateNativeSampler( const sirGLSamplerDispatch_t *pAPI, const sirGraphicsSamplerDesc_t *pDesc,
                                              uint32_t *pName ) noexcept;
SIRGraphicsResult sirGLDeleteNativeSampler( const sirGLSamplerDispatch_t *pAPI, uint32_t nName ) noexcept;

#endif // __SIR_GLSAMPLERS_H__
