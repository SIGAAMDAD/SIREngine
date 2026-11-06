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

#ifndef __SIRENGINE_SIRGLFORMATS_H__
#define __SIRENGINE_SIRGLFORMATS_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/Drivers/OpenGL/SIRGLFunctionLoader.h>
#include <Renderer/SIRGraphicsDriverABI.h>

typedef struct sirGLFormatDispatch_s
{
	void ( *pGetInternalformativ )( uint32_t nTarget, uint32_t nFormat, uint32_t nParameter, int32_t nCount, int32_t *pValues );
	uint32_t ( *pGetError )( void );
} sirGLFormatDispatch_t;

sirGraphicsResult_t sirGLLoadFormatDispatch( sirGLProcedureLookup_t pLookup, void *pContext, sirGLFormatDispatch_t *pAPI ) noexcept;
sirGraphicsResult_t sirGLQueryNativeFormat( const sirGLFormatDispatch_t *pAPI, sirGraphicsABIFormat_t *pFormat ) noexcept;

#endif // __SIRENGINE_SIRGLFORMATS_H__
