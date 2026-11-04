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

#ifndef __SIRENGINE_SIRGRAPHICSREGISTRY_H__
#define __SIRENGINE_SIRGRAPHICSREGISTRY_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Renderer/SIRRenderingDevice.h>

typedef struct sirGraphicsSelection_s
{
	sirGraphicsBackend_t eBackend;
	sirGraphicsVersion_t nMinimumApiVersion;
	uint64_t nRequiredFeatures;
	bool bAllowSoftware;
} sirGraphicsSelection_t;

// Nonowning registration. Startup code registers an explicit API/version
// implementation; no dynamic allocation, hidden fallback or global singleton.
class SIRGraphicsRegistry
{
private:
	static constexpr uint32_t s_nDriverCapacity = 16;
	SIRGraphicsDriver *m_pszDrivers[s_nDriverCapacity] = {};
	uint32_t m_nDriverCount = 0;

public:
	SIRGraphicsRegistry( void ) noexcept = default;
	SIRGraphicsRegistry( const SIRGraphicsRegistry & ) = delete;
	SIRGraphicsRegistry &operator=( const SIRGraphicsRegistry & ) = delete;

	sirGraphicsResult_t registerDriver( SIRGraphicsDriver *pDriver ) noexcept;
	sirGraphicsResult_t unregisterDriver( sirGraphicsBackend_t eBackend ) noexcept;
	SIRGraphicsDriver *findDriver( sirGraphicsBackend_t eBackend ) const noexcept;
	sirGraphicsResult_t selectDriver( const sirGraphicsSelection_t &selection, SIRGraphicsDriver **ppDriver,
	                                  sirGraphicsAdapterInfo_t *pAdapter ) const noexcept;
	uint32_t getCount( void ) const noexcept;
	uint32_t getCapacity( void ) const noexcept;
};

#endif // __SIRENGINE_SIRGRAPHICSREGISTRY_H__
