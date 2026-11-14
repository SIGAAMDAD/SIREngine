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

#ifndef __SIRENGINE_SIRRENDERACCELERATIONSTRUCTURE_H__
#define __SIRENGINE_SIRRENDERACCELERATIONSTRUCTURE_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRRenderTypes.h>

// Optional GPU ray-tracing object. The backing SIRRenderStorageBuffer is
// borrowed; the acceleration-structure object is not another asset copy.
// Vulkan may allocate native AS metadata; D3D12 uses its buffer storage.
class SIRRenderAccelerationStructure final
{
public:
	SIRRenderAccelerationStructure( void ) noexcept = default;
	SIRRenderAccelerationStructure( sirGraphicsHandle_t hStructure, sirGraphicsHandle_t hBacking ) noexcept
	    : m_hStructure( hStructure ), m_hBacking( hBacking )
	{
	}

	sirGraphicsHandle_t getHandle( void ) const noexcept
	{
		return m_hStructure;
	}
	sirGraphicsHandle_t getBackingBuffer( void ) const noexcept
	{
		return m_hBacking;
	}
	bool isValid( void ) const noexcept
	{
		return m_hStructure.nGeneration != 0 && m_hStructure.nDeviceGeneration != 0 &&
		       m_hStructure.eType == SIR_RENDER_HANDLE_ACCELERATION_STRUCTURE && m_hBacking.nGeneration != 0 &&
		       m_hBacking.nDeviceGeneration == m_hStructure.nDeviceGeneration && m_hBacking.eType == SIR_GRAPHICS_HANDLE_BUFFER;
	}

private:
	sirGraphicsHandle_t m_hStructure = {};
	sirGraphicsHandle_t m_hBacking = {};
};

#endif // __SIRENGINE_SIRRENDERACCELERATIONSTRUCTURE_H__
