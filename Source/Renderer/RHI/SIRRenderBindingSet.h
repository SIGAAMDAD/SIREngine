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

#ifndef __SIR_RENDER_BINDING_SET_H__
#define __SIR_RENDER_BINDING_SET_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRRenderTypes.h>

// Shader-visible bindings referencing resource handles and views.
// Nonowning, zero-allocation typed façade; the device owns resource lifetime.
class SIRRenderBindingSet
{
public:
	constexpr explicit SIRRenderBindingSet( void ) noexcept = default;

	sirGraphicsHandle_t getHandle( void ) const noexcept
	{
		return m_hHandle;
	}
	bool isValid( void ) const noexcept
	{
		return m_hHandle.nGeneration != 0 && m_hHandle.nDeviceGeneration != 0 && m_hHandle.eType == SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_BINDING_SET;
	}

private:
	sirGraphicsHandle_t m_hHandle {};
};

#endif // __SIR_RENDER_BINDING_SET_H__
