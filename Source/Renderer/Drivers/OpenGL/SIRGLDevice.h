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

#ifndef __SIR_GL_DEVICE_H__
#define __SIR_GL_DEVICE_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include "SIRGLShared.h"
#include <Renderer/RHI/SIRRenderDevice.h>

// A shared common interface for an OpenGL device context
class SIRGLDevice : public SIRRenderDevice
{
public:
	SIRGLDevice( const SIRGLDevice& ) noexcept = delete;
	SIRGLDevice( SIRGLDevice&& ) noexcept = delete;
	virtual ~SIRGLDevice() noexcept;

#include "SIRGLProcMethods.inl"

protected:
	explicit SIRGLDevice( void ) noexcept;

};

#endif // __SIR_GL_DEVICE_H__
