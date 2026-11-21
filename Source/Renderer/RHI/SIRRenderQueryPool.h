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

#ifndef __SIRENGINE_SIRRENDERQUERYPOOL_H__
#define __SIRENGINE_SIRRENDERQUERYPOOL_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRRenderTypes.h>

// Device-owned query slots; query data must be resolved into a buffer.
// Nonowning, zero-allocation typed façade; the device owns resource lifetime.
class SIRRenderQueryPool final
{
public:
	SIRRenderQueryPool( void ) noexcept = default;
	explicit SIRRenderQueryPool( sirGraphicsHandle_t hHandle ) noexcept : m_hHandle( hHandle )
	{
	}

	sirGraphicsHandle_t getHandle( void ) const noexcept
	{
		return m_hHandle;
	}
	bool isValid( void ) const noexcept
	{
		return m_hHandle.nGeneration != 0 && m_hHandle.nDeviceGeneration != 0 && m_hHandle.eType == SIR_RENDER_HANDLE_QUERY_POOL;
	}

private:
	sirGraphicsHandle_t m_hHandle = {};
};

#endif // __SIRENGINE_SIRRENDERQUERYPOOL_H__
