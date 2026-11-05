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

#ifndef __SIRENGINE_SIRD3D11DRIVER_H__
#define __SIRENGINE_SIRD3D11DRIVER_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRGraphicsRegistry.h>

// Registration is explicit and caller-owned; no implicit platform selection.
// Device discovery, creation, and COM ownership are introduced by D11-02.
class SIRD3D11Driver final : public SIRGraphicsDriver
{
public:
	SIRD3D11Driver( void ) noexcept = default;
	~SIRD3D11Driver( void ) noexcept override = default;

	sirGraphicsBackend_t getBackend( void ) const noexcept override;
	sirGraphicsResult_t getAdapterCount( uint32_t *pCount ) const noexcept override;

	sirGraphicsResult_t queryAdapter( uint32_t nAdapterIndex, sirGraphicsAdapterInfo_t *pInfo ) const noexcept override;
	sirGraphicsResult_t createDevice( const sirGraphicsDeviceCreateDesc_t& desc, SIRRenderingDevice **ppDevice ) noexcept override;
	sirGraphicsResult_t destroyDevice( SIRRenderingDevice *pDevice ) noexcept override;

protected:
private:
};

sirGraphicsResult_t sirD3D11RegisterDriver( SIRGraphicsRegistry *pRegistry, SIRD3D11Driver *pDriver ) noexcept;

#endif // __SIRENGINE_SIRD3D11DRIVER_H__
