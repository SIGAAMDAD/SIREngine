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

#include <Core/Console/SIRConsoleSystem.h>

SIRConsoleSystem::SIRConsoleSystem( const sirConsoleSystemConfig_t& config ) noexcept
    : m_Config( config ), m_Service(), m_LastPumpReport{}, m_eLastPumpResult( SIR_REGISTRATION_NOT_INITIALIZED )
{
}

SIRConsoleSystem::~SIRConsoleSystem( void ) noexcept = default;

bool SIRConsoleSystem::isInitialized( void ) const noexcept
{
	return m_Service.isInitialized();
}

SIRConsoleService& SIRConsoleSystem::getConsole( void ) noexcept
{
	return m_Service;
}

const sirConsolePumpReport_t& SIRConsoleSystem::getLastPumpReport( void ) const noexcept
{
	return m_LastPumpReport;
}

sirRegistrationResult_t SIRConsoleSystem::getLastPumpResult( void ) const noexcept
{
	return m_eLastPumpResult;
}

sirSystemResult_t SIRConsoleSystem::initialize( void ) noexcept
{
	if ( m_Service.isInitialized() ) {
		return SIR_SYSTEM_BUSY;
	}
	if ( m_Config.pRegistration == nullptr || m_Config.pCVars == nullptr || m_Config.pPool == nullptr ) {
		return SIR_SYSTEM_ERROR;
	}
	m_eLastPumpResult = m_Service.initialize( *m_Config.pRegistration, *m_Config.pCVars, *m_Config.pPool, m_Config.storage );
	if ( m_eLastPumpResult != SIR_REGISTRATION_OK ) {
		return SIR_SYSTEM_ERROR;
	}
	m_LastPumpReport = {};
	return SIR_SYSTEM_OK;
}

void SIRConsoleSystem::update( float fDeltaSeconds ) noexcept
{
	( void )fDeltaSeconds;
	if ( !m_Service.isInitialized() ) {
		m_eLastPumpResult = SIR_REGISTRATION_NOT_INITIALIZED;
		return;
	}
	m_eLastPumpResult = m_Service.pump( m_Config.nPumpBudget, &m_LastPumpReport );
}

sirSystemResult_t SIRConsoleSystem::shutdown( void ) noexcept
{
	// The caller must have joined producers and module callbacks before shutdown.
	m_Service.shutdown();
	m_eLastPumpResult = SIR_REGISTRATION_NOT_INITIALIZED;
	m_LastPumpReport = {};
	return SIR_SYSTEM_OK;
}
