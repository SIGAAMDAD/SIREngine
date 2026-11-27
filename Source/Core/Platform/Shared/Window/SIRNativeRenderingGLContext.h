/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef __SIR_NATIVE_RENDERING_GL_CONTEXT_H__
#define __SIR_NATIVE_RENDERING_GL_CONTEXT_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Memory/Pointers/SIRSharedPtr.h>
#include "SIRNativeRenderingContext.h"
#include "SIRNativeWindowDriver.h"

enum class SIRGLProfile : uint8_t
{
	Core,
	Compatibility,
	Embedded,

	Default = Core,
};

struct SIRGLContext
{
	uint32_t nVersionMajor;
	uint32_t nVersionMinor;

	int32_t nRedSize;
	int32_t nGreenSize;
	int32_t nBlueSize;
	int32_t nAlphaSize;

	int32_t nDepthSize;
	int32_t nStencilSize;

	bool bDebug : 1;
	bool bForwardCompatibile : 1;
	bool bRobustAccess : 1;
	bool bNoError : 1;
	bool bDoubleBuffer : 1;
};

typedef void (*glProc_t)( const char *pszProcName );

class SIRNativeRenderingGLContext : public SIRNativeRenderingContext
{
public:
	~SIRNativeRenderingGLContext() noexcept = default;

	virtual bool makeCurrent( void ) noexcept = 0;
	virtual bool releaseCurrent( void ) noexcept = 0;
	virtual bool swapBuffers( void ) noexcept = 0;
	virtual bool isCurrent( void ) const noexcept = 0;

	virtual glProc_t getProcFunc( void ) noexcept = 0;

	const SIRGLContext& getContext( void ) const noexcept;

protected:
	explicit SIRNativeRenderingGLContext( const SIRGLContext& context ) noexcept
		: m_Context( context )
	{
	}

	SIRGLContext m_Context;
};

SIR_FORCEINLINE const SIRGLContext& SIRNativeRenderingGLContext::getContext( void ) const noexcept
{
	return m_Context;
}

#endif // __SIR_NATIVE_RENDERING_GL_CONTEXT_H__
