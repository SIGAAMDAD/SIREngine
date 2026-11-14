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

#ifndef __SIRENGINE_SIRRENDERSWAPCHAIN_H__
#define __SIRENGINE_SIRRENDERSWAPCHAIN_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRRenderingDevice.h>
#include <Renderer/SIRRenderTypes.h>

class SIRRenderingDevice;

// Surface/window generation belongs to the platform subsystem. This facade
// borrows the device-owned surface; native swapchain/images stay backend-owned.
class SIRRenderSwapchain final
{
public:
	SIRRenderSwapchain( void ) noexcept = default;
	SIRRenderSwapchain( SIRRenderingDevice& device, sirGraphicsHandle_t hSurface ) noexcept : m_pDevice( &device ), m_hSurface( hSurface )
	{
	}

	sirGraphicsHandle_t getHandle( void ) const noexcept
	{
		return m_hSurface;
	}
	bool isValid( void ) const noexcept
	{
		return m_pDevice != nullptr && m_hSurface.nGeneration != 0 && m_hSurface.nDeviceGeneration != 0 &&
		       m_hSurface.eType == SIR_GRAPHICS_HANDLE_SURFACE;
	}

	sirGraphicsResult_t acquire( sirGraphicsSurfaceImage_t *pImage ) noexcept;
	sirGraphicsResult_t present( const sirGraphicsSurfaceImage_t& image, const sirGraphicsCompletion_t& completion ) noexcept;
	sirGraphicsResult_t resize( const sirGraphicsNativeSurface_t& native, const sirGraphicsPresentDesc_t& desc ) noexcept;

private:
	SIRRenderingDevice *m_pDevice = nullptr;
	sirGraphicsHandle_t m_hSurface = {};
};

#endif // __SIRENGINE_SIRRENDERSWAPCHAIN_H__
