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

#ifndef __SIR_NATIVE_RENDERING_CONTEXT_H__
#define __SIR_NATIVE_RENDERING_CONTEXT_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include "SIRNativeWindowDriver.h"

enum SIRNativeRenderingContextKind : uint8_t
{
	OpenGL,
	Vulkan,
	DXGI,
	Direct3D9,
	Direct3D8,
	DirectDraw
};

enum SIRNativeRenderingContextState : uint8_t
{
	Uninitialized,
	Initializing,
	Ready,
	Suspended,
	Lost,
	Retiring,
	Destroyed
};

enum SIRNativeRenderingContextResult : uint8_t
{
	Success,
	InvalidArgument,
	InvalidWindow,
	Unsupported,
	NotReady,
	DeviceLost,
	SurfaceLost,
	Failure
};

class SIRNativeRenderingContext
{
public:
	~SIRNativeRenderingContext() noexcept = default;

	virtual SIRNativeRenderingContextKind getKind( void ) const noexcept = 0;
	virtual SIRNativeRenderingContextState getState( void ) const noexcept = 0;

	SIRWindowHandle getWindow( void ) const noexcept;

protected:
	explicit SIRNativeRenderingContext( void ) noexcept = default;

	SIRWindowHandle m_hWindow;
};

#endif // __SIR_NATIVE_RENDERING_CONTEXT_H__
