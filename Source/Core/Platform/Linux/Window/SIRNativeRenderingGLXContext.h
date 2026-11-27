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

#ifndef __SIR_NATIVE_GLX_CONTEXT_H__
#define __SIR_NATIVE_GLX_CONTEXT_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Platform/Shared/Window/SIRNativeRenderingContext.h>
#include <Core/Platform/Shared/Window/SIRNativeRenderingGLContext.h>
#include <Core/Platform/Shared/Window/SIRNativeWindowDriver.h>
#include <GL/glx.h>

class SIRNativeRenderingGLXContext final : public SIRNativeRenderingGLContext
{
public:
	explicit SIRNativeRenderingGLXContext( Display *pDisplay, Window hWindow, const SIRGLContext& context ) noexcept;
	~SIRNativeRenderingGLXContext() noexcept;

	virtual SIRNativeRenderingContextKind getKind( void ) const noexcept override;
	virtual SIRNativeRenderingContextState getState( void ) const noexcept override;

	virtual bool makeCurrent( void ) noexcept override;
	virtual bool releaseCurrent( void ) noexcept override;
	virtual bool swapBuffers( void ) noexcept override;
	virtual bool isCurrent( void ) const noexcept override;

	virtual glProc_t getProcFunc( void ) noexcept override;

private:
	Display *m_pDisplay;
	GLXContext m_hContext;
	GLXDrawable m_hGLXWindow;
};

#endif // _SIR_NATIVE_GLX_CONTENT_H__
