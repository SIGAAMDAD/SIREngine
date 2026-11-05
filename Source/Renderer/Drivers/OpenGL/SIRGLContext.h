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

#ifndef __SIRENGINE_SIRGLCONTEXT_H__
#define __SIRENGINE_SIRGLCONTEXT_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRGraphicsDriverABI.h>

typedef struct sirGLNativeContext_s sirGLNativeContext_t;

typedef struct sirGLProbe_s
{
	uint32_t nMajor;
	uint32_t nMinor;
	uint32_t nCoreProfileMask;
	uint32_t nMaximumTextureSize;
	uint32_t nMaximumColorAttachments;
	uint32_t nMaximumUniformBindings;
	uint32_t nMaximumStorageBindings;
	uint32_t nMaximumSamples;
	char szVendor[128];
	char szRenderer[128];
} sirGLProbe_t;

sirGraphicsResult_t sirGLCreateNativeContext( const sirGraphicsABIHost_t *pHost, sirGLNativeContext_t **ppContext ) noexcept;
void sirGLDestroyNativeContext( sirGLNativeContext_t *pContext ) noexcept;
sirGraphicsResult_t sirGLReadNativeProbe( sirGLNativeContext_t *pContext, sirGLProbe_t *pProbe ) noexcept;
bool sirGLContextOwnsThread( const sirGLNativeContext_t *pContext ) noexcept;
void *sirGLResolveNativeProcedure( void *pUserData, const char *pszName ) noexcept;

#endif // __SIRENGINE_SIRGLCONTEXT_H__
