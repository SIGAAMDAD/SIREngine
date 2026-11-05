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

#ifndef __SIRENGINE_SIRCONSOLESYSTEM_H__
#define __SIRENGINE_SIRCONSOLESYSTEM_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Console/SIRConsoleService.h>
#include <Core/Systems/SIREngineSystem.h>

// Borrowed dependencies and caller-owned, SIRMemory-backed console storage.
// Register upstream systems before this system in SIRApplication.
typedef struct sirConsoleSystemConfig_s
{
	SIRRegistrationCatalog *pRegistration;
	SIRCVarRegistry *pCVars;
	SIRStringPool *pPool;
	sirConsoleStorage_t storage;
	uint32_t nPumpBudget;
} sirConsoleSystemConfig_t;

// Pumps console commands on the application owner thread. Provider-specific
// latched CVars and sink notifications remain at their explicit safe points.
class SIRConsoleSystem final : public SIREngineSystem
{
public:
	explicit SIRConsoleSystem( const sirConsoleSystemConfig_t& config ) noexcept;
	~SIRConsoleSystem( void ) noexcept override;

	bool isInitialized( void ) const noexcept;
	SIRConsoleService& getConsole( void ) noexcept;
	const sirConsolePumpReport_t& getLastPumpReport( void ) const noexcept;
	sirRegistrationResult_t getLastPumpResult( void ) const noexcept;

	sirSystemResult_t initialize( void ) noexcept override;
	void update( float fDeltaSeconds ) noexcept override;
	sirSystemResult_t shutdown( void ) noexcept override;

protected:
private:
	sirConsoleSystemConfig_t m_Config;
	SIRConsoleService m_Service;
	sirConsolePumpReport_t m_LastPumpReport;
	sirRegistrationResult_t m_eLastPumpResult;
};

#endif // __SIRENGINE_SIRCONSOLESYSTEM_H__
